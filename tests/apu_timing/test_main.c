#include "apu_timing.h"
#include <stdio.h>

static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "Failed line %u: %s\n", (unsigned)__LINE__, #x); } } while (0)

int main(void)
{
    /* Long uptime overflows the old count*1000000 expression. */
    CHECK(mcpx_apu_clock_microseconds(INT64_C(10000000000000), 10000000) == INT64_C(1000000000000));
    CHECK(mcpx_apu_clock_microseconds(INT64_C(10000000000007), 10000000) == INT64_C(1000000000000));
    CHECK(mcpx_apu_clock_microseconds(INT64_C(864000000000000), 10000000) == INT64_C(86400000000000));
    CHECK(mcpx_apu_clock_microseconds(123456789, 3579545) == 34489520);

    int64_t deadline = 1000000, start = deadline;
    unsigned fraction = 0;
    /* Exactly 90 minutes of 48k PCM, no cumulative integer rounding drift. */
    for (unsigned block = 0; block < 1012500; ++block)
        mcpx_apu_advance_deadline(&deadline, &fraction);
    CHECK(deadline - start == INT64_C(5400000000));
    CHECK(fraction == 0);

    deadline = 1000000;
    fraction = 2;
    CHECK(!mcpx_apu_rebase_deadline(&deadline, &fraction, 1017000));
    CHECK(deadline == 1000000 && fraction == 2);
    /* Recover the 17ms scheduling interruption without dropping old slices. */
    for (unsigned block = 0; block < 4; ++block)
        mcpx_apu_advance_deadline(&deadline, &fraction);
    CHECK(deadline == 1021334 && fraction == 0);
    CHECK(!mcpx_apu_rebase_deadline(&deadline, &fraction, deadline + 64000));
    CHECK(mcpx_apu_rebase_deadline(&deadline, &fraction, deadline + 64001));
    CHECK(deadline == 1085335 && fraction == 0);
    deadline = 0; fraction = 2;
    CHECK(mcpx_apu_rebase_deadline(&deadline, &fraction, 1234));
    CHECK(deadline == 1234 && fraction == 0);
    printf("APU timing: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
