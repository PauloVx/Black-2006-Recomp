#include <assert.h>
#include <stdint.h>
#include "../../src/black_localization.h"

int main(void)
{
    assert(black_localization_fallback_record(0x3A7C7126u) == 0x3A7C7126u);
    assert(black_localization_fallback_record(0x46C7A416u) == 0x545DE522u);
    assert(black_localization_fallback_record(0x545DE522u) == 0x545DE522u);
    assert(black_localization_fallback_record(0x6761CAE2u) == 0u);
    assert(black_localization_fallback_record(0x12345678u) == 0u);
    return 0;
}
