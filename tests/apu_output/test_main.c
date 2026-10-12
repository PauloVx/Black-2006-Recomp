/* Execute the complete production backend against a fake XAudio2 graph. The
 * fake keeps PCM pointers alive until consumption or DestroyVoice, checks
 * graph teardown order, and forbids resource calls from the engine callback. */
#include "apu_xaudio2.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#define STDMETHODCALLTYPE
#define COINIT_MULTITHREADED 0
#define XAUDIO2_DEFAULT_PROCESSOR 0
#define XAUDIO2_DEFAULT_FREQ_RATIO 2.0f
#define XAUDIO2_COMMIT_NOW 0
#define XAUDIO2_VOICE_NOSAMPLESPLAYED 1
#define WAVE_FORMAT_PCM 1
#define FAILED(x) ((x) < 0)
#define SUCCEEDED(x) ((x) >= 0)
#define RPC_E_CHANGED_MODE ((HRESULT)-2)
typedef long HRESULT;
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef uint64_t ULONGLONG;
typedef unsigned char BYTE;
typedef struct { unsigned short wFormatTag, nChannels; unsigned long nSamplesPerSec,
    nAvgBytesPerSec; unsigned short nBlockAlign, wBitsPerSample, cbSize; } WAVEFORMATEX;
typedef struct { unsigned BuffersQueued; uint64_t SamplesPlayed; } XAUDIO2_VOICE_STATE;
typedef struct { unsigned AudioBytes; const BYTE *pAudioData; } XAUDIO2_BUFFER;
typedef struct IXAudio2EngineCallback IXAudio2EngineCallback;
typedef struct { void (*OnProcessingPassStart)(IXAudio2EngineCallback *);
    void (*OnProcessingPassEnd)(IXAudio2EngineCallback *);
    void (*OnCriticalError)(IXAudio2EngineCallback *, HRESULT); } IXAudio2EngineCallbackVtbl;
