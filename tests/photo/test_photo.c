/* Photo mode: the free camera, its limits and controls, photos, and the native panel.
 *
 *   photo-test MENUS.INI OUTDIR        MENUS.INI is the built-in definition (extract_definition.py)
 *
 * With BLACK_GAME_DIR set to the game's files, the panel is also rendered to OUTDIR\photo-*.bmp with the game's own
 * fonts, over a grey picture. */
#include "pc_menu.h"
#include "pc_photo.h"
#include "pc_input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int g_fail, g_pass;
#define CHECK(c) do { if (c) g_pass++; else { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CLOSE_TO(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))

/* ---- a synthetic game ---- */

static int g_can_start = 1, g_result = 1, g_captures;
static char g_capture_path[MAX_PATH + 64];
static int can_start(void) { return g_can_start; }
static void capture(const char *path) { g_captures++; snprintf(g_capture_path, sizeof(g_capture_path), "%s", path); }
static int capture_result(void) { return g_result; }
static const PcPhotoHost g_photo_host = { can_start, capture, capture_result };

/* The game's camera: at (10, 2, -5), looking along yaw 30 degrees, level; `hand` -1 mirrors right. */
static void game_matrix(float m[4][3], double yaw_deg, double hand)
{
    const double y = yaw_deg * 3.14159265358979323846 / 180.0;
    m[2][0] = (float)sin(y); m[2][1] = 0; m[2][2] = (float)cos(y);
    m[0][0] = (float)(hand * cos(y)); m[0][1] = 0; m[0][2] = (float)(hand * -sin(y));
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0;
    m[3][0] = 10; m[3][1] = 2; m[3][2] = -5;
}

static int same(float a[4][3], float b[4][3], double e)
{
    int i, k;
    for (i = 0; i < 4; i++) for (k = 0; k < 3; k++) if (!CLOSE_TO(a[i][k], b[i][k], e)) return 0;
    return 1;
}

static void frame(float m[4][3], int ms) { Sleep((DWORD)ms); pc_photo_camera(m, 0.7f); }

/* ---- the camera ---- */

