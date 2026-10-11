/* ui_gallery.c - SCENE_UI_GALLERY, a hidden debug scene that shows every
 * widget of the UI kit (owner: foundation). L / R flip pages; each page
 * is interactive. Reach it with the runner script command
 * `scene UI_GALLERY` (or from the stub title's scene launcher).
 */
#include "ui_internal.h"

enum { PG_WINDOWS, PG_METERS, PG_LIST, PG_PAINT, PG_FONT, PG_MODALS, PG_BACKDROPS,
       PG_COUNT };
static const char *const page_names[PG_COUNT] = {
    "Windows", "Text & meters", "List", "Paint picker", "Font & icons",
    "Modals", "Backdrops"
};

static int g_page, g_page_t, g_slider_sel, g_btn_sel, g_backdrop;
static uint8_t g_slider[3] = { 9, 4, 12 };
static uint8_t g_paint = PAINT_BUBBLEGUM;
static UiList g_list, g_modals;
static char g_name[DOOMO_NAME_LEN] = "Pudding";

static const char *const modal_items[] = {
    "Confirm", "Text entry", "Dialogue + choice", "Narrator",
    "Toasts", "Hell dialogue"
};

static const char *species_item(int i, char *buf)
{
    doomo_snprintf(buf, 48, "%s", doomo_species_names[i % SPECIES_COUNT]);
    return buf;
}

static int locked_species(int i)
{
    return !unlock_option_ok(CLIST_SPECIES, i);
}

static void gallery_enter(int arg)
{
    DoomoResident *r = town_res(0);
    (void)arg;
    g_page = 0;
    g_page_t = 0;
    memset(&g_list, 0, sizeof(g_list));
    g_list.item_fn = species_item;
    g_list.count = SPECIES_COUNT;
    g_list.visible = plat_screen_h() >= 200 ? 12 : 7;
    memset(&g_modals, 0, sizeof(g_modals));
    g_modals.items = modal_items;
    g_modals.count = DOOMO_COUNTOF(modal_items);
    g_modals.visible = DOOMO_COUNTOF(modal_items);
    /* a demo citizen to talk to */
    if (r && r->status == RES_EMPTY) {
        resident_generate(r, 7);
        memcpy(r->name, "Mochi", 6);
    }
}

/* ---- modal callbacks --------------------------------------------------- */
static void on_confirm(int yes)
{
    ui_toast(yes ? STR_ICON_HEART " Adopted! So fluffy." : "Maybe next time...");
}
static void on_name(int ok)
{
    char msg[48];
    if (!ok) { ui_toast("Never mind!"); return; }
    doomo_snprintf(msg, sizeof(msg), "Hello, %s!", g_name);
    ui_toast(msg);
}
static void on_choice(int c)
{
    static const char *const replies[] = {
        STR_ICON_HEART " Yay! Cupcake party!", "Aww, okay!", "Only a little cursed."
    };
    ui_toast(replies[DOOMO_CLAMP(c, 0, 2)]);
}
static void on_hell_done(void) { ui_dialog_style(WIN_CUTE); }

static void run_modal(int i)
{
    static const char *const opts[] = { "Yes please!", "Maybe later", "Is it cursed?" };
    switch (i) {
    case 0: ui_confirm("Adopt this fluffy little demon? It bites, but only cutely.",
                       on_confirm); break;
    case 1: ui_text_entry("Name your plush!", g_name, DOOMO_NAME_LEN, on_name); break;
    case 2:
        ui_say(0, "Hiii! I'm Mochi. Welcome to Hellvile Park, the fluffiest place in "
                  "all of HaHaland!");
        ui_say(0, "Over the wall is Hellvile. Nana says the folks there are grumpy, "
                  "but I think they just need hugs. And maybe cupcakes. Lots of "
                  "cupcakes. Like, a whole mountain of them!");
        ui_choice(0, "Wanna share a cupcake?", opts, 3, on_choice);
        break;
    case 3:
        ui_say(-1, "Meanwhile, deep beneath the wall, something purple stirred...");
        break;
    case 4:
        ui_toast(STR_ICON_STAR " NEW UNLOCK! Flame Hair");
        ui_toast(STR_ICON_COIN " +50 H$");
        break;
    default:
        ui_dialog_style(WIN_HELL);
        ui_say(0, "The vape cloud... it whispers my name... MOCHIIII...");
        ui_say_done(on_hell_done);
        break;
    }
}

