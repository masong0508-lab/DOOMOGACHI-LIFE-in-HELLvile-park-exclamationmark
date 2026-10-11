/* boot.c - SCENE_BOOT: "a HaHaland production" (owner: foundation).
 *
 * A plush heart with a face drops in, squashes, bounces, settles and
 * breathes; sparkles orbit it, the studio name pops in letter by letter,
 * and a giant heart iris wipes to SCENE_TITLE. A / START skip ahead.
 */
#include "ui_internal.h"

#define T_LAND   24
#define T_TEXT   30
#define T_WIPE   104
#define T_END    124

static void heart_face(int cx, int cy, int s, int t)
{
    int ex = s * 3 / 10 + 1, ey = cy - s / 6;
    int blink = (t % 75) < 4 || (t % 75 >= 8 && t % 75 < 10 && (t / 75) % 3 == 1);
    uint8_t ink = PAL_RAMP(RAMP_PASTEL_PINK, 0);
    int ew = DOOMO_MAX(s / 8, 2), eh = DOOMO_MAX(s / 5, 3);
    int i;
    if (blink) {
        for (i = -ew; i <= ew; i++) {
            plat_rect(cx - ex + i, ey + (i == -ew || i == ew), 1, 1, ink);
            plat_rect(cx + ex + i, ey + (i == -ew || i == ew), 1, 1, ink);
        }
    } else {
        ui_fill_ellipse(cx - ex, ey, ew, eh, ink);
        ui_fill_ellipse(cx + ex, ey, ew, eh, ink);
        /* two glossy glints per eye */
        plat_rect(cx - ex - ew / 2 - 1, ey - eh / 2 - 1, DOOMO_MAX(ew / 2 + 1, 2), 2, PAL_WHITE);
        plat_rect(cx + ex - ew / 2 - 1, ey - eh / 2 - 1, DOOMO_MAX(ew / 2 + 1, 2), 2, PAL_WHITE);
        plat_rect(cx - ex + ew / 2, ey + eh / 2, 1, 1, PAL_WHITE);
        plat_rect(cx + ex + ew / 2, ey + eh / 2, 1, 1, PAL_WHITE);
        /* a hint of colour in the iris */
        plat_rect(cx - ex - ew / 2, ey + eh / 2 - 1, ew, 1, PAL_PAINT(PAINT_GRAPE, 1));
        plat_rect(cx + ex - ew / 2, ey + eh / 2 - 1, ew, 1, PAL_PAINT(PAINT_GRAPE, 1));
    }
    /* blush */
    ui_fill_ellipse(cx - ex - ew - 2, ey + eh + 2, DOOMO_MAX(s / 7, 2), 1,
                    PAL_RAMP(RAMP_PASTEL_PINK, 6));
    ui_fill_ellipse(cx + ex + ew + 2, ey + eh + 2, DOOMO_MAX(s / 7, 2), 1,
                    PAL_RAMP(RAMP_PASTEL_PINK, 6));
    /* little cat mouth "w" */
    {
        int my = ey + eh + 1;
        plat_rect(cx - 3, my, 1, 1, ink);
        plat_rect(cx - 2, my + 1, 2, 1, ink);
        plat_rect(cx, my, 1, 1, ink);
        plat_rect(cx + 1, my + 1, 2, 1, ink);
        plat_rect(cx + 3, my, 1, 1, ink);
    }
}

static void boot_enter(int arg)
{
    (void)arg;
    audio_music(SONG_NONE);
}

static void boot_update(void)
{
    uint32_t t = scene_ticks();
    if (t == T_LAND) audio_sfx(SFX_BELL);
    if (t == T_TEXT + 2) audio_sfx(SFX_SPARKLE);
    if (t == T_WIPE) audio_sfx(SFX_HAPPY);
    if (t >= T_END) {
        scene_set(&SCENE_TITLE, 0);
        return;
    }
    if (t < T_WIPE && t > 4 && btn_pressed(BTN_A | BTN_START | BTN_B)) {
        /* skip: straight to the wipe */
        scene_set(&SCENE_TITLE, 0);
    }
}

