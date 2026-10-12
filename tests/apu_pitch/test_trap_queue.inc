/* A completed short sound must not cut ongoing sounds out of its final VP
 * tick. Its one IDLE trap also supplies the guest's unlink cursor, which must
 * remain paired with the payload while the other voices are queued. */
static void init_trap_queue_voice(void)
{
    init_voice(96, 1, 0);
    for (int n = 0; n < 96; ++n)
        *(int16_t *)(ram + PCM_RAM + n * 2) = 4000;
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLA, 0xffffffffu, 0xffff000f);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLB, 0xffffffffu, 0xffffffffu);
    voice_set_mask(d, VOICE, NV_PAVS_VOICE_TAR_VOLC, 0xffffffffu, 0xffffffffu);
    for (int v = 65; v <= 69; ++v) {
        memcpy(ram + VOICE_RAM + v * NV_PAVS_SIZE,
               ram + VOICE_RAM + VOICE * NV_PAVS_SIZE, NV_PAVS_SIZE);
        voice_reset_filters(d, (uint16_t)v);
    }
    voice_set_mask(d, 65, NV_PAVS_VOICE_PAR_STATE,
                   NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE, 0);
    voice_set_mask(d, 67, NV_PAVS_VOICE_PAR_STATE,
                   NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE, 0);
    for (int v = 64; v <= 67; ++v)
        voice_set_mask(d, (uint16_t)v, NV_PAVS_VOICE_TAR_PITCH_LINK,
                       NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                       v == 67 ? 0xffff : v + 1);
    voice_set_mask(d, 66, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V0BIN, 1);
    voice_set_mask(d, 69, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V0BIN, 2);
    voice_set_mask(d, 68, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V0BIN, 3);
    d->regs[NV_PAPU_TVL3D] = 69;
    d->regs[NV_PAPU_TVLMP] = 68;
    d->regs[NV_PAPU_CVL3D] = 0xeeee;
    d->regs[NV_PAPU_NVL3D] = 0xdddd;
    d->regs[NV_PAPU_CVLMP] = 0xcccc;
    d->regs[NV_PAPU_NVLMP] = 0xbbbb;
    d->regs[NV_PAPU_FETFORCE1] = NV_PAPU_FETFORCE1_SE2FE_IDLE_VOICE;
}

static void trap_queue_frame(float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    memset(mixbins, 0, sizeof(float) * NUM_MIXBINS * NUM_SAMPLES_PER_FRAME);
    qemu_mutex_lock(&d->lock);
    mcpx_apu_vp_frame(d, mixbins);
    qemu_mutex_unlock(&d->lock);
}

static void check_trap_tick(const float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    for (int bin = 0; bin < 4; ++bin)
        for (int sample = 0; sample < NUM_SAMPLES_PER_FRAME; ++sample)
            check(mixbins[bin][sample] == 4000 / 32768.0f,
                  "idle trap tick retains every sample of active voices in all lists");
}