static void test_camera(void)
{
    float game[4][3], m[4][3];
    double dist;
    int i;
    pc_photo_set_shown(1);
    for (i = 0; i < 2; i++) {
        /* no input: the game's own view, in either handedness */
        game_matrix(game, 30, i ? -1.0 : 1.0);
        memcpy(m, game, sizeof(m));
        CHECK(pc_photo_camera(m, 0.7f) == 1);
        CHECK(same(m, game, 1e-4));
        memcpy(m, game, sizeof(m));
        frame(m, 20);
        CHECK(same(m, game, 1e-4));
        pc_photo_reset();
    }
    /* W for about half a second at NORMAL (1.5 m/s): forward along the view */
    game_matrix(game, 30, 1.0);
    memcpy(m, game, sizeof(m));
    pc_photo_camera(m, 0.7f);
    pc_input_key_event(0x11, 0, 1, 0);
    for (i = 0; i < 10; i++) { memcpy(m, game, sizeof(m)); frame(m, 50); }
    pc_input_key_event(0x11, 0, 0, 0);
    dist = sqrt((m[3][0] - 10.0) * (m[3][0] - 10.0) + (m[3][2] + 5.0) * (m[3][2] + 5.0));
    CHECK(dist > 0.4 && dist < 1.2);
    CHECK(CLOSE_TO(atan2(m[3][0] - 10.0, m[3][2] + 5.0), 30 * 3.14159265358979323846 / 180.0, 0.02));
    CHECK(CLOSE_TO(m[3][1], 2.0, 1e-4));
    /* mouse right turns right: the forward vector moves away from the matrix's first row, which BLACK points to the
     * screen's left */
    {
        const double before = m[2][0] * game[0][0] + m[2][2] * game[0][2];
        pc_input_capture_changed(1);
        pc_input_mouse_motion(300, 0);
        memcpy(m, game, sizeof(m));
        frame(m, 10);
        CHECK(m[2][0] * game[0][0] + m[2][2] * game[0][2] < before - 0.2);
    }
    /* the range: Shift and W at VERY FAST cannot leave 12 m around the start */
    CHECK(pc_photo_row_change(PCP_ROW_SPEED, 1) && pc_photo_row_change(PCP_ROW_SPEED, 1));
    pc_input_key_event(0x2A, 0, 1, 0);
    pc_input_key_event(0x11, 0, 1, 0);
    for (i = 0; i < 20; i++) { memcpy(m, game, sizeof(m)); frame(m, 60); }
    pc_input_key_event(0x11, 0, 0, 0);
    pc_input_key_event(0x2A, 0, 0, 0);
    dist = sqrt((m[3][0] - 10.0) * (m[3][0] - 10.0) + (m[3][2] + 5.0) * (m[3][2] + 5.0));
    CHECK(dist <= 12.0 + 1e-3 && dist > 11.0);
    /* E rises, at most half the range above the start */
    pc_input_key_event(0x12, 0, 1, 0);
    for (i = 0; i < 20; i++) { memcpy(m, game, sizeof(m)); frame(m, 60); }
    pc_input_key_event(0x12, 0, 0, 0);
    CHECK(m[3][1] <= 2.0 + 6.0 + 1e-3 && m[3][1] > 7.0);
    /* zoom: the wheel narrows, within its bounds */
    CHECK(CLOSE_TO(pc_photo_zoom(), 1.0, 1e-6));
    pc_input_wheel_event(1);
    pc_input_wheel_event(1);
    memcpy(m, game, sizeof(m)); frame(m, 5);
    CHECK(CLOSE_TO(pc_photo_zoom(), 1.0 / (1.08 * 1.08), 1e-4));
    for (i = 0; i < 80; i++) pc_input_wheel_event(1);
    memcpy(m, game, sizeof(m)); frame(m, 5);
    CHECK(CLOSE_TO(pc_photo_zoom(), 0.15, 1e-4));
    /* roll: the panel's five-degree steps turn right and up about forward */
    pc_photo_reset();
    memcpy(m, game, sizeof(m)); pc_photo_camera(m, 0.7f);
    for (i = 0; i < 6; i++) pc_photo_row_change(PCP_ROW_ROLL, 1);
    memcpy(m, game, sizeof(m)); frame(m, 5);
    CHECK(CLOSE_TO(m[1][1], cos(30 * 3.14159265358979323846 / 180.0), 1e-3));
    CHECK(same(m, game, 1.0) && CLOSE_TO(m[2][0], game[2][0], 1e-4));
    {
        char v[32];
        pc_photo_row_value(PCP_ROW_ROLL, v, sizeof(v));
        CHECK(!strcmp(v, "30"));
        pc_photo_row_value(PCP_ROW_FOV, v, sizeof(v));
        CHECK(!strcmp(v, "70"));                                          /* 2 * atan(0.7) */
        pc_photo_row_value(PCP_ROW_WEAPON, v, sizeof(v));
        CHECK(!strcmp(v, "HIDDEN") && pc_photo_weapon_hidden());
        pc_photo_row_change(PCP_ROW_WEAPON, 1);
        CHECK(!pc_photo_weapon_hidden());
        pc_photo_row_change(PCP_ROW_WEAPON, 1);
    }
    /* a photo: a .png in the photo folder, named from the time, then what happened to it */
    g_captures = 0;
    g_result = 0;
    CHECK(pc_photo_take());
    memcpy(m, game, sizeof(m)); frame(m, 5);
    CHECK(g_captures == 1 && strstr(g_capture_path, "BLACK_") && strstr(g_capture_path, ".png"));
    CHECK(strstr(pc_photo_help(), "PHOTO SAVED") != NULL && pc_photo_shutter() && !pc_photo_shutter());
    g_result = -1;
    CHECK(strstr(pc_photo_help(), "COULD NOT") != NULL);
    g_result = 1;
    /* leaving: the game's camera again */
    pc_photo_set_shown(0);
    memcpy(m, game, sizeof(m));
    CHECK(pc_photo_camera(m, 0.7f) == 0 && same(m, game, 0) && CLOSE_TO(pc_photo_zoom(), 1.0, 0));
}

