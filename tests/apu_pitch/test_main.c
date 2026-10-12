/* Compile the actual production VP in this TU to exercise its private source
 * fetch/resampling and PIO routines without an audio device or game launch.
 * Expected output comes from a separate absolute-position PCM oracle. */
#include APU_VP_TEST_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core_irq.inc"

uint8_t *g_apu_ram_ptr;
MCPXAPUState *g_state;
struct McpxApuDebug g_dbg, g_dbg_cache;
int g_dbg_voice_monitor = -1;
uint64_t g_dbg_muted_voices[4];
volatile uint64_t g_apu_frames_full, g_apu_frames_monitor;

enum { VOICE = 64, VOICE_RAM = 0x1000, NOTIFY_RAM = 0xC000,
       SGE_RAM = 0x10000, SSL_RAM = 0x11000, PCM_RAM = 0x20000,
       RAM_SIZE = 512 * 1024, MAX_OUTPUT = 32768 };
static unsigned failures, checks;
static MCPXAPUState *d;
static uint8_t *ram;
static int source_length, source_channels;
static int source_adpcm;

static void check(int condition, const char *name)
{
    checks++;
    if (!condition) {
        if (failures < 24) fprintf(stderr, "FAIL %s\n", name);
        failures++;
    }
}

static int16_t source_pcm(int index, int channel)
{
    return (int16_t)((index * 79 + channel * 137) % 20001 - 10000);
}

static float reference_pcm(int index, int channel)
{
    /* ADPCM fixtures use index0 and zero nibbles, so every decoded sample
     * stays exactly at its block's predictor, without a decoder oracle. */
    if (source_adpcm) index = index / 64 * 64;
    return source_pcm(index, source_channels == 1 ? 0 : channel) / 32768.0f;
}

static void init_voice(int length, int channels, int16_t pitch)
{
    memset(d, 0, sizeof(*d));
    memset(ram, 0, RAM_SIZE);
    qemu_mutex_init(&d->lock);
    qemu_cond_init(&d->cond);
    d->ram_ptr = g_apu_ram_ptr = ram;
    d->regs[NV_PAPU_VPVADDR] = VOICE_RAM;
    d->regs[NV_PAPU_FENADDR] = NOTIFY_RAM;
    d->regs[NV_PAPU_VPSGEADDR] = SGE_RAM;
    d->regs[NV_PAPU_VPSSLADDR] = SSL_RAM;
    d->regs[NV_PAPU_TVL2D] = VOICE;
    d->regs[NV_PAPU_TVL3D] = d->regs[NV_PAPU_TVLMP] = 0xFFFF;
    d->regs[NV_PAPU_FECV] = VOICE;
    for (int page = 0; page < 32; page++)
        *(uint32_t *)(ram + SGE_RAM + page * 8) = PCM_RAM + page * 4096;
    source_length = length;
    source_channels = channels;
    source_adpcm = 0;
    for (int i = 0; i < length; i++)
        for (int ch = 0; ch < channels; ch++)
            *(int16_t *)(ram + PCM_RAM + 2 * (i * channels + ch)) = source_pcm(i, ch);
    uint32_t format = (1u << 28) | (1u << 30);
    if (channels == 2) format |= NV_PAVS_VOICE_CFG_FMT_STEREO | (1u << 16);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_CFG_FMT, 0xFFFFFFFFu, format);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_PAR_NEXT,
                   NV_PAVS_VOICE_PAR_NEXT_EBO, length - 1);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_PAR_STATE,
                   NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE, 1);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_PITCH_LINK,
                   NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH, (uint16_t)pitch);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_PITCH_LINK,
                   NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE, 0xFFFF);
    voice_reset_filters(d, VOICE);
}

static void close_voice(void)
{
    qemu_mutex_destroy(&d->lock);
}

static int active(void)
{
    return voice_get_mask(d, VOICE, NV_PAVS_VOICE_PAR_STATE,
                          NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE) != 0;
}

