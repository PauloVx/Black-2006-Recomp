#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

typedef unsigned char KIRQL;
static __declspec(thread) uint32_t fixture_args[3], g_eax;
static __declspec(thread) unsigned overlap_role;
#define STACK_ARG(i) fixture_args[i]
static volatile LONG checks, failures;
#define CHECK(condition, message) do { \
    InterlockedIncrement(&checks); \
    if (!(condition) && InterlockedIncrement(&failures) <= 12) \
        fprintf(stderr, "FAIL: %s\n", message); \
} while (0)

static HANDLE overlap_reached, overlap_release;
static int overlap_enabled;
static void fixture_overlap_point(void)
{
    if (overlap_enabled && overlap_role == 1) {
        SetEvent(overlap_reached);
        CHECK(WaitForSingleObject(overlap_release, 2000) == WAIT_OBJECT_0,
              "controlled producer resumes");
    }
}

/* PRODUCTION_QUEUE */

static volatile uint64_t g_dpc_drains, g_dpc_deferred;
static int allow_drain = 1, callback_requeue;
static unsigned callback_count;
static uint32_t callback_tokens[4];
static DWORD WINAPI callback_producer(LPVOID ignored)
{
    (void)ignored;
    fixture_args[0] = 0xBEEFu;
    fixture_args[1] = 0xBEEF1234u;
    fixture_args[2] = 0xBEEF5678u;
    bridge_KeInsertQueueDpc();
    CHECK(g_eax == 1, "callback cross-thread requeue accepted");
    return 0;
}
static BOOLEAN xbox_IrqlEnterDpc(KIRQL *saved)
{
    *saved = 0;
    return (BOOLEAN)allow_drain;
}
static void xbox_IrqlLeaveDpc(KIRQL saved) { (void)saved; }
static void bridge_publish_irql(void) {}
static int kernel_run_dpc(uint32_t dpc, uint32_t arg1, uint32_t arg2)
{
    CHECK(callback_count < 4, "bounded callback count");
    if (callback_count < 4) callback_tokens[callback_count++] = dpc;
    if (dpc == 0xBEEFu)
        CHECK(arg1 == 0xBEEF1234u && arg2 == 0xBEEF5678u, "requeue payload intact");
    if (callback_requeue && dpc == 0xA11u) {
        HANDLE thread = CreateThread(NULL, 0, callback_producer, NULL, 0, NULL);
        CHECK(thread != NULL, "callback producer created");
        if (thread) {
            /* A queue lock held across the callback deadlocks this producer. */
            CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0,
                  "queue lock released before callback");
            CloseHandle(thread);
        }
    }
    return 1;
}

/* PRODUCTION_DRAIN */

static DWORD WINAPI overlapped_producer(LPVOID role_ptr)
{
    overlap_role = (unsigned)(uintptr_t)role_ptr;
    fixture_args[0] = overlap_role == 1 ? 0xAAu : 0x600D3u;
    fixture_args[1] = fixture_args[0] ^ 0x12345678u;
    fixture_args[2] = ~fixture_args[0];
    bridge_KeInsertQueueDpc();
    CHECK(g_eax == 1, "both controlled producers report accepted");
    return 0;
}

static void test_overlap(void)
{
    HANDLE a, b;
    PendingDpc d;
    unsigned count = 0, gpu_seen = 0;
    overlap_reached = CreateEventW(NULL, TRUE, FALSE, NULL);
    overlap_release = CreateEventW(NULL, TRUE, FALSE, NULL);
    overlap_enabled = 1;
    a = CreateThread(NULL, 0, overlapped_producer, (void *)(uintptr_t)1, 0, NULL);
    CHECK(WaitForSingleObject(overlap_reached, 2000) == WAIT_OBJECT_0,
          "first producer stopped after tail read");
    b = CreateThread(NULL, 0, overlapped_producer, (void *)(uintptr_t)2, 0, NULL);
    /* Old code completes B while A owns the stale tail; fixed code blocks B. */
    CHECK(WaitForSingleObject(b, FIXTURE_OLD_CONTROL ? 2000 : 100)
              == (FIXTURE_OLD_CONTROL ? WAIT_OBJECT_0 : WAIT_TIMEOUT),
          "second producer obeys queue exclusion before first resumes");
    SetEvent(overlap_release);
    CHECK(WaitForSingleObject(a, 2000) == WAIT_OBJECT_0, "first producer finished");
    CHECK(WaitForSingleObject(b, 2000) == WAIT_OBJECT_0, "GPU-like producer finished");
    overlap_enabled = 0;
    while (kernel_dpc_pop(&d)) {
        count++;
        CHECK(d.arg1 == (d.dpc ^ 0x12345678u) && d.arg2 == ~d.dpc,
              "controlled payload tuple intact");
        gpu_seen += d.dpc == 0x600D3u;
    }
    CHECK(count == 2, "both accepted DPCs retained without capacity overflow");
    CHECK(gpu_seen == 1, "masked-GPU completion token retained");
    printf("controlled overlap: %u queued callbacks, GPU-like token seen %u\n", count, gpu_seen);
    CloseHandle(a); CloseHandle(b);
    CloseHandle(overlap_reached); CloseHandle(overlap_release);
}

