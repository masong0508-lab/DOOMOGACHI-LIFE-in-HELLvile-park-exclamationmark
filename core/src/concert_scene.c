/* concert_scene.c - SCENE_CONCERT: the Concert Hall (owner: concert).
 *
 *   setup    the stage manager: Solo / Band / Choir (up to 25 citizens),
 *            genre (their favourite is marked with a heart), stage theme,
 *            choir size + arrangement, "write another song", a preview of
 *            the title and the first lines, and the line-up
 *   intro    the curtain with tonight's bill, curtain up, lights up
 *   show     the song plays on the tone channels while the citizen(s) sing
 *            with their own voices; karaoke pill with a bouncing heart;
 *            press A on the beat to whip up the crowd (combo -> pyro)
 *   encore   a hyped crowd chants ENCORE! (chorus + outro, faster)
 *   bow      confetti, hearts, curtain down
 *   results  stars + a critic's review, happiness for the singer (and the
 *            choir), news, unlocks; Save Single / Edit in Studio / Again /
 *            Done
 *   parade   the Hit Parade: the island's saved singles, replayable
 *
 * State: one static struct (~1 KB). The background music is paused while
 * the show plays (audio_music(SONG_NONE)) and restored on leave.
 */
#include "concert_int.h"

enum { CS_SETUP, CS_INTRO, CS_SHOW, CS_ENCORE_ASK, CS_ENCORE, CS_BOW, CS_RESULTS, CS_PARADE };
enum { ROW_ACT, ROW_GENRE, ROW_STAGE, ROW_SIZE, ROW_LAYOUT, ROW_TAKE, ROW_START, ROW_COUNT };
enum { RES_SINGLE, RES_STUDIO, RES_AGAIN, RES_DONE, RES_COUNT };

#define INTRO_TICKS 96
#define BOW_TICKS 75

/* (integration, memory budget) the show's state (1 KB) lives in the shared
 * scene scratch (scene.h, tag CONC) while SCENE_CONCERT runs, not in .bss;
 * the Music Studio it may open on top keeps nothing there */
#define CONCERT_SCRATCH_TAG SAVE_EXT_TAG('C', 'O', 'N', 'C')
typedef struct ConcertState {
    int state, st_t;
    int lead, mode, genre, take, stage_sel, stage, layout, choir_n;
    int row, res_sel, parade_sel, from_parade;
    int prev_song;
    int hype, combo, best_combo, perfects, stars, gain, encore_done, applied, saved, exported;
    int popup_t, popup_kind;
    int cheer_cool;
    int last_line;
    int studio;                 /* performing a Music Studio song         */
    char parade_title[CONCERT_SINGLES][CONCERT_TITLE_LEN];   /* chart cache */
    uint8_t slot_of[SONG_SINGERS];   /* studio choir slot -> cast index    */
    char band[32];
    ConcertSong song;
    ConcertCast cast;
    ConcertPlayer pl;
    StageView v;
    uint32_t t;
} ConcertState;
DOOMO_STATIC_ASSERT(sizeof(ConcertState) <= DOOMO_SCENE_SCRATCH, concert_state_fits_the_scratch);
static ConcertState *cs;
#define C (*cs)

static const char *const mode_names[CMODE_COUNT] = { "Solo", "Band", "Choir" };
static const char *const layout_names[CHOIR_LAYOUT_COUNT] = { "Risers", "Semicircle", "Sections",
                                                              "Pyramid" };

/* The Music Studio's song while it performs on our stage (it lives in the
 * studio's state, which stays put under us on the scene stack). */
static const DoomoSong *studio_song;

int concert_stage_song(const DoomoSong *s)
{
    int i, any = 0;
    if (!s || !(s->flags & SONGF_VALID)) return 0;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(s, i) >= 0) any = 1;
    if (!any) return 0;                          /* nobody to sing it */
    studio_song = s;
    scene_push(&SCENE_CONCERT, CONCERT_ARG_STUDIO);
    return 1;
}