static void gallery_update(void)
{
    g_page_t++;
    if (btn_pressed(BTN_R)) { g_page = (g_page + 1) % PG_COUNT; g_page_t = 0; audio_sfx(SFX_BLIP); return; }
    if (btn_pressed(BTN_L)) { g_page = (g_page + PG_COUNT - 1) % PG_COUNT; g_page_t = 0; audio_sfx(SFX_BLIP); return; }
    switch (g_page) {
    case PG_METERS:
        if (btn_repeat(BTN_UP)) g_slider_sel = (g_slider_sel + 2) % 3;
        if (btn_repeat(BTN_DOWN)) g_slider_sel = (g_slider_sel + 1) % 3;
        if (btn_repeat(BTN_LEFT) && g_slider[g_slider_sel] > 0) g_slider[g_slider_sel]--;
        if (btn_repeat(BTN_RIGHT) && g_slider[g_slider_sel] < 15) g_slider[g_slider_sel]++;
        if (btn_pressed(BTN_A)) g_btn_sel ^= 1;
        break;
    case PG_LIST:
        ui_list_locked_fn = locked_species;
        if (ui_list_update(&g_list) == UI_SELECT) ui_toast(doomo_species_names[g_list.sel]);
        ui_list_locked_fn = NULL;
        break;
    case PG_PAINT:
        if (ui_paint_picker_update(&g_paint, 8) == UI_SELECT)
            ui_toast(doomo_paint_names[g_paint]);
        break;
    case PG_MODALS:
        if (ui_list_update(&g_modals) == UI_SELECT) run_modal(g_modals.sel);
        break;
    case PG_BACKDROPS:
        if (btn_pressed(BTN_UP | BTN_DOWN | BTN_A)) g_backdrop = (g_backdrop + 1) % 6;
        break;
    default:
        break;
    }
    if (btn_pressed(BTN_B) && g_page != PG_LIST && g_page != PG_MODALS && g_page != PG_PAINT) {
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
    }
}

static const char *const style_names[6] = { "CUTE", "MINT", "CREAM", "HELL", "LCD", "DARK" };

static void draw_windows(int w, int h)
{
    int i, cols = 3, gw = (w - 16) / cols, gh = (h - 44) / 2;
    for (i = 0; i < 6; i++) {
        int x = 8 + (i % cols) * gw, y = 24 + (i / cols) * (gh + 4);
        if (ui_window_pop(x, y, gw - 6, gh - 2, i, g_page_t - i * 2)) {
            const UiStyle *st = ui_style(i);
            ui_text(x + 7, y + 6, style_names[i], st->text);
            ui_text(x + 7, y + 16, "Hi!" STR_ICON_HEART, st->text_dim);
            ui_heart(x + gw - 18, y + gh - 14, 4, st->accent);
        }
    }
}

