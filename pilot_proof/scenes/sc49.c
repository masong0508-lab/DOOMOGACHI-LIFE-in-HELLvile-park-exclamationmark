/* sc49.c - SC49 (4-01) ACT CARD / BANDSTAND. 23:00 ________ (blank). The
 * freeze holds. Nana: "Cl..." Her eyes to the Mayor; the Mayor to the sky;
 * Sunny opens their mouth: nothing. The pet looks into Sunny's face. */
#include "act4_common.h"
#define SHOTS(X) X(main, 20.0)
#include "scene_tl.h"
static const float NA = 4.4f, MAY = 6.2f, SUN = 7.6f, PET = 9.6f, SHUT = 11.2f, LEAN = 13.4f;
static const Cut C[] = { { 0, 0, 0, 1, 1, 0, 1 }, { 3.0f, 0, -14, 1.6f, 1.75f, 0, 0 }, { NA - 0.4f, 34, -6, 4.0f, 4.4f, 0, 0 }, { MAY, 62, -6, 4.0f, 4.3f, 0, 0 },
    { SUN, 50, 14, 3.6f, 4.0f, 0, 2 }, { LEAN, 6, -18, 5.0f, 5.4f, 0, 0 }, { 16.0f, 0, -14, 1.6f, 1.7f, 0, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 7, ta, 20);
    if (k == 1) { tk_clear(PAL_PAINT(PAINT_CARAMEL, 1)); tk_rect(40, 6, 160, 154, PAL_UI_CREAM); tk_frame(40, 6, 160, 154, PAL_PAINT(PAINT_CHOCOLATE, 1));
        static const char *L[6] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 weather", "23:00 ________" };
        for (int i = 0; i < 6; i++) { tk_text(56, 22 + i * 22, L[i], PAL_PAINT(PAINT_NAVY, 1), 1); if (i < 3) tk_text(44, 22 + i * 22, "v", PAL_PAINT(PAINT_CHERRY, 1), 1); }
        tk_text(180, 150, "ereh", PAL_PAINT(PAINT_BONE, 1), 1);   /* back to front, too faint to read */
        return; }
    if (k == 2) {   /* Sunny opens their mouth: nothing */
        town(ta, 0, -1, 1);
        Sunny u; sunny_default(&u, 50, 20, 1.0f, 0); u.suitcase = 0; u.look_x = -1; sunny_draw(&u);
        int open = ta >= SUN && ta < SHUT; if (open) wd_blob(50.8f, 20 - 23 + 3.4f, 1.6f, 1.4f, 0, PAL_PAINT(PAINT_CHERRY, 0), 0);
        float pl = ta >= PET ? 1 : 0; pet_draw(50 + pl * 3, 20 - 23 - 4 + pl * 3, 0.9f, ta * 30, -1, 0);
        return; }
    bead_flame = 0; town(ta, 0, -1, 1);
    Sunny u; sunny_default(&u, 50, 20, 0.85f, 0); u.suitcase = 0; sunny_draw(&u); pet_draw(50, 20 - 23 * 0.85f - 4, 0.75f, 0, 0, 0);
    if (ta >= NA && ta < NA + 1.2f) { bubble_side = 1; bubble("Cl...", 3, (int)wd_sx(nana_mouth_x), (int)wd_sy(nana_mouth_y)); bubble_side = 0; }
    if (ta >= LEAN) { float lean = seg(ta, LEAN, LEAN + 0.5f); wd_blob(6 + lean * 1.4f, -18.6f, 0.8f, 0.6f, lean * 0.5f, PAL_PAINT(PAINT_SKY, 1), WB_OUT); }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) { tk_note(0, 0.4, 83, TKV_SINE, 0.25f); tk_note(NA, 0.2, 60, TKV_TRI, 0.12f); }