/* ====================================================================== */
/* helpers                                                                  */
int concert_demo_resident(int id)
{
    const DoomoResident *r;
    if (id < 0 || id > DOOMO_PLAYER_ID) return -1;
    r = town_res(id);
    if (id < DOOMO_MAX_RESIDENTS && (!r || r->status == RES_EMPTY)) {
        town_new(TOWN_FREEPLAY, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
        sim_init();
    }
    return id;
}

static int lead_edgy(void)
{
    const DoomoResident *r = town_res(C.lead);
    return r && C.lead < DOOMO_MAX_RESIDENTS && (RES_DISTRICT(C.lead) || (r->flags & RF_HELLBORN));
}

static int auto_stage(void)
{
    const ConcertGenre *g = &concert_genres[C.genre];
    if ((g->edgy || lead_edgy()) && concert_stage_unlocked(STAGE_HELLFIRE) && g->edgy) return STAGE_HELLFIRE;
    if ((g->edgy || lead_edgy()) && concert_stage_unlocked(STAGE_GRAVEYARD)) return STAGE_GRAVEYARD;
    if ((C.genre == 4 || C.genre == 10 || C.genre == 1) && concert_stage_unlocked(STAGE_DREAMLAND))
        return STAGE_DREAMLAND;
    if ((C.genre == 8 || C.genre == 2 || C.genre == 11 || C.genre == 0) &&
        concert_stage_unlocked(STAGE_DISCO_INFERNO))
        return STAGE_DISCO_INFERNO;
    return STAGE_SUGAR_BOWL;
}

static void recompose(void)
{
    concert_song_make(&C.song, C.lead, C.genre, C.take);
}

static void recast(void)
{
    concert_cast_make(&C.cast, C.lead, C.mode, C.choir_n, C.layout);
    concert_band_name(C.lead, C.genre, C.song.seed, C.band, (int)sizeof(C.band));
}

static int row_visible(int r)
{
    if (r == ROW_SIZE) return C.mode != CMODE_SOLO;
    if (r == ROW_LAYOUT) return C.mode == CMODE_CHOIR;
    return 1;
}

static void parade_refresh(void);

static void goto_state(int s)
{
    C.state = s;
    C.st_t = 0;
    if (s == CS_PARADE) parade_refresh();
}

/* the chart's titles (composing a song is too slow to do every frame) */
static void parade_refresh(void)
{
    int i;
    for (i = 0; i < CONCERT_SINGLES; i++) {
        ConcertSingle sg;
        C.parade_title[i][0] = 0;
        if (concert_single_get(i, &sg)) {
            concert_song_make(&C.song, sg.singer, sg.genre, sg.take);
            memcpy(C.parade_title[i], C.song.title, CONCERT_TITLE_LEN);
        }
    }
}

static void leave(void)
{
    concert_play_silence();
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

/* the crowd: fans of the genre first, then anyone who is around */
static void pick_crowd(void)
{
    int k, n = 0, pass, start = (int)(doomo_hash32(C.song.seed + 5u) % DOOMO_MAX_RESIDENTS);
    for (pass = 0; pass < 2 && n < CROWD_MAX; pass++)
        for (k = 0; k < DOOMO_MAX_RESIDENTS && n < CROWD_MAX; k++) {
            int id = (start + k * 7) % DOOMO_MAX_RESIDENTS, j, dup = 0;
            const DoomoResident *r = town_res(id);
            if (!r || (r->status != RES_ALIVE && r->status != RES_GHOST)) continue;
            if (pass == 0 && r->like_music != C.genre) continue;
            for (j = 0; j < C.cast.n; j++) if (C.cast.ids[j] == id) dup = 1;
            for (j = 0; j < n; j++) if (C.v.crowd[j] == id) dup = 1;
            if (!dup) C.v.crowd[n++] = (int16_t)id;
        }
    while (n < CROWD_MAX) C.v.crowd[n++] = -1;
    C.v.ncrowd = (plat_screen_h() >= 200 ? 7 : 6) + (plat_screen_h() >= 200 ? 11 : 9);
    if (C.v.ncrowd > CROWD_MAX) C.v.ncrowd = CROWD_MAX;
    /* a choir of 25 already uses half the portrait cache: the back rows
     * reuse the front row's faces (as silhouettes nobody can tell) */
    if (C.mode == CMODE_CHOIR) {
        int k, front = plat_screen_h() >= 200 ? 7 : 6;
        for (k = front; k < CROWD_MAX; k++) C.v.crowd[k] = C.v.crowd[k % front];
    }
}

static int fans_in_crowd(void)
{
    int i, f = 0;
    for (i = 0; i < C.v.ncrowd; i++) {
        const DoomoResident *r = C.v.crowd[i] >= 0 ? town_res(C.v.crowd[i]) : NULL;
        if (r && r->like_music == C.genre) f++;
    }
    return f;
}

/* the stage as it looks before the show (setup preview) */
static void prep_view(void)
{
    memset(&C.v, 0, sizeof(C.v));
    C.v.cast = &C.cast;
    C.v.genre = C.genre;
    C.v.stage = C.stage_sel >= 0 ? C.stage_sel : auto_stage();
    C.v.lights = 10;
    C.v.sec = SEC_INTRO;
    C.v.hype = 30;
    /* an anonymous crowd: the line-up already fills the portrait cache */
    {
        int i;
        for (i = 0; i < CROWD_MAX; i++) C.v.crowd[i] = -1;
        C.v.ncrowd = plat_screen_h() >= 200 ? 18 : 15;
    }
}

/* ====================================================================== */
/* lifecycle                                                                */
static void setup_defaults(int lead, int mode)
{
    const DoomoResident *r = town_res(lead);
    C.lead = lead;
    C.mode = mode;
    C.genre = r ? r->like_music % MUSIC_COUNT : 0;
    C.take = (int)(doomo_town.day & 3);
    C.stage_sel = -1;                         /* auto */
    C.layout = CHOIR_RISERS;
    C.choir_n = 12;
    C.row = ROW_START;
    recompose();
    recast();
    prep_view();
}

static void stage_begin(void);

static void start_show(void)
{
    C.stage = C.stage_sel >= 0 ? C.stage_sel : auto_stage();
    recompose();
    recast();
    stage_begin();
}

/* a Music Studio song: its choir, title and genre on our stage */
static void start_studio_show(void)
{
    const DoomoSong *s = studio_song;
    int i;
    memset(&C.cast, 0, sizeof(C.cast));
    for (i = 0; i < BAND_COUNT; i++) C.cast.band[i] = CONCERT_NOBODY;
    for (i = 0; i < SONG_SINGERS; i++) {
        int id = song_singer_id(s, i);
        C.slot_of[i] = 0xFF;
        C.cast.ids[i] = CONCERT_NOBODY;
        if (id < 0) continue;
        C.slot_of[i] = C.cast.n;
        C.cast.section[C.cast.n] = (uint8_t)song_singer_section(s, i);
        C.cast.ids[C.cast.n++] = (uint16_t)id;
    }
    C.cast.mode = (uint8_t)(C.cast.n > 1 ? CMODE_CHOIR : CMODE_SOLO);
    C.cast.layout = (uint8_t)(s->choir_layout % CHOIR_LAYOUT_COUNT);
    C.mode = C.cast.mode;
    C.lead = C.cast.ids[0];
    C.genre = s->genre % MUSIC_COUNT;
    memset(&C.song, 0, sizeof(C.song));
    memcpy(C.song.title, s->title, CONCERT_TITLE_LEN - 1);
    C.song.edgy = (uint8_t)((s->flags & SONGF_HELLISH) ? 1 : 0);
    C.song.bpm = s->bpm;
    C.song.genre = (uint8_t)C.genre;
    C.song.seed = s->seed ^ 0x5EEDu;
    C.studio = 1;
    C.stage = auto_stage();
    stage_begin();
}

static void stage_begin(void)
{
    memset(&C.v, 0, sizeof(C.v));
    C.v.cast = &C.cast;
    C.v.genre = C.genre;
    C.v.stage = C.stage;
    C.v.curtain = 256;
    pick_crowd();
    C.hype = 20 + fans_in_crowd() * 4;
    C.combo = C.best_combo = C.perfects = 0;
    C.encore_done = C.applied = C.saved = C.exported = 0;
    C.last_line = -1;
    audio_music(SONG_NONE);
    concert_play_silence();
    goto_state(CS_INTRO);
}

static void concert_enter(int arg)
{
    int id = -1, mode = CMODE_SOLO;
    cs = (ConcertState *)scene_scratch(CONCERT_SCRATCH_TAG, (uint32_t)sizeof(ConcertState));
    memset(&C, 0, sizeof(C));
    C.prev_song = audio_music_current();
    if (arg == CONCERT_ARG_PARADE) {
        goto_state(CS_PARADE);
        return;
    }
    if (arg == CONCERT_ARG_STUDIO) {
        if (studio_song) { start_studio_show(); return; }
        goto_state(CS_PARADE);
        return;
    }
    if (arg <= CONCERT_ARG_CHOIR) { mode = CMODE_CHOIR; id = concert_demo_resident(CONCERT_ARG_CHOIR - arg); }
    else if (arg <= CONCERT_ARG_BAND) { mode = CMODE_BAND; id = concert_demo_resident(CONCERT_ARG_BAND - arg); }
    else if (arg < 0) id = concert_demo_resident(-1 - arg);
    else id = arg;
    if (id < 0 || !town_res(id) || town_res(id)->status == RES_EMPTY ||
        town_res(id)->status == RES_DEAD) {
        ui_toast("Nobody is here to sing...");
        goto_state(CS_PARADE);
        return;
    }
    setup_defaults(id, mode);
    goto_state(CS_SETUP);
}

static void concert_leave(void)
{
    if (!scene_scratch_mine(CONCERT_SCRATCH_TAG)) { song_stop(); concert_play_silence(); return; }
    if (C.studio) {
        /* back to the studio, which keeps the background music paused */
        song_stop();
        studio_song = NULL;
        concert_play_silence();
        return;
    }
    concert_play_silence();
    if (C.prev_song > SONG_NONE) audio_music(C.prev_song);
    else concert_restore_music();
}

/* ====================================================================== */
/* results                                                                  */
static void apply_results(void)
{
    DoomoResident *r = town_res(C.lead);
    int i, gain;
    char news[NEWS_LEN];
    static const char *const star_s[6] = { "", "*", "**", "***", "****", "*****" };
    if (C.applied) return;
    C.applied = 1;
    C.stars = 1 + (C.hype >= 25) + (C.hype >= 50) + (C.hype >= 75) + (C.hype >= 92);
    gain = 8 + C.stars * 3 + (C.encore_done ? 4 : 0);
    C.gain = gain;
    if (r) {
        r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + gain);
        r->stat[STAT_SOCIAL] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SOCIAL] + 10);
        if (r->level < 99) r->level++;
    }
    for (i = 1; i < C.cast.n; i++) {
        DoomoResident *m = town_res(C.cast.ids[i]);
        if (!m) continue;
        m->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, m->stat[STAT_HAPPY] + 5 + C.stars);
        m->stat[STAT_SOCIAL] = (uint8_t)DOOMO_MIN(100, m->stat[STAT_SOCIAL] + 6);
        if (rel_find(C.lead, C.cast.ids[i])) rel_adjust(C.lead, C.cast.ids[i], C.mode == CMODE_BAND ? 4 : 2);
    }
    progress_add(PROG_CONCERTS, 1);
    if (C.mode == CMODE_BAND) unlock_grant(UL_PART_RIPPED_JEANS);
    if (C.mode == CMODE_CHOIR && C.cast.n >= SONG_SINGERS) unlock_grant(UL_PART_STAR_FRECKLES);
    if (C.mode == CMODE_CHOIR && C.genre == 10) unlock_grant(UL_PART_STAR_HALO);
    if (C.genre >= 13) unlock_grant(UL_PART_MULLET);
    if (C.genre == 12) unlock_grant(UL_PART_EMO_FRINGE);
    if (C.stars >= 5) unlock_grant(UL_PART_SPARKLE_AURA);
    if (C.encore_done) unlock_grant(UL_PART_POMPADOUR);
    if (lead_edgy()) unlock_grant(UL_STAGE_GRAVEYARD);
    doomo_snprintf(news, sizeof(news), "%s sang \"%s\" at the Concert Hall! %s", concert_name(C.lead),
                   C.song.title, star_s[C.stars]);
    sim_news_push(news);
    audio_sfx(C.stars >= 3 ? SFX_HAPPY : SFX_BLIP);
}