/* ---- the panel ---- */

static int f_gamecode(void *c, const char *n, const char *const a[3], int f) { (void)c; (void)n; (void)a; (void)f; return 1; }
static void f_page(void *c, const char *n) { (void)c; (void)n; }
static int f_value(void *c, const char *n, int *o) { (void)c; (void)n; *o = 0; return 0; }
static int f_text(void *c, const char *s, uint16_t *o, unsigned cap)
{
    static const char *const keys[][2] = { { "$FE_SELECT", "SELECT" }, { "$FE_CONFIRM", "CONFIRM" }, { "$FE_BACK", "BACK" } };
    unsigned i, k;
    (void)c;
    for (i = 0; i < 3; i++) if (!strcmp(s, keys[i][0])) {
        for (k = 0; keys[i][1][k] && k + 1 < cap; k++) o[k] = (uint16_t)keys[i][1][k];
        o[k] = 0;
        return (int)k;
    }
    return -1;
}
static void f_sound(void *c, const char *clip) { (void)c; (void)clip; }
static PcMenuHost g_host = { NULL, f_gamecode, f_page, f_value, f_text, f_sound, NULL, NULL, NULL, NULL };
static void tick(const char *page) { pc_menu_tick(page, 1, &g_host); }
static void press(int action, const char *page) { pc_menu_test_action(action); tick(page); }

static uint8_t *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) { b[n] = 0; if (size) *size = (size_t)n; }
    return b;
}

static void test_panel(const char *definition_path)
{
    char *def = (char *)read_all(definition_path, NULL), log[4096], dump[16384];
    float m[4][3];
    CHECK(def != NULL);
    if (!def) return;
    CHECK(pc_menu_check_definition(def, log, sizeof(log)) >= 13 && !log[0]);
    if (log[0]) printf("%s", log);
    pc_menu_test_reset(def);
    tick("");
    tick("PauseMenu");
    /* CONTINUE, OBJECTIVES, RESTART, OPTIONS, PHOTO MODE */
    g_can_start = 0;
    press(PCM_ACT_DOWN, "PauseMenu"); press(PCM_ACT_DOWN, "PauseMenu"); press(PCM_ACT_DOWN, "PauseMenu"); press(PCM_ACT_DOWN, "PauseMenu");
    press(PCM_ACT_CONFIRM, "PauseMenu");
    CHECK(!pc_photo_active());                                            /* a scripted camera: refused */
    g_can_start = 1;
    press(PCM_ACT_CONFIRM, "PauseMenu");
    CHECK(pc_photo_active() && pc_menu_test_focus() == 0);
    game_matrix(m, 0, 1.0);
    pc_photo_camera(m, 0.7f);
    tick("PauseMenu");
    pc_menu_test_dump(dump, sizeof(dump));
    CHECK(strstr(dump, "FIELD OF VIEW") && strstr(dump, "EXIT PHOTO MODE") && strstr(dump, "70"));
    CHECK(!strstr(dump, "CONTINUE"));
    press(PCM_ACT_RIGHT, "PauseMenu");                                    /* field of view, wider */
    CHECK(pc_photo_zoom() > 1.0f);
    pc_photo_hide_panel();
    pc_photo_camera(m, 0.7f);
    tick("PauseMenu");
    CHECK(pc_menu_test_dump(dump, sizeof(dump)) == 0 || !strstr(dump, "FIELD OF VIEW"));
    press(PCM_ACT_DOWN, "PauseMenu");                                     /* any panel input brings it back */
    CHECK(!pc_photo_panel_hidden() && pc_menu_test_focus() == 0);
    press(PCM_ACT_BACK, "PauseMenu");                                     /* back to the pause menu */
    CHECK(!pc_photo_active());
    free(def);
}

/* ---- pictures ---- */

