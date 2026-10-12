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
static const char *const subtitle_languages[] = { "en-US", "pt-BR", "es", "fr", "de", "it" };
static const char *const subtitle_directories[] = { "", "pt-BR", "es", "fr", "de", "it" };
static const char *const subtitle_stems[] = { "03_N", "05_N", "07_N", "09_N", "11_N", "13_N", "15_N", "17_N", "18_N" };
static uint64_t next_subtitle_session = 1000;

static void reset_page(void)
{
    /* test_reset installs the supplied definition in memory and fixes the
     * definition path to <test>; production local/menus.ini is never opened. */
    pc_menu_test_reset(page_definition);
    pc_menu_tick("", 1, &host);
}

static int make_fixture_directory(const char *path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int write_subtitle_fixture(const char *locale_dir, const char *stem, const char *end_time, const char *text)
{
    char path[128];
    FILE *file;
    int n = locale_dir && *locale_dir
        ? snprintf(path, sizeof(path), "subtitles\\%s\\%s.srt", locale_dir, stem)
        : snprintf(path, sizeof(path), "subtitles\\%s.srt", stem);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    file = fopen(path, "wb");
    if (!file) return 0;
    if (fprintf(file, "1\r\n00:00:00,000 --> %s\r\n%s\r\n\r\n", end_time, text) < 0) {
        fclose(file); return 0;
    }
    return fclose(file) == 0;
}

static void activate_subtitle(const char *language, const char *stem)
{
    const uint64_t session = next_subtitle_session++;
    _putenv_s("RECOMP_LANGUAGE", language);
    pc_movie_skip_test_reset();
    pc_movie_skip_test_time(1000);
    pc_movie_skip_update(session, PC_MOVIE_SKIP_CUTSCENE, 1);
    pc_movie_skip_subtitle_target(session, stem);
    pc_movie_skip_test_time(1200);
    pc_movie_skip_update(session, PC_MOVIE_SKIP_CUTSCENE, 1);
}

static unsigned subtitle_length(const uint16_t *text)
{
    unsigned n = 0;
    while (text[n]) ++n;
    return n;
}

static int subtitle_equals_ascii(const uint16_t *text, const char *expected)
{
    while (*expected && *text == (unsigned char)*expected) { ++text; ++expected; }
    return !*expected && !*text;
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
    if (!make_fixture_directory("subtitle-fixture")) {
        SetCurrentDirectoryA(previous); return 0;
    }
    if (!SetCurrentDirectoryA("subtitle-fixture")) { SetCurrentDirectoryA(previous); return 0; }
    if (!make_fixture_directory("subtitles")) {
        SetCurrentDirectoryA(previous); return 0;
    }
    {
        unsigned i;
        for (i = 1; i < sizeof(subtitle_directories) / sizeof(subtitle_directories[0]); ++i) {
            char path[64];
            const int n = snprintf(path, sizeof(path), "subtitles\\%s", subtitle_directories[i]);
            if (n < 0 || (size_t)n >= sizeof(path) || !make_fixture_directory(path)) {
                SetCurrentDirectoryA(previous); return 0;
            }
        }
    }
    return 1;
}

typedef struct BrightStats {
    int x0, y0, x1, y1;
    unsigned pixels, rows;
} BrightStats;

static int bright_pixel(uint32_t p)
{
    return (p >> 24) > 220 && ((p >> 16) & 255u) > 220 && ((p >> 8) & 255u) > 220 && (p & 255u) > 220;
}

static BrightStats measure_bright(const uint32_t *pixels, unsigned w, unsigned h,
                                  int x0, int y0, int x1, int y1)
{
    BrightStats out = { (int)w, (int)h, -1, -1, 0, 0 };
    int y, x, in_row = 0;
    if (!pixels) return out;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)w) x1 = (int)w;
    if (y1 > (int)h) y1 = (int)h;
    for (y = y0; y < y1; ++y) {
        int row_bright = 0;
        for (x = x0; x < x1; ++x) {
            if (!bright_pixel(pixels[(size_t)y * w + x])) continue;
            row_bright = 1;
            ++out.pixels;
            if (x < out.x0) out.x0 = x;
            if (x > out.x1) out.x1 = x;
            if (y < out.y0) out.y0 = y;
            if (y > out.y1) out.y1 = y;
        }
        if (row_bright && !in_row) ++out.rows;
        in_row = row_bright;
    }
    return out;
}