/* weak reference: the Music Studio's song book (studio module) */
extern DoomoSong doomo_song_book[SONG_BOOK_SIZE] __attribute__((weak));

static int studio_slot(void)
{
    int i;
    DoomoSong *book = doomo_song_book;
    if (!book) return -1;
    for (i = 0; i < SONG_BOOK_SIZE; i++)
        if (!(book[i].flags & SONGF_VALID)) return i;
    for (i = 0; i < SONG_BOOK_SIZE; i++)              /* replace a generated, unloved one */
        if (!(book[i].flags & (SONGF_CUSTOM | SONGF_FAVORITE)) && book[i].seed) return i;
    return -2;
}

static void edit_in_studio(void)
{
    const DoomoScene *studio = scene_by_name("STUDIO");
    int slot = studio_slot();
    if (!studio || slot == -1) {
        ui_toast("The Music Studio is still being built!");
        audio_sfx(SFX_ERROR);
        return;
    }
    if (slot < 0) {
        ui_toast("Your song book is full!");
        audio_sfx(SFX_ERROR);
        return;
    }
    concert_song_export(&C.song, &C.cast, &doomo_song_book[slot]);
    C.exported = 1;
    concert_play_silence();
    audio_sfx(SFX_SELECT);
    scene_push(studio, slot);
}

static void save_single(void)
{
    concert_single_save(C.lead, C.genre, C.take);
    C.saved = 1;
    unlock_grant(UL_PART_SPARKLE_EYES);
    ui_toast(STR_ICON_STAR " A new single tops the Hit Parade!");
    audio_sfx(SFX_SPARKLE);
}

/* ====================================================================== */
/* update                                                                   */
static void setup_update(void)
{
    int dir = btn_repeat(BTN_RIGHT) ? 1 : btn_repeat(BTN_LEFT) ? -1 : 0;
    int r, tpb = 1800 / (C.song.bpm ? C.song.bpm : 120);
    /* the light show idles to the song's tempo */
    C.v.beat = (int)(C.t % (uint32_t)tpb);
    C.v.kick = C.v.beat < 5 ? 5 - C.v.beat : 0;
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); leave(); return; }
    if (btn_repeat(BTN_DOWN) || btn_repeat(BTN_UP)) {
        int d = btn_repeat(BTN_DOWN) ? 1 : -1;
        r = C.row;
        do { r = (r + d + ROW_COUNT) % ROW_COUNT; } while (!row_visible(r));
        C.row = r;
        audio_sfx(SFX_CURSOR);
    }
    if (dir) {
        switch (C.row) {
        case ROW_ACT:
            C.mode = (C.mode + dir + CMODE_COUNT) % CMODE_COUNT;
            recast();
            break;
        case ROW_GENRE:
            C.genre = (C.genre + dir + MUSIC_COUNT) % MUSIC_COUNT;
            recompose();
            concert_band_name(C.lead, C.genre, C.song.seed, C.band, (int)sizeof(C.band));
            break;
        case ROW_STAGE: {
            int s = C.stage_sel, k;
            for (k = 0; k < STAGE_COUNT + 1; k++) {
                s += dir;
                if (s < -1) s = STAGE_COUNT - 1;
                if (s >= STAGE_COUNT) s = -1;
                if (s < 0 || concert_stage_unlocked(s)) break;
            }
            C.stage_sel = s;
            break; }
        case ROW_SIZE:
            if (C.mode == CMODE_CHOIR) {
                C.choir_n = DOOMO_CLAMP(C.choir_n + dir, 2, SONG_SINGERS);
                recast();
            } else {
                /* band: roll a new band name */
                concert_band_name(C.lead, C.genre, C.song.seed + C.t, C.band, (int)sizeof(C.band));
            }
            break;
        case ROW_LAYOUT:
            C.layout = (C.layout + dir + CHOIR_LAYOUT_COUNT) % CHOIR_LAYOUT_COUNT;
            recast();
            break;
        case ROW_TAKE:
            C.take = (C.take + dir + 4) & 3;
            recompose();
            break;
        default: break;
        }
        prep_view();
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_A)) {
        if (C.row == ROW_TAKE) {
            C.take = (C.take + 1) & 3;
            recompose();
            audio_sfx(SFX_SPARKLE);
        } else {
            audio_sfx(SFX_SELECT);
            start_show();
        }
    }
}