static int write_bmp(const char *path, const uint32_t *px, unsigned w, unsigned h)
{
    FILE *f = fopen(path, "wb");
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    unsigned y;
    if (!f) return 0;
    memset(&fh, 0, sizeof(fh)); memset(&ih, 0, sizeof(ih));
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(ih); fh.bfSize = fh.bfOffBits + w * h * 4;
    ih.biSize = sizeof(ih); ih.biWidth = (LONG)w; ih.biHeight = -(LONG)h; ih.biPlanes = 1; ih.biBitCount = 32;
    fwrite(&fh, sizeof(fh), 1, f); fwrite(&ih, sizeof(ih), 1, f);
    for (y = 0; y < h; y++) fwrite(px + (size_t)y * w, 4, w, f);
    fclose(f);
    return 1;
}

static int load_fonts(void)
{
    const char *dir = getenv("BLACK_GAME_DIR");
    static const uint32_t ids[2] = { 4u, 3u };
    static const int slots[2] = { PCM_IMAGE_LOGO, PCM_IMAGE_BULLET };
    char path[512];
    size_t size;
    uint8_t *d;
    int ok = 1;
    if (!dir || !*dir) return 0;
    snprintf(path, sizeof(path), "%s/language/fonts/Small.bin", dir);
    d = read_all(path, &size); ok &= d && pc_menu_load_font_file(PCM_FONT_SMALL, d, size); free(d);
    snprintf(path, sizeof(path), "%s/language/fonts/Big.bin", dir);
    d = read_all(path, &size); ok &= d && pc_menu_load_font_file(PCM_FONT_BIG, d, size); free(d);
    snprintf(path, sizeof(path), "%s/export/FrontEnd/Core.bin", dir);
    d = read_all(path, &size); ok &= d && pc_menu_load_bundle_images(d, size, "core", ids, slots, 2) == 2; free(d);
    return ok;
}

static void render(const char *definition_path, const char *outdir, int focus, const char *name)
{
    char *def = (char *)read_all(definition_path, NULL), path[600];
    uint32_t *px = (uint32_t *)calloc(640 * 480, 4);
    float m[4][3];
    int i;
    if (!def || !px) { free(def); free(px); return; }
    pc_menu_test_reset(def);
    tick("");
    tick("PhotoMode");
    game_matrix(m, 0, 1.0);
    pc_photo_camera(m, 0.7f);
    for (i = 0; i < focus; i++) press(PCM_ACT_DOWN, "PhotoMode");
    Sleep(400);
    tick("PhotoMode");
    for (i = 0; i < 640 * 480; i++) {                                     /* a picture behind: a soft gradient */
        const unsigned v = 60 + (unsigned)(i % 640) / 8;
        px[i] = 0xFF000000u | (v << 16) | (v << 8) | (v - 10);
    }
    pc_menu_test_render(px, 640, 480);
    snprintf(path, sizeof(path), "%s\\photo-%s.bmp", outdir, name);
    write_bmp(path, px, 640, 480);
    printf("rendered %s\n", path);
    free(px); free(def);
}

int main(int argc, char **argv)
{
    char env[MAX_PATH + 32], dir[MAX_PATH];
    const char *outdir = argc > 2 ? argv[2] : ".";
    if (argc < 2) { printf("usage: photo-test MENUS.INI [OUTDIR]\n"); return 2; }
    snprintf(dir, sizeof(dir), "%s\\photos", outdir);
    snprintf(env, sizeof(env), "RECOMP_PHOTO_DIR=%s", dir);
    _putenv(env);
    snprintf(env, sizeof(env), "RECOMP_MENUS_INI=%s", argv[1]);
    _putenv(env);
    _putenv("RECOMP_LANGUAGE=en-US");
    _putenv("RECOMP_BLACK_INPUT_MODE=auto");
    _putenv("RECOMP_BLACK_PROMPT_MODE=keyboard_mouse");
    pc_input_init();
    pc_photo_set_host(&g_photo_host);
    pc_photo_install();
    pc_menu_install();
    test_camera();
    test_panel(argv[1]);
    if (load_fonts()) {
        render(argv[1], outdir, 0, "panel");
        render(argv[1], outdir, 5, "panel-take");
    } else {
        printf("(BLACK_GAME_DIR not set: the panel is not rendered)\n");
    }
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