static void idle_trap_queue_case(void)
{
    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];
    init_trap_queue_voice();
    trap_queue_frame(mixbins);
    check_trap_tick(mixbins);
    check(d->regs[NV_PAPU_FEDECPARAM] == 65 &&
          d->regs[NV_PAPU_FEDECMETH] == SE2FE_IDLE_VOICE &&
          d->regs[NV_PAPU_CVL2D] == 65 && d->regs[NV_PAPU_NVL2D] == 66,
          "first IDLE payload remains paired with its current/next unlink cursor");
    check(d->regs[NV_PAPU_CVL3D] == 0xeeee && d->regs[NV_PAPU_NVL3D] == 0xdddd &&
          d->regs[NV_PAPU_CVLMP] == 0xcccc && d->regs[NV_PAPU_NVLMP] == 0xbbbb,
          "later lists do not overwrite register cursors while trap is pending");
    check(d->vp.filters[64].resample_fetched == 32 &&
          d->vp.filters[66].resample_fetched == 32 &&
          d->vp.filters[68].resample_fetched == 32 &&
          d->vp.filters[69].resample_fetched == 32,
          "every continuing voice consumes exactly one tick");
    trap_queue_frame(mixbins);
    int silent = 1;
    for (int bin = 0; bin < NUM_MIXBINS; ++bin)
        for (int n = 0; n < NUM_SAMPLES_PER_FRAME; ++n) silent &= mixbins[bin][n] == 0;
    check(silent && d->vp.filters[66].resample_fetched == 32 &&
          d->regs[NV_PAPU_FEDECPARAM] == 65,
          "already pending trap neither advances voices nor changes payload");

    /* Model the ISR unlinking its captured voice and releasing FECTL. */
    voice_set_mask(d, 64, NV_PAVS_VOICE_TAR_PITCH_LINK,
                   NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE, 66);
    d->regs[NV_PAPU_FECTL] = 0;
    trap_queue_frame(mixbins);
    check_trap_tick(mixbins);
    check(d->regs[NV_PAPU_FEDECPARAM] == 67 &&
          d->regs[NV_PAPU_CVL2D] == 67 && d->regs[NV_PAPU_NVL2D] == 0xffff,
          "later idle voice gets its own paired trap after ISR clear");

    voice_set_mask(d, 66, NV_PAVS_VOICE_TAR_PITCH_LINK,
                   NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE, 0xffff);
    d->regs[NV_PAPU_FECTL] = 0;
    trap_queue_frame(mixbins);
    check_trap_tick(mixbins);
    check(d->vp.filters[66].resample_fetched == 96 &&
          d->regs[NV_PAPU_FEDECPARAM] == 67,
          "ISR cleanup loses no continuing source samples or pending idle voice");
    close_voice();

    /* An invalid idle cycle still obeys the existing per-list bound and
     * cannot block a valid active voice in a later list. */
    init_trap_queue_voice();
    d->regs[NV_PAPU_TVL2D] = 65;
    voice_set_mask(d, 65, NV_PAVS_VOICE_TAR_PITCH_LINK,
                   NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE, 65);
    trap_queue_frame(mixbins);
    check(d->regs[NV_PAPU_FEDECPARAM] == 65 &&
          d->regs[NV_PAPU_CVL2D] == 65 && d->regs[NV_PAPU_NVL2D] == 65 &&
          mixbins[2][0] == 4000 / 32768.0f && mixbins[3][0] == 4000 / 32768.0f &&
          d->vp.voice_work_dispatch.queue_len == 0,
          "idle cycle is bounded, preserves payload, and processes later active lists");
    close_voice();
}

typedef struct {
    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME];
} TrapWaitContext;

static DWORD WINAPI trap_wait_thread(LPVOID opaque)
{
    TrapWaitContext *ctx = opaque;
    trap_queue_frame(ctx->mixbins);
    return 0;
}

static void locked_trap_queue_case(void)
{
    for (int shutdown = 0; shutdown < 2; ++shutdown) {
        init_trap_queue_voice();
        voice_lock(d, VOICE, true);
        TrapWaitContext ctx = {{{0}}};
        HANDLE worker = CreateThread(NULL, 0, trap_wait_thread, &ctx, 0, NULL);
        if (!worker) { check(0, "create isolated locked-voice worker"); exit(2); }
        uint64_t deadline = GetTickCount64() + 2000;
        while (g_vp_waiting_on != VOICE && GetTickCount64() < deadline) Sleep(1);
        check(g_vp_waiting_on == VOICE, "queued locked voice really enters wait");
        qemu_mutex_lock(&d->lock);
        if (shutdown) d->exiting = true;
        else d->pause_requested = true;
        qemu_cond_signal(&d->cond);
        qemu_mutex_unlock(&d->lock);
        DWORD status = WaitForSingleObject(worker, 2000);
        check(status == WAIT_OBJECT_0, "pause or shutdown releases locked voice wait");
        if (status != WAIT_OBJECT_0) {
            fprintf(stderr, "locked voice failed to leave wait on %s\n",
                    shutdown ? "shutdown" : "pause");
            exit(3); /* Own isolated test process only. */
        }
        CloseHandle(worker);
        int silent = 1;
        for (int bin = 0; bin < NUM_MIXBINS; ++bin)
            for (int n = 0; n < NUM_SAMPLES_PER_FRAME; ++n)
                silent &= ctx.mixbins[bin][n] == 0;
        check(silent && d->vp.voice_work_dispatch.queue_len == 0 &&
              d->vp.filters[64].resample_fetched == 0 &&
              d->vp.filters[66].resample_fetched == 0 &&
              d->vp.filters[68].resample_fetched == 0 &&
              d->vp.filters[69].resample_fetched == 0,
              "pause or shutdown clears queue without processing any voice");
        close_voice();
    }
}

static HANDLE trap_irq_event;
static volatile LONG trap_irq_assertions;