static int same_pixels_in_region(const uint32_t *a, const uint32_t *b, unsigned w,
                                 int x0, int y0, int x1, int y1)
{
    int y;
    if (!a || !b || x0 < 0 || x1 > (int)w || x1 < x0) return 0;
    for (y = y0; y < y1; ++y)
        if (memcmp(a + (size_t)y * w + x0, b + (size_t)y * w + x0,
                   (size_t)(x1 - x0) * sizeof(*a)) != 0) return 0;
    return 1;
}

static void test_subtitle_locale_matrix(void)
{
    uint16_t caption[PC_MOVIE_SUBTITLE_CHARS];
    unsigned language, stem;
    for (language = 0; language < sizeof(subtitle_languages) / sizeof(subtitle_languages[0]); ++language) {
        for (stem = 0; stem < sizeof(subtitle_stems) / sizeof(subtitle_stems[0]); ++stem) {
            char expected[64];
            snprintf(expected, sizeof(expected), "%s/%s", subtitle_languages[language], subtitle_stems[stem]);
            CHECK(write_subtitle_fixture(subtitle_directories[language], subtitle_stems[stem], "00:00:05,000", expected),
                  "write synthetic track for %s / %s", subtitle_languages[language], subtitle_stems[stem]);
        }
    }
    for (language = 0; language < sizeof(subtitle_languages) / sizeof(subtitle_languages[0]); ++language) {
        for (stem = 0; stem < sizeof(subtitle_stems) / sizeof(subtitle_stems[0]); ++stem) {
            char expected[64];
            snprintf(expected, sizeof(expected), "%s/%s", subtitle_languages[language], subtitle_stems[stem]);
            activate_subtitle(subtitle_languages[language], subtitle_stems[stem]);
            CHECK(pc_movie_skip_subtitle(caption, PC_MOVIE_SUBTITLE_CHARS) && subtitle_equals_ascii(caption, expected),
                  "real parser selects %s / %s", subtitle_languages[language], subtitle_stems[stem]);
        }
    }
    pc_movie_skip_clear();
}

static void test_subtitle_capacity(void)
{
    char text[256];
    uint16_t caption[256];
    volatile unsigned shared_capacity = (unsigned)PC_MOVIE_SUBTITLE_CHARS;
    unsigned i;
    memset(text, 'L', sizeof(text) - 1);
    text[sizeof(text) - 1] = 0;
    CHECK(shared_capacity >= 256u,
          "shared movie subtitle capacity keeps 255 cue units plus its terminator");
    CHECK(write_subtitle_fixture("", "03_N", "00:00:05,000", text), "write maximum-length synthetic cue");
    activate_subtitle("en-US", "03_N");
    CHECK(pc_movie_skip_subtitle(caption, 256),
          "parser retains a cue longer than the old 160-unit buffer");
    CHECK(subtitle_length(caption) == 255 && caption[255] == 0,
          "parser retains all 255 UTF-16 units and the terminator");
    for (i = 0; i < 255; ++i)
        if (caption[i] != 'L') break;
    CHECK(i == 255, "maximum-length cue is not truncated or corrupted");
}

static const uint32_t *draw_caption(const char *language, const char *stem, const char *text,
                                    unsigned w, unsigned h, int px, int py, unsigned pw, unsigned ph,
                                    int *changed)
{
    const uint32_t *pixels = NULL;
    if (!write_subtitle_fixture("", stem, "00:00:05,000", text)) {
        CHECK(0, "write synthetic layout cue");
        return NULL;
    }
    reset_page();
    activate_subtitle(language, stem);
    *changed = 0;
    CHECK(pc_menu_overlay(w, h, px, py, pw, ph, &pixels, changed) && pixels && *changed,
          "draw active caption in requested picture viewport");
    return pixels;
}

