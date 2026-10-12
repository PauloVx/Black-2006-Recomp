/* Exercise the actual scene coordinator across a prolonged loss of depth.
 * Observing presentation time must not conceal the lack of focus observations. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "kelvin_post_order.h"
#define DXGI_FORMAT_B8G8R8A8_UNORM 87
#define PCV_AO_OFF 0
#define PCV_AA_FXAA 1
typedef void ID3D11Resource;
typedef struct KGpuSurf {
    int bpp, dxfmt;
    bool depth, real;
    unsigned hw, hh;
    void *srv, *rtv, *tex;
} KGpuSurf;
static struct { void *ctx; uint64_t frames; long long qpf; } G;
static struct {
    bool done, scene_fx, ao_skip_traced, post_trace, focus_skipped;
    int depth_stats, ao_method, dof, aa, sharpen;
    uint64_t ao_camera;
    char ao_scene_prefix[1], ao_dump[1];
    unsigned focus_cur;
    long long last_qpc, focus_trace_qpc;
    void *copy, *copy_srv, *focus_rtv[2], *focus_srv[2], *coc_rtv, *coc_srv;
    void *ps_focus, *ps_coc, *ps_dof, *ps_fxaa, *ps_sharp;
} s_post;
static long long clock_tick;
static unsigned checks, focus_passes, other_passes;
static float focus_alpha;
static int resource_failure;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #v); exit(1); } } while (0)
static long long qpc_now(void) { return clock_tick; }
static bool post_init(void) { return resource_failure != 1; }
static bool post_scene_resources(unsigned w, unsigned h) { (void)w; (void)h; return resource_failure != 2; }
static void kg_post_depth_stats(KGpuSurf *zs) { (void)zs; abort(); }
static void kg_post_ao_scene(KGpuSurf *cs, KGpuSurf *zs) { (void)cs; (void)zs; }
static void post_dof_preset(int dof, float *taps, float *radius) { (void)dof; *taps = 16; *radius = 0.01f; }
static void ID3D11DeviceContext_CopyResource(void *ctx, ID3D11Resource *dst, ID3D11Resource *src) { (void)ctx; (void)dst; (void)src; }
static void post_cb(float w, float h, float taps, float radius, float alpha, float depth_scale,
                    float zone, float near_blur, float sharp, float ow, float oh) {
    (void)w; (void)h; (void)taps; (void)radius; (void)depth_scale;
    (void)zone; (void)near_blur; (void)sharp; (void)ow; (void)oh;
    focus_alpha = alpha;
}
static void post_pass(void *shader, void *rtv, unsigned w, unsigned h, void *t0, void *t1, void *t2, void *t3) {
    (void)rtv; (void)w; (void)h; (void)t0; (void)t1; (void)t2; (void)t3;
    if (shader == s_post.ps_focus) ++focus_passes;
    else ++other_passes;
}
static void kg_cache_invalidate(void) {}
#include "focus_gap.extracted.inc"
static void reset(void) {
    memset(&s_post, 0, sizeof(s_post));
    s_post.scene_fx = true; s_post.dof = 3;
    /* Distinct shader tags let the fixture observe focus execution. */
    s_post.ps_focus = (void *)(uintptr_t)1;
    s_post.ps_coc = (void *)(uintptr_t)2;
    s_post.ps_dof = (void *)(uintptr_t)3;
    G.qpf = 1000;
    clock_tick = 1000; resource_failure = 0;
    focus_alpha = -1; focus_passes = other_passes = 0;
}
int main(void) {
    int token;
    const KGpuSurf color = {4, DXGI_FORMAT_B8G8R8A8_UNORM, false, true, 640, 480, &token, &token, &token};
    const KGpuSurf depth = {4, 0, true, true, 640, 480, &token, &token, &token};
    KGpuSurf cs = color, zs = depth;
    /* Continuous valid observations retain normal exponential smoothing. */
    reset(); kg_post_scene(&cs, &zs);
    CHECK(focus_passes == 1); CHECK(focus_alpha == 1.0f); CHECK(s_post.last_qpc == 1000);
    clock_tick = 1016; kg_post_scene(&cs, &zs);
    CHECK(focus_passes == 2); CHECK(fabsf(focus_alpha - (float)(1.0 - exp(-0.016 / 0.25))) < 1e-6f);
    CHECK(s_post.last_qpc == 1016);
    /* A 960ms missing-depth interval must reacquire rather than blend near focus.
     * Cover each production rejection gate and the DoF-disabled state. */
    for (int kind = 0; kind < 9; ++kind) {
        reset(); cs = color; zs = depth; kg_post_scene(&cs, &zs);
        CHECK(s_post.last_qpc == 1000);
        for (int frame = 1; frame <= 60; ++frame) {
            clock_tick = 1000 + frame * 16;
            switch (kind) {
            case 0: break; /* no depth surface */
            case 1: zs.depth = false; break;
            case 2: zs.real = false; break;
            case 3: zs.srv = NULL; break;
            case 4: zs.hw = 320; break;
            case 5: zs.hh = 240; break;
            case 6: s_post.dof = 0; break;
            case 7: resource_failure = 1; break;
            case 8: resource_failure = 2; break;
            }
            kg_post_scene(&cs, kind == 0 ? NULL : &zs);
            CHECK(focus_passes == 1); CHECK(s_post.last_qpc == 1000);
        }
        zs = depth; s_post.dof = 3; resource_failure = 0; clock_tick = 1976;
        kg_post_scene(&cs, &zs);
        CHECK(focus_passes == 2); CHECK(focus_alpha == 1.0f); CHECK(s_post.last_qpc == 1976);
    }
    /* A brief dropped-depth frame still smooths over the actual observation gap. */
    reset(); kg_post_scene(&cs, &zs);
    clock_tick = 1016; kg_post_scene(&cs, NULL);
    clock_tick = 1032; kg_post_scene(&cs, &zs);
    CHECK(fabsf(focus_alpha - (float)(1.0 - exp(-0.032 / 0.25))) < 1e-6f);
    printf("PASS: %u production focus-gap checks\n", checks);
    return 0;
}
