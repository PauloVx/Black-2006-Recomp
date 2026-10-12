#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

typedef unsigned char KIRQL;
#define XBOX_THREAD_LOCAL __declspec(thread)
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define XBOX_DEVICE_IRQL 5
#define XBOX_LOG_WARN 0
#define XBOX_LOG_HAL 0
#define xbox_log(...) ((void)0)

static volatile LONG checks, failures;
#define CHECK(condition, message) do { \
    InterlockedIncrement(&checks); \
    if (!(condition) && InterlockedIncrement(&failures) <= 16) \
        fprintf(stderr, "FAIL: %s\n", message); \
} while (0)

/* PRODUCTION_IRQL */

static __declspec(thread) uint32_t g_eax, g_ecx, g_edx, g_esp;
static uint32_t guest_memory[1024];
#define GUEST32(address) guest_memory[(uint32_t)(address) / 4u]
#define OWNER 0x200u
#define VOICE 0x500u
#define ACTIVE_HEAD 0x400u
#define COMPLETE_HEAD (OWNER + 0x4A0u)
#define PRIMARY_NODE 0x54Cu
#define COMPLETE_NODE 0x554u
#define STACK_TOP 0x100u
#define VOICE_ID 7u

static volatile LONG generated_calls, completions, irq_acks;
static HANDLE dispatch_held, dispatch_release, isr_finished;

/* PRODUCTION_COMPLETION_HELPER */

static void initialize_lists(void)
{
    GUEST32(VOICE + 8) = OWNER;
    GUEST32(ACTIVE_HEAD) = GUEST32(ACTIVE_HEAD + 4) = PRIMARY_NODE;
    GUEST32(PRIMARY_NODE) = GUEST32(PRIMARY_NODE + 4) = ACTIVE_HEAD;
    GUEST32(COMPLETE_HEAD) = GUEST32(COMPLETE_HEAD + 4) = COMPLETE_HEAD;
    GUEST32(COMPLETE_NODE) = GUEST32(COMPLETE_NODE + 4) = COMPLETE_NODE;
}

/* A synthetic generated routine supplies eligible primary removal and ABI
 * effects, then invokes the actual production completion-list helper. */
static void sub_0021C7EF_gen(void)
{
    uint32_t next, previous;
    InterlockedIncrement(&generated_calls);
    CHECK(g_current_irql == XBOX_DEVICE_IRQL, "generated callback keeps device IRQL");
    CHECK(g_holds_interrupt == 1, "generated callback retains ISR exclusion");
    CHECK(g_holds_dispatch == 1, "generated callback owns dispatch exclusion");
    if (GUEST32(PRIMARY_NODE) != PRIMARY_NODE) {
        next = GUEST32(PRIMARY_NODE);
        previous = GUEST32(PRIMARY_NODE + 4);
        GUEST32(next + 4) = previous;
        GUEST32(previous) = next;
        GUEST32(PRIMARY_NODE) = GUEST32(PRIMARY_NODE + 4) = PRIMARY_NODE;
        CHECK(black_audio_queue_completion(VOICE) == COMPLETE_HEAD,
              "production completion helper returns native head value");
        InterlockedIncrement(&completions);
    }
    g_eax = 0xCA11AB1Eu;
    g_ecx = 0xEC001122u;
    g_edx = 0xED001122u;
    g_esp += 8;
}

/* PRODUCTION_WRAPPER */

static void setup_call(void)
{
    g_eax = 0xAA55AA55u;
    g_ecx = OWNER;
    g_edx = 0xDD55DD55u;
    g_esp = STACK_TOP;
    GUEST32(g_esp) = 0xABCDEF00u;
    GUEST32(g_esp + 4) = VOICE_ID;
}

