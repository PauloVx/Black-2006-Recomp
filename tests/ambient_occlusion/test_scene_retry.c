/* Execute the production AO pass coordinator against synthetic surfaces. No game files/GPU required. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kelvin_post_order.h"
#define MAX_PATH 260
#define DXGI_FORMAT_B8G8R8A8_UNORM 87
#define PCV_AO_OFF 0
#define PCV_AO_HBAO 2
#define PCV_AO_HBAO_PLUS 3
#define PCV_AO_GTAO 4
typedef int ID3D11PixelShader;
typedef void ID3D11Resource;
typedef struct KGpuSurf {
    int bpp, dxfmt;
    bool depth, real;
    unsigned hw, hh;
    unsigned gw, gh;
    void *srv, *rtv, *tex;
} KGpuSurf;
static struct { void *ctx; uint64_t frames; } G;
static struct {
    bool ao_done, ao_scene_done, ao_dump_done, post_trace, scene_fx, done;
    unsigned world;
    int ao_method, ao_quality;
    uint64_t ao_camera, ao_scene_frame, ao_dump_frame;
    char ao_scene_prefix[MAX_PATH], ao_dump[MAX_PATH];
    ID3D11PixelShader *ps_ao_ssao, *ps_ao_hbao, *ps_ao_hbaop, *ps_ao_gtao;
    ID3D11PixelShader *ps_ao_blur_h, *ps_ao_blur_v, *ps_ao_comp;
    void *copy, *copy_srv, *ao_rtv[2], *ao_srv[2];
} s_post;
typedef struct KPixelState { bool depth_write, blend, depth_test; int depth_func; } KPixelState;
static uint64_t s_ao_draw_camera = 1;
static unsigned checks, copies, passes, invalidations;
static int resource_failure;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
static bool post_init(void) { return resource_failure != 1; }
static bool post_scene_resources(unsigned w, unsigned h) { (void)w;(void)h;return resource_failure != 2; }
static bool post_ao_init(void) { return resource_failure != 3; }
static bool post_ao_resources(unsigned w, unsigned h) { (void)w;(void)h;return resource_failure != 4; }
static bool post_ao_cb(unsigned w, unsigned h) { (void)w;(void)h;return resource_failure != 5 && s_post.ao_camera != 0; }
static bool kg_ao_camera(uint64_t camera, float *x, float *y) { (void)camera;*x=*y=1;return true; }
static uint8_t *post_ao_colour_capture(void *tex, unsigned w, unsigned h, const char *path) { (void)tex;(void)w;(void)h;(void)path;abort(); }
static void post_ao_mask_capture(unsigned w, unsigned h, uint64_t frame) { (void)w;(void)h;(void)frame;abort(); }
static void post_ao_scene_compare(KGpuSurf *cs, uint8_t *before, const char *path, uint64_t frame) { (void)cs;(void)before;(void)path;(void)frame;abort(); }
static void kg_cache_invalidate(void) { ++invalidations; }
static void ID3D11DeviceContext_CopyResource(void *ctx, ID3D11Resource *dst, ID3D11Resource *src) { (void)ctx;(void)dst;(void)src;++copies; }
static void post_pass(ID3D11PixelShader *shader, void *rtv, unsigned w, unsigned h, void *t0, void *t1, void *t2, void *t3) {
    (void)shader;(void)rtv;(void)w;(void)h;(void)t0;(void)t1;(void)t2;(void)t3;++passes;
}
static void kg_post_scene(KGpuSurf *cs, KGpuSurf *zs) { (void)cs;(void)zs;abort(); }
#include "scene_retry.extracted.inc"
static void reset(void) {
    memset(&s_post, 0, sizeof(s_post));
    s_post.scene_fx = true;
    s_post.ao_method = PCV_AO_HBAO_PLUS;
    s_post.ao_camera = 1;
    copies = passes = invalidations = 0;
    resource_failure = 0;
}
static void retry(KGpuSurf *cs, KGpuSurf *zs) {
    CHECK(!s_post.ao_done); CHECK(copies == 0); CHECK(passes == 0); CHECK(invalidations == 0);
    resource_failure = 0; s_post.ao_camera = 1; s_post.ao_method = PCV_AO_HBAO_PLUS;
    kg_post_ao_scene(cs, zs);
    CHECK(s_post.ao_done); CHECK(copies == 1); CHECK(passes == 4); CHECK(invalidations == 1);
    /* A later eligible draw must not darken the same completed scene twice. */
    kg_post_ao_scene(cs, zs);
    CHECK(copies == 1); CHECK(passes == 4); CHECK(invalidations == 1);
}
int main(void) {
    int token;
    KGpuSurf cs = {4, DXGI_FORMAT_B8G8R8A8_UNORM, false, true, 640, 480, 640, 480, &token, &token, &token};
    KGpuSurf zs = {4, 0, true, true, 640, 480, 640, 480, &token, &token, &token}, invalid;
    reset(); kg_post_ao_scene(&cs, NULL); retry(&cs, &zs);
    reset(); kg_post_ao_scene(NULL, &zs); retry(&cs, &zs);
    for (int i = 0; i < 5; ++i) {
        reset(); invalid = zs;
        switch(i) { case 0: invalid.depth=false;break;case 1:invalid.real=false;break;case 2:invalid.srv=NULL;break;case 3:invalid.hw=320;break;case 4:invalid.hh=240;break; }
        kg_post_ao_scene(&cs, &invalid); retry(&cs, &zs);
    }
    for (int failure = 1; failure <= 5; ++failure) {
        reset(); resource_failure=failure; kg_post_ao_scene(&cs, &zs); retry(&cs, &zs);
    }
    reset(); s_post.ao_camera=0; kg_post_ao_scene(&cs, &zs); retry(&cs, &zs);
    reset(); s_post.ao_method=PCV_AO_OFF; kg_post_ao_scene(&cs, &zs); retry(&cs, &zs);
    reset(); invalid=cs;invalid.bpp=2;kg_post_ao_scene(&invalid,&zs);retry(&cs,&zs);
    reset(); invalid=cs;invalid.dxfmt=0;kg_post_ao_scene(&invalid,&zs);retry(&cs,&zs);
    /* The actual production trigger must retry too, rather than consuming a
     * first non-depth-writing blend that has no real scene depth attached. */
    {
        const KPixelState world = {true,false,true,3}, blended = {false,true,false,0};
        reset();
        for (int i = 0; i < 4; ++i) kg_post_note_draw(&world, &cs, &zs);
        CHECK(s_post.world == 4); CHECK(!s_post.ao_done);
        kg_post_note_draw(&blended, &cs, NULL);
        CHECK(!s_post.ao_done); CHECK(copies == 0); CHECK(passes == 0);
        kg_post_note_draw(&blended, &cs, &zs);
        CHECK(s_post.ao_done); CHECK(copies == 1); CHECK(passes == 4);
        kg_post_note_draw(&blended, &cs, &zs);
        CHECK(copies == 1); CHECK(passes == 4);
    }
    printf("PASS: %u AO retry checks against the actual production coordinator\n",checks);
    return 0;
}