static float ratio_for_pitch(int16_t pitch)
{
    return 1.0f / powf(2.0f, pitch / 4096.0f);
}

static int render(float out[][2], int requested, float ratio, int split)
{
    static const int chunks[] = { 1, 7, 3, 31, 2, 17, 32, 5 };
    int count = 0, calls = 0;
    while (count < requested && active() && calls < MAX_OUTPUT) {
        int n = split ? chunks[calls % 8] : 32;
        if (n > requested - count) n = requested - count;
        int got = voice_resample(d, VOICE, &out[count], n, ratio);
        calls++;
        if (got <= 0) break;
        count += got;
    }
    return count;
}

static void compare_reference(const float out[][2], int count, double step,
                              int loop_start, const char *label)
{
    int mismatch = 0;
    for (int n = 0; n < count; n++) {
        double position = n * step;
        int index = (int)floor(position);
        float fraction = (float)(position - index);
        int next = index + 1;
        if (loop_start >= 0) {
            if (index >= source_length)
                index = loop_start + (index - source_length) % (source_length - loop_start);
            if (next >= source_length)
                next = loop_start + (next - source_length) % (source_length - loop_start);
        } else {
            if (index >= source_length) index = source_length - 1;
            if (next >= source_length) next = source_length - 1;
        }
        for (int ch = 0; ch < 2; ch++) {
            float a = reference_pcm(index, ch), b = reference_pcm(next, ch);
            float expected = a + (b - a) * fraction;
            if (fabsf(out[n][ch] - expected) > 0.000002f) mismatch++;
        }
    }
    check(mismatch == 0, label);
}

static void rate_cases(void)
{
    struct { const char *name; int16_t pitch; } cases[] = {
        { "48 kHz unity", 0 }, { "24 kHz", -4096 },
        { "22.05 kHz", -4597 }, { "32 kHz guest", -2396 },
        { "44.1 kHz guest", -501 }, { "96 kHz positive pitch", 4096 }
    };
    float (*out)[2] = calloc(MAX_OUTPUT, sizeof(*out));
    float (*split_out)[2] = calloc(MAX_OUTPUT, sizeof(*split_out));
    for (unsigned c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        for (int channels = 1; channels <= 2; channels++) {
            int length = 4099;
            float ratio = ratio_for_pitch(cases[c].pitch);
            int expected_count = (int)ceil(length * (double)ratio);
            init_voice(length, channels, cases[c].pitch);
            int got = render(out, MAX_OUTPUT, ratio, 0);
            check(got == expected_count, cases[c].name);
            check(!active(), "end deactivates only after duration");
            check(d->vp.filters[VOICE].resample_fetched == (uint64_t)length,
                  "all source frames consumed once without reading beyond EBO");
            compare_reference(out, got, 1.0 / ratio, -1, "absolute PCM interpolation oracle");
            if (cases[c].pitch == 0) {
                int exact = got == length;
                for (int n = 0; n < got; n++)
                    for (int ch = 0; ch < 2; ch++)
                        exact &= out[n][ch] == reference_pcm(n, ch);
                check(exact, "unity output is bit exact");
            }
            close_voice();
            init_voice(length, channels, cases[c].pitch);
            int split_got = render(split_out, MAX_OUTPUT, ratio, 1);
            check(split_got == got && !memcmp(out, split_out, got * sizeof(*out)),
                  "chunk splits preserve output and duration");
            close_voice();
            printf("%s %s: %d source -> %d output (expected %d)\n",
                   cases[c].name, channels == 1 ? "mono" : "stereo", length, got, expected_count);
        }
    }
    free(out);
    free(split_out);
}