struct IXAudio2EngineCallback { IXAudio2EngineCallbackVtbl *lpVtbl; };
typedef struct IXAudio2SourceVoice IXAudio2SourceVoice;
typedef struct IXAudio2MasteringVoice IXAudio2MasteringVoice;
typedef struct { void (*DestroyVoice)(IXAudio2SourceVoice *); } SourceVtbl;
typedef struct { void (*DestroyVoice)(IXAudio2MasteringVoice *); } MasterVtbl;
struct IXAudio2SourceVoice { SourceVtbl *lpVtbl; };
struct IXAudio2MasteringVoice { MasterVtbl *lpVtbl; };
typedef struct IXAudio2 {
    unsigned id, alive;
    IXAudio2EngineCallback *callback;
    char teardown[8]; unsigned teardown_size;
} IXAudio2;
typedef struct {
    IXAudio2SourceVoice iface;
    IXAudio2 *engine;
    unsigned alive, queued, starts, stops;
    uint64_t played;
    XAUDIO2_BUFFER queue[3];
    int16_t expected[3];
} FakeSource;
typedef struct { IXAudio2MasteringVoice iface; IXAudio2 *engine; unsigned alive; } FakeMaster;
static IXAudio2 engines[64];
static FakeSource sources[64];
static FakeMaster masters[64];
static unsigned checks, failures, calls, creates, co_inits, co_uninits;
static unsigned fail_stage, fail_submit, fail_start, fail_stop, critical_on_create, critical_on_register;
static DWORD thread_id=42;
static ULONGLONG clock_ms;
static ULONGLONG create_delay_ms;
static HRESULT co_result;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr,"Failed line %u: %s\n",(unsigned)__LINE__,#x); } } while(0)
static void owner_call(void) { ++calls; CHECK(thread_id==42); }
static void event(IXAudio2 *e, char c)
{ CHECK(e->teardown_size<sizeof(e->teardown)-1); e->teardown[e->teardown_size++]=c; }
static void validate(FakeSource *v)
{
    CHECK(v->alive && v->engine->alive);
    for (unsigned j=0;j<v->queued;++j) {
        const int16_t *p=(const int16_t*)v->queue[j].pAudioData;
        for (unsigned i=0;i<v->queue[j].AudioBytes/sizeof(*p);++i)
            CHECK(p[i]==v->expected[j]);
    }
}
static void destroy_source(IXAudio2SourceVoice *voice)
{
    owner_call(); FakeSource *v=(FakeSource*)voice; validate(v);
    CHECK(!v->engine->callback); event(v->engine,'S');
    v->alive=0; v->queued=0;
}
static void destroy_master(IXAudio2MasteringVoice *voice)
{
    owner_call(); FakeMaster *v=(FakeMaster*)voice;
    CHECK(v->alive && !sources[v->engine->id].alive);
    event(v->engine,'M'); v->alive=0;
}
static SourceVtbl source_vtable={destroy_source};
static MasterVtbl master_vtable={destroy_master};
static HRESULT fake_co_init(void *unused, unsigned mode)
{ (void)unused; (void)mode; owner_call(); ++co_inits; return co_result; }
static void fake_co_uninit(void) { owner_call(); ++co_uninits; }
static HRESULT fake_create(IXAudio2 **out, unsigned flags, unsigned processor)
{
    (void)flags; (void)processor; owner_call(); CHECK(creates<63); ++creates;
    clock_ms+=create_delay_ms;
    *out=NULL; if (fail_stage==1) return -1;
    IXAudio2 *e=&engines[creates]; e->id=creates; e->alive=1; *out=e; return 0;
}
static HRESULT fake_register(IXAudio2 *e, IXAudio2EngineCallback *cb)
{
    owner_call(); CHECK(e->alive && !e->callback);
    if (fail_stage==2) return -1;
    e->callback=cb;
    if (critical_on_register) {
        DWORD previous=thread_id; thread_id=7;
        cb->lpVtbl->OnCriticalError(cb,-56); thread_id=previous;
    }
    return 0;
}
static void fake_unregister(IXAudio2 *e, IXAudio2EngineCallback *cb)
{ owner_call(); CHECK(e->alive && e->callback==cb); e->callback=NULL; event(e,'R'); }
static void fake_release(IXAudio2 *e)
{
    owner_call(); CHECK(e->alive && !e->callback && !sources[e->id].alive && !masters[e->id].alive);
    event(e,'E'); e->alive=0;
}
static HRESULT fake_master(IXAudio2 *e, IXAudio2MasteringVoice **out, unsigned channels,
    unsigned rate, unsigned flags, void *device, void *effects, unsigned category)
{
    (void)flags; (void)device; (void)effects; (void)category; owner_call();
    CHECK(channels==2 && rate==48000); *out=NULL; if (fail_stage==3) return -1;
    FakeMaster *v=&masters[e->id]; v->iface.lpVtbl=&master_vtable; v->engine=e; v->alive=1;
    *out=&v->iface; return 0;
}
static HRESULT fake_source(IXAudio2 *e, IXAudio2SourceVoice **out, WAVEFORMATEX *format,
    unsigned flags, float ratio, void *cb, void *sends, void *effects)
{
    (void)flags; (void)ratio; (void)cb; (void)sends; (void)effects; owner_call();
    CHECK(format->nChannels==2 && format->nSamplesPerSec==48000 && format->nBlockAlign==4);
    *out=NULL; if (fail_stage==4) return -1;
    FakeSource *v=&sources[e->id]; v->iface.lpVtbl=&source_vtable; v->engine=e; v->alive=1;
    *out=&v->iface;
    if (critical_on_create) {
        DWORD previous=thread_id; thread_id=7;
        e->callback->lpVtbl->OnCriticalError(e->callback,-55); thread_id=previous;
    }
    return 0;
}
static void fake_state(IXAudio2SourceVoice *voice, XAUDIO2_VOICE_STATE *state, unsigned flags)
{
    (void)flags; owner_call(); FakeSource *v=(FakeSource*)voice; validate(v);
    state->BuffersQueued=v->queued; state->SamplesPlayed=v->played;
}
static HRESULT fake_submit(IXAudio2SourceVoice *voice, XAUDIO2_BUFFER *b, void *unused)
{
    (void)unused; owner_call(); FakeSource *v=(FakeSource*)voice; validate(v);
    if (fail_submit) return -1;
    CHECK(v->queued<3); if (v->queued>=3) return -1;
    v->queue[v->queued]=*b; v->expected[v->queued]=*(const int16_t*)b->pAudioData; ++v->queued;
    return 0;
}
static HRESULT fake_start(IXAudio2SourceVoice *voice, unsigned flags, unsigned op)
{
    (void)flags; (void)op; owner_call(); FakeSource *v=(FakeSource*)voice; validate(v);
    if (fail_start) return -1; ++v->starts; return 0;
}
static HRESULT fake_stop(IXAudio2SourceVoice *voice, unsigned flags, unsigned op)
{
    (void)flags; (void)op; owner_call(); FakeSource *v=(FakeSource*)voice; validate(v);
    CHECK(!v->queued); if (fail_stop) return -1; ++v->stops; return 0;
}
static LONG atomic_exchange(volatile LONG *p,LONG value) { LONG old=*p; *p=value; return old; }
static LONG atomic_increment(volatile LONG *p) { return ++*p; }
static LONG atomic_compare(volatile LONG *p,LONG value,LONG expected)
{ LONG old=*p; if (old==expected) *p=value; return old; }
#define CoInitializeEx fake_co_init
#define CoUninitialize fake_co_uninit
#define GetCurrentThreadId() thread_id
#define GetTickCount64() clock_ms
#define XAudio2Create fake_create
#define IXAudio2_RegisterForCallbacks fake_register
#define IXAudio2_UnregisterForCallbacks fake_unregister
#define IXAudio2_CreateMasteringVoice fake_master
#define IXAudio2_CreateSourceVoice fake_source
#define IXAudio2_Release fake_release
#define IXAudio2SourceVoice_GetState fake_state
#define IXAudio2SourceVoice_SubmitSourceBuffer fake_submit
#define IXAudio2SourceVoice_Start fake_start
#define IXAudio2SourceVoice_Stop fake_stop
#define InterlockedExchange atomic_exchange
#define InterlockedIncrement atomic_increment
#define InterlockedCompareExchange atomic_compare
#include "backend.inc"

