/* Exercise the production hint/page renderer with an authored one-texel font.
 * No retail fonts, images, menu definitions or persisted settings are loaded. */
#include "pc_menu.h"
#include "pc_movie_skip.h"
#include "pc_video_menu.h"
#include "pc_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static unsigned checks, failures;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++failures; fprintf(stderr, "FAIL line %d: ", __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* The test never opens Video Settings or takes input. Keep those unrelated
 * services inactive, so they cannot initialize or write a profile. */
PcDevice pc_input_prompt_device(void) { return PC_DEVICE_KEYBOARD_MOUSE; }
void pc_input_set_filter_slot(int slot, const PcInputFilter *filter) { (void)slot; (void)filter; }
void pc_video_menu_hide_hint(int hide) { (void)hide; }
void pc_video_menu_set_native_opener(void (*request)(void)) { (void)request; }
const char *pc_video_menu_row_help(int row) { (void)row; return ""; }
int pc_video_menu_row_locked(int row) { (void)row; return 1; }
size_t pc_video_menu_row_value(int row, char *out, size_t cap)
{ (void)row; if (cap) out[0] = 0; return 0; }
int pc_video_menu_row_change(int row, int dir)
{ (void)row; (void)dir; fputs("Unexpected settings change\n", stderr); exit(1); }
void pc_video_menu_reset_settings(void)
{ fputs("Unexpected settings reset\n", stderr); exit(1); }
int pc_video_menu_overlay(unsigned w, unsigned h, const uint32_t **pixels, int *changed)
{ (void)w; (void)h; *pixels = NULL; *changed = 0; return 0; }

static const char page_definition[] =
    "[style]\nintro = 0\nanimation = 0\nsounds = off\n"
    "[page TestPage]\ntitle = \"SYNTHETIC PAGE\"\nlogo = none\n"
    "item = \"SYNTHETIC ITEM\" | none\n";
static const PcMenuHost host = { 0 };

static void reset_page(void)
{
    /* test_reset installs the supplied definition in memory and fixes the
     * definition path to <test>; production local/menus.ini is never opened. */
    pc_menu_test_reset(page_definition);
    pc_menu_tick("", 1, &host);
}

static int prepare_subtitle_fixture(char previous[MAX_PATH])
{
    char module[MAX_PATH];
    char *slash;
    DWORD length = GetCurrentDirectoryA(MAX_PATH, previous);
    if (!length || length >= MAX_PATH) return 0;
    length = GetModuleFileNameA(NULL, module, MAX_PATH);
    if (!length || length >= MAX_PATH) return 0;
    slash = strrchr(module, '\\');
    if (!slash) return 0;
    *slash = 0;
    if (!SetCurrentDirectoryA(module)) return 0;
    if (!CreateDirectoryA("subtitle-fixture", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        SetCurrentDirectoryA(previous); return 0;
    }
    if (!SetCurrentDirectoryA("subtitle-fixture")) { SetCurrentDirectoryA(previous); return 0; }
    if (!CreateDirectoryA("subtitles", NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        SetCurrentDirectoryA(previous); return 0;
    }
    {
        static const char srt[] =
            "1\r\n00:00:00,000 --> 00:00:01,000\r\nCUTSCENE CAPTION\r\n\r\n";
        FILE *file = fopen("subtitles\\03_N.srt", "wb");
        if (!file) { SetCurrentDirectoryA(previous); return 0; }
        if (fwrite(srt, 1, sizeof(srt) - 1, file) != sizeof(srt) - 1) {
            fclose(file); SetCurrentDirectoryA(previous); return 0;
        }
        if (fclose(file) != 0) { SetCurrentDirectoryA(previous); return 0; }
    }
    return 1;
}

static void test_subtitle_overlay(unsigned w, unsigned h, int px, int py, unsigned pw, unsigned ph,
                                  uint32_t *base_copy, size_t bytes)
{
    char previous[MAX_PATH];
    const uint32_t *pixels = NULL;
    uint16_t caption[160];
    uint32_t *caption_copy;
    int changed = 0;

    if (!prepare_subtitle_fixture(previous)) {
        CHECK(0, "prepare isolated SRT fixture under test output");
        return;
    }
    _putenv("RECOMP_LANGUAGE=en-US");
    pc_movie_skip_test_reset();
    reset_page();
    CHECK(pc_menu_tick("TestPage", 1, &host), "open page under subtitle layer");
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && pixels, "capture uncaptioned page pixels");
    if (!pixels) { SetCurrentDirectoryA(previous); return; }
    memcpy(base_copy, pixels, bytes);

    pc_movie_skip_test_time(1000);
    pc_movie_skip_update(42, PC_MOVIE_SKIP_CUTSCENE, 1);
    pc_movie_skip_subtitle_target(42, "03_N");
    pc_movie_skip_test_time(1200);
    pc_movie_skip_update(42, PC_MOVIE_SKIP_CUTSCENE, 1);
    CHECK(pc_movie_skip_subtitle(caption, 160) && caption[0] == 'C' && caption[15] == 'N' && caption[16] == 0,
          "real movie session parses the active SRT cue");
    changed = 0;
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && pixels &&
          memcmp(base_copy, pixels, bytes) != 0, "subtitle cue overlays the native page");
    if (!pixels) { SetCurrentDirectoryA(previous); return; }
    caption_copy = (uint32_t *)malloc(bytes);
    if (!caption_copy) { CHECK(0, "allocate subtitle comparison pixels"); SetCurrentDirectoryA(previous); return; }
    memcpy(caption_copy, pixels, bytes);

    /* The skip hint and caption share a layer: hiding the hint must preserve the caption. */
    pc_menu_set_movie_skip_prompt(1);
    changed = 0;
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed &&
          memcmp(caption_copy, pixels, bytes) != 0, "skip hint composes with subtitle");
    pc_menu_set_movie_skip_prompt(0);
    changed = 0;
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed &&
          !memcmp(caption_copy, pixels, bytes), "removing hint preserves subtitle over page");
    free(caption_copy);

    pc_movie_skip_test_time(1600);
    pc_movie_skip_update(42, PC_MOVIE_SKIP_CUTSCENE, 1);
    pc_movie_skip_test_time(2000);
    pc_movie_skip_update(42, PC_MOVIE_SKIP_CUTSCENE, 1);
    changed = 0;
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && pixels &&
          !memcmp(base_copy, pixels, bytes), "caption expiry restores the exact underlying page");
    SetCurrentDirectoryA(previous);
}