static void test_caption_layout(void)
{
    static const char explicit_lines[] = "FIRST LINE\nSECOND LINE";
    char word_wrap[PC_MOVIE_SUBTITLE_CHARS];
    char long_word[PC_MOVIE_SUBTITLE_CHARS];
    const unsigned w = 1280, h = 720;
    const uint32_t *pixels;
    BrightStats bounds;
    int changed, n = 0, i;

    pixels = draw_caption("en-US", "03_N", explicit_lines, w, h, 0, 0, w, h, &changed);
    bounds = measure_bright(pixels, w, h, 0, 0, (int)w, (int)h);
    CHECK(bounds.rows == 2, "explicit SRT newline produces two caption rows");

    for (i = 0; i < 40; ++i) n += snprintf(word_wrap + n, sizeof(word_wrap) - (size_t)n, "WORD ");
    word_wrap[n] = 0;
    pixels = draw_caption("en-US", "03_N", word_wrap, w, h, 0, 0, w, h, &changed);
    bounds = measure_bright(pixels, w, h, 0, 0, (int)w, (int)h);
    CHECK(bounds.rows >= 3 && bounds.rows <= 5, "caption wraps at spaces into visible rows");
    CHECK(bounds.pixels && bounds.x0 > 8 && bounds.x1 < (int)w - 8 && bounds.y0 > 8 && bounds.y1 < (int)h - 8,
          "wrapped caption remains inside the picture with safe margins");

    memset(long_word, 'W', sizeof(long_word) - 1);
    long_word[sizeof(long_word) - 1] = 0;
    pixels = draw_caption("en-US", "03_N", long_word, w, h, 0, 0, w, h, &changed);
    bounds = measure_bright(pixels, w, h, 0, 0, (int)w, (int)h);
    CHECK(bounds.rows >= 3 && bounds.rows <= 5, "an overlong word is split across lines (bright rows=%u, pixels=%u)", bounds.rows, bounds.pixels);
    CHECK(bounds.pixels && bounds.x0 > 8 && bounds.x1 < (int)w - 8 && bounds.y0 > 8 && bounds.y1 < (int)h - 8,
          "split long-word caption remains inside the picture");
}

static void check_picture_bounds(const char *label, unsigned w, unsigned h,
                                 int px, int py, unsigned pw, unsigned ph)
{
    char long_word[PC_MOVIE_SUBTITLE_CHARS];
    const uint32_t *pixels;
    BrightStats bounds;
    int changed;
    memset(long_word, 'W', sizeof(long_word) - 1);
    long_word[sizeof(long_word) - 1] = 0;
    pixels = draw_caption("en-US", "03_N", long_word, w, h, px, py, pw, ph, &changed);
    bounds = measure_bright(pixels, w, h, 0, 0, (int)w, (int)h);
    CHECK(bounds.pixels && bounds.x0 >= px && bounds.x1 < px + (int)pw &&
          bounds.y0 >= py && bounds.y1 < py + (int)ph,
          "%s caption is clipped to its movie picture", label);
    CHECK(bounds.x0 >= px + 8 && bounds.x1 < px + (int)pw - 8 &&
          bounds.y0 >= py + 8 && bounds.y1 < py + (int)ph - 8,
          "%s caption keeps a safe margin inside its movie picture", label);
}