static void reset(void)
{
    thread_id=42; xa2_shutdown();
    memset(engines,0,sizeof(engines)); memset(sources,0,sizeof(sources)); memset(masters,0,sizeof(masters));
    creates=co_inits=co_uninits=calls=0; fail_stage=fail_submit=fail_start=fail_stop=critical_on_create=critical_on_register=0;
    clock_ms=create_delay_ms=0; co_result=0;
}
static FakeSource *current(void) { CHECK(g_xa2_source!=NULL); return (FakeSource*)g_xa2_source; }
static int packet(int value)
{
    int16_t samples[2048]; for (unsigned i=0;i<2048;++i) samples[i]=(int16_t)value;
    return xa2_submit_samples(samples,1024);
}
static void consume(int expected)
{
    FakeSource *v=current(); validate(v); CHECK(v->queued>0); CHECK(v->expected[0]==expected);
    CHECK(v->queue[0].AudioBytes==1024*4); v->played+=1024;
    --v->queued; memmove(v->queue,v->queue+1,v->queued*sizeof(v->queue[0]));
    memmove(v->expected,v->expected+1,v->queued*sizeof(v->expected[0]));
}
static void critical(HRESULT error)
{
    IXAudio2EngineCallback *cb=g_xa2->callback; unsigned before=calls;
    thread_id=7; cb->lpVtbl->OnCriticalError(cb,error); thread_id=42;
    CHECK(calls==before); /* Callback must only publish atomics. */
}
static Xa2OutputStats stats(void) { Xa2OutputStats s; xa2_get_output_stats(&s); return s; }
static void fifo_and_priming(void)
{
    reset(); CHECK(xa2_init());
    for (unsigned p=1;p<=3;++p) { CHECK(packet(p)); CHECK(current()->starts==(p>=2 ? 1u:0u)); }
    CHECK(stats().submitted_samples==3072); CHECK(!packet(9)); CHECK(stats().queue_full==1);
    consume(1); CHECK(packet(4)); consume(2); consume(3); consume(4);
    fail_submit=1; int next=g_xa2_next_buf;
    CHECK(!packet(4)); CHECK(next==g_xa2_next_buf && current()->queued==0);
    CHECK(stats().submitted_samples==4096 && stats().submit_failed==1);
    CHECK(current()->stops==1 && stats().reprimes==1 && !g_xa2_started);
    CHECK(!xa2_submit_samples(NULL,1024)); CHECK(!xa2_submit_samples((int16_t*)g_xa2_bufs,-1));
    CHECK(!xa2_submit_samples((int16_t*)g_xa2_bufs,0));
    fail_submit=0; fail_start=1;
    CHECK(packet(4)); CHECK(!packet(4)); CHECK(!g_xa2_started && stats().start_failed==1);
    fail_start=0; CHECK(packet(4)); CHECK(current()->queued==3 && current()->starts==2);
    consume(4); consume(4); consume(4); fail_stop=1;
    CHECK(packet(4)); CHECK(g_xa2_started && current()->starts==2 && stats().stop_failed==1);
    consume(4); fail_stop=0;
    CHECK(packet(4)); CHECK(!g_xa2_started && current()->queued==1 && stats().reprimes==2);
    CHECK(packet(4)); CHECK(g_xa2_started && current()->starts==3);
    consume(4); consume(4); xa2_shutdown(); CHECK(co_inits==1 && co_uninits==1);
    CHECK(strcmp(engines[1].teardown,"RSME")==0); CHECK(stats().played_samples==10240);
}
static void recover_owned_graph(void)
{
    reset(); CHECK(xa2_init()); CHECK(packet(1)); CHECK(packet(2)); CHECK(packet(3));
    consume(1); CHECK(packet(4)); CHECK(stats().played_samples==1024);
    /* A full healthy voice and silent samples never recreate the graph. */
    for(unsigned i=0;i<5;++i) CHECK(!packet(0));
    CHECK(creates==1 && !stats().recovery_attempts);
    critical(-99); CHECK(packet(5));
    CHECK(strcmp(engines[1].teardown,"RSME")==0 && !sources[1].alive);
    CHECK(current()->queued==1 && !g_xa2_started && current()->expected[0]==5);
    CHECK(stats().critical_errors==1 && stats().critical_error_status==-99);
    CHECK(stats().recoveries==1 && stats().recovery_attempts==1);
    CHECK(stats().submitted_samples==5120 && stats().played_samples==1024);
    CHECK(stats().discarded_samples==3072 && co_inits==1 && !co_uninits);
    CHECK(packet(6)); CHECK(current()->starts==1); consume(5); consume(6);
    CHECK(stats().played_samples==3072); xa2_shutdown(); CHECK(co_uninits==1);
    CHECK(stats().played_samples==3072 && stats().discarded_samples==3072);
}
static void recovery_failures(void)
{
    const char *expected[]={"","","E","RE","RME"};
    for(unsigned stage=1;stage<=4;++stage) {
        reset(); CHECK(xa2_init()); CHECK(packet(10)); CHECK(packet(11));
        critical(-77); fail_stage=stage; CHECK(!packet(12));
        CHECK(xa2_is_active() && !g_xa2_source && co_inits==1 && !co_uninits);
        CHECK(strcmp(engines[1].teardown,"RSME")==0);
        CHECK(strcmp(engines[2].teardown,expected[stage])==0);
        CHECK(stats().recovery_attempts==1 && stats().recovery_failed==1);
        CHECK(stats().discarded_samples==2048);
        fail_stage=0; clock_ms=999; CHECK(!packet(12)); CHECK(creates==2);
        clock_ms=1000; CHECK(packet(12)); CHECK(creates==3 && current()->queued==1);
        CHECK(stats().recovery_attempts==2 && stats().recoveries==1);
        fail_start=1; CHECK(!packet(13)); CHECK(current()->queued==2);
        fail_start=0; CHECK(packet(14)); CHECK(current()->starts==1 && current()->queued==3);
        consume(12); consume(13); consume(14); xa2_shutdown(); CHECK(co_uninits==1);
    }
}
static void com_and_owner(void)
{
    for(unsigned stage=1;stage<=4;++stage) {
        reset(); fail_stage=stage; CHECK(!xa2_init()); CHECK(!xa2_is_active());
        CHECK(co_inits==1 && co_uninits==1 && !g_xa2 && !g_xa2_source && !g_xa2_master);
        xa2_shutdown(); CHECK(co_uninits==1);
    }
    reset(); co_result=1; CHECK(xa2_init()); xa2_shutdown(); CHECK(co_uninits==1); /* S_FALSE lease */
    reset(); co_result=RPC_E_CHANGED_MODE; CHECK(xa2_init()); xa2_shutdown(); CHECK(!co_uninits);
    reset(); co_result=-10; CHECK(!xa2_init()); CHECK(!creates && !co_uninits);
    reset(); CHECK(xa2_init()); unsigned before=calls;
    thread_id=8; CHECK(!xa2_init()); CHECK(!packet(1)); xa2_shutdown();
    CHECK(calls==before && xa2_is_active()); thread_id=42; xa2_shutdown(); CHECK(co_uninits==1);
    /* Shutdown remains balanced even while graph creation is unavailable. */
    reset(); CHECK(xa2_init()); critical(-88); fail_stage=1; CHECK(!packet(1));
    xa2_shutdown(); CHECK(!xa2_is_active() && co_uninits==1);
}