/* the karaoke + stage view from the player */
static void sync_view(int ev)
{
    StageView *v = &C.v;
    int calm = opt_get(OPT_REDUCE_FLASH);          /* no strobing lamps */
    v->t = C.t;
    if (ev & PEV_BEAT) v->beat = 0; else v->beat++;
    if ((ev & PEV_KICK) && !calm) v->kick = 6; else if (v->kick) v->kick--;
    if ((ev & PEV_SNARE) && !calm) v->snare = 6; else if (v->snare) v->snare--;
    if (ev & PEV_ONSET) v->onset = 6; else if (v->onset) v->onset--;
    if (C.studio) {
        uint32_t m = song_pos_singers(), out = 0;
        int i;
        for (i = 0; i < SONG_SINGERS; i++)
            if ((m >> i) & 1u && C.slot_of[i] != 0xFF) out |= 1u << C.slot_of[i];
        if (!m && song_pos_line() >= 0 && song_playing()) out = 1u;
        v->singing = out;
        v->sec = !song_playing() ? SEC_END : concert_mask_count(out) > 1 ? SEC_CHORUS1 : SEC_VERSE1;
    } else {
        v->sec = C.pl.song ? concert_play_section(&C.pl) : SEC_END;
        v->singing = C.pl.line >= 0 ? C.pl.singing : 0;
    }
    v->hype = C.hype;
    if (v->pyro) v->pyro--;
    if (v->cheer) v->cheer--;
    if (v->confetti) v->confetti--;
    if (v->sec == SEC_CHORUS1 || v->sec == SEC_CHORUS2) v->confetti = 4;
    v->encore = C.state == CS_ENCORE;
}

static void cheer(void)
{
    /* how far from the nearest beat (in 1/256 ticks)? */
    int step_in_beat = C.pl.step == 0xFFFF ? 0 : C.pl.step & 3;
    int pos = step_in_beat * C.pl.step_len + C.pl.phase, beat = C.pl.step_len * 4;
    int d;
    if (C.studio) {
        beat = 256 * 1800 / (C.song.bpm ? C.song.bpm : 120);
        pos = 256 * DOOMO_MIN(song_beat_age(), 200);
        if (pos > beat) pos = beat;
    }
    d = pos < beat / 2 ? pos : beat - pos;
    if (C.cheer_cool > 0) return;
    C.cheer_cool = 4;
    C.v.cheer = 10;
    if (d <= 2 * 256) {
        C.combo++;
        C.perfects++;
        C.hype = DOOMO_MIN(100, C.hype + 4);
        C.popup_kind = 2;
    } else if (d <= 4 * 256) {
        C.combo++;
        C.hype = DOOMO_MIN(100, C.hype + 2);
        C.popup_kind = 1;
    } else {
        C.combo = 0;
        C.popup_kind = 0;
    }
    C.popup_t = 18;
    if (C.combo > C.best_combo) C.best_combo = C.combo;
    if (C.combo && (C.combo % 8) == 0) {
        C.v.pyro = 24;
        audio_sfx(SFX_CONFETTI);
    } else {
        audio_sfx(C.popup_kind == 2 ? SFX_SPARKLE : SFX_BLIP);
    }
}

static void confirm_stop(int yes)
{
    if (yes) {
        if (C.studio) song_stop();
        concert_play_silence();
        C.pl.song = NULL;
        goto_state(CS_BOW);
    }
}

static void show_update(int encore)
{
    int ev;
    if (C.cheer_cool) C.cheer_cool--;
    if (C.popup_t) C.popup_t--;
    if (C.v.curtain > 0) C.v.curtain = DOOMO_MAX(0, C.v.curtain - 16);
    if (C.v.lights < 16) C.v.lights++;
    if (C.studio) {
        song_tick();
        ev = song_beat_age() == 0 ? PEV_BEAT | PEV_KICK : 0;
        if (song_pos_line() != C.last_line && song_pos_line() >= 0) ev |= PEV_BAR;
        if (!song_playing()) ev |= PEV_END;
    } else {
        ev = concert_play_tick(&C.pl);
    }
    sync_view(ev);
    if ((ev & PEV_BAR) && (C.studio ? song_pos_line() : C.pl.line) >= 0 &&
        (C.studio ? song_pos_line() : C.pl.line) != C.last_line) {
        C.last_line = C.studio ? song_pos_line() : C.pl.line;
        C.hype = DOOMO_MIN(100, C.hype + 1 + (C.mode == CMODE_CHOIR ? 1 : 0));
    }
    if ((ev & PEV_BAR) && !(C.st_t & 63) && C.hype > 10) C.hype--;   /* hype fades */
    if (btn_pressed(BTN_A)) cheer();
    if (btn_pressed(BTN_B) || btn_pressed(BTN_START)) {
        concert_play_silence();
        ui_confirm("Stop the show early?", confirm_stop);
        return;
    }
    if (ev & PEV_END) {
        if (!encore && C.hype >= 55 && !C.studio) {
            goto_state(CS_ENCORE_ASK);
            audio_sfx(SFX_HAPPY);
        } else {
            goto_state(CS_BOW);
        }
    }
}

static void results_update(void)
{
    if (C.st_t < 20) return;
    if (btn_repeat(BTN_RIGHT) || btn_repeat(BTN_DOWN)) { C.res_sel = (C.res_sel + 1) % RES_COUNT; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT) || btn_repeat(BTN_UP)) { C.res_sel = (C.res_sel + RES_COUNT - 1) % RES_COUNT; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_B)) { leave(); return; }
    if (!btn_pressed(BTN_A)) return;
    switch (C.res_sel) {
    case RES_SINGLE:
        if (C.saved) { ui_toast("Already on the Hit Parade!"); audio_sfx(SFX_ERROR); }
        else save_single();
        break;
    case RES_STUDIO: edit_in_studio(); break;
    case RES_AGAIN:
        audio_sfx(SFX_SELECT);
        if (C.from_parade) start_show();
        else goto_state(CS_SETUP);
        break;
    default:
        audio_sfx(SFX_SELECT);
        if (C.from_parade) { goto_state(CS_PARADE); C.from_parade = 0; }
        else leave();
        break;
    }
}