static void boot_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int t = (int)scene_ticks();
    int big = sh >= 200;
    int s = big ? 32 : 24;                    /* heart half-size */
    int cx = sw / 2, rest_y = sh * 2 / 5 - (big ? 6 : 4);
    int hy, hsx = s, hsy = s, i;

    ui_backdrop(WIN_CREAM, (uint32_t)t);

    /* ---- heart motion: fall, squash, bounce, settle, breathe ---------- */
    if (t < T_LAND) {
        int f = T_LAND - t;
        hy = rest_y - f * f * (rest_y + s) / (T_LAND * T_LAND);
        hsy = s + (t > T_LAND - 8 ? 2 : 0);            /* stretch as it falls */
        hsx = s - (t > T_LAND - 8 ? 1 : 0);
    } else {
        int u = t - T_LAND;
        static const int8_t squash[] = { 5, 4, 2, -1, -3, -2, -1, 0, 1, 1, 0 };
        static const int8_t hop[] = { 0, 0, -3, -7, -9, -8, -5, -1, 0, 0, 0 };
        if (u < (int)sizeof(squash)) {
            hsx = s + squash[u] * s / 16;
            hsy = s - squash[u] * s / 16;
            hy = rest_y + hop[u] + (squash[u] > 0 ? squash[u] * s / 20 : 0);
        } else {
            int br = ui_wave(u, 50, 2);
            hsx = s + (br > 0);
            hsy = s;
            hy = rest_y + ui_wave(u, 50, 1);
        }
    }
    /* ground shadow */
    {
        int dist = rest_y - hy;
        int rx = s - DOOMO_CLAMP(dist / 3, 0, s - 4), y0 = rest_y + s + 3;
        int k;
        for (k = -2; k <= 2; k++) {
            int half = rx * (6 - (k < 0 ? -k : k)) / 6;
            plat_shade_rect(cx - half, y0 + k, half * 2 + 1, 1, 11);
        }
    }
    /* orbiting sparkles once landed */
    if (t > T_LAND + 4) {
        for (i = 0; i < 6; i++) {
            int a = (t - T_LAND) * 24 + i * (DOOMO_ANG_FULL / 6);
            int rr = s + 12 + ui_wave(t + i * 9, 40, 2);
            int px = cx + ((fx_cos(a) * rr) >> 16);
            int py = hy + ((fx_sin(a) * (rr * 2 / 3)) >> 16);
            ui_sparkle(px, py, (t / 2 + i * 5) & 15, i & 1 ? PAL_UI_GOLD : PAL_WHITE);
        }
    }
    /* the heart itself, with squash & stretch */
    ui_heart_squash(cx, hy, s, hsx * 256 / s, hsy * 256 / s, PAL_RAMP(RAMP_PASTEL_PINK, 4));
    heart_face(cx, hy, s, t);

    /* ---- studio name -------------------------------------------------- */
    if (t >= T_TEXT) {
        static const char *const word = "HaHaland";
        int scale = big ? 3 : 2;
        int ww = ui_text_big_width(word, scale);
        int small = font_text_width("a ");
        int tx = cx - (ww + small) / 2 + small, ty = rest_y + s + (big ? 16 : 10);
        int n = (t - T_TEXT) / 2 + 1, k;
        ui_text_outline(tx - small, ty + 8 * scale - 9, "a", PAL_RAMP(RAMP_PASTEL_PINK, 1), PAL_WHITE);
        for (k = 0; k < 8 && k < n; k++) {
            char ch[2];
            int age = t - T_TEXT - k * 2;
            static const int8_t pop[] = { 6, 2, -3, -4, -2, 0, 1, 1, 0 };
            int dy = age < (int)sizeof(pop) ? pop[age] : (ui_wave(t + k * 6, 48, 1));
            ch[0] = word[k];
            ch[1] = 0;
            ui_text_big(tx + k * FONT_ADVANCE * scale, ty + dy, ch, scale,
                        k % 2 ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_ROSE, 1),
                        PAL_RAMP(RAMP_PASTEL_PINK, 1));
        }
        if (t >= T_TEXT + 18) {
            const char *p = "~ production ~";
            int py = ty + 9 * scale + 4;
            int age = t - T_TEXT - 18;
            if (age < 6) py += 6 - age;
            ui_text_outline(cx - font_text_width(p) / 2, py, p, PAL_RAMP(RAMP_PASTEL_PINK, 1),
                            PAL_WHITE);
        }
    }

    /* ---- heart iris wipe to the title ---------------------------------- */
    if (t >= T_WIPE) {
        int u = t - T_WIPE;
        int sz = 4 + u * u * (sw + sh) / 260;
        ui_heart_flat(cx, hy, sz + 3, PAL_RAMP(RAMP_PASTEL_PINK, 3));
        ui_heart_flat(cx, hy, sz, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    }
}

const DoomoScene SCENE_BOOT = { "BOOT", boot_enter, NULL, boot_update, boot_draw, 0 };
