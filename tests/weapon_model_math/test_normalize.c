/* Whole-uint64 oracle for the authored paired-register carry fixture. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint64_t numerator,denominator,normalized_numerator,normalized_denominator;
static unsigned rotations,checks;
#include "normalize.extracted.inc"
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); exit(1); } } while(0)
static uint64_t random64(uint64_t *s)
{
    *s^=*s<<13;*s^=*s>>7;*s^=*s<<17;return *s;
}
static void check(uint64_t n,uint64_t d)
{
    unsigned shift=0;
    uint64_t expected_d=d;
    while(expected_d>UINT32_MAX) { expected_d>>=1; ++shift; }
    numerator=n;denominator=d;sub_000A3380();
    CHECK(rotations==shift);
    CHECK(normalized_numerator==(n>>shift));
    CHECK(normalized_denominator==expected_d);
}
int main(void)
{
    uint64_t state=UINT64_C(0x20261010), power=1;
    unsigned i;
    for(i=0;i<64;++i) {
        uint64_t d=UINT64_C(1)<<i;
        check(UINT64_MAX,d);check(d,d);check(d-1,d);check(d+1,d);
    }
    for(i=0;i<12;++i) { power*=40;check(UINT64_C(0xE69A1DD748000000),power); }
    for(i=0;i<100000;++i) {
        uint64_t n=random64(&state),d=random64(&state);
        check(n,d);
    }
    printf("%u synthetic carry normalization checks passed\n",checks);
    return 0;
}
