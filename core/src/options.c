/* options.c - the data-driven settings list (owner: foundation).
 * Option values live in doomo_town.opt[] (saved with the island). The
 * options screen (freeplay) is generated from doomo_option_info[]. */
#include "doomo/town.h"
#include "doomo/ui.h"
#include "doomo/font.h"
#include "doomo/lang.h"

const char *const doomo_option_page_names[OPTPAGE_COUNT] = {
    "Game", "Controls", "Shooter", "Life Sim", "Audio", "Display",
    "Performance", "Accessibility", "Cheats"
};

static const char *const v_off_on[] = { "Off", "On" };
static const char *const v_gore[] = { "Cute", "Brutal", "Per weapon", "Cute + Gore" };
static const char *const v_monster[] = { "Off", "ON (monster!)" };
static const char *const v_diff[] = { "Plushie", "Normal", "Nightmare",
                                      "Ultra-Hellvile" };
static const char *const v_text_speed[] = { "Slow", "Normal", "Fast", "Instant" };
static const char *const v_sim_speed[] = { "Chill", "Normal", "Fast", "Turbo" };
static const char *const v_day_len[] = { "Real time", "2 hours", "1 hour",
                                         "30 minutes", "15 minutes" };
static const char *const v_autosave[] = { "Off", "Every day", "Every hour" };
static const char *const v_camera[] = { "First person", "Third person" };
static const char *const v_scheme[] = { "Classic", "Modern", "Dual stick" };
static const char *const v_level4[] = { "Off", "Low", "Medium", "High" };
static const char *const v_lock[] = { "Hold", "Toggle" };
static const char *const v_cross[] = { "Dot", "Cross", "Circle", "Heart",
                                       "Star", "Skull", "Paw", "None" };
static const char *const v_cross_col[] = { "White", "Pink", "Mint", "Gold",
                                           "Red", "Cyan" };
static const char *const v_fov[] = { "Narrow", "Normal", "Wide" };
static const char *const v_level3[] = { "Off", "Low", "Full" };
static const char *const v_hud[] = { "Cute", "Classic", "Minimal", "Hidden" };
static const char *const v_shoulder[] = { "Right", "Left" };
static const char *const v_blood[] = { "Red", "Green", "Rainbow", "Confetti" };
static const char *const v_confetti[] = { "Some", "Lots", "Absurd" };
static const char *const v_density[] = { "Sparse", "Normal", "Swarm" };
static const char *const v_possess[] = { "Off", "Rare", "Normal", "Frequent" };
static const char *const v_events[] = { "Calm", "Normal", "Chaotic" };
static const char *const v_weather[] = { "Off", "Normal", "Lots" };
static const char *const v_lcd[] = { "Off", "Pet screen", "Everything" };
static const char *const v_skin[WSKIN_COUNT] = {
    "Strawberry", "Mint Choc", "Vanilla", "Lavender", "Sky", "Peach",
    "Lemon", "Bubblegum", "Hellfire", "Midnight", "Tamagotchi", "Goth"
};
static const char *const v_font[] = { "Normal", "Bold", "Shadowed" };
static const char *const v_colorblind[] = { "Off", "Protan", "Deutan", "Tritan" };
static const char *const v_clock[] = { "12-hour", "24-hour" };
static const char *const v_title[] = { "Pastel Dawn", "Starry Night",
                                       "Candy Storm", "Hellvile Sunset",
                                       "Vape Apocalypse", "United Park" };
static const char *const v_cursor[] = { "Heart", "Paw", "Star", "Sparkle",
                                        "Skull", "Fang" };
static const char *const v_filter[] = { "None", "Storybook", "VHS",
                                        "Game Pal", "Sepia", "Nightvision" };
static const char *const v_bounce[] = { "Off", "Subtle", "Boingy" };
static const char *const v_turn[] = { "R1 right", "R1 left" };
static const char *const v_fx[] = { "Low", "Normal" };
static const char *const v_dist[] = { "Near", "Normal" };
static const char *const v_feel[] = { "Classic", "Punchy", "Brutal" };
/* each language in its own language, never translated (not in es.tsv) */
static const char *const v_lang[] = { "English", "Espa" STR_ES_n "ol" };