static DWORD WINAPI guest_dispatch_owner(LPVOID ignored)
{
    KIRQL saved;
    (void)ignored;
    saved = xbox_KeRaiseIrqlToDpcLevel();
    CHECK(saved == PASSIVE_LEVEL && g_holds_dispatch == 1,
          "foreign guest thread owns dispatch at level 2");
    SetEvent(dispatch_held);
    CHECK(WaitForSingleObject(dispatch_release, 2000) == WAIT_OBJECT_0,
          "guest owner receives release");
    xbox_KfLowerIrql(saved);
    CHECK(g_current_irql == PASSIVE_LEVEL && g_holds_dispatch == 0,
          "foreign guest thread balances dispatch ownership");
    return 0;
}

static DWORD WINAPI interrupt_attempt(LPVOID ignored)
{
    KIRQL saved;
    (void)ignored;
    xbox_IrqlEnterIsr(&saved);
    setup_call();
    sub_0021C7EF();
    /* The outer ISR acknowledges its trap regardless of the completion result. */
    InterlockedIncrement(&irq_acks);
    CHECK(g_esp == STACK_TOP + 8, "deferred ret4 consumes return and one argument");
    CHECK(g_eax == VOICE_ID && g_ecx == OWNER && g_edx == 0xDD55DD55u,
          "busy early return preserves original register effects");
    CHECK(g_current_irql == XBOX_DEVICE_IRQL && g_holds_interrupt == 1,
          "deferred completion leaves device IRQL and interrupt lock unchanged");
    CHECK(g_holds_dispatch == 0, "failed try acquires no dispatch ownership");
    xbox_IrqlLeaveIsr(saved);
    CHECK(g_current_irql == PASSIVE_LEVEL && g_holds_interrupt == 0,
          "ISR restores passive state after acknowledgement");
    SetEvent(isr_finished);
    return 0;
}

static void test_foreign_owner_and_retry(void)
{
    HANDLE guest, isr;
    DWORD result;
    KIRQL saved;
    initialize_lists();
    dispatch_held = CreateEventW(NULL, TRUE, FALSE, NULL);
    dispatch_release = CreateEventW(NULL, TRUE, FALSE, NULL);
    isr_finished = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(dispatch_held && dispatch_release && isr_finished, "coordination events created");
    guest = CreateThread(NULL, 0, guest_dispatch_owner, NULL, 0, NULL);
    CHECK(guest != NULL, "guest owner thread created");
    CHECK(WaitForSingleObject(dispatch_held, 2000) == WAIT_OBJECT_0,
          "guest has dispatch before ISR begins");
    isr = CreateThread(NULL, 0, interrupt_attempt, NULL, 0, NULL);
    CHECK(isr != NULL, "ISR thread created");
    result = WaitForSingleObject(isr_finished, 250);
    CHECK(result == WAIT_OBJECT_0, "completion try returns while foreign owner still holds dispatch");
    if (result == WAIT_OBJECT_0) {
        CHECK(generated_calls == 0 && completions == 0, "busy completion makes no list mutation");
        CHECK(irq_acks == 1, "ISR acknowledgement progresses during dispatch contention");
        CHECK(GUEST32(PRIMARY_NODE) == ACTIVE_HEAD && GUEST32(ACTIVE_HEAD) == PRIMARY_NODE,
              "deferred voice remains linked for a later hardware trap");
        CHECK(GUEST32(COMPLETE_HEAD) == COMPLETE_HEAD && GUEST32(COMPLETE_NODE) == COMPLETE_NODE,
              "completion list remains empty and reciprocal");
    }
    /* Always release the owner, including when a blocking implementation fails. */
    SetEvent(dispatch_release);
    CHECK(WaitForSingleObject(guest, 2000) == WAIT_OBJECT_0, "guest owner finishes");
    CHECK(WaitForSingleObject(isr, 2000) == WAIT_OBJECT_0, "ISR thread finishes");
    CloseHandle(guest); CloseHandle(isr);

    xbox_IrqlEnterIsr(&saved);
    setup_call();
    sub_0021C7EF();
    InterlockedIncrement(&irq_acks);
    CHECK(g_esp == STACK_TOP + 8 && g_eax == 0xCA11AB1Eu
              && g_ecx == 0xEC001122u && g_edx == 0xED001122u,
          "successful wrapper retains generated ABI effects");
    CHECK(g_current_irql == XBOX_DEVICE_IRQL && g_holds_dispatch == 0
              && g_holds_interrupt == 1,
          "successful wrapper balances dispatch without changing device IRQL");
    CHECK(completions == 1, "deferred voice completes once after owner releases dispatch");
    CHECK(GUEST32(ACTIVE_HEAD) == ACTIVE_HEAD && GUEST32(PRIMARY_NODE) == PRIMARY_NODE,
          "completed primary list and node are detached cleanly");
    CHECK(GUEST32(COMPLETE_HEAD) == COMPLETE_NODE && GUEST32(COMPLETE_HEAD + 4) == COMPLETE_NODE
              && GUEST32(COMPLETE_NODE) == COMPLETE_HEAD && GUEST32(COMPLETE_NODE + 4) == COMPLETE_HEAD,
          "completion insertion has reciprocal links");
    setup_call();
    sub_0021C7EF();
    CHECK(completions == 1 && GUEST32(COMPLETE_NODE) == COMPLETE_HEAD,
          "already completed primary is ineligible for duplicate completion");
    xbox_IrqlLeaveIsr(saved);
    CHECK(g_current_irql == PASSIVE_LEVEL && !g_holds_dispatch && !g_holds_interrupt,
          "retry leaves no IRQL or lock ownership behind");
    CloseHandle(dispatch_held); CloseHandle(dispatch_release); CloseHandle(isr_finished);
}