static void parade_update(void)
{
    int n = concert_single_count();
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); leave(); return; }
    if (!n) return;
    if (btn_repeat(BTN_DOWN)) { C.parade_sel = (C.parade_sel + 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_UP)) { C.parade_sel = (C.parade_sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_SELECT) || btn_pressed(BTN_X)) {
        concert_single_remove(C.parade_sel);
        if (C.parade_sel >= concert_single_count()) C.parade_sel = 0;
        parade_refresh();
        audio_sfx(SFX_CANCEL);
        ui_toast("Dropped off the chart.");
        return;
    }
    if (btn_pressed(BTN_A)) {
        ConcertSingle s;
        const DoomoResident *r;
        if (!concert_single_get(C.parade_sel, &s)) return;
        r = town_res(s.singer);
        if (!r || r->status == RES_EMPTY || r->status == RES_DEAD) {
            ui_toast("That singer has left the island...");
            audio_sfx(SFX_ERROR);
            return;
        }
        setup_defaults(s.singer, CMODE_SOLO);
        C.genre = s.genre;
        C.take = s.take;
        C.from_parade = 1;
        audio_sfx(SFX_SELECT);
        start_show();
    }
}

static void concert_update(void)
{
    if (!scene_scratch_mine(CONCERT_SCRATCH_TAG)) {
        /* cannot happen (the studio keeps nothing in the scratch) */
        plat_log("concert: scene scratch lost, closing");
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        return;
    }
    C.t++;
    C.st_t++;
    switch (C.state) {
    case CS_SETUP: setup_update(); break;
    case CS_INTRO:
        C.v.t = C.t;
        C.v.lights = C.st_t > 60 ? (C.st_t - 60) / 2 : 0;
        if (C.st_t > 60) C.v.curtain = DOOMO_MAX(0, 256 - (C.st_t - 60) * 8);
        if (C.st_t == 62) audio_sfx(SFX_HAPPY);
        if (btn_pressed(BTN_A) && C.st_t < 60) C.st_t = 60;
        if (C.st_t >= INTRO_TICKS) {
            if (C.studio) song_play(studio_song, -1);
            else concert_play_start(&C.pl, &C.song, &C.cast, 0);
            goto_state(CS_SHOW);
        }
        break;
    case CS_SHOW: show_update(0); break;
    case CS_ENCORE_ASK:
        sync_view(C.st_t % 9 == 0 ? PEV_BEAT | PEV_KICK : 0);
        C.v.cheer = 2;
        if (C.st_t > 20 && btn_pressed(BTN_A)) {
            C.encore_done = 1;
            C.hype = DOOMO_MIN(100, C.hype + 10);
            concert_play_start(&C.pl, &C.song, &C.cast, 1);
            C.v.pyro = 30;
            audio_sfx(SFX_CONFETTI);
            goto_state(CS_ENCORE);
        } else if (C.st_t > 20 && btn_pressed(BTN_B)) {
            goto_state(CS_BOW);
        }
        break;
    case CS_ENCORE: show_update(1); break;
    case CS_BOW:
        sync_view(0);
        C.v.sec = SEC_END;
        C.v.confetti = 3;
        C.v.cheer = 2;
        if (C.st_t > 40) C.v.curtain = DOOMO_MIN(256, (C.st_t - 40) * 10);
        if (C.st_t == 2) audio_sfx(SFX_CONFETTI);
        if (C.st_t >= BOW_TICKS || (btn_pressed(BTN_A) && C.st_t > 10)) {
            C.v.curtain = 256;
            if (C.studio) {
                /* a studio session: take a bow, then back to the studio */
                DoomoResident *r = town_res(C.lead);
                if (r) r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + 5);
                leave();
                return;
            }
            apply_results();
            C.res_sel = RES_SINGLE;
            goto_state(CS_RESULTS);
        }
        break;
    case CS_RESULTS:
        C.v.t = C.t;
        results_update();
        break;
    case CS_PARADE: parade_update(); break;
    }
}

/* ====================================================================== */
/* drawing                                                                  */
static int W(void) { return plat_screen_w(); }
static int H(void) { return plat_screen_h(); }

static void draw_lineup(int x0, int x1, int feet)
{
    int i, n = C.cast.n, span = x1 - x0, step;
    if (n <= 0) return;
    step = n > 1 ? DOOMO_MIN(18, span / n) : 0;
    for (i = n - 1; i >= 0; i--) {
        const DoomoImage *img = bodyspr_image(C.cast.ids[i], 0, 0);
        int x = x0 + (span - step * (n - 1)) / 2 + i * step;
        int b = ((C.t / 6 + (uint32_t)i) % 8) == 0 ? -1 : 0;
        if (img) plat_image(x - img->w / 2, feet - img->h + b, img, 1, IMG_SHADOW, 0);
    }
}

static void setup_draw(void)
{
    const DoomoResident *r = town_res(C.lead);
    int edgy = lead_edgy() || concert_genres[C.genre].edgy;
    int style = edgy ? WIN_HELL : WIN_CUTE, big = H() >= 200;
    int px = big ? 10 : 4, py = big ? 26 : 20, cw = big ? 80 : 70;
    int mx = px + cw + (big ? 8 : 4), mw = W() - mx - (big ? 10 : 4), my = py;
    int rowh = big ? 15 : 12, rows = 0, i, y, ch;
    const UiStyle *st = ui_style(style);
    StageLayout L;
    char buf[48];
    /* the stage behind, dimmed: genre light show + stage theme preview */
    concert_layout(&L);
    C.v.t = C.t;
    concert_draw_stage(&C.v, &L);
    concert_draw_crowd(&C.v, &L);
    plat_shade_rect(0, 0, W(), H(), 9);
    ui_title(W() / 2, big ? 6 : 3, STR_ICON_NOTE " CONCERT HALL " STR_ICON_NOTE, style);
    for (i = 0; i < ROW_COUNT; i++) if (row_visible(i)) rows++;
    ch = rows * rowh + 10;
    /* the star of the show */
    ui_window(px, py, cw, ch, style);
    {
        const DoomoImage *img = portrait_image(C.lead, EXPR_HAPPY);
        int iy = py + 4 + (ch - 10 - (big ? 80 : 68)) / 2;
        if (img) plat_image(px + (cw - img->w) / 2, iy, img, 1, 0, 0);
        ui_text_center(px + cw / 2, iy + 58, r ? r->name : "???", st->text);
        if (ch >= 84) {
            concert_job_of(C.lead, buf, big ? 13 : 11);
            ui_text_center(px + cw / 2, iy + 68, buf, st->text_dim);
        }
    }
    ui_window(mx, my, mw, ch, style);
    y = my + 6;
    for (i = 0; i < ROW_COUNT; i++) {
        const char *val = "";
        if (!row_visible(i)) continue;
        switch (i) {
        case ROW_ACT: val = mode_names[C.mode]; break;
        case ROW_GENRE:
            doomo_snprintf(buf, sizeof(buf), "%s%s", doomo_music_names[C.genre],
                           r && r->like_music == C.genre ? " " STR_ICON_HEART : "");
            val = buf;
            break;
        case ROW_STAGE:
            val = C.stage_sel < 0 ? "Auto" : concert_stage_names[C.stage_sel];
            break;
        case ROW_SIZE:
            if (C.mode == CMODE_CHOIR) { doomo_snprintf(buf, sizeof(buf), "%d voices", C.cast.n); val = buf; }
            else val = C.band;
            break;
        case ROW_LAYOUT: val = layout_names[C.layout]; break;
        case ROW_TAKE: val = C.song.title; break;
        default: break;
        }
        if (i == ROW_START) {
            int bw = big ? 120 : 100;
            ui_button(mx + (mw - bw) / 2 + 4, y - 2, bw, "Start the show!", C.row == i, style);
        } else {
            static const char *const labels[ROW_COUNT] = { "Act", "Genre", "Stage", "Size",
                                                           "Layout", "Song", "" };
            const char *lab = labels[i];
            char vb[24];
            int maxc = (mw - 18 - ui_text_width(lab) - 34) / FONT_ADVANCE;
            if (i == ROW_SIZE && C.mode == CMODE_BAND) lab = "Band";
            if (i == ROW_SIZE && C.mode == CMODE_CHOIR) lab = "Choir";
            /* long band names / titles: clip with an ellipsis */
            if ((int)strlen(val) > maxc && maxc > 3 && maxc < (int)sizeof(vb)) {
                memcpy(vb, val, (size_t)(maxc - 2));
                vb[maxc - 2] = '.'; vb[maxc - 1] = '.'; vb[maxc] = 0;
                val = vb;
            }
            ui_value_row(mx + 14, y, mw - 18, lab, val, C.row == i);
        }
        y += rowh;
    }
    /* the first lines of the song (big screens) */
    if (big) {
        int sy = my + ch + 6;
        ui_window(mx, sy, mw, 34, WIN_CREAM);
        doomo_snprintf(buf, sizeof(buf), STR_ICON_NOTE " %s", C.song.lines[0]);
        ui_text(mx + 8, sy + 7, buf, ui_style(WIN_CREAM)->text);
        doomo_snprintf(buf, sizeof(buf), "  %s", C.song.lines[1]);
        ui_text(mx + 8, sy + 18, buf, ui_style(WIN_CREAM)->text_dim);
    }
    /* tonight's line-up, standing on the stage */
    {
        int feet = H() - (big ? 22 : 16), x0 = big ? 20 : 8, x1 = W() - x0;
        if (C.mode != CMODE_SOLO) draw_lineup(x0, x1, feet);
    }
    ui_hint(STR_ICON_A " Start  " STR_ICON_B " Leave  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Change", style);
}

