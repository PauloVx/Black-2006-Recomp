/* The PC crosshair: how shots, movement, zoom and kills move it, on a test clock. */
#include "pc_crosshair.h"
#include "pc_video.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static double s_now;
static double test_now(void) { return s_now; }

/* Ticks every 1/120 s for `seconds`, then the look of that moment. */
static int run(double seconds, int control, float speed, unsigned shots, float zoom, PcCrosshairDraw *out)
{
    const double end = s_now + seconds;
    int shown = 0;
    while (s_now < end) {
        s_now += 1.0 / 120.0;
        pc_crosshair_tick(control, speed, shots, zoom);
        shown = pc_crosshair_frame(out);
    }
    return shown;
}

static void settings(unsigned crosshair, unsigned hitmarkers)
{
    PcVideoSettings s;
    pc_video_get(&s);
    s.crosshair = crosshair;
    s.hitmarkers = hitmarkers;
    CHECK(pc_video_set(&s));
}

int main(void)
{
    char path[MAX_PATH];
    PcCrosshairDraw d;
    float rest;
    GetTempPathA(sizeof(path), path);
    strcat_s(path, sizeof(path), "black-crosshair-test.ini");
    DeleteFileA(path);
    _putenv_s("RECOMP_CROSSHAIR", ""); _putenv_s("RECOMP_HITMARKERS", "");
    pc_video_test_reset(path);
    pc_crosshair_test_clock(test_now);

    /* Fresh settings keep both additions off, including after gameplay ticks and hit events. */
    {
        PcVideoSettings s;
        pc_video_get(&s);
        CHECK(s.crosshair == PCX_ORIGINAL && s.hitmarkers == 0);
    }
    CHECK(!pc_crosshair_frame(&d));
    CHECK(!run(0.5, 1, 0.0f, 0, 1.0f, &d) && !pc_crosshair_replaces_dot());
    pc_crosshair_hit(PCX_MARK_HIT);
    pc_crosshair_hit(PCX_MARK_KILL);
    CHECK(!pc_crosshair_frame(&d));
    settings(PCX_CROSS, 1);
    run(0.7, 1, 0.0f, 0, 1.0f, &d);
    CHECK(run(0.5, 1, 0.0f, 0, 1.0f, &d) && d.style == PCX_CROSS && d.alpha > 0.5f && d.marker_alpha == 0.0f);
    rest = d.gap;
    CHECK(rest >= 5.0f && rest <= 9.0f);

    /* three bullets open it at once; it closes within half a second */
    s_now += 1.0 / 120.0;
    pc_crosshair_tick(1, 0.0f, 3, 1.0f);
    CHECK(pc_crosshair_frame(&d) && d.gap > rest + 10.0f);
    CHECK(run(0.6, 1, 0.0f, 3, 1.0f, &d) && d.gap < rest + 1.0f);
    CHECK(pc_crosshair_since_shot() > 0.5 && pc_crosshair_since_shot() < 0.7);

    /* a long burst stays below the cap */
    {
        unsigned shots = 3, i;
        for (i = 0; i < 60; ++i) { shots++; run(0.05, 1, 0.0f, shots, 1.0f, &d); }
        CHECK(d.gap > rest + 15.0f && d.gap <= 46.0f);
        run(1.0, 1, 0.0f, shots, 1.0f, &d);
        /* the mission's count restarting is not a shot */
        s_now += 1.0 / 120.0;
        pc_crosshair_tick(1, 0.0f, 0, 1.0f);
        CHECK(pc_crosshair_frame(&d) && d.gap < rest + 1.0f);
    }

    /* running opens it as long as it lasts */
    CHECK(run(1.0, 1, 5.0f, 0, 1.0f, &d) && d.gap > rest + 8.0f);
    CHECK(run(1.0, 1, 0.0f, 0, 1.0f, &d) && d.gap < rest + 1.0f);

    /* aiming down the sights hides it; the player not in control, or no ticks, hide it */
    CHECK(!run(0.3, 1, 0.0f, 0, 0.5f, &d));
    CHECK(run(0.3, 1, 0.0f, 0, 1.0f, &d));
    CHECK(pc_crosshair_replaces_dot());
    CHECK(!run(0.3, 0, 0.0f, 0, 1.0f, &d) && !pc_crosshair_replaces_dot());
    run(0.3, 1, 0.0f, 0, 0.5f, &d);
    CHECK(!pc_crosshair_replaces_dot());
    run(0.3, 1, 0.0f, 0, 1.0f, &d);
    s_now += 0.5;
    CHECK(!pc_crosshair_frame(&d) && !pc_crosshair_replaces_dot());

    /* a kill flashes a marker that fades; a hit is shorter and does not cut a kill short */
    run(0.1, 1, 0.0f, 0, 1.0f, &d);
    pc_crosshair_hit(PCX_MARK_KILL);
    CHECK(pc_crosshair_frame(&d) && d.marker_kill && d.marker_alpha > 0.9f && d.marker_outer > d.marker_inner);
    pc_crosshair_hit(PCX_MARK_HIT);
    CHECK(pc_crosshair_frame(&d) && d.marker_kill);
    CHECK(run(0.7, 1, 0.0f, 0, 1.0f, &d) && d.marker_alpha == 0.0f);
    pc_crosshair_hit(PCX_MARK_HIT);
    CHECK(pc_crosshair_frame(&d) && !d.marker_kill && d.marker_alpha > 0.9f);
    run(0.4, 1, 0.0f, 0, 1.0f, &d);
    CHECK(d.marker_alpha == 0.0f);

    /* the game's dot alone: nothing drawn, except a marker; markers off: no marker */
    settings(PCX_ORIGINAL, 1);
    CHECK(!run(0.2, 1, 0.0f, 0, 1.0f, &d) && !pc_crosshair_replaces_dot());
    pc_crosshair_hit(PCX_MARK_KILL);
    CHECK(pc_crosshair_frame(&d) && d.alpha == 0.0f && d.marker_alpha > 0.9f);
    settings(PCX_CIRCLE, 0);
    run(0.2, 1, 0.0f, 0, 1.0f, &d);
    pc_crosshair_hit(PCX_MARK_KILL);
    CHECK(pc_crosshair_frame(&d) && d.style == PCX_CIRCLE && d.marker_alpha == 0.0f);
    settings(PCX_ORIGINAL, 0);
    CHECK(!run(0.2, 1, 0.0f, 0, 1.0f, &d));

    pc_crosshair_test_clock(NULL);
    DeleteFileA(path);
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    else printf("crosshair: all checks passed\n");
    return failures ? 1 : 0;
}