/* Compile the actual APU frame-thread routine, including its paused startup
 * path. Its output init, idle handshake and finalization share this owner. */
#define NUM_SAMPLES_PER_FRAME 32
#define NV_PAPU_SECTL 0
#define NV_PAPU_FECTL 1
#define NV_PAPU_SECTL_XCNTMODE 1
#define NV_PAPU_SECTL_XCNTMODE_OFF 0
#define NV_PAPU_FECTL_FEMETHMODE_TRAPPED 2
#define NV_PAPU_FECTL_FEMETHMODE_HALTED 4
#define GET_MASK(value,mask) ((value)&(mask))
#define qatomic_read(p) (*(p))
#define MCPX_APU_DEVICE(p) ((MCPXAPUState*)(p))
typedef int Error;
typedef struct {
    int lock, cond, idle_cond;
    bool exiting, pause_requested, is_idle, set_irq;
    uint32_t regs[2]; unsigned ep_frame_div;
    struct { bool ep_fifo_frame_valid; int16_t frame_buf[256][2]; } monitor;
} MCPXAPUState;
static struct { bool active; } g_test_tone;
static int g_mixer_active_count;
static uint64_t g_apu_frames_full, g_apu_frames_monitor;
static int locked, initialized, finalized, idle_signaled;
static char lifecycle[16]; static unsigned lifecycle_count;
static void life(char c) { CHECK(lifecycle_count<sizeof(lifecycle)-1); lifecycle[lifecycle_count++]=c; }
static void qemu_mutex_lock(int *lock) { (void)lock; owner_call(); CHECK(!locked); locked=1; life('L'); }
static void qemu_mutex_unlock(int *lock) { (void)lock; owner_call(); CHECK(locked && finalized); locked=0; life('U'); }
static void mcpx_apu_monitor_init(MCPXAPUState *d, Error **err)
{ (void)d; owner_call(); CHECK(locked && !initialized); initialized=1; *err=NULL; life('I'); CHECK(xa2_init()); }
static void warn_reportf_err(Error *err, const char *message) { (void)err; (void)message; CHECK(0); }
static void mcpx_apu_monitor_finalize(MCPXAPUState *d)
{ (void)d; owner_call(); CHECK(locked && initialized && !finalized); finalized=1; life('F'); xa2_shutdown(); }
static MCPXAPUState *frame_state;
static void qemu_cond_signal(int *cond)
{ CHECK(cond==&frame_state->idle_cond && initialized && locked && xa2_is_active()); ++idle_signaled; life('S'); }
static void qemu_cond_wait(int *cond,int *lock)
{ CHECK(cond==&frame_state->cond && lock==&frame_state->lock && frame_state->is_idle && locked); life('W'); frame_state->exiting=true; }
static void update_irq(MCPXAPUState *d) { (void)d; CHECK(locked); }
static void throttle(MCPXAPUState *d) { (void)d; CHECK(locked && initialized && !finalized); life('T'); }
static void se_frame(MCPXAPUState *d) { CHECK(locked && xa2_is_active()); life('V'); d->exiting=true; }
static void mcpx_apu_monitor_frame(MCPXAPUState *d) { CHECK(locked && xa2_is_active()); life('A'); d->exiting=true; }
#include "frame_thread.inc"
static void frame_owner_lifecycle(void)
{
    for(unsigned mode=0;mode<3;++mode) {
        reset(); MCPXAPUState d={0}; frame_state=&d;
        locked=initialized=finalized=idle_signaled=0; lifecycle_count=0; memset(lifecycle,0,sizeof(lifecycle));
        g_test_tone.active=false; g_mixer_active_count=0;
        d.pause_requested=mode==0; d.regs[NV_PAPU_SECTL]=mode==1 ? 1:0;
        CHECK(mcpx_apu_frame_thread(&d)==NULL);
        CHECK(strcmp(lifecycle,mode==0 ? "LISWFU":mode==1 ? "LITVFU":"LITAFU")==0);
        CHECK(!locked && initialized && finalized && idle_signaled==(mode==0));
        CHECK(!xa2_is_active() && co_inits==1 && co_uninits==1);
    }
}
static void critical_during_creation(void)
{
    reset(); CHECK(xa2_init()); critical(-77); critical_on_create=1;
    CHECK(!packet(1)); CHECK(!g_xa2_source && xa2_is_active());
    CHECK(stats().critical_errors==2 && stats().recovery_failed==1 && !stats().recoveries);
    CHECK(strcmp(engines[2].teardown,"RSME")==0);
    clock_ms=999; CHECK(!packet(1)); CHECK(creates==2);
    critical_on_create=0; clock_ms=1000; CHECK(packet(1)); CHECK(creates==3 && stats().recoveries==1);
    xa2_shutdown(); CHECK(co_uninits==1);
    reset(); CHECK(xa2_init()); critical(-77); critical_on_register=1; fail_stage=4;
    CHECK(!packet(1)); CHECK(stats().critical_errors==2 && stats().recovery_failed==1);
    CHECK(strcmp(engines[2].teardown,"RME")==0);
    /* Even a repeated pending signal with no live voice cannot bypass the
     * existing delay. The failing graph is already fully destroyed. */
    atomic_exchange(&g_xa2_critical_pending,1); clock_ms=500; CHECK(!packet(1)); CHECK(creates==2);
    clock_ms=999; CHECK(!packet(1)); CHECK(creates==2);
    clock_ms=1000; CHECK(!packet(1)); CHECK(creates==3 && stats().critical_errors==3);
    clock_ms=1500; CHECK(!packet(1)); CHECK(creates==3);
    fail_stage=critical_on_register=0; clock_ms=2000; CHECK(packet(1)); CHECK(creates==4);
    xa2_shutdown(); CHECK(co_uninits==1);
}
static void retry_deadlines(void)
{
    reset(); CHECK(xa2_init()); critical(-77); CHECK(packet(1)); CHECK(creates==2);
    clock_ms=100; critical(-78); CHECK(!packet(2));
    CHECK(!g_xa2_source && xa2_is_active() && creates==2);
    CHECK(stats().discarded_samples==1024 && stats().recoveries==1);
    clock_ms=999; CHECK(!packet(2)); CHECK(creates==2);
    clock_ms=1000; CHECK(packet(2)); CHECK(creates==3 && stats().recoveries==2);
    consume(2); xa2_shutdown(); CHECK(co_uninits==1);
    reset(); CHECK(xa2_init()); clock_ms=100; critical(-77); fail_stage=1; create_delay_ms=500;
    CHECK(!packet(1)); CHECK(clock_ms==600 && creates==2);
    fail_stage=0; create_delay_ms=0; clock_ms=1000; CHECK(!packet(1)); CHECK(creates==2);
    clock_ms=1599; CHECK(!packet(1)); CHECK(creates==2);
    clock_ms=1600; CHECK(packet(1)); CHECK(creates==3); xa2_shutdown(); CHECK(co_uninits==1);
}
int main(void)
{
    fifo_and_priming(); recover_owned_graph(); recovery_failures(); com_and_owner();
    critical_during_creation(); retry_deadlines(); frame_owner_lifecycle();
    printf("APU output graph: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