static void draw_bill(const StageLayout *L)
{
    /* tonight's playbill, pinned to the curtain under a spotlight */
    char buf[48];
    int w = L->big ? 230 : 190, h = L->big ? 96 : 82, x = (L->w - w) / 2;
    int y = (L->h - h) / 2 - (L->big ? 10 : 8) + ui_wave((int)C.t, 60, 1), k, nw;
    const UiStyle *st = ui_style(WIN_CREAM);
    uint8_t ink = PAL_PAINT(PAINT_CHERRY, 0), gold = PAL_PAINT(PAINT_HONEY, 2);
    for (k = 0; k < 3; k++) {
        int rr = (L->big ? 130 : 105) - k * 18;
        int yy;
        for (yy = -rr / 2; yy <= rr / 2; yy += 2) {
            int dx = (int)fx_isqrt((uint32_t)(rr * rr / 4 - yy * yy)) * 2;
            int y0 = L->h / 2 + yy - 6;
            int x0 = L->w / 2 - dx, x1 = L->w / 2 + dx;
            if ((x0 + y0) & 1) x0++;
            if (k == 2 && x1 > x0) plat_checker(x0, y0, x1 - x0, 2, PAL_PAINT(PAINT_ROSE, 2));
        }
    }
    ui_window(x, y, w, h, WIN_CREAM);
    ui_frame(x + 4, y + 4, w - 8, h - 8, gold);
    ui_text_center(L->w / 2, y + 8, STR_ICON_STAR " TONIGHT ONLY " STR_ICON_STAR, ink);
    nw = ui_text_big_width(concert_name(C.lead), 2);
    ui_text_big(L->w / 2 - nw / 2, y + 19, concert_name(C.lead), 2, gold, ink);
    if (C.studio) doomo_snprintf(buf, sizeof(buf), "fresh from the Music Studio!");
    else if (C.mode == CMODE_BAND) doomo_snprintf(buf, sizeof(buf), "with %s!", C.band);
    else if (C.mode == CMODE_CHOIR) doomo_snprintf(buf, sizeof(buf), "and a choir of %d voices!", C.cast.n);
    else doomo_snprintf(buf, sizeof(buf), "live and solo!");
    ui_text_center(L->w / 2, y + 40, buf, st->text);
    doomo_snprintf(buf, sizeof(buf), STR_ICON_NOTE " \"%s\" " STR_ICON_NOTE, C.song.title);
    ui_text_center(L->w / 2, y + 52, buf, ink);
    doomo_snprintf(buf, sizeof(buf), "a %s spectacular", doomo_music_names[C.genre]);
    ui_text_center(L->w / 2, y + h - 18, buf, st->text_dim);
    for (k = 0; k < 4; k++)
        ui_sparkle(x + (k & 1 ? w - 3 : 3), y + (k & 2 ? h - 3 : 3), (int)(C.t / 2 + (uint32_t)k * 4) & 15,
                   PAL_PAINT(PAINT_LEMON, 2));
    if (C.st_t < 60 && ((C.t / 15) & 1))
        ui_text_center(L->w / 2, y + h + 6, STR_ICON_A " Raise the curtain!", PAL_WHITE);
}

