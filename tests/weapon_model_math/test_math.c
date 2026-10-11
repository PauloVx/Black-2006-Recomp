/* Independent uint64 arithmetic oracle with synthetic guest memory. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static uint8_t ram[0x400000];
static uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esi,g_edi,g_esp;
#define MEM32(a) (*(uint32_t *)(ram+(uint32_t)(a)))
#define PUSH32(sp,v) do { (sp)-=4; MEM32(sp)=(v); } while(0)
#define POP32(sp,v) do { (v)=MEM32(sp); (sp)+=4; } while(0)
#define RECOMP_ABI_CALL(va,fn) fn()
#define CMP_A(a,b) ((uint32_t)(a)>(uint32_t)(b))
#define CMP_B(a,b) ((uint32_t)(a)<(uint32_t)(b))
#define CMP_BE(a,b) ((uint32_t)(a)<=(uint32_t)(b))
#define CMP_NE(a,b) ((a)!=(b))
#define CMP_L(a,b) ((int32_t)(a)<(int32_t)(b))
#define TEST_NZ(a,b) (((a)&(b))!=0)
static unsigned checks, mismatches;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); exit(1); } } while(0)
#include "math.extracted.inc"

static uint64_t call(void (*fn)(void),uint64_t n,uint64_t d,unsigned pop)
{
    uint64_t result;
    g_esp=0x300000;
    g_eax=0xAAAAAAAA;g_edx=0xDDDDDDDD;g_ecx=0xCCCCCCCC;
    g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
    MEM32(g_esp)=0xDEADBEEF;
    MEM32(g_esp+4)=(uint32_t)n;MEM32(g_esp+8)=(uint32_t)(n>>32);
    MEM32(g_esp+12)=(uint32_t)d;MEM32(g_esp+16)=(uint32_t)(d>>32);
    fn();
    result=((uint64_t)g_edx<<32)|g_eax;
    CHECK(g_esp==0x300000+pop);
    CHECK(g_ebx==0x12345678 && g_esi==0x23456789 && g_edi==0x3456789A);
    CHECK(MEM32(0x300000)==0xDEADBEEF);
    CHECK(MEM32(0x300004)==(uint32_t)n && MEM32(0x300008)==(uint32_t)(n>>32));
    CHECK(MEM32(0x30000C)==(uint32_t)d && MEM32(0x300010)==(uint32_t)(d>>32));
    return result;
}
static void compare(const char *label,uint64_t actual,uint64_t expected,int broken)
{
    ++checks;
    if(actual==expected) return;
    ++mismatches;
    if(broken || mismatches<8)
        fprintf(stderr,"%s got=%016" PRIX64 " expected=%016" PRIX64 "\n",label,actual,expected);
    if(!broken) exit(1);
}
static void pair(uint64_t n,uint64_t d,int broken)
{
    CHECK(d!=0);
    compare("quotient",call(sub_000A3380,n,d,20),n/d,broken);
    compare("remainder",call(sub_000A30B0,n,d,20),n%d,broken);
}
static uint64_t random64(uint64_t *s)
{
    *s ^= *s<<13;*s ^= *s>>7;*s ^= *s<<17;return *s;
}
int main(int argc,char **argv)
{
    uint64_t power=1, state=UINT64_C(0x26061010), n,d;
    unsigned i,j;
    const int broken=argc>1 && !strcmp(argv[1],"broken");
    static const uint64_t edges[]={0,1,2,0x7FFFFFFF,0x80000000,0xFFFFFFFF,
        UINT64_C(0x100000000),UINT64_C(0x100000001),UINT64_C(0xFFFFFFFF00000000),
        UINT64_C(0x8000000000000000),UINT64_C(0xFFFFFFFFFFFFFFFF)};
    if(argc>1 && !strcmp(argv[1],"save-compat")) {
        /* XCreateSaveGame's UTF-16 name hash reduces modulo 2^48-59.
         * Check every top16 quotient estimate at both correction thresholds,
         * including original generated code with the four rotations absent. */
        static const uint32_t middle[]={0u,1u,65534u,65535u};
        unsigned k,t;
        for(t=0;t<65536u;++t) {
            uint32_t threshold=(uint32_t)(UINT64_C(0x100000000)-59u*((uint64_t)t+1u));
            uint32_t low[]={0u,threshold-1u,threshold,threshold+1u,UINT32_MAX};
            for(j=0;j<4u;++j) for(k=0;k<5u;++k)
                pair(((uint64_t)t<<48)|((uint64_t)middle[j]<<32)|low[k],
                     UINT64_C(0x0000FFFFFFFFFFC5),0);
        }
        printf("%u exact-generated legacy save-modulus checks passed\n",checks);
        return 0;
    }
    for(i=0;i<12;++i) {
        power*=40;
        MEM32(0x28B578+8*i)=(uint32_t)power;
        MEM32(0x28B57C+8*i)=(uint32_t)(power>>32);
    }
    /* Real compact names used by original weapon creation/fire code. */
    compare("BG1_AK1_LOD",call(sub_000C09F0,UINT64_C(0x5446127297C60000),
        UINT64_C(0xE69A1DD748000000),4),UINT64_C(0x544612729DD18B40),broken);
    compare("BG1_SNR_LOD",call(sub_000C09F0,UINT64_C(0x54461524B8230000),
        UINT64_C(0xE69A1DD748000000),4),UINT64_C(0x54461524BE2E8B40),broken);
    pair(UINT64_C(0xE69A1DD748000000),UINT64_C(0x0000002625A00000),broken);
    pair(UINT64_C(0x0000004000000000),UINT64_C(0x0000002625A00000),broken);
    if(broken) {
        CHECK(mismatches>=4);
        printf("Original generated code: %u expected mismatches reproduced (%u checks)\n",mismatches,checks);
        return 0;
    }
    for(i=0;i<sizeof(edges)/sizeof(edges[0]);++i)
        for(j=1;j<sizeof(edges)/sizeof(edges[0]);++j) pair(edges[i],edges[j],0);
    /* Powers of 40 are the complete model-name concatenation divisor family. */
    power=1;
    for(i=0;i<12;++i) {
        power*=40;
        pair(power-1,power,0);pair(power,power,0);pair(power+1,power,0);
        pair(UINT64_MAX,power,0);
    }
    for(i=0;i<100000;++i) {
        n=random64(&state);d=random64(&state)|1u;
        if(i%3==0)d=(uint32_t)d;
        if(i%5==0)d=(d&UINT64_C(0xFFFFFFFF00000000))|UINT64_C(0x100000000);
        pair(n,d,0);
    }
    printf("%u exact-generated arithmetic/name/ABI checks passed\n",checks);
    return 0;
}
