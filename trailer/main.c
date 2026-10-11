/* main.c - render the trailer.
 *   trailer --list
 *   trailer --png SCENE FRAME out.png         one frame (4x scaled)
 *   trailer --sheet SCENE out.png [STEP]      contact sheet of a scene
 *   trailer --raw > frames.rgb               all frames, 240x160 rgb24 @30fps
 *   trailer --wav out.wav                     the soundtrack
 */
#include "tk.h"
#include "timeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tk_audio_render(double total, const char *path);
int tk_write_png_rgb(const char *path, const uint8_t *rgb, int w, int h);
void tk_fb_to_rgb(uint8_t *out);

static int frames_of(int i) { return (int)(TK_SCENES[i].seconds * TK_FPS + 0.5); }
static int find(const char *name) {
    for (int i = 0; i < TK_SCENE_COUNT; i++)
        if (!strcmp(TK_SCENES[i].name, name) || strstr(TK_SCENES[i].name, name)) return i;
    fprintf(stderr, "unknown scene %s\n", name);
    exit(1);
}
static void render(int i, int f) {
    tk_clear(PAL_BLACK);
    TK_SCENES[i].draw(f, frames_of(i));
}

int main(int argc, char **argv) {
    static uint8_t rgb[TW * TH * 3];
    if (argc >= 2 && !strcmp(argv[1], "--list")) {
        double t = 0;
        for (int i = 0; i < TK_SCENE_COUNT; i++) {
            printf("%-16s start %6.2fs  %5.2fs  %4d frames\n", TK_SCENES[i].name, t, TK_SCENES[i].seconds, frames_of(i));
            t += TK_SCENES[i].seconds;
        }
        printf("total %.2fs\n", t);
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[1], "--png")) {
        int i = find(argv[2]), f = atoi(argv[3]);
        render(i, f);
        tk_fb_to_rgb(rgb);
        static uint8_t big[TW * 4 * TH * 4 * 3];
        for (int y = 0; y < TH * 4; y++)
            for (int x = 0; x < TW * 4; x++)
                memcpy(big + (y * TW * 4 + x) * 3, rgb + ((y / 4) * TW + x / 4) * 3, 3);
        return !tk_write_png_rgb(argv[4], big, TW * 4, TH * 4);
    }
    if (argc >= 4 && !strcmp(argv[1], "--sheet")) {
        int i = find(argv[2]);
        int n = frames_of(i);
        int step = argc >= 5 ? atoi(argv[4]) : (n + 15) / 16;
        if (step < 1) step = 1;
        int count = (n + step - 1) / step, cols = 4, rows = (count + cols - 1) / cols;
        int W = cols * (TW * 2 + 4), H = rows * (TH * 2 + 4);
        uint8_t *sheet = calloc((size_t)W * H * 3, 1);
        for (int k = 0; k < count; k++) {
            render(i, k * step);
            tk_text(2, 2, "", PAL_WHITE, 1);
            char lbl[16]; snprintf(lbl, sizeof lbl, "%d", k * step);
            tk_text_outline(2, 2, lbl, PAL_WHITE, PAL_BLACK, 1);
            tk_fb_to_rgb(rgb);
            int ox = (k % cols) * (TW * 2 + 4), oy = (k / cols) * (TH * 2 + 4);
            for (int y = 0; y < TH * 2; y++)
                for (int x = 0; x < TW * 2; x++)
                    memcpy(sheet + ((oy + y) * W + ox + x) * 3, rgb + ((y / 2) * TW + x / 2) * 3, 3);
        }
        int ok = tk_write_png_rgb(argv[3], sheet, W, H);
        free(sheet);
        return !ok;
    }
    if (argc >= 2 && !strcmp(argv[1], "--raw")) {
        for (int i = 0; i < TK_SCENE_COUNT; i++)
            for (int f = 0; f < frames_of(i); f++) {
                render(i, f);
                tk_fb_to_rgb(rgb);
                fwrite(rgb, 1, sizeof rgb, stdout);
            }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "--wav")) {
        tk_music();
        double t = 0;
        for (int i = 0; i < TK_SCENE_COUNT; i++) {
            TK_SCENES[i].audio(t, TK_SCENES[i].seconds);
            t += TK_SCENES[i].seconds;
        }
        return !tk_audio_render(t + 0.5, argv[2]);
    }
    fprintf(stderr, "usage: see main.c\n");
    return 2;
}