static void ends_loops_pause_seek_restart(void)
{
    float out[2048][2], fresh[128][2];
    int lengths[] = { 1, 2, 31, 32, 33, 64, 65 };
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        init_voice(lengths[i], 1, -4096);
        int got = render(out, 2048, 2.0f, 1);
        check(got == 2 * lengths[i] && !active(), "inclusive EBO and slow tail lifetime");
        compare_reference(out, got, 0.5, -1, "final sample holds through final duration");
        close_voice();
    }

    init_voice(65, 2, -2396);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_LOOP, 1);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_CUR_PSH_SAMPLE,
                   NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO, 9);
    float ratio = ratio_for_pitch(-2396);
    int got = render(out, 2048, ratio, 1);
    check(got == 2048 && active(), "loop remains active across many boundaries");
    compare_reference(out, got, 1.0 / ratio, 9, "nonzero LBO loop interpolation");
    close_voice();

    init_voice(512, 1, -2396);
    got = render(out, 13, ratio, 1);
    uint32_t cbo = voice_get_mask(d, VOICE, NV_PAVS_VOICE_PAR_OFFSET,
                                  NV_PAVS_VOICE_PAR_OFFSET_CBO);
    double phase = d->vp.filters[VOICE].resample_phase;
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_PAR_STATE, NV_PAVS_VOICE_PAR_STATE_PAUSED, 1);
    check(voice_resample(d, VOICE, &out[got], 32, ratio) < 0, "paused resampler emits no samples");
    check(voice_get_mask(d, VOICE, NV_PAVS_VOICE_PAR_OFFSET,
                        NV_PAVS_VOICE_PAR_OFFSET_CBO) == cbo &&
          d->vp.filters[VOICE].resample_phase == phase, "pause preserves source and phase");
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_PAR_STATE, NV_PAVS_VOICE_PAR_STATE_PAUSED, 0);
    got += render(&out[got], 100, ratio, 1);
    compare_reference(out, got, 1.0 / ratio, -1, "resume is continuous");

    fe_method(d, NV1BA0_PIO_SET_VOICE_BUF_CBO, 101);
    check(voice_resample(d, VOICE, fresh, 1, ratio) == 1 &&
          fresh[0][0] == reference_pcm(101, 0), "active CBO seek clears lookahead");
    d->regs[NV_PAPU_TVL2D] = 0xFFFF;
    d->regs[NV_PAPU_FEAV] = 1u << 16;
    fe_method(d, NV1BA0_PIO_VOICE_ON, VOICE);
    got = render(out, 128, ratio, 1);
    compare_reference(out, got, 1.0 / ratio, -1, "VOICE_ON restarts from zero with no stale phase");
    close_voice();

    init_voice(512, 1, 0);
    check(voice_resample(d, VOICE, out, 32, 1.0f) == 32 &&
          voice_get_mask(d, VOICE, NV_PAVS_VOICE_PAR_OFFSET,
                         NV_PAVS_VOICE_PAR_OFFSET_CBO) == 32, "unity avoids extra block read-ahead");
    fe_method(d, NV1BA0_PIO_SET_VOICE_TAR_PITCH, (uint32_t)(uint16_t)-4096 << 16);
    check(voice_resample(d, VOICE, out, 32, 2.0f) == 32 &&
          out[0][0] == reference_pcm(32, 0) &&
          out[1][0] == (reference_pcm(32, 0) + reference_pcm(33, 0)) * 0.5f,
          "pitch change preserves current source position");
    close_voice();
}

static uint8_t *notifier(int ssl)
{
    return ram + NOTIFY_RAM + 16 * (MCPX_HW_NOTIFIER_BASE_OFFSET +
        VOICE * MCPX_HW_NOTIFIER_COUNT + ssl) + 15;
}