static void test_caption_viewports(void)
{
    uint32_t *first;
    const uint32_t *pixels = NULL;
    BrightStats old_bounds, new_bounds;
    int changed = 0;
    const unsigned w = 1280, h = 720;
    static const char text[] = "VIEWPORT RESIZE CAPTION";
    size_t bytes = (size_t)w * h * 4;
    CHECK(write_subtitle_fixture("", "03_N", "00:00:05,000", text), "write viewport fixture");
    reset_page(); activate_subtitle("en-US", "03_N");
    CHECK(pc_menu_overlay(w, h, 100, 60, 1080, 600, &pixels, &changed) && pixels && changed,
          "draw initial viewport caption");
    first = (uint32_t *)malloc(bytes);
    if (!first || !pixels) { CHECK(0, "allocate viewport snapshot"); free(first); return; }
    memcpy(first, pixels, bytes);
    old_bounds = measure_bright(pixels, w, h, 0, 0, (int)w, (int)h);

    pc_movie_skip_test_time(1300); pc_movie_skip_update(next_subtitle_session - 1, PC_MOVIE_SKIP_CUTSCENE, 1);
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 400, 60, 800, 600, &pixels, &changed) && pixels && changed,
          "same-client movie viewport resize invalidates the caption cache");
    CHECK(memcmp(first, pixels, bytes) != 0, "same-client viewport resize redraws caption pixels");
    new_bounds = measure_bright(pixels, w, h, 0, 0, (int)w, (int)h);
    CHECK(new_bounds.pixels && old_bounds.pixels && new_bounds.x0 > old_bounds.x0 + 100,
          "caption follows the resized movie viewport");
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 400, 60, 800, 600, &pixels, &changed) && !changed,
          "unchanged same-client viewport reuses its caption pixels");
    free(first);
}

static void test_full_picture_caption_preserves_hint(void)
{
    char long_word[PC_MOVIE_SUBTITLE_CHARS];
    const uint32_t *pixels = NULL;
    BrightStats caption, corner;
    int changed = 0;
    const unsigned w = 1280, h = 720;
    uint32_t *hint_only = (uint32_t *)malloc((size_t)w * h * 4);
    if (!hint_only) { CHECK(0, "allocate hint-only reference image"); return; }
    memset(long_word, 'W', sizeof(long_word) - 1);
    long_word[sizeof(long_word) - 1] = 0;
    CHECK(write_subtitle_fixture("", "03_N", "00:00:05,000", long_word), "write full-picture long cue");
    reset_page(); activate_subtitle("en-US", "03_N");
    pc_menu_set_movie_skip_prompt(1);
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && pixels && changed,
          "draw long caption and client hint over the full-picture movie");
    caption = measure_bright(pixels, w, h, 80, 40, (int)w - 80, 620);
    corner = measure_bright(pixels, w, h, 1100, 640, 1270, 700);
    CHECK(caption.pixels && caption.rows >= 3, "long full-picture caption remains visible across rows (bright rows=%u, pixels=%u)", caption.rows, caption.pixels);
    CHECK(corner.pixels, "skip hint remains visible beside a long full-picture caption");

    pc_movie_skip_subtitle_target(next_subtitle_session - 1, "INVALID");
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && changed && pixels,
          "capture the hint layer without a caption");
    if (pixels) memcpy(hint_only, pixels, (size_t)w * h * 4);
    corner = measure_bright(hint_only, w, h, 1100, 640, 1270, 700);
    CHECK(corner.pixels, "hint-only reference contains the client-corner prompt");
    pc_movie_skip_subtitle_target(next_subtitle_session - 1, "03_N");
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && changed && pixels,
          "restore the long caption while keeping the hint visible");
    CHECK(same_pixels_in_region(hint_only, pixels, w, 1100, 640, 1270, 700),
          "long caption leaves the client-anchored hint pixels intact");

    pc_movie_skip_test_time(1300); pc_movie_skip_update(next_subtitle_session - 1, PC_MOVIE_SKIP_CUTSCENE, 1);
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && !changed,
          "unchanged long caption and hint reuse the composed image");
    CHECK(same_pixels_in_region(hint_only, pixels, w, 1100, 640, 1270, 700),
          "caption cache reuse preserves the client-corner hint pixels");

    pc_menu_set_movie_skip_prompt(0);
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && changed,
          "hiding the hint redraws the long caption");
    caption = measure_bright(pixels, w, h, 80, 40, (int)w - 80, 620);
    CHECK(caption.pixels, "hiding the hint preserves the long full-picture caption");
    CHECK(!same_pixels_in_region(hint_only, pixels, w, 1100, 640, 1270, 700),
          "hiding the hint removes its pixels while retaining the caption");
    pc_movie_skip_subtitle_target(next_subtitle_session - 1, "INVALID");
    changed = 0;
    CHECK(!pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && changed,
          "hiding the hint leaves no prompt when the caption is absent");

    pc_movie_skip_subtitle_target(next_subtitle_session - 1, "03_N");
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && changed,
          "caption remains active while the hint is hidden");
    caption = measure_bright(pixels, w, h, 80, 40, (int)w - 80, 620);
    CHECK(caption.pixels, "caption-only layer is visible before restoring the hint");
    pc_menu_set_movie_skip_prompt(1);
    changed = 0;
    CHECK(pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed) && changed,
          "showing the hint redraws it beside the long caption");
    corner = measure_bright(pixels, w, h, 1100, 640, 1270, 700);
    CHECK(corner.pixels && same_pixels_in_region(hint_only, pixels, w, 1100, 640, 1270, 700),
          "hint is restored without clearing the long caption");
    pc_movie_skip_clear();
    pc_menu_set_movie_skip_prompt(0);
    (void)pc_menu_overlay(w, h, 0, 0, w, h, &pixels, &changed);
    free(hint_only);
}