static void test_recursion_and_irql_nesting(void)
{
    KIRQL dpc_saved, isr_saved, raised_saved;
    CHECK(xbox_IrqlTryEnterDispatch(), "passive dispatch try succeeds");
    CHECK(xbox_IrqlTryEnterDispatch(), "same-owner dispatch try is recursive");
    CHECK(g_current_irql == PASSIVE_LEVEL && g_holds_dispatch == 2,
          "recursive dispatch exclusion does not raise IRQL");
    xbox_IrqlLeaveDispatch(); xbox_IrqlLeaveDispatch();
    CHECK(!g_holds_dispatch, "recursive dispatch leaves balance ownership");

    CHECK(xbox_IrqlEnterDpc(&dpc_saved), "normal DPC enters");
    CHECK(xbox_IrqlTryEnterDispatch(), "dispatch try nests within DPC");
    CHECK(g_current_irql == DISPATCH_LEVEL && g_holds_dispatch == 2,
          "DPC nesting retains dispatch IRQL");
    xbox_IrqlLeaveDispatch();
    xbox_IrqlEnterIsr(&isr_saved);
    CHECK(isr_saved == DISPATCH_LEVEL && g_current_irql == XBOX_DEVICE_IRQL,
          "ISR nests above a DPC");
    CHECK(xbox_IrqlTryEnterDispatch(), "device-level ISR can recursively take existing dispatch");
    CHECK(g_holds_dispatch == 2 && g_holds_interrupt == 1,
          "ISR completion ownership nests with DPC ownership");
    xbox_IrqlLeaveDispatch();
    xbox_IrqlLeaveIsr(isr_saved);
    CHECK(g_current_irql == DISPATCH_LEVEL && g_holds_dispatch == 1 && !g_holds_interrupt,
          "ISR exit restores the enclosing DPC");
    raised_saved = xbox_KfRaiseIrql(XBOX_DEVICE_IRQL);
    CHECK(xbox_IrqlTryEnterDispatch(), "raised device level can nest dispatch exclusion");
    xbox_IrqlLeaveDispatch();
    xbox_KfLowerIrql(raised_saved);
    xbox_IrqlLeaveDpc(dpc_saved);
    CHECK(g_current_irql == PASSIVE_LEVEL && !g_holds_dispatch && !g_holds_interrupt,
          "all nested DPC ISR and raised ownership balances");
}

/* The callback models state-based work: it reads the voice's state when the
 * deferred list is drained. The data field and callback are synthetic. */
