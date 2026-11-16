/* studio_start.c - ST-EASY: the "Start a song" card (docs/design/STUDIO_PLAN.md 2.1) (owner: studio).
 *
 * Shown on the SONG tab when the studio opens on an empty song and on "New song" (FILE):
 *   Style   one of the 16 composer styles by a short friendly name
 *   Mood    Happy / Sweet / Sad / Spooky / Fierce  = the key mode (and with it the chord family) + the key
 *   Tempo   Slow / Medium / Fast / Frantic         = the style's own bpm range, four points
 *   Length  Short / Song / Long                    = 8 / 12 / 16 order passes (16 / 24 / 32 bars)
 *   Shuffle (a new seed, the same picks, plays at once)   Make it! (compose with this seed, play, card closes)
 *   Inspire me! and Templates (the same ones as on FILE)
 * studio_make_song() is a pure function of the picks, the seed and the song's choir / words: the same picks and
 * seed always give the same song, so "what Shuffle played" is what "Make it!" keeps.
 * The card's state (row, picks, seed) lives in the scene scratch (st_easy()); only the open flag is in `studio`.
 */
#include "studio_int.h"

enum { CR_STYLE, CR_MOOD, CR_TEMPO, CR_LENGTH, CR_SHUFFLE, CR_MAKE, CR_INSPIRE, CR_TEMPLATES, CR_COUNT };

static const char *const style_names[MUSIC_COUNT] = {
    "Bubblegum", "Lo-fi", "City Pop", "Chiptune", "Lullaby", "Jazz", "Classical", "Folk", "Disco", "Shanty",
    "Hymn", "Synthwave", "Goth", "Punk", "Doom", "Industrial"
};
static const char *const mood_names[SMOOD_COUNT] = { "Happy", "Sweet", "Sad", "Spooky", "Fierce" };
static const char *const tempo_names[STEMPO_COUNT] = { "Slow", "Medium", "Fast", "Frantic" };
static const char *const length_names[SLEN_COUNT] = { "Short", "Song", "Long" };
/* mood -> key mode + key: major C, soft pentatonic F, minor A, harmonic minor E, blues D.
 * (Tables of 8 bytes or less would land in the PS1's resident .sdata; this one is 10 bytes: overlay rodata.) */
static const struct { uint8_t scale, key; } mood_tab[SMOOD_COUNT] = {
    { SONG_SCALE_MAJOR, 0 }, { SONG_SCALE_PENTATONIC, 5 }, { SONG_SCALE_MINOR, 9 },
    { SONG_SCALE_HARMONIC_MINOR, 4 }, { SONG_SCALE_BLUES, 2 }
};

const char *studio_style_name(int style) { return style_names[DOOMO_CLAMP(style, 0, MUSIC_COUNT - 1)]; }
const char *studio_mood_name(int mood) { return mood_names[DOOMO_CLAMP(mood, 0, SMOOD_COUNT - 1)]; }
const char *studio_tempo_name(int tempo) { return tempo_names[DOOMO_CLAMP(tempo, 0, STEMPO_COUNT - 1)]; }
const char *studio_length_name(int length) { return length_names[DOOMO_CLAMP(length, 0, SLEN_COUNT - 1)]; }

int studio_pick_bpm(int style, int tempo)
{
    int lo, hi, bpm;
    studio_style_bpm(style, &lo, &hi);
    switch (DOOMO_CLAMP(tempo, 0, STEMPO_COUNT - 1)) {
    case STEMPO_SLOW: bpm = lo - 6; break;
    case STEMPO_MEDIUM: bpm = (lo + hi) / 2; break;
    case STEMPO_FAST: bpm = hi; break;
    default: bpm = hi + 20; break;
    }
    return DOOMO_CLAMP(bpm, 60, 220);
}

int studio_pick_passes(int length)
{
    return length <= SLEN_SHORT ? 8 : length == SLEN_SONG ? 12 : SONG_ORDER_MAX;
}

