/* Synthetic RAM only; executes the authored diagnostic helper verbatim. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#include <xmmintrin.h>

#define XBOX_CONTIG_BASE 0x80000000u
#define XBOX_CONTIG_SIZE 0x04000000u
#define RAM_BYTES 0x00400000u
static unsigned char ram[RAM_BYTES], before[RAM_BYTES];
static ptrdiff_t g_xbox_mem_offset;
static void *xbox_GetMemoryBase(void) { return ram; }
static size_t xbox_GetMappedSize(void) { return RAM_BYTES; }
static void *xbox_GetPhysicalMemoryBase(void) { return NULL; }
#define GUEST32(a) (*(uint32_t *)(ram+(a)))
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while(0)
#include "trace.extracted.inc"

int main(int argc, char **argv)
{
    const uint32_t actor=0x10000u, manager=0x20000u, slots=0x30000u;
    unsigned i;
    (void)argv;
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)ram;
    _putenv(argc>1 ? "RECOMP_BLACK_WEAPON_TRACE=0" : "RECOMP_BLACK_WEAPON_TRACE=1");
    GUEST32(0x2D0D64)=manager;
    GUEST32(actor+0x258)=slots;
    GUEST32(actor+0x29C)=0x35000;
    GUEST32(actor+0xC4)=1;
    ram[actor+0x2C4]=2;
    ram[slots+0x19]=255; /* Corrupt slot index must never form a dereference. */
    GUEST32(slots+0xCu)=0x11111111u;
    GUEST32(slots+0x10u)=0x22222222u;
    GUEST32(slots+0x14u)=0x33333333u;
    GUEST32(actor+0x1000u+0x258u)=RAM_BYTES-10u; /* Struct cannot fit in RAM. */
    GUEST32(manager+0xA0)=0x7F800001; /* Signalling NaN stays integer data. */
    memcpy(before,ram,sizeof(ram));
    for(i=0;i<272u;++i) {
        fenv_t a,b;
        unsigned csr=0x1F80u|((i%4)<<13)|(i%64), observed;
        uint32_t object=i%7==0 ? 0xFFFFFFF0u : i%7==1 ? 0u : i%7==2 ? actor+0x1000u : actor;
        fesetenv(FE_DFL_ENV); _mm_setcsr(csr);
        memset(&a,0,sizeof(a)); fegetenv(&a);
        black_weapon_trace(1u+i%4u, i%4==3 ? manager : object);
        observed=_mm_getcsr(); memset(&b,0,sizeof(b)); fegetenv(&b);
        fesetenv(FE_DFL_ENV); _mm_setcsr(0x1F80u);
        CHECK(observed==csr); CHECK(memcmp(&a,&b,sizeof(a))==0);
    }
    CHECK(memcmp(before,ram,sizeof(ram))==0);
    printf("%u trace state/memory checks passed\n", checks);
    return 0;
}