static void test_subtitle_overlay(unsigned w, unsigned h, int px, int py, unsigned pw, unsigned ph,
                                  uint32_t *base_copy, size_t bytes)
{
    const uint32_t *pixels = NULL;
    uint16_t caption[PC_MOVIE_SUBTITLE_CHARS];
    uint32_t *caption_copy;
    int changed = 0;

    CHECK(write_subtitle_fixture("", "03_N", "00:00:01,000", "CUTSCENE CAPTION"), "write page restoration cue");
    pc_movie_skip_test_reset();
    reset_page();
    CHECK(pc_menu_tick("TestPage", 1, &host), "open page under subtitle layer");
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && pixels, "capture uncaptioned page pixels");
    if (!pixels) return;
    memcpy(base_copy, pixels, bytes);

    activate_subtitle("en-US", "03_N");
    CHECK(pc_movie_skip_subtitle(caption, PC_MOVIE_SUBTITLE_CHARS) && subtitle_equals_ascii(caption, "CUTSCENE CAPTION"),
          "real movie session parses the active SRT cue");
    changed = 0;
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && pixels &&
          memcmp(base_copy, pixels, bytes) != 0, "subtitle cue overlays the native page");
    if (!pixels) return;
    caption_copy = (uint32_t *)malloc(bytes);
    if (!caption_copy) { CHECK(0, "allocate subtitle comparison pixels"); return; }
    memcpy(caption_copy, pixels, bytes);

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
    pc_movie_skip_update(next_subtitle_session - 1, PC_MOVIE_SKIP_CUTSCENE, 1);
    pc_movie_skip_test_time(2000);
    pc_movie_skip_update(next_subtitle_session - 1, PC_MOVIE_SKIP_CUTSCENE, 1);
    changed = 0;
    CHECK(pc_menu_overlay(w,h,px,py,pw,ph,&pixels,&changed) && changed && pixels &&
          !memcmp(base_copy, pixels, bytes), "caption expiry restores the exact underlying page");
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
    {
        char previous[MAX_PATH];
        if (!prepare_subtitle_fixture(previous)) {
            CHECK(0, "prepare isolated subtitle fixture directory");
        } else {
            test_subtitle_locale_matrix();
            test_subtitle_capacity();
            test_caption_layout();
            check_picture_bounds("letterboxed", 1920, 1200, 0, 60, 1920, 1080);
            check_picture_bounds("ultrawide", 3440, 1440, 760, 180, 1920, 1080);
            test_caption_viewports();
            test_full_picture_caption_preserves_hint();
            test_subtitle_overlay(w,h,px,py,pw,ph,before,(size_t)w*h*4);
            SetCurrentDirectoryA(previous);
        }
    }
    free(before); reset_page();
    printf("%s: %u movie overlay checks, %u failed\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
