/* The debug overlay's rows: frame rate, frame times, latency, ping and the game's rows, on a test clock. */
#include "pc_debug.h"
#include "pc_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static double s_now = 100.0;
static double test_now(void) { return s_now; }

static const char *value(const PcDebugRow *rows, int n, const char *label)
{
    int i;
    for (i = 0; i < n; i++) if (!strcmp(rows[i].label, label)) return rows[i].value;
    return NULL;
}

/* `frames` game frames at `hz`, each presented `delay` seconds after it is published. */
static void play(int frames, double hz, double delay)
{
    int i;
    for (i = 0; i < frames; i++) {
        const int64_t stamp = pc_debug_note_publish();
        s_now += delay;
        pc_debug_note_present(stamp, 1);
        pc_debug_note_present(stamp, 0);               /* an overlay redraw of the same frame */
        pc_debug_note_render(2.5, 1500, 1707, 960, 2560, 1440);
        s_now += 1.0 / hz - delay;
    }
}

int main(void)
{
    char path[MAX_PATH];
    PcDebugRow rows[PC_DEBUG_ROWS];
    int n, changed;
    GetTempPathA(sizeof(path), path);
    strcat_s(path, sizeof(path), "black-debug-test.ini");
    DeleteFileA(path);
    pc_video_test_reset(path);
    pc_debug_test_clock(test_now);
    pc_debug_test_reset();

    /* hidden: no rows, and Insert shows it */
    CHECK(pc_debug_rows(rows, PC_DEBUG_ROWS, &changed) == 0 && !changed);
    pc_debug_toggle();
    CHECK(pc_debug_visible());
    n = pc_debug_rows(rows, PC_DEBUG_ROWS, &changed);
    CHECK(n > 0 && changed);
    CHECK(value(rows, n, "FPS") && !strcmp(value(rows, n, "FPS"), "-"));
    CHECK(!value(rows, n, "PING"));                   /* no network session: no ping row */

    /* 60 frames a second, 8 ms from publishing to the screen */
    play(30, 60.0, 0.008);
    n = pc_debug_rows(rows, PC_DEBUG_ROWS, &changed);
    CHECK(changed);
    CHECK(value(rows, n, "FPS") && !strcmp(value(rows, n, "FPS"), "60"));
    CHECK(value(rows, n, "FRAME TIME") && !strncmp(value(rows, n, "FRAME TIME"), "16.7 MS", 7));
    CHECK(value(rows, n, "GAME") && !strcmp(value(rows, n, "GAME"), "60 FPS"));
    CHECK(value(rows, n, "GPU") && !strcmp(value(rows, n, "GPU"), "2.5 MS   1500 DRAWS"));
    CHECK(value(rows, n, "RESOLUTION") && !strcmp(value(rows, n, "RESOLUTION"), "1707 X 960 > 2560 X 1440"));
    /* the game's own frame (16.7 ms at 60) and the 8 ms to the screen */
    CHECK(value(rows, n, "LATENCY") && !strcmp(value(rows, n, "LATENCY"), "25 MS"));

    /* rows hold still between refreshes */
    s_now += 0.05;
    pc_debug_rows(rows, PC_DEBUG_ROWS, &changed);
    CHECK(!changed);

    /* a hitch shows as the worst frame time */
    play(10, 60.0, 0.008);
    s_now += 0.1;
    play(10, 60.0, 0.008);
    n = pc_debug_rows(rows, PC_DEBUG_ROWS, &changed);
    CHECK(value(rows, n, "FRAME TIME") && strstr(value(rows, n, "FRAME TIME"), "MAX 116.7"));

    /* a network session, and the game's rows */
    pc_debug_note_ping(24);
    pc_debug_game_row(0, "POSITION", "%.1f   %.1f   %.1f", 1.0, 2.0, -3.5);
    s_now += 0.3;
    n = pc_debug_rows(rows, PC_DEBUG_ROWS, &changed);
    CHECK(value(rows, n, "PING") && !strcmp(value(rows, n, "PING"), "24 MS"));
    CHECK(value(rows, n, "POSITION") && !strcmp(value(rows, n, "POSITION"), "1.0   2.0   -3.5"));
    CHECK(n <= PC_DEBUG_ROWS);

    /* Insert again hides it */
    pc_debug_toggle();
    CHECK(!pc_debug_visible() && pc_debug_rows(rows, PC_DEBUG_ROWS, &changed) == 0);

    pc_debug_test_clock(NULL);
    DeleteFileA(path);
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