static void draw_meters(int w, int h)
{
    const UiStyle *st;
    int x = 8, y = 22, ww = w - 16, hh = h - 38, by;
    static const char *const labels[3] = { "Eye size", "Ear fluff", "Tail wag" };
    int i;
    ui_window(x, y, ww, hh, WIN_CREAM);
    st = ui_style(WIN_CREAM);
    ui_text_wrap(x + 8, y + 7, ww - 16,
                 "Mochi Pudding is a Sweet Sunbeam who loves Cupcakes and is "
                 "scared of Thunder.", st->text, 1);
    by = y + 7 + 2 * FONT_LINE + 6;
    plat_text(x + 8, by, "\x92", PAL_PAINT(PAINT_TANGERINE, 1));
    ui_bar(x + 18, by, ww / 2 - 26, 7, 72, 100, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    plat_text(x + ww / 2, by, "\x80", PAL_PAINT(PAINT_ROSE, 1));
    ui_bar(x + ww / 2 + 10, by, ww / 2 - 18, 7, 90, 100, PAL_UI_GOLD);
    plat_text(x + 8, by + 11, "\x91", PAL_PAINT(PAINT_LEMON, 0));
    ui_bar(x + 18, by + 11, ww / 2 - 26, 7, 18, 100, PAL_PAINT(PAINT_MINT, 1));
    plat_text(x + ww / 2, by + 11, "\x81", PAL_PAINT(PAINT_GRAPE, 1));
    ui_bar(x + ww / 2 + 10, by + 11, ww / 2 - 18, 7, 40, 100, PAL_PAINT(PAINT_VAPE_PURPLE, 1));
    for (i = 0; i < 3; i++)
        ui_slider(x + 8, by + 25 + i * 11, ww - 16, labels[i], g_slider[i], 15, i == g_slider_sel);
    ui_button(x + 24, y + hh - 19, 56, "Yes!", g_btn_sel == 0, WIN_CREAM);
    ui_button(x + 100, y + hh - 19, 56, "Maybe", g_btn_sel == 1, WIN_CREAM);
}

static void draw_list(int w, int h)
{
    int lw = DOOMO_MIN(w - 16, 140), lh = g_list.visible * UI_LIST_ROW_H + 10;
    int x = 8, y = 22;
    (void)h;
    if (ui_window_pop(x, y, lw, lh, WIN_CUTE, g_page_t)) {
        ui_list_locked_fn = locked_species;
        ui_list_draw(&g_list, x + 4, y + 5, lw - 8, WIN_CUTE);
        ui_list_locked_fn = NULL;
    }
    {
        int px = x + lw + 8, pw = w - px - 8;
        if (pw > 40 && ui_window_pop(px, y, pw, 70, WIN_MINT, g_page_t - 3)) {
            const UiStyle *st = ui_style(WIN_MINT);
            const DoomoUnlock *u = unlock_find(UL_CAT_PART, CLIST_SPECIES, g_list.sel);
            ui_text(px + 6, y + 6, doomo_species_names[g_list.sel], st->text);
            ui_text_wrap(px + 6, y + 18, pw - 12,
                         u && !unlock_has(u->id) ? u->how : "Unlocked!", st->text_dim, 1);
        }
    }
}

static void draw_paint(int w, int h)
{
    int gw = 8 * 12, x = (w - gw - 20) / 2, y = 26;
    (void)h;
    if (ui_window_pop(x, y, gw + 20, 4 * 11 + 34, WIN_CUTE, g_page_t)) {
        const UiStyle *st = ui_style(WIN_CUTE);
        const DoomoUnlock *u = unlock_find(UL_CAT_PAINT, 0, g_paint);
        ui_paint_picker_draw(x + 11, y + 9, g_paint, 8);
        ui_text_center(w / 2, y + 4 * 11 + 16,
                       u && !unlock_has(u->id) ? u->how : doomo_paint_names[g_paint],
                       st->text);
    }
}

static void draw_font(int w, int h)
{
    const UiStyle *st;
    char line[24];
    int x = 8, y = 22, c, row = 0;
    ui_window(x, y, w - 16, h - 38, WIN_CREAM);
    st = ui_style(WIN_CREAM);
    for (c = 0x20; c <= 0x9F; c += 16) {
        int k;
        for (k = 0; k < 16; k++) {
            int ch = c + k;
            line[k] = (char)(ch == 0x7F ? ' ' : ch);
        }
        line[16] = 0;
        ui_text(x + 8, y + 6 + row * FONT_LINE, line, row >= 6 ? st->accent : st->text);
        row++;
    }
    ui_text(x + 8, y + 6 + row * FONT_LINE + 2, "The quick brown fox jumps", st->text);
    ui_text(x + 8, y + 6 + (row + 1) * FONT_LINE + 2, "over the lazy dog. gjpqy!", st->text_dim);
}

static void draw_modals(int w, int h)
{
    int lw = 132, lh = g_modals.visible * UI_LIST_ROW_H + 10, x = (w - lw) / 2, y = 30;
    (void)h;
    if (ui_window_pop(x, y, lw, lh, WIN_MINT, g_page_t))
        ui_list_draw(&g_modals, x + 4, y + 5, lw - 8, WIN_MINT);
}

static void gallery_draw(void)
{
    int w = plat_screen_w(), h = plat_screen_h();
    char title[40];
    ui_backdrop(g_page == PG_BACKDROPS ? g_backdrop : WIN_CUTE, doomo_frame_count());
    switch (g_page) {
    case PG_WINDOWS: draw_windows(w, h); break;
    case PG_METERS: draw_meters(w, h); break;
    case PG_LIST: draw_list(w, h); break;
    case PG_PAINT: draw_paint(w, h); break;
    case PG_FONT: draw_font(w, h); break;
    case PG_MODALS: draw_modals(w, h); break;
    default: {
        int i;
        for (i = 0; i < 5; i++) ui_cursor_glyph(w / 2 - 40 + i * 16, h / 2, i, PAL_PAINT(PAINT_ROSE, 1));
        ui_star(w / 2 - 30, h / 2 + 26, 8, PAL_UI_GOLD);
        ui_heart(w / 2, h / 2 + 26, 9, PAL_RAMP(RAMP_PASTEL_PINK, 4));
        ui_heart(w / 2 + 30, h / 2 + 26, 6, PAL_PAINT(PAINT_MINT, 1));
        ui_text_big(w / 2 - ui_text_big_width(style_names[g_backdrop], 2) / 2, h / 2 - 30,
                    style_names[g_backdrop], 2, PAL_WHITE, PAL_UI_PINK_D);
        break; }
    }
    doomo_snprintf(title, sizeof(title), "UI %d/%d: %s", g_page + 1, PG_COUNT, page_names[g_page]);
    ui_title(w / 2, 4, title, WIN_CUTE);
    ui_hint("L/R page  " STR_ICON_A " try  " STR_ICON_B " back", WIN_CUTE);
}

const DoomoScene SCENE_UI_GALLERY = { "UI_GALLERY", gallery_enter, NULL, gallery_update,
                                      gallery_draw, 0 };