static void trap_irq_line(int asserted)
{
    if (asserted) {
        InterlockedIncrement(&trap_irq_assertions);
        SetEvent(trap_irq_event);
    }
}

typedef struct {
    int delivered;
    int payload_and_cursors;
} TrapIrqContext;

static DWORD WINAPI trap_irq_thread(LPVOID opaque)
{
    TrapIrqContext *ctx = opaque;
    /* Model the kernel's connected ISR: it runs only after the real core
     * asserts its enabled device line, and enters the production guard. */
    if (WaitForSingleObject(trap_irq_event, 750) != WAIT_OBJECT_0) return 0;
    mcpx_apu_irq_service_begin(d);
    ctx->delivered = 1;
    ctx->payload_and_cursors =
        d->regs[NV_PAPU_FEDECMETH] == SE2FE_IDLE_VOICE &&
        d->regs[NV_PAPU_FEDECPARAM] == 65 &&
        d->regs[NV_PAPU_CVL2D] == 65 && d->regs[NV_PAPU_NVL2D] == 66;
    /* This interrupt is the sole owner allowed to release the continuing
     * voice. Its PIO write must preserve the still-pending trap payload. */
    mcpx_apu_vp_write(d, NV1BA0_PIO_VOICE_LOCK, 0, 4);
    d->regs[NV_PAPU_FECTL] = NV_PAPU_FECTL_FEMETHMODE_FREE_RUNNING;
    d->regs[NV_PAPU_ISTS] &= ~NV_PAPU_ISTS_FETINTSTS;
    update_irq(d);
    mcpx_apu_irq_service_end(d);
    return 0;
}

static void irq_unlocks_trap_queue_case(void)
{
    init_trap_queue_voice();
    d->regs[NV_PAPU_IEN] = NV_PAPU_ISTS_GINTSTS | NV_PAPU_ISTS_FETINTSTS;
    voice_lock(d, VOICE, true);
    trap_irq_assertions = 0;
    trap_irq_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!trap_irq_event) { check(0, "create IRQ delivery event"); exit(2); }
    mcpx_apu_set_irq_line(trap_irq_line);
    TrapIrqContext irq = {0};
    TrapWaitContext ctx = {{{0}}};
    HANDLE handler = CreateThread(NULL, 0, trap_irq_thread, &irq, 0, NULL);
    HANDLE worker = CreateThread(NULL, 0, trap_wait_thread, &ctx, 0, NULL);
    if (!handler || !worker) { check(0, "create IRQ continuation workers"); exit(2); }
    DWORD status = WaitForSingleObject(worker, 1000);
    int completed = status == WAIT_OBJECT_0;
    check(completed, "fresh idle trap reaches ISR before locked continuing voice wait completes");
    if (!completed) {
        /* A negative control must report the missed deadline and cancel its
         * own worker, rather than leave the suite hanging on the old wait. */
        qemu_mutex_lock(&d->lock);
        d->pause_requested = true;
        qemu_cond_signal(&d->cond);
        qemu_mutex_unlock(&d->lock);
        status = WaitForSingleObject(worker, 1000);
        if (status != WAIT_OBJECT_0) exit(3);
    }
    if (WaitForSingleObject(handler, 1000) != WAIT_OBJECT_0) exit(3);
    check(irq.delivered && trap_irq_assertions == 1,
          "enabled production core trap IRQ is published once before source processing");
    check(irq.payload_and_cursors, "ISR receives original paired idle payload and unlink cursors");
    check(d->regs[NV_PAPU_FEDECMETH] == SE2FE_IDLE_VOICE &&
          d->regs[NV_PAPU_FEDECPARAM] == 65 &&
          d->regs[NV_PAPU_CVL2D] == 65 && d->regs[NV_PAPU_NVL2D] == 66,
          "IRQ unlock and continuation preserve captured trap payload and cursors");
    check(!d->set_irq && !is_voice_locked(d, VOICE),
          "IRQ-dependent voice unlock completes without leaving a pending publication");
    check_trap_tick(ctx.mixbins);
    check(d->vp.filters[64].resample_fetched == 32 &&
          d->vp.filters[66].resample_fetched == 32 &&
          d->vp.filters[68].resample_fetched == 32 &&
          d->vp.filters[69].resample_fetched == 32,
          "IRQ unlock processes each continuing voice exactly once");
    mcpx_apu_set_irq_line(NULL);
    CloseHandle(worker);
    CloseHandle(handler);
    CloseHandle(trap_irq_event);
    trap_irq_event = NULL;
    close_voice();
}
