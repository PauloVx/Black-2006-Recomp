"""Generate a synthetic normalization fixture through the actual title hook."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'scripts'))
from generate import unsigned_division_hooks

# Authored 64-bit-pair normalization, no retail game code. Separate SHR/RCR
# pairs must carry the high word's outgoing bit into its own low word.
body = '''void sub_000A3380(void)
{
    uint32_t l_ecx = (uint32_t)(denominator >> 32), l_ebx = (uint32_t)denominator;
    uint32_t l_edx = (uint32_t)(numerator >> 32), l_eax = (uint32_t)numerator;
    int _cf = 0;
    rotations = 0;
    while (l_ecx) {
        _cf = (int)(l_ecx & 1u);
        l_ecx >>= 1;
    /* TODO: rcr ebx, 1 */
        _cf = (int)(l_edx & 1u);
        l_edx >>= 1;
    /* TODO: rcr eax, 1 */
        ++rotations;
    }
    normalized_denominator = ((uint64_t)l_ecx << 32) | l_ebx;
    normalized_numerator = ((uint64_t)l_edx << 32) | l_eax;
}
'''
Path(sys.argv[1]).write_text(unsigned_division_hooks('sub_000A3380', body))
