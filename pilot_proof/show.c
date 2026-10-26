#include "show.h"
#include "tk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void line_at(const char *id, double t) {
    const char *p = getenv("CUES_OUT");
    if (!p) return;
    FILE *f = fopen(p, "a");
    if (!f) return;
    fprintf(f, "%s %.3f\n", id, t);
    fclose(f);
}

#define MAXENV 24
static struct { char id[48]; int n; float *v; } envs[MAXENV];
static int nenv;
static int env_load(const char *id) {
    for (int i = 0; i < nenv; i++) if (!strcmp(envs[i].id, id)) return i;
    if (nenv >= MAXENV) return -1;
    int i = nenv++;
    snprintf(envs[i].id, sizeof envs[i].id, "%s", id);
    envs[i].n = -1; envs[i].v = NULL;
    const char *d = getenv("ENV_DIR");
    if (!d) return i;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.env", d, id);
    FILE *f = fopen(path, "r");
    if (!f) return i;
    int cap = 1024; envs[i].v = malloc(cap * sizeof(float)); envs[i].n = 0;
    float x;
    while (fscanf(f, "%f", &x) == 1) {
        if (envs[i].n == cap) { cap *= 2; envs[i].v = realloc(envs[i].v, cap * sizeof(float)); }
        envs[i].v[envs[i].n++] = x;
    }
    fclose(f);
    return i;
}
float lip(const char *id, int fr, int fallback) {
    if (fr < 0) return 0;
    int i = env_load(id);
    if (i >= 0 && envs[i].n >= 0) {
        if (fr >= envs[i].n) return 0;
        float a = envs[i].v[fr], b = fr + 1 < envs[i].n ? envs[i].v[fr + 1] : 0;
        return 0.6f * a + 0.4f * b;   /* tiny look-ahead: the beak opens on the consonant */
    }
    if (fr >= fallback) return 0;
    return 0.55f * fabsf(sinf(fr * 0.55f)) * (0.7f + 0.3f * sinf(fr * 0.17f));
}

int bubble_side;   /* 0 auto, 1 right, -1 left, 2 below */
void bubble(const char *text, int chars, int sx, int sy) {
    int len = (int)strlen(text), w = len * 6 + 12;
    if (w > 118) w = 118;
    int lines = (len * 6 + (w - 12) - 1) / (w - 12) + (len * 6 > (w - 12) ? 1 : 0), h = lines * 10 + 8;
    if (lines == 1) h = 18;
    /* beside the speaker's head: right if it fits, else left; never over the face */
    int gap = 18, x, y = sy - h / 2 - 12, side;
    if (bubble_side == 2) { x = sx - w / 2; side = 0; if (x < 3) x = 3; if (x + w > TW - 3) x = TW - 3 - w; }
    else if (sx + gap + w <= TW - 3 && bubble_side != -1) { x = sx + gap; side = 1; }
    else if (sx - gap - w >= 3) { x = sx - gap - w; side = -1; }
    else { x = sx - w / 2; side = 0; if (x < 3) x = 3; if (x + w > TW - 3) x = TW - 3 - w; }
    if (side == 0) y = sy + 26;
    if (y < 3) y = 3;
    if (y + h > TH - 3) y = TH - 3 - h;
    uint8_t ol = PAL_PAINT(PAINT_JET, 1);
    if (side) {
        int ex = side > 0 ? x + 1 : x + w - 1, ty = y + h / 2 + 2;
        tk_tri(ex, ty - 5, ex, ty + 3, sx + side * 6, sy - 2, ol);
        tk_tri(ex + side, ty - 3, ex + side, ty + 1, sx + side * 8, sy - 2, PAL_WHITE);
    } else {
        tk_tri(sx - 5, y + 2, sx + 5, y + 2, sx, sy + 8, ol);
        tk_tri(sx - 3, y + 3, sx + 3, y + 3, sx, sy + 11, PAL_WHITE);
    }
    tk_rect(x + 2, y, w - 4, h, ol); tk_rect(x, y + 2, w, h - 4, ol);
    tk_rect(x + 2, y + 1, w - 4, h - 2, PAL_WHITE); tk_rect(x + 1, y + 2, w - 2, h - 4, PAL_WHITE);
    tk_rect(x + 3, y + h - 3, w - 6, 1, PAL_PAINT(PAINT_SKY, 2));
    tk_text_wrap(x + 6, y + 5, w - 12, text, PAL_PAINT(PAINT_NAVY, 0), chars);
}