void studio_make_song(DoomoSong *s, const StartPick *p)
{
    /* the order lists: intro hook 3, verse 0, chorus 1, bridge 2; every one ends on the chorus */
    static const uint8_t ords[36] = { 3, 0, 0, 1, 0, 1, 2, 1,                                  /* 8 passes at 0  */
                                      3, 0, 0, 1, 0, 0, 1, 2, 1, 0, 1, 1,                      /* 12 passes at 8 */
                                      3, 0, 0, 1, 0, 0, 1, 2, 1, 0, 0, 1, 2, 1, 1, 1 };       /* 16 passes at 20 */
    int style = DOOMO_CLAMP(p->style, 0, MUSIC_COUNT - 1), mood = DOOMO_CLAMP(p->mood, 0, SMOOD_COUNT - 1);
    int n = studio_pick_passes(p->length);
    const uint8_t *ord = ords + (n <= 8 ? 0 : n <= 12 ? 8 : 20);
    s->flags |= SONGF_VALID | SONGF_CUSTOM;
    s->key = mood_tab[mood].key;
    s->order_len = 1;                         /* a fresh order: the composer lays its own, we replace it below */
    studio_compose_scale(s, style, p->seed, mood_tab[mood].scale);
    s->bpm = (uint8_t)studio_pick_bpm(style, p->tempo);
    memset(s->order, 0, sizeof(s->order));
    memcpy(s->order, ord, (size_t)n);
    s->order_len = (uint8_t)n;
    if (!s->title[0] || !memcmp(s->title, "New Song", 9) || !memcmp(s->title, "Untitled", 9))
        doomo_snprintf(s->title, SONG_TITLE_LEN, "My %s", style_names[style]);
}

/* ---- the card ------------------------------------------------------------------------------------ */
static void pick_of(const StEasy *ez, StartPick *p)
{
    p->style = ez->style; p->mood = ez->mood; p->tempo = ez->tempo; p->length = ez->length; p->seed = ez->seed;
}

void st_card_open(void)
{
    StEasy *ez = st_easy();
    studio.card = 1;
    if (!ez) { studio.card = 0; return; }
    ez->csel = CR_MAKE;
    ez->style = 0;
    ez->mood = SMOOD_HAPPY;
    ez->tempo = STEMPO_MEDIUM;
    ez->length = SLEN_SONG;
    ez->seed = doomo_hash32(doomo_frame_count() * 2654435761u + 0x51ED27u) | 1u;
    st_help_arm();
}

void st_card_close(void) { studio.card = 0; }

static void make(StEasy *ez, int shuffle)
{
    StartPick p;
    if (shuffle) ez->seed = doomo_hash32(ez->seed * 2246822519u + 0x9E3779B9u) | 1u;
    pick_of(ez, &p);
    song_stop();
    ed_undo_make();                            /* Shuffle again / Make it! after a Shuffle: replace that take, so the
                                                * one step this card adds always has the small before-image */
    ed_touch(SS, (int)sizeof(DoomoSong));
    st_adv_take();
    studio_make_song(SS, &p);
    ed_commit();
    ed_mark_make(&p);
    studio.slot = -1;
    song_play_at(SS, -1, 0, 0);
    audio_sfx(SFX_SPARKLE);
    if (shuffle) {
        st_msg("%s %s, take %u", studio_mood_name(p.mood), studio_style_name(p.style), (unsigned)(p.seed % 100));
    } else {
        studio.card = 0;
        st_msg("Made it! START stops the song");
    }
}

static void change(StEasy *ez, int row, int d)
{
    switch (row) {
    case CR_STYLE: ez->style = (uint8_t)((ez->style + d + MUSIC_COUNT) % MUSIC_COUNT); break;
    case CR_MOOD: ez->mood = (uint8_t)((ez->mood + d + SMOOD_COUNT) % SMOOD_COUNT); break;
    case CR_TEMPO: ez->tempo = (uint8_t)((ez->tempo + d + STEMPO_COUNT) % STEMPO_COUNT); break;
    case CR_LENGTH: ez->length = (uint8_t)((ez->length + d + SLEN_COUNT) % SLEN_COUNT); break;
    default: return;
    }
    audio_sfx(SFX_CURSOR);
}

