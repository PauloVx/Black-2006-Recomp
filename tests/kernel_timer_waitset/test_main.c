#include <stdint.h>
#include <stdio.h>

typedef uintptr_t HANDLE;
typedef uint32_t DWORD;
typedef int64_t LONGLONG;
typedef struct { LONGLONG QuadPart; } LARGE_INTEGER;
#define FALSE 0
#define NULL_HANDLE ((HANDLE)0)
#define TIMER_ALL_ACCESS 0x1F0003u
#define SENTINEL ((HANDLE)0xCAFE1234u)
#define DEVICE_HANDLE ((HANDLE)0x11)
#define GPU_HANDLE ((HANDLE)0x22)
#define TIMER_HANDLE ((HANDLE)0x33)

static unsigned checks, failures, cases;
#define CHECK(condition, message) do { \
    checks++; \
    if (!(condition)) { \
        if (failures++ < 8) fprintf(stderr, "case %u: %s\n", cases, message); \
    } \
} while (0)

static HANDLE g_irq_wake, scenario_gpu, scenario_cached_timer;
static int scenario_timer_tried, scenario_timer_available, scenario_arm_ok;
static LONGLONG scenario_due;
static unsigned create_calls, arm_calls, wait_calls, sleep_calls;
static HANDLE observed[3];
static DWORD observed_count, observed_ms;
static LONGLONG armed_due;

static HANDLE kelvin_irq_event(void) { return scenario_gpu; }
static HANDLE CreateWaitableTimerExW(void *attributes, void *name,
                                    DWORD flags, DWORD access)
{
    CHECK(attributes == NULL && name == NULL, "anonymous timer creation");
    CHECK(flags == 2 && access == TIMER_ALL_ACCESS, "high-resolution timer flags");
    create_calls++;
    return scenario_timer_available ? TIMER_HANDLE : NULL_HANDLE;
}
static LONGLONG kernel_vblank_wait_100ns(DWORD limit)
{
    CHECK(limit == 10, "vblank wait bound");
    return scenario_due;
}
static DWORD kernel_vblank_wait_ms(DWORD limit)
{
    CHECK(limit == 10, "fallback wait bound");
    return 7;
}
static int SetWaitableTimer(HANDLE timer, LARGE_INTEGER *due, long period,
                           void *callback, void *context, int resume)
{
    CHECK(timer == TIMER_HANDLE && period == 0 && !callback && !context && !resume,
          "one-shot timer arming");
    arm_calls++;
    armed_due = due->QuadPart;
    return scenario_arm_ok;
}
static DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles,
                                    int all, DWORD ms)
{
    unsigned i;
    CHECK(!all && count >= 1 && count <= 3, "valid wait count and mode");
    wait_calls++;
    observed_count = count;
    observed_ms = ms;
    for (i = 0; i < count && i < 3; i++) observed[i] = handles[i];
    return 0;
}
static void Sleep(DWORD ms)
{
    sleep_calls++;
    observed_ms = ms;
}

static void production_wait(void)
{
    /* PRODUCTION_WAIT_BODY */
}

int main(void)
{
    unsigned device, gpu, cached, tried, available, arm, due_case;
    static const LONGLONG dues[] = {-1, 0, 10000};
    for (device = 0; device < 2; device++)
    for (gpu = 0; gpu < 2; gpu++)
    for (cached = 0; cached < 2; cached++)
    for (tried = 0; tried < 2; tried++)
    for (available = 0; available < 2; available++)
    for (arm = 0; arm < 2; arm++)
    for (due_case = 0; due_case < 3; due_case++) {
        HANDLE expected[3];
        unsigned expected_count = 0, i;
        int has_timer = tried ? cached : available;
        int should_wait = has_timer ? (dues[due_case] > 0 && arm) : (device || gpu);
        int should_sleep = !has_timer && !device && !gpu;
        cases++;
        g_irq_wake = device ? DEVICE_HANDLE : NULL_HANDLE;
        scenario_gpu = gpu ? GPU_HANDLE : NULL_HANDLE;
        scenario_cached_timer = cached ? TIMER_HANDLE : NULL_HANDLE;
        scenario_timer_tried = tried;
        scenario_timer_available = available;
        scenario_arm_ok = arm;
        scenario_due = dues[due_case];
        create_calls = arm_calls = wait_calls = sleep_calls = 0;
        observed_count = observed_ms = 0;
        armed_due = 0;
        production_wait();
        CHECK(create_calls == (unsigned)!tried, "create once or use cached availability");
        CHECK(arm_calls == (unsigned)(has_timer && scenario_due > 0), "arm only future deadlines");
        CHECK(wait_calls == (unsigned)should_wait, "wait branch selection");
        CHECK(sleep_calls == (unsigned)should_sleep, "sleep only with no wait sources");
        if (arm_calls) CHECK(armed_due == -scenario_due, "relative deadline conversion");
        if (should_wait) {
            if (device) expected[expected_count++] = DEVICE_HANDLE;
            if (gpu) expected[expected_count++] = GPU_HANDLE;
            if (has_timer) expected[expected_count++] = TIMER_HANDLE;
            CHECK(observed_count == expected_count, "all available sources retained");
            CHECK(observed_ms == (has_timer ? 20u : 7u), "bounded wait timeout");
            for (i = 0; i < expected_count; i++)
                CHECK(observed[i] == expected[i], "wait source identity and order");
        } else if (should_sleep) CHECK(observed_ms == 7, "fallback sleep timeout");
    }
    printf("%s timer wait-set: %u cases, %u checks, %u failures\n",
           failures ? "FAIL" : "PASS", cases, checks, failures);
    return failures ? 1 : 0;
}