static const uint32_t pending_voices[3] = {VOICE, 0x700u, 0x900u};
static unsigned callback_counts[3], callback_states[3], callback_order[8], callback_total;
#define SYNTHETIC_STATE_OFFSET 0x70u

static int pending_node_index(uint32_t node)
{
    unsigned i;
    for (i = 0; i < 3; i++)
        if (node == pending_voices[i] + 0x54u) return (int)i;
    return -1;
}

static void initialize_pending_list(void)
{
    unsigned i;
    GUEST32(COMPLETE_HEAD) = GUEST32(COMPLETE_HEAD + 4) = COMPLETE_HEAD;
    callback_total = 0;
    for (i = 0; i < 3; i++) {
        uint32_t voice = pending_voices[i], node = voice + 0x54u;
        GUEST32(voice + 8) = OWNER;
        GUEST32(node) = GUEST32(node + 4) = node;
        callback_counts[i] = callback_states[i] = 0;
    }
}

static void check_pending_order(const unsigned *expected, unsigned length)
{
    uint32_t node = GUEST32(COMPLETE_HEAD), previous = COMPLETE_HEAD;
    unsigned i;
    for (i = 0; i < length; i++) {
        uint32_t wanted = pending_voices[expected[i]] + 0x54u;
        CHECK(node == wanted, "pending list order remains stable");
        if (pending_node_index(node) < 0) return;
        CHECK(GUEST32(node + 4) == previous, "pending node has reciprocal previous link");
        previous = node;
        node = GUEST32(node);
    }
    CHECK(node == COMPLETE_HEAD, "pending list ends at sentinel without self-node cycle");
    CHECK(GUEST32(COMPLETE_HEAD + 4) == previous, "sentinel tail matches final pending node");
}

static unsigned drain_pending_list(void)
{
    unsigned drained = 0;
    /* Bound only this asset-free diagnostic so an old self-node cycle fails
     * deterministically instead of hanging the test process. */
    while (GUEST32(COMPLETE_HEAD) != COMPLETE_HEAD && drained < 4) {
        uint32_t node = GUEST32(COMPLETE_HEAD), next, previous;
        int index = pending_node_index(node);
        CHECK(index >= 0, "drain sees a valid synthetic voice node");
        if (index < 0) break;
        next = GUEST32(node);
        if (next != node) {
            previous = GUEST32(node + 4);
            GUEST32(next + 4) = previous;
            GUEST32(previous) = next;
            GUEST32(node) = GUEST32(node + 4) = node;
        }
        callback_counts[index]++;
        callback_states[index] = GUEST32(pending_voices[index] + SYNTHETIC_STATE_OFFSET);
        if (callback_total < 8) callback_order[callback_total] = (unsigned)index;
        callback_total++;
        drained++;
    }
    CHECK(GUEST32(COMPLETE_HEAD) == COMPLETE_HEAD,
          "drain terminates after consuming pending nodes");
    return drained;
}

