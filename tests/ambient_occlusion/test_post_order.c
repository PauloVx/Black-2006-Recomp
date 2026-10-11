#include "kelvin_post_order.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); exit(1); } } while (0)

int main(void)
{
    bool done = false;
    CHECK(!kg_post_order_should_apply_ao_before_blend(3, false, true, &done));
    CHECK(!done);
    CHECK(!kg_post_order_should_apply_ao_before_blend(4, true, true, &done));
    CHECK(!kg_post_order_should_apply_ao_before_blend(4, false, false, &done));
    CHECK(kg_post_order_should_apply_ao_before_blend(4, false, true, &done));
    CHECK(!done); /* eligibility does not consume an unsuccessful attempt */
    done = true; /* the renderer marks a successful AO pass */
    CHECK(!kg_post_order_should_apply_ao_before_blend(5, false, true, &done));
    CHECK(kg_post_focus_blend(-1.0) == 1.0f); /* no history */
    CHECK(kg_post_focus_blend(0.0) == 1.0f); /* discontinuous clock */
    CHECK(kg_post_focus_blend(0.5) == 1.0f); /* interrupted scene */
    CHECK(kg_post_focus_blend(2.0) == 1.0f); /* loading gap */
    {
        const float at30 = kg_post_focus_blend(1.0 / 30.0);
        const float at60 = kg_post_focus_blend(1.0 / 60.0);
        CHECK(at30 > at60 && at60 > 0.0f && at30 < 1.0f);
        CHECK(fabs((1.0 - at30) - (1.0 - at60) * (1.0 - at60)) < 1e-6);
        CHECK(kg_post_focus_blend(0.2) > kg_post_focus_blend(0.1));
    }
    printf("PASS: %u post-order checks\n", checks);
    return 0;
}