static void setup_stream(int a_length, int b_length)
{
    uint32_t fmt = voice_get_mask(d, VOICE, NV_PAVS_VOICE_CFG_FMT, 0xFFFFFFFFu);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_CFG_FMT, 0xFFFFFFFFu,
                   fmt | NV_PAVS_VOICE_CFG_FMT_DATA_TYPE | NV_PAVS_VOICE_CFG_FMT_PERSIST);
    d->vp.ssl[VOICE].base[0] = 0;
    d->vp.ssl[VOICE].count[0] = 1;
    d->vp.ssl[VOICE].base[1] = 1;
    d->vp.ssl[VOICE].count[1] = b_length ? 1 : 0;
    *(uint32_t *)(ram + SSL_RAM) = PCM_RAM;
    *(uint32_t *)(ram + SSL_RAM + 4) = a_length | (1u << 16);
    *(uint32_t *)(ram + SSL_RAM + 8) = PCM_RAM + a_length * 2;
    *(uint32_t *)(ram + SSL_RAM + 12) = b_length | (1u << 16);
}

static void stream_cases(void)
{
    float out[256][2];
    init_voice(2, 1, -4096);
    setup_stream(1, 1);
    check(voice_resample(d, VOICE, out, 1, 2.0f) == 1, "one sample SSL first half");
    check(*notifier(0) == 0, "SSL A notification waits for sample duration");
    check(voice_resample(d, VOICE, &out[1], 1, 2.0f) == 1, "one sample SSL second half");
    check(*notifier(0) == NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS,
          "SSL A notifies at playback boundary");
    d->vp.ssl[VOICE].count[0] = 0;
    check(render(&out[2], 2, 2.0f, 1) == 2, "SSL B joins without missing endpoint");
    compare_reference(out, 4, 0.5, -1, "SSL interpolation and endpoint samples");
    close_voice();

    init_voice(1, 1, -32768);
    setup_stream(1, 0);
    check(render(out, 255, 256.0f, 1) == 255 && *notifier(0) == 0,
          "SSL slow sample survives multiple VP frames before notify");
    check(render(&out[255], 1, 256.0f, 1) == 1 &&
          *notifier(0) == NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS,
          "SSL extreme ratio notifies after 256 outputs");
    close_voice();

    init_voice(2, 1, -4096);
    setup_stream(1, 0);
    check(render(out, 2, 2.0f, 1) == 2, "persistent stream exhausts current SSL");
    check(*notifier(0) == NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS,
          "starved next SSL cannot block current SSL DONE");
    d->vp.ssl[VOICE].count[0] = 0;
    check(voice_resample(d, VOICE, &out[2], 1, 2.0f) < 0 && active(),
          "persistent starvation returns without hanging or ending");
    d->vp.ssl[VOICE].count[1] = 1;
    *(uint32_t *)(ram + SSL_RAM + 12) = 1 | (1u << 16);
    check(render(&out[2], 2, 2.0f, 1) == 2, "persistent SSL resumes after supply");
    check(out[0][0] == reference_pcm(0, 0) && out[1][0] == reference_pcm(0, 0) &&
          out[2][0] == reference_pcm(1, 0) && out[3][0] == reference_pcm(1, 0),
          "stream starvation holds endpoint and preserves source timeline");
    close_voice();

    init_voice(1, 1, -4096);
    setup_stream(1, 0);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_PERSIST, 0);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_PAR_STATE,
                   NV_PAVS_VOICE_PAR_STATE_EACUR, NV_PAVS_VOICE_PAR_STATE_EFCUR_RELEASE);
    check(render(out, 8, 2.0f, 1) == 2 && !active(),
          "nonpersistent stream drains fractional final sample");
    close_voice();
}