int main(void)
{
    const unsigned w = 1280, h = 720, pw = 1080, ph = 600;
    const int px = 100, py = 60;
    const uint32_t *pixels = NULL;
    uint32_t *before = (uint32_t *)malloc((size_t)w * h * 4);
    int changed = 0, x, y, x0 = (int)w, y0 = (int)h, x1 = -1, y1 = -1;
    int bright = 0, premultiplied = 1;
    unsigned box[4], i;
    PcMenuGlyph glyphs[95];
    const uint32_t texel = 0xFFFFFFFFu;
    PcMenuFont font;
    if (!before) return 2;
    memset(glyphs, 0, sizeof(glyphs));
    for (i = 0; i < 95; ++i) {
        glyphs[i].code = (uint16_t)(32 + i);
        glyphs[i].w = i ? 1.0f : 0; glyphs[i].h = i ? 1.0f : 0;
        glyphs[i].advance = 0.5f;
    }
    memset(&font, 0, sizeof(font));
    font.pixels = &texel; font.width = font.height = 1; font.nominal = 1;
    font.glyphs = glyphs; font.count = 95;
    CHECK(pc_menu_set_font(PCM_FONT_SMALL, &font), "register synthetic font");
    CHECK(pc_menu_has_font(PCM_FONT_SMALL) && !pc_menu_has_font(-1) && !pc_menu_has_font(PCM_FONT_COUNT),
          "font readiness and invalid slots");
    reset_page();
    CHECK(!pc_menu_active(), "the hint needs no native menu");
    pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed);
    pc_menu_set_movie_skip_prompt(1);
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && pixels,
          "show hint without a page");
    if (!pixels) { free(before); return 1; }
    for (y = 0; y < (int)h; ++y) for (x = 0; x < (int)w; ++x) {
        const uint32_t p = pixels[(size_t)y * w + x];
        if (!p) continue;
        if (x < x0) x0 = x;
        if (y < y0) y0 = y;
        if (x > x1) x1 = x;
        if (y > y1) y1 = y;
        if ((p & 255u) > 160) ++bright;
        if (((p >> 16) & 255u) > (p >> 24) || ((p >> 8) & 255u) > (p >> 24) || (p & 255u) > (p >> 24)) premultiplied = 0;
    }
    CHECK(x0 > (int)w/2 && y0 > (int)h*8/10 && x1 > (int)w-100 && y1 > (int)h-100 &&
          x1 < (int)w-10 && y1 < (int)h-10, "client bottom-right safe margin");
    CHECK(bright > 20, "bright text over hint background");
    CHECK(premultiplied, "premultiplied alpha");
    CHECK(pc_menu_overlay_dirty(box) && box[0] == 0 && box[1] == 0 && box[2] == w && box[3] == h,
          "showing hint invalidates upload");
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && !changed, "reuse unchanged hint pixels");
    memcpy(before,pixels,(size_t)w*h*4);
    CHECK(pc_menu_overlay(w,h,px,180,pw,480,&pixels,&changed) && !changed, "movie rectangle keeps client anchor");
    CHECK(!memcmp(before,pixels,(size_t)w*h*4), "letterboxing leaves hint pixels unchanged");
    CHECK(pc_menu_overlay(3440,1440,760,0,1920,1440,&pixels,&changed) && changed,
          "ultrawide client redraws outside fitted movie");
    CHECK(pixels[(size_t)(1440-61)*3440+(3440-61)] != 0, "ultrawide scaled safe margin");
    CHECK(pc_menu_overlay(640,480,0,0,640,480,&pixels,&changed) && changed, "resize redraws hint");
    pc_menu_set_movie_skip_prompt(0);
    CHECK(!pc_menu_overlay(640,480,0,0,640,480,&pixels,&changed) && changed, "last layer removal redraws");
    CHECK(!pc_menu_overlay(640,480,0,0,640,480,&pixels,&changed) && !changed, "no repeated hidden redraw");
    reset_page();
    CHECK(pc_menu_tick("TestPage",1,&host), "open the synthetic native page");
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && pixels, "draw page before composing hint");
    memcpy(before,pixels,(size_t)w*h*4);
    pc_menu_set_movie_skip_prompt(1);
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && memcmp(before,pixels,(size_t)w*h*4),
          "hint composes over existing page");
    pc_menu_set_movie_skip_prompt(0);
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && !memcmp(before,pixels,(size_t)w*h*4),
          "hiding hint restores exact cached page");
    CHECK(pc_menu_overlay_dirty(box) && box[2] == w && box[3] == h, "page restoration replaces composed upload");
    reset_page();
    test_subtitle_overlay(w,h,px,py,pw,ph,before,(size_t)w*h*4);
    free(before); reset_page();
    printf("%s: %u movie overlay checks, %u failed\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