#define PRODUCERS 4
#define PER_PRODUCER 5000
static unsigned seen[PRODUCERS * PER_PRODUCER];
static volatile LONG producers_done;
static DWORD WINAPI stress_producer(LPVOID producer_ptr)
{
    uint32_t base = (uint32_t)(uintptr_t)producer_ptr * PER_PRODUCER;
    uint32_t i;
    for (i = 0; i < PER_PRODUCER; i++) {
        uint32_t id = base + i + 1;
        while (!kernel_dpc_push(id, id ^ 0x55AA55AAu, ~id)) SwitchToThread();
    }
    InterlockedIncrement(&producers_done);
    return 0;
}

static void test_stress(void)
{
    HANDLE threads[PRODUCERS];
    DWORD deadline = GetTickCount() + 10000;
    unsigned i, received = 0;
    PendingDpc d;
    for (i = 0; i < PRODUCERS; i++)
        threads[i] = CreateThread(NULL, 0, stress_producer, (void *)(uintptr_t)i, 0, NULL);
    while ((InterlockedCompareExchange(&producers_done, 0, 0) < PRODUCERS || kernel_dpc_count())
           && (int32_t)(GetTickCount() - deadline) < 0) {
        if (kernel_dpc_pop(&d)) {
            CHECK(d.dpc >= 1 && d.dpc <= PRODUCERS * PER_PRODUCER, "stress token range");
            if (d.dpc >= 1 && d.dpc <= PRODUCERS * PER_PRODUCER) seen[d.dpc - 1]++;
            CHECK(d.arg1 == (d.dpc ^ 0x55AA55AAu) && d.arg2 == ~d.dpc, "stress payload tuple intact");
            received++;
        } else SwitchToThread();
    }
    CHECK(InterlockedCompareExchange(&producers_done, 0, 0) == PRODUCERS, "all stress producers finished");
    CHECK(received == PRODUCERS * PER_PRODUCER, "all accepted stress entries delivered");
    for (i = 0; i < PRODUCERS * PER_PRODUCER; i++) CHECK(seen[i] == 1, "stress entry delivered exactly once");
    for (i = 0; i < PRODUCERS; i++) {
        CHECK(WaitForSingleObject(threads[i], 2000) == WAIT_OBJECT_0, "stress producer joined");
        CloseHandle(threads[i]);
    }
}

static void test_api_and_drain(void)
{
    PendingDpc d;
    unsigned i;
    fixture_args[0] = 0;
    bridge_KeInsertQueueDpc();
    CHECK(!g_eax && !kernel_dpc_count(), "null bridge enqueue rejected");
    for (i = 0; i < XBOX_MAX_PENDING_DPC - 1; i++)
        CHECK(kernel_dpc_push(i + 1, i + 2, i + 3), "fill queue to capacity");
    fixture_args[0] = 0xFFu;
    bridge_KeInsertQueueDpc();
    CHECK(!g_eax && kernel_dpc_count() == XBOX_MAX_PENDING_DPC - 1, "full bridge enqueue rejected");
    for (i = 0; kernel_dpc_pop(&d); i++)
        CHECK(d.dpc == i + 1 && d.arg1 == i + 2 && d.arg2 == i + 3, "FIFO and payload after ring wrap");
    CHECK(i == XBOX_MAX_PENDING_DPC - 1, "capacity entries preserved");
    kernel_queue_dpc(0xA11u, 0, 0);
    allow_drain = 0;
    kernel_drain_dpcs();
    CHECK(g_dpc_deferred == 1 && kernel_dpc_count() == 1 && !callback_count,
          "IRQL deferral keeps pending DPC");
    allow_drain = callback_requeue = 1;
    kernel_drain_dpcs();
    CHECK(callback_count == 2 && callback_tokens[0] == 0xA11u && callback_tokens[1] == 0xBEEFu,
          "drain processes callback requeue in order");
    CHECK(!kernel_dpc_count() && g_dpc_drains == 1, "drain empties queue");
}

int main(void)
{
    CHECK(!kernel_dpc_count(), "queue starts empty");
    test_overlap();
    /* The deterministic old-code loss is sufficient; avoid its unsafe stress. */
    if (!failures) {
        test_stress();
        test_api_and_drain();
    }
    printf("%s production DPC queue: %ld checks, %ld failures\n",
           failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