static void production_frame_case(void)
{
    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME] = { { 0 } };
    init_voice(1, 1, -4096);
    /* Only bin0 is audible, volume0=unity. Other bins have full attenuation. */
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLA, 0xFFFFFFFFu, 0xFFFF000Fu);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLB, 0xFFFFFFFFu, 0xFFFFFFFFu);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLC, 0xFFFFFFFFu, 0xFFFFFFFFu);
    qemu_mutex_lock(&d->lock);
    mcpx_apu_vp_frame(d, mixbins);
    qemu_mutex_unlock(&d->lock);
    check(!active(), "production frame completes short static voice");
    check(mixbins[0][0] == reference_pcm(0, 0) &&
          mixbins[0][1] == reference_pcm(0, 0) && mixbins[0][2] == 0.0f,
          "production voice_process mixes final slow tail before inactive");
    close_voice();

    /* The real VP walker also owns signed pitch extraction and passes its
     * computed ratio to resampling. Exercise the two pitches seen in BLACK. */
    int16_t guest_pitches[] = { -2396, -501 };
    for (unsigned p = 0; p < sizeof(guest_pitches) / sizeof(guest_pitches[0]); p++) {
        float out[512][2];
        init_voice(257, 1, guest_pitches[p]);
        voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLA, 0xFFFFFFFFu, 0xFFFF000Fu);
        voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLB, 0xFFFFFFFFu, 0xFFFFFFFFu);
        voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLC, 0xFFFFFFFFu, 0xFFFFFFFFu);
        int frames = 0;
        while (active() && frames < 16) {
            memset(mixbins, 0, sizeof(mixbins));
            qemu_mutex_lock(&d->lock);
            mcpx_apu_vp_frame(d, mixbins);
            qemu_mutex_unlock(&d->lock);
            for (int n = 0; n < 32; n++)
                out[frames * 32 + n][0] = out[frames * 32 + n][1] = mixbins[0][n];
            frames++;
        }
        float ratio = ratio_for_pitch(guest_pitches[p]);
        int count = (int)ceil(257 * (double)ratio);
        check(frames == (count + 31) / 32 && !active(),
              "production VP guest pitch lifetime follows output duration");
        compare_reference(out, count, 1.0 / ratio, -1,
                          "production VP applies real signed guest pitch");
        int silent_tail = 1;
        for (int n = count; n < frames * 32; n++) silent_tail &= out[n][0] == 0.0f;
        check(silent_tail, "production VP zero pads final partial frame");
        close_voice();
    }
}

static void adpcm_cases(void)
{
    float out[512][2];
    int16_t pitches[] = { 0, -4096, -2396 };
    for (int channels = 1; channels <= 2; channels++) {
        for (unsigned p = 0; p < sizeof(pitches) / sizeof(pitches[0]); p++) {
            init_voice(128, channels, pitches[p]);
            source_adpcm = 1;
            memset(ram + PCM_RAM, 0, 2 * 36 * channels);
            for (int block = 0; block < 2; block++)
                for (int ch = 0; ch < channels; ch++)
                    *(int16_t *)(ram + PCM_RAM + block * 36 * channels + ch * 4) =
                        source_pcm(block * 64, ch);
            voice_set_mask(d, VOICE, NV_PAVS_VOICE_CFG_FMT,
                           NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE,
                           NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_ADPCM);
            float ratio = ratio_for_pitch(pitches[p]);
            int got = render(out, 512, ratio, 1);
            check(got == (int)ceil(128 * (double)ratio) && !active(),
                  "mono/stereo ADPCM playback duration and tail");
            compare_reference(out, got, 1.0 / ratio, -1,
                              "actual ADPCM decode plus fractional block-boundary oracle");
            close_voice();
        }
    }
}

#include "test_trap_queue.inc"

int main(void)
{
    d = calloc(1, sizeof(*d));
    ram = calloc(1, RAM_SIZE);
    if (!d || !ram) return 2;
    rate_cases();
    ends_loops_pause_seek_restart();
    stream_cases();
    production_frame_case();
    adpcm_cases();
    idle_trap_queue_case();
    locked_trap_queue_case();
    irq_unlocks_trap_queue_case();
    free(ram);
    free(d);
    if (failures) {
        fprintf(stderr, "apu-pitch: %u failures in %u checks\n", failures, checks);
        return 1;
    }
    printf("apu-pitch: ALL PASS (%u checks)\n", checks);
    return 0;
}