void st_card_update(void)
{
    StEasy *ez = st_easy();
    if (!ez) { studio.card = 0; return; }
    if (btn_repeat(BTN_UP)) { ez->csel = (uint8_t)((ez->csel + CR_COUNT - 1) % CR_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { ez->csel = (uint8_t)((ez->csel + 1) % CR_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) change(ez, ez->csel, -1);
    if (btn_repeat(BTN_RIGHT)) change(ez, ez->csel, 1);
    if (btn_pressed(BTN_A)) {
        switch (ez->csel) {
        case CR_SHUFFLE: make(ez, 1); break;
        case CR_MAKE: make(ez, 0); break;
        case CR_INSPIRE: st_file_inspire(); break;
        case CR_TEMPLATES: st_file_templates(); break;
        default: change(ez, ez->csel, 1); break;
        }
    } else if (btn_pressed(BTN_B)) {
        studio.card = 0;
        audio_sfx(SFX_CANCEL);
        st_help_arm();                         /* the settings view says what it is for */
        st_msg("A blank page. SEL+Up: help");
        input_eat();
    }
}

int st_card_prompts(UiPrompt *p)
{
    StEasy *ez = st_easy();
    int n = 0, row = ez ? ez->csel : CR_MAKE;
    static const char *const a_verb[CR_COUNT] = { "Next", "Next", "Next", "Next", "Shuffle", "Make it!",
                                                  "Inspire me!", "Templates" };
#define PR(b, pr, v) do { p[n].btn = (uint16_t)(b); p[n].prio = (uint8_t)(pr); p[n].verb = (v); n++; } while (0)
    PR(BTN_A, 0, a_verb[row]);
    if (row < CR_SHUFFLE) PR(BTN_LEFT | BTN_RIGHT, 1, "Change");
    PR(BTN_START, 2, st_play_verb());
    PR(BTN_B, 3, "Skip");
#undef PR
    return n;
}

void st_card_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    StEasy *ez = st_easy();
    int wide = st_wide(), rowh = wide ? 16 : 12, i, top = r.y + (wide ? 22 : 17);
    char buf[40];
    st_panel(r.x, r.y, r.w, r.h, WIN_CREAM);
    ui_text(r.x + 10, r.y + 5, "Start a song", st->text);
    st_note_glyph(r.x + r.w - 20, r.y + 3 + ui_wave((int)studio.t, 16, 1), st_pattern_color(0, 1));
    st_note_glyph(r.x + r.w - 32, r.y + 4, st_pattern_color(1, 1));
    if (!ez) return;
    for (i = 0; i < CR_COUNT; i++) {
        int y = top + i * rowh + (i >= CR_SHUFFLE ? 3 : 0), sel = i == ez->csel, ty = y + (rowh - 9) / 2;
        uint8_t tc = sel ? PAL_WHITE : st->text;
        static const char *const labels[CR_COUNT] = { "Style", "Mood", "Tempo", "Length", "Shuffle",
                                                      "Make it!", "Inspire me!", "Templates..." };
        if (sel) {
            ui_round_rect(r.x + 3, y - 1, r.w - 6, rowh, 3, st->accent);
            ui_cursor(r.x + 4, y + (rowh - 8) / 2);
        }
        if (i < CR_SHUFFLE) {
            const char *v = "";
            int bpm = 0;
            switch (i) {
            case CR_STYLE: v = studio_style_name(ez->style); break;
            case CR_MOOD: v = studio_mood_name(ez->mood); break;
            case CR_TEMPO: v = studio_tempo_name(ez->tempo); bpm = studio_pick_bpm(ez->style, ez->tempo); break;
            default: v = studio_length_name(ez->length); bpm = -studio_pick_passes(ez->length) * 2; break;
            }
            ui_text(r.x + 14, ty, labels[i], sel ? PAL_WHITE : st->text_dim);
            if (bpm > 0) doomo_snprintf(buf, sizeof(buf), "%s %s %s  %d bpm", sel ? "<" : " ", v, sel ? ">" : " ", bpm);
            else if (bpm < 0) doomo_snprintf(buf, sizeof(buf), "%s %s %s  %d bars", sel ? "<" : " ", v, sel ? ">" : " ", -bpm);
            else doomo_snprintf(buf, sizeof(buf), "%s %s %s", sel ? "<" : " ", v, sel ? ">" : " ");
            ui_text(r.x + (wide ? 76 : 58), ty, buf, tc);
        } else {
            if (i == CR_SHUFFLE) ui_sparkle(r.x + 19, y + rowh / 2 - 1, (int)(studio.t >> 1) & 15, PAL_PAINT(PAINT_LEMON, 1));
            else if (i == CR_MAKE) ui_star(r.x + 18, y + rowh / 2 - 1, 3, PAL_PAINT(PAINT_HONEY, 2));
            ui_text(r.x + 28, ty, labels[i], tc);
        }
    }
}