static void test_duplicate_pending_completion(void)
{
    static const unsigned single[] = {0};
    static const unsigned all[] = {0, 1, 2};
    static const unsigned middle[] = {1};
    uint32_t links[6];
    unsigned i, drained;
    CHECK(xbox_IrqlTryEnterDispatch(), "membership tests hold production dispatch exclusion");

    initialize_pending_list();
    GUEST32(VOICE + SYNTHETIC_STATE_OFFSET) = 10;
    CHECK(black_audio_queue_completion(VOICE) == COMPLETE_HEAD,
          "first completion returns native head");
    GUEST32(VOICE + SYNTHETIC_STATE_OFFSET) = 20;
    CHECK(black_audio_queue_completion(VOICE) == COMPLETE_HEAD,
          "repeated pending completion returns native head");
    if (FIXTURE_OLD_MEMBERSHIP_CONTROL) {
        CHECK(GUEST32(COMPLETE_HEAD) == COMPLETE_NODE
                  && GUEST32(COMPLETE_NODE) == COMPLETE_NODE
                  && GUEST32(COMPLETE_NODE + 4) == COMPLETE_NODE,
              "old membership control recreates captured sentinel to self-node corruption");
        printf("old membership control: sentinel -> node, node -> itself reproduced\n");
    }
    check_pending_order(single, 1);
    drained = drain_pending_list();
    CHECK(drained == 1 && callback_total == 1 && callback_counts[0] == 1,
          "repeated completion before drain produces one callback");
    CHECK(callback_states[0] == 20, "coalesced callback reads current voice state");
    CHECK(GUEST32(COMPLETE_NODE) == COMPLETE_NODE && GUEST32(COMPLETE_NODE + 4) == COMPLETE_NODE,
          "drained completion node becomes eligible for fresh work");
    GUEST32(VOICE + SYNTHETIC_STATE_OFFSET) = 30;
    black_audio_queue_completion(VOICE);
    check_pending_order(single, 1);
    drained = drain_pending_list();
    CHECK(drained == 1 && callback_counts[0] == 2 && callback_states[0] == 30,
          "fresh completion after drain is accepted and called once");

    initialize_pending_list();
    for (i = 0; i < 3; i++) {
        GUEST32(pending_voices[i] + SYNTHETIC_STATE_OFFSET) = 100 + i;
        black_audio_queue_completion(pending_voices[i]);
        links[i * 2] = GUEST32(pending_voices[i] + 0x54u);
        links[i * 2 + 1] = GUEST32(pending_voices[i] + 0x58u);
    }
    /* Snapshot after the complete list exists, including changed predecessor links. */
    for (i = 0; i < 3; i++) {
        links[i * 2] = GUEST32(pending_voices[i] + 0x54u);
        links[i * 2 + 1] = GUEST32(pending_voices[i] + 0x58u);
    }
    GUEST32(pending_voices[2] + SYNTHETIC_STATE_OFFSET) = 202;
    black_audio_queue_completion(pending_voices[2]);
    GUEST32(pending_voices[1] + SYNTHETIC_STATE_OFFSET) = 201;
    black_audio_queue_completion(pending_voices[1]);
    for (i = 0; i < 3; i++)
        CHECK(GUEST32(pending_voices[i] + 0x54u) == links[i * 2]
                  && GUEST32(pending_voices[i] + 0x58u) == links[i * 2 + 1],
              "repeating tail and middle completion preserves existing links");
    check_pending_order(all, 3);
    drained = drain_pending_list();
    CHECK(drained == 3 && callback_total == 3 && callback_order[0] == 0
              && callback_order[1] == 1 && callback_order[2] == 2,
          "multi-node drain invokes callbacks once in original order");
    CHECK(callback_counts[0] == 1 && callback_counts[1] == 1 && callback_counts[2] == 1,
          "repeated pending tail and middle each retain one work item");
    CHECK(callback_states[0] == 100 && callback_states[1] == 201 && callback_states[2] == 202,
          "each coalesced work item observes its current state");
    GUEST32(pending_voices[1] + SYNTHETIC_STATE_OFFSET) = 301;
    black_audio_queue_completion(pending_voices[1]);
    check_pending_order(middle, 1);
    drained = drain_pending_list();
    CHECK(drained == 1 && callback_counts[1] == 2 && callback_states[1] == 301,
          "middle voice can queue fresh work after a full drain");

    xbox_IrqlLeaveDispatch();
    CHECK(!g_holds_dispatch && g_current_irql == PASSIVE_LEVEL,
          "membership tests balance ownership without changing IRQL");
}

int main(void)
{
    test_foreign_owner_and_retry();
    test_recursion_and_irql_nesting();
    test_duplicate_pending_completion();
    printf("BLACK completion exclusion (%s): %ld checks, %ld failures\n",
           FIXTURE_OLD_CONTROL ? "old guard control"
               : FIXTURE_OLD_MEMBERSHIP_CONTROL ? "old membership control" : "current", checks, failures);
    return failures ? 1 : 0;
}
