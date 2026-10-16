/* Split-screen: the views' layout, and player two's camera starting on player one's. */
#include "pc_split.h"
#include "pc_video.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

/* A camera looking along yaw / pitch from `pos`; `hand` -1 mirrors right as the game's matrices do. */
static void pose(float m[4][3], double yaw, double pitch, double hand, const float pos[3])
{
    const double f[3] = { cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw) };
    double r[3] = { f[2], 0, -f[0] }, u[3], l;
    int k;
    l = sqrt(r[0] * r[0] + r[2] * r[2]);
    r[0] *= hand / l; r[2] *= hand / l;
    u[0] = f[1] * r[2] - f[2] * r[1]; u[1] = f[2] * r[0] - f[0] * r[2]; u[2] = f[0] * r[1] - f[1] * r[0];
    for (k = 0; k < 3; k++) { m[0][k] = (float)r[k]; m[1][k] = (float)u[k]; m[2][k] = (float)f[k]; m[3][k] = pos[k]; }
}

static double diff(float a[4][3], float b[4][3])
{
    double d = 0;
    int i, k;
    for (i = 0; i < 4; i++) for (k = 0; k < 3; k++) d = fmax(d, fabs((double)a[i][k] - (double)b[i][k]));
    return d;
}

int main(void)
{
    static const float pos[3] = { 12.5f, -3.0f, 140.25f };
    float p1[4][3], m[4][3];
    char path[MAX_PATH];
    int r[4];

    pc_split_rect_for(1, 0, r);
    CHECK(r[0] == 0 && r[1] == 0 && r[2] == 640 && r[3] == 240);
    pc_split_rect_for(1, 1, r);
    CHECK(r[0] == 0 && r[1] == 240 && r[2] == 640 && r[3] == 480);
    pc_split_rect_for(0, 0, r);
    CHECK(r[0] == 0 && r[1] == 0 && r[2] == 640 && r[3] == 480);

    /* RECOMP_SPLITSCREEN pins the setting */
    GetTempPathA(sizeof(path), path);
    strcat_s(path, sizeof(path), "black-split-test.ini");
    DeleteFileA(path);
    _putenv_s("RECOMP_SPLITSCREEN", "1");
    pc_video_test_reset(path);
    CHECK(pc_split_enabled());

    /* the first call takes player one's camera as it is (no second controller moves it here) */
    pose(p1, 0.7, -0.2, -1.0, pos);
    pc_split_camera(m, p1);
    CHECK(diff(m, p1) < 1e-4);
    pose(p1, 2.0, 0.4, -1.0, pos);
    pc_split_camera(m, p1);                                /* it keeps its own camera afterwards */
    CHECK(diff(m, p1) > 0.1);

    pc_split_note_frame(1);
    CHECK(pc_split_frame());
    pc_split_note_frame(0);
    CHECK(!pc_split_frame());

    if (failures) return 1;
    printf("split: ok\n");
    return 0;
}