static void draw_hud(const StageLayout *L)
{
    char buf[40];
    int x = 4, y = L->truss_h + 2, w = L->big ? 60 : 44;
    /* hype meter */
    ui_round_rect(x - 1, y - 1, w + 14, 9, 4, PAL_RAMP(RAMP_STONE, 0));
    ui_heart(x + 4, y + 3, 3 + (C.v.kick > 3), PAL_PAINT(PAINT_BUBBLEGUM, 1));
    ui_bar(x + 10, y + 1, w, 5, C.hype, 100, C.hype >= 75 ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 2));
    /* progress */
    {
        int prog = concert_play_progress(&C.pl);
        if (C.studio && studio_song) {
            uint32_t len = song_length_ticks(studio_song);
            prog = len ? (int)(DOOMO_MIN(song_pos_ticks(), len) * 255u / len) : 0;
        }
        plat_rect(0, L->truss_h - 2, L->w * prog / 255, 1, PAL_PAINT(PAINT_LEMON, 2));
    }
    /* combo */
    if (C.combo >= 3) {
        doomo_snprintf(buf, sizeof(buf), "x%d", C.combo);
        ui_text_outline(L->w - 6 - ui_text_width(buf), y, buf, PAL_PAINT(PAINT_LEMON, 2), PAL_RAMP(RAMP_STONE, 0));
    }
    /* now playing */
    if (C.state == CS_SHOW && C.st_t < 120) {
        doomo_snprintf(buf, sizeof(buf), STR_ICON_NOTE " %s", C.song.title);
        ui_title(L->w / 2, y + 10 - (C.st_t < 10 ? 10 - C.st_t : 0), buf, C.song.edgy ? WIN_HELL : WIN_CUTE);
        if (C.st_t > 20 && C.st_t < 110)
            ui_text_center(L->w / 2, y + 26, STR_ICON_A " on the beat = cheer!", PAL_WHITE);
    }
    if (C.state == CS_ENCORE && C.st_t < 60)
        ui_title(L->w / 2, y + 10, "ENCORE!", WIN_HELL);
    /* cheer popup */
    if (C.popup_t) {
        static const char *const pops[3] = { "...", "Good!", "PERFECT!" };
        int py = L->stage_front + 2 - (18 - C.popup_t);
        ui_text_outline(L->w / 2 - ui_text_width(pops[C.popup_kind]) / 2 + (L->w / 4), py,
                        pops[C.popup_kind], C.popup_kind == 2 ? PAL_PAINT(PAINT_LEMON, 2) : PAL_WHITE,
                        PAL_RAMP(RAMP_STONE, 0));
    }
}

static void draw_karaoke(const StageLayout *L)
{
    const ConcertPlayer *p = &C.pl;
    if (C.studio) {
        int line = song_pos_line(), syl = song_pos_syllable();
        if (line >= 0 && line < SONG_LYRIC_LINES && studio_song && studio_song->lyrics[line][0]) {
            const char *ln = studio_song->lyrics[line];
            uint8_t st[CONCERT_SYL_MAX + 1];
            int n = song_syllables(ln, st, CONCERT_SYL_MAX), lit, ball;
            if (syl < 0) syl = -1;
            lit = syl + 1 >= n ? (int)strlen(ln) : st[syl + 1];
            ball = syl >= 0 && syl < n ? st[syl] : -1;
            if (syl < 0) lit = 0;
            concert_draw_lyric(L, ln, lit, ball, C.song.edgy, C.t);
        } else if (song_playing()) {
            concert_draw_lyric(L, "~ " STR_ICON_NOTE " ~", 5, -1, C.song.edgy, C.t);
        }
        return;
    }
    if (p->song && p->line >= 0) {
        const char *ln = C.song.lines[(int)p->line];
        int lit = p->sung >= p->line_syl ? (int)strlen(ln) : p->syl_start[p->sung];
        int ball = p->sung > 0 ? p->syl_start[p->sung - 1] : -1;
        concert_draw_lyric(L, ln, lit, ball, C.song.edgy, C.t);
        /* who is singing: a little chibi badge */
        if (C.cast.n > 1 && p->voice_k >= 0) {
            const DoomoImage *img = bodyspr_image(C.cast.ids[(int)p->voice_k], 0, 0);
            int len = (int)strlen(ln), pw = len * FONT_ADVANCE + 16;
            if (pw > L->w - 4) pw = L->w - 4;
            if (img) plat_image((L->w - pw) / 2 - 12, L->lyric_y - 8, img, 1, IMG_SHADOW, 0);
            if (concert_mask_count(p->singing) > 1) {
                char buf[16];
                doomo_snprintf(buf, sizeof(buf), "x%d", concert_mask_count(p->singing));
                ui_text_outline((L->w + pw) / 2 + 3, L->lyric_y + 4, buf, PAL_PAINT(PAINT_LEMON, 2),
                                PAL_RAMP(RAMP_STONE, 0));
            }
        }
    } else if (p->song) {
        int sec = concert_play_section(p);
        const char *s = sec == SEC_INTRO ? concert_genres[C.genre].adlib :
                        sec == SEC_OUTRO ? concert_genres[C.genre].adlib : "~ " STR_ICON_NOTE " ~";
        concert_draw_lyric(L, s, (int)strlen(s), -1, C.song.edgy, C.t);
    }
}

static void stars_draw(int cx, int y, int n, int t)
{
    int i;
    for (i = 0; i < 5; i++) {
        int x = cx + (i - 2) * 16, on = i < n, pop = t - i * 6;
        int r = on ? (pop < 0 ? 0 : pop < 6 ? 3 + pop : 7) : 5;
        if (r <= 0) continue;
        ui_star(x, y, r, on ? PAL_PAINT(PAINT_LEMON, 2) : PAL_RAMP(RAMP_STONE, 2));
    }
}

static void results_draw(const StageLayout *L)
{
    int big = L->big, w = big ? 260 : 220, h = big ? 150 : 128, x = (L->w - w) / 2, y = big ? 30 : 12, i;
    int style = C.song.edgy ? WIN_HELL : WIN_CUTE;
    char buf[64];
    const UiStyle *st;
    plat_shade_rect(0, 0, L->w, L->h, 8);
    if (!ui_window_pop(x, y, w, h, style, C.st_t)) return;
    st = ui_style(style);
    doomo_snprintf(buf, sizeof(buf), "\"%s\"", C.song.title);
    ui_title(L->w / 2, y - 6, buf, style);
    stars_draw(L->w / 2, y + 18, C.stars, C.st_t);
    {
        const char *rev = concert_review(C.stars, C.genre, C.song.edgy, C.song.seed + (uint32_t)C.stars);
        int critic = C.v.crowd[0] >= 0 ? C.v.crowd[0] : -1;
        doomo_snprintf(buf, sizeof(buf), "\"%s\"", rev);
        ui_text_wrap(x + 10, y + 32, w - 20, buf, st->text, 1);
        doomo_snprintf(buf, sizeof(buf), "- %s, %s", critic >= 0 ? concert_name(critic) : "a fan",
                       C.song.edgy ? "Hellvile Howler" : "Hillvile Herald");
        ui_text(x + w - 12 - ui_text_width(buf), y + (big ? 56 : 52), buf, st->text_dim);
    }
    doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " %s: happiness +%d", concert_name(C.lead), C.gain);
    ui_text(x + 10, y + (big ? 72 : 66), buf, st->text);
    if (C.mode == CMODE_CHOIR) doomo_snprintf(buf, sizeof(buf), STR_ICON_NOTE " The whole choir feels closer!");
    else if (C.mode == CMODE_BAND) doomo_snprintf(buf, sizeof(buf), STR_ICON_NOTE " The band is tighter than ever!");
    else doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR " Best combo: %d  Perfects: %d", C.best_combo, C.perfects);
    ui_text(x + 10, y + (big ? 84 : 77), buf, st->text);
    {
        static const char *const labels[RES_COUNT] = { "Save Single", "Edit in Studio", "Again", "Done" };
        int bw = big ? 112 : 96, gap = big ? 8 : 6, bx = x + (w - (bw * 2 + gap)) / 2 + 4, by = y + h - (big ? 40 : 34);
        for (i = 0; i < RES_COUNT; i++) {
            const char *lab = labels[i];
            if (i == RES_SINGLE && C.saved) lab = "Saved!";
            ui_button(bx + (i & 1) * (bw + gap), by + (i >> 1) * 16, bw, lab, C.res_sel == i, style);
        }
    }
    if (C.stars >= 4) concert_confetti(x, y, w, h, C.t, 20, 1);
}