#define OPT(n, vals, d, pg) { n, (uint8_t)DOOMO_COUNTOF(vals), d, vals, pg }
#define OPTN(n, cnt, d, pg) { n, cnt, d, NULL, pg }

const DoomoOptionInfo doomo_option_info[OPT_COUNT] = {
    /* OPT_GORE */            OPT("Gore", v_gore, GORE_CUTE, OPTPAGE_GAME),
    /* OPT_MONSTER_MODE */    OPT("Monster Mode", v_monster, 0, OPTPAGE_GAME),
    /* OPT_DIFFICULTY */      OPT("Difficulty", v_diff, DIFF_NORMAL, OPTPAGE_GAME),
    /* OPT_MUSIC_VOL */       OPTN("Music volume", 11, 7, OPTPAGE_AUDIO),
    /* OPT_SFX_VOL */         OPTN("Sound volume", 11, 8, OPTPAGE_AUDIO),
    /* OPT_VOICE_VOL */       OPTN("Voice volume", 11, 8, OPTPAGE_AUDIO),
    /* OPT_VOICE_BABBLE */    OPT("Voice babble", v_off_on, 1, OPTPAGE_AUDIO),
    /* OPT_TEXT_SPEED */      OPT("Text speed", v_text_speed, 1, OPTPAGE_GAME),
    /* OPT_SIM_SPEED */       OPT("Sim speed", v_sim_speed, 1, OPTPAGE_SIM),
    /* OPT_DAY_LENGTH */      OPT("Day length", v_day_len, 2, OPTPAGE_SIM),
    /* OPT_AUTOSAVE */        OPT("Autosave", v_autosave, 1, OPTPAGE_GAME),
    /* OPT_CAMERA_DEFAULT */  OPT("Camera", v_camera, 0, OPTPAGE_SHOOTER),
    /* OPT_CONTROL_SCHEME */  OPT("Controls", v_scheme, CONTROLS_MODERN, OPTPAGE_CONTROLS),
    /* OPT_INVERT_LOOK */     OPT("Invert look", v_off_on, 0, OPTPAGE_CONTROLS),
    /* OPT_LOOK_SPEED */      OPTN("Look speed", 10, 5, OPTPAGE_CONTROLS),
    /* OPT_AIM_ASSIST */      OPT("Aim assist", v_level4, 2, OPTPAGE_CONTROLS),
    /* OPT_LOCK_ON */         OPT("Lock-on", v_lock, 1, OPTPAGE_CONTROLS),
    /* OPT_CROSSHAIR */       OPT("Crosshair", v_cross, 0, OPTPAGE_SHOOTER),
    /* OPT_CROSSHAIR_COLOR */ OPT("Crosshair colour", v_cross_col, 1, OPTPAGE_SHOOTER),
    /* OPT_FOV */             OPT("Field of view", v_fov, 1, OPTPAGE_SHOOTER),
    /* OPT_HEAD_BOB */        OPT("Head bob", v_level3, 2, OPTPAGE_SHOOTER),
    /* OPT_SCREEN_SHAKE */    OPT("Screen shake", v_level3, 2, OPTPAGE_ACCESS),
    /* OPT_DAMAGE_NUMBERS */  OPT("Damage numbers", v_off_on, 1, OPTPAGE_SHOOTER),
    /* OPT_HIT_MARKERS */     OPT("Hit markers", v_off_on, 1, OPTPAGE_SHOOTER),
    /* OPT_HUD_LAYOUT */      OPT("HUD layout", v_hud, 0, OPTPAGE_DISPLAY),
    /* OPT_SHOULDER */        OPT("Shoulder", v_shoulder, 0, OPTPAGE_SHOOTER),
    /* OPT_BLOOD_COLOR */     OPT("Blood colour", v_blood, 0, OPTPAGE_GAME),
    /* OPT_CONFETTI_AMOUNT */ OPT("Confetti", v_confetti, 1, OPTPAGE_GAME),
    /* OPT_DEMON_DENSITY */   OPT("Demon density", v_density, 1, OPTPAGE_SHOOTER),
    /* OPT_POSSESSION_RATE */ OPT("Possessions", v_possess, 2, OPTPAGE_SIM),
    /* OPT_EVENT_RATE */      OPT("Town events", v_events, 1, OPTPAGE_SIM),
    /* OPT_WEATHER */         OPT("Weather", v_weather, 1, OPTPAGE_SIM),
    /* OPT_LCD_FILTER */      OPT("LCD filter", v_lcd, 1, OPTPAGE_DISPLAY),
    /* OPT_WINDOW_SKIN */     OPT("Window skin", v_skin, WSKIN_STRAWBERRY, OPTPAGE_DISPLAY),
    /* OPT_FONT_STYLE */      OPT("Font style", v_font, 0, OPTPAGE_DISPLAY),
    /* OPT_SUBTITLES */       OPT("Subtitles", v_off_on, 1, OPTPAGE_ACCESS),
    /* OPT_COLORBLIND */      OPT("Colourblind aid", v_colorblind, 0, OPTPAGE_ACCESS),
    /* OPT_REDUCE_FLASH */    OPT("Reduce flashes", v_off_on, 0, OPTPAGE_ACCESS),
    /* OPT_PET_NOTIFICATIONS */ OPT("Pet alerts", v_off_on, 1, OPTPAGE_SIM),
    /* OPT_NEWS_TICKER */     OPT("News ticker", v_off_on, 1, OPTPAGE_SIM),
    /* OPT_CLOCK_24H */       OPT("Clock", v_clock, 0, OPTPAGE_DISPLAY),
    /* OPT_TITLE_THEME */     OPT("Title theme", v_title, 0, OPTPAGE_DISPLAY),
    /* OPT_CURSOR */          OPT("Cursor", v_cursor, 0, OPTPAGE_DISPLAY),
    /* OPT_CAMERA_FILTER */   OPT("Camera filter", v_filter, 0, OPTPAGE_DISPLAY),
    /* OPT_UI_BOUNCE */       OPT("UI bounce", v_bounce, 2, OPTPAGE_ACCESS),
    /* OPT_CHEAT_BIG_HEAD */  OPT("Big-head mode", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_CONFETTI */  OPT("Confetti everything", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_INFINITE_CANDY */ OPT("Infinite candy", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_CUTE_DEMONS */    OPT("Cute demons", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_BRUTAL_RESIDENTS */ OPT("Brutal residents", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_LOW_GRAV */  OPT("Low gravity", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_PAPER_MODE */ OPT("Paper mode", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_GOD_MODE */  OPT("Plush armor (god)", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_RICH */      OPT("Bottomless wallet", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_DISCO */     OPT("Disco lights", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_TINY_DEMONS */ OPT("Tiny demons", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_CHEAT_ALL_GHOSTS */ OPT("Everyone's a ghost", v_off_on, 0, OPTPAGE_CHEATS),
    /* OPT_COMBAT_FEEL */     OPT("Combat feel", v_feel, 2, OPTPAGE_SHOOTER),
    /* OPT_TURN_SWAP */       OPT("Turn buttons", v_turn, 0, OPTPAGE_CONTROLS),
    /* OPT_EFFECTS */         OPT("Effects detail", v_fx, 1, OPTPAGE_PERF),
    /* OPT_DRAW_DIST */       OPT("Draw distance", v_dist, 1, OPTPAGE_PERF),
    /* OPT_SHOW_FPS */        OPT("Show FPS", v_off_on, 0, OPTPAGE_PERF),
    /* OPT_LANGUAGE */        OPT("Language", v_lang, LANG_EN, OPTPAGE_GAME)
};

int opt_get(int opt)
{
    int v;
    if (opt < 0 || opt >= OPT_COUNT) return 0;
    v = doomo_town.opt[opt];
    if (v >= doomo_option_info[opt].count) v = doomo_option_info[opt].def;
    return v;
}

void opt_set(int opt, int value)
{
    if (opt < 0 || opt >= OPT_COUNT) return;
    value = DOOMO_CLAMP(value, 0, doomo_option_info[opt].count - 1);
    doomo_town.opt[opt] = (uint8_t)value;
    if (opt == OPT_LANGUAGE) lang_changed();
}

void opt_defaults(void)
{
    int i;
    memset(doomo_town.opt, 0, sizeof(doomo_town.opt));
    for (i = 0; i < OPT_COUNT; i++) doomo_town.opt[i] = doomo_option_info[i].def;
}