static void parade_draw(void)
{
    int big = H() >= 200, n = concert_single_count(), i, rowh = big ? 40 : 28, y0 = big ? 32 : 22;
    int x = big ? 16 : 8, w = W() - 2 * x;
    char buf[64];
    ui_backdrop(WIN_DARK, C.t);
    ui_title(W() / 2, big ? 8 : 4, STR_ICON_STAR " HIT PARADE " STR_ICON_STAR, WIN_CUTE);
    for (i = 0; i < CONCERT_SINGLES; i++) {
        ConcertSingle s;
        int y = y0 + i * (rowh + (big ? 6 : 3)), sel = i == C.parade_sel && n > 0;
        int bx = x + (sel ? 0 : 6), bw = w - (sel ? 0 : 12);
        if (!concert_single_get(i, &s)) {
            /* an empty spot on the chart */
            ui_window(bx, y, bw, rowh, WIN_DARK);
            doomo_snprintf(buf, sizeof(buf), "#%d", i + 1);
            ui_text_big(bx + 8, y + rowh / 2 - 7, buf, 2, PAL_GRAY, PAL_GRAY_D);
            ui_text(bx + 50, y + rowh / 2 - 4, i == 0 ? "No hits yet! Put on a concert" : "Your next hit goes here?",
                    PAL_GRAY_L);
            continue;
        }
        {
            const DoomoImage *img;
            ui_window(bx, y, bw, rowh, sel ? WIN_CUTE : WIN_CREAM);
            doomo_snprintf(buf, sizeof(buf), "#%d", i + 1);
            ui_text_big(bx + 8, y + rowh / 2 - 7, buf, 2, i == 0 ? PAL_PAINT(PAINT_HONEY, 2) : PAL_WHITE,
                        PAL_PAINT(PAINT_CHERRY, 0));
            img = bodyspr_image(s.singer, 0, 0);
            if (img) plat_image(bx + 40, y + rowh - img->h - 2 + (sel && (C.t / 8) & 1 ? -1 : 0), img, 1, IMG_SHADOW, 0);
            doomo_snprintf(buf, sizeof(buf), "\"%s\"", C.parade_title[i]);
            ui_text(bx + 62, y + (big ? 8 : 4), buf, ui_style_text(WIN_CUTE));
            doomo_snprintf(buf, sizeof(buf), "%s - %s", concert_name(s.singer), doomo_music_names[s.genre]);
            ui_text(bx + 62, y + (big ? 20 : 15), buf, ui_style(WIN_CUTE)->text_dim);
            if (i == 0) ui_sparkle(bx + bw - 10, y + 6, (int)(C.t / 2) & 15, PAL_PAINT(PAINT_LEMON, 2));
            if (sel) ui_cursor(x - 2, y + rowh / 2 - 4);
        }
    }
    ui_hint(n ? STR_ICON_A " Play  SELECT Remove  " STR_ICON_B " Back" : STR_ICON_B " Back", WIN_CUTE);
}

static void stage_draw(void)
{
    StageLayout L;
    concert_layout(&L);
    C.v.t = C.t;
    concert_draw_stage(&C.v, &L);
    concert_draw_performers(&C.v, &L);
    concert_draw_fx(&C.v, &L);
    concert_draw_crowd(&C.v, &L);
    concert_draw_curtain(&C.v, &L);
    if (C.state == CS_INTRO && C.v.curtain > 128) draw_bill(&L);
    if (C.state == CS_SHOW || C.state == CS_ENCORE) {
        draw_karaoke(&L);
        draw_hud(&L);
    }
    if (C.state == CS_ENCORE_ASK) {
        int y = L.h / 3;
        ui_text_big(L.w / 2 - ui_text_big_width("ENCORE!", 3) / 2, y + ui_wave((int)C.t, 16, 3), "ENCORE!", 3,
                    PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_CHERRY, 0));
        ui_text_outline(L.w / 2 - ui_text_width("The crowd wants more!") / 2, y + 30, "The crowd wants more!",
                        PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0));
        ui_hint(STR_ICON_A " Encore!  " STR_ICON_B " Take a bow", WIN_CUTE);
    }
    if (C.state == CS_BOW) {
        int k;
        for (k = 0; k < 8; k++) {
            int hx = (k * 37 + (int)C.t * 2) % L.w, hy = L.stage_front - (int)((C.t * 2 + (uint32_t)k * 23) % (uint32_t)(L.stage_front));
            ui_heart(hx, hy, 3, PAL_PAINT(k & 1 ? PAINT_BUBBLEGUM : PAINT_ROSE, 1));
        }
        if (C.st_t > 10 && C.st_t < 60)
            ui_text_big(L.w / 2 - ui_text_big_width("Thank you!", 2) / 2, L.h / 3, "Thank you!", 2,
                        PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0));
    }
    if (C.state == CS_RESULTS) results_draw(&L);
}

static void concert_draw(void)
{
    if (!scene_scratch_mine(CONCERT_SCRATCH_TAG)) { plat_clear(PAL_BLACK); return; }
    switch (C.state) {
    case CS_SETUP: setup_draw(); break;
    case CS_PARADE: parade_draw(); break;
    default: stage_draw(); break;
    }
}

const DoomoScene SCENE_CONCERT = {
    "CONCERT", concert_enter, concert_leave, concert_update, concert_draw, 0
};

/* ====================================================================== */
/* the activities (activities.h)                                            */
static const DoomoActivity concert_acts[] = {
    { "Concert", "Sing their very own song on stage!", ACTLOC_CONCERT_HALL,
      ACTF_NEEDS_RESIDENT | ACTF_FREEPLAY | ACTF_STORY, ICON_NOTE, 1, ACT_ALWAYS, &SCENE_CONCERT, 0 },
    { "Hit Parade", "Replay the island's top singles", ACTLOC_CONCERT_HALL,
      ACTF_FREEPLAY | ACTF_STORY, ICON_STAR, 1, ACT_ALWAYS, &SCENE_CONCERT, CONCERT_ARG_PARADE },
    { "Peek at a Dream", "Tiptoe into a sleeper's dream...", ACTLOC_APARTMENTS,
      ACTF_NEEDS_RESIDENT | ACTF_FREEPLAY | ACTF_STORY | ACTF_NIGHT_ONLY, ICON_ZZZ, 2, ACT_ALWAYS,
      &SCENE_DREAM, 0 },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_concert = concert_acts;
