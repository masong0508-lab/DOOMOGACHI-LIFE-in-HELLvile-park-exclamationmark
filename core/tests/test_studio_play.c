/* test_studio_play.c - the PLAY tab of the Music Studio (ST-PLAY; owner: studio): quantize, where in a looped
 * range the player is, the jam stays in the song's scale, live recording (a take is one undo step, ties, overdub
 * and replace, drums) and the recorded take playing back. The recorder is driven with scripted buttons through the
 * studio scene, like the player does it. */
#include "test.h"
#include "../src/studio_int.h"

/* studio_play.c (not in studio_int.h: pure helpers, here for the tests) */
int  play_degree_note(const DoomoSong *s, int trk, int oct, int degree);
void play_chord_notes(const DoomoSong *s, int pc, int oct, int out[3]);
void play_song_roots(const DoomoSong *s, int basic, uint8_t out[4]);
int  play_loop_pos(const DoomoSong *s, int from, int to, uint32_t ticks, int *frac);
int  play_quantize_step(int step, int frac, int q, int from, int to);
const char *play_q_text(int q, int wide);
const char *play_count_text(int beats, char *buf, int cap);
void play_head_text(char *buf, int drums, int oct, int row, int keys4, int replace, int range);
void play_chord_text(char *buf, int root_pc, int third);

static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}

static void press(uint32_t b)
{
    frames(1, b);
    frames(1, 0);
}

static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

static void open_play(void)
{
    int i;
    test_reset();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_STUDIO, -2);                  /* a new song on the FILE tab */
    for (i = 0; i < 12; i++) frames(1, 0);
    studio.song.bpm = 120;
    studio.song.swing = 0;
    studio.song.key = 0;
    studio.song.scale = SONG_SCALE_MAJOR;
    studio.pat = 0;
    st_set_tab(STAB_PLAY);
    frames(2, 0);
}

static int in_scale(const DoomoSong *s, int m) { return song_scale_snap(s, m) == m; }

/* a menu choice by its place in the PLAY tools menu */
static void menu_pick(int item)
{
    int i;
    frames(1, BTN_SELECT);
    frames(1, 0);                                  /* a clean tap of SELECT: the tools menu */
    CHECK_EQ(st_popup_open(), 1);
    for (i = 0; i < item; i++) press(BTN_DOWN);
    press(BTN_A);
    CHECK_EQ(st_popup_open(), 0);
}

static void rec_start_keys(void)
{
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_START);
    frames(1, 0);
}

/* run to the start of the loop after the count-in; returns the frames it took */
static int run_count_in(void)
{
    int n = 0;
    while (!song_playing() && n < 200) { frames(1, 0); n++; }
    return n;
}

/* the player's tick of the loop is `t` after this returns (the next frame makes it t + 1) */
static void run_to_tick(uint32_t t)
{
    while (song_pos_ticks() < t) frames(1, 0);
}

/* hit pad(s) on tick T of the loop: the frame that makes it tick T carries the press */
static void hit_at(uint32_t T, uint32_t b)
{
    run_to_tick(T - 1);
    frames(1, b);
}

/* ====================================================================== */
TEST(studio_play_quantize_table)
{
    /* steps are 16ths; frac is 0..255 into the step */
    CHECK_EQ(play_quantize_step(5, 17, 3, 0, 32), 5);       /* 1/16: just after step 5: step 5 */
    CHECK_EQ(play_quantize_step(5, 127, 3, 0, 32), 5);
    CHECK_EQ(play_quantize_step(5, 128, 3, 0, 32), 6);      /* half way: the next step */
    CHECK_EQ(play_quantize_step(5, 250, 3, 0, 32), 6);
    CHECK_EQ(play_quantize_step(5, 200, 0, 0, 32), 5);      /* off: the step you were in, never later */
    CHECK_EQ(play_quantize_step(5, 255, 0, 0, 32), 5);
    CHECK_EQ(play_quantize_step(5, 0, 2, 0, 32), 6);        /* 1/8: the nearest even step: 5 is a tie, goes up */
    CHECK_EQ(play_quantize_step(4, 100, 2, 0, 32), 4);
    CHECK_EQ(play_quantize_step(6, 200, 2, 0, 32), 6);
    CHECK_EQ(play_quantize_step(7, 100, 2, 0, 32), 8);
    CHECK_EQ(play_quantize_step(2, 0, 1, 0, 32), 4);        /* 1/4: the nearest beat (2 is a tie: up) */
    CHECK_EQ(play_quantize_step(1, 255, 1, 0, 32), 0);      /* 1.99 is nearer the beat at 0 than the one at 4 */
    CHECK_EQ(play_quantize_step(5, 100, 1, 0, 32), 4);
    CHECK_EQ(play_quantize_step(6, 200, 1, 0, 32), 8);
    /* the end of the loop wraps to its start */
    CHECK_EQ(play_quantize_step(31, 200, 3, 0, 32), 0);
    CHECK_EQ(play_quantize_step(15, 200, 3, 0, 16), 0);
    CHECK_EQ(play_quantize_step(31, 200, 3, 16, 32), 16);
    CHECK_EQ(play_quantize_step(30, 200, 1, 0, 32), 0);     /* 1/4 rounds 30.8 to 32 = the start */
    /* never before the loop's first step */
    CHECK_EQ(play_quantize_step(17, 0, 1, 16, 32), 16);
    CHECK_EQ(play_quantize_step(17, 10, 2, 16, 32), 18);      /* 17.04 is nearer 18 than 16 */
}

TEST(studio_play_loop_position_matches_the_player)
{
    static const int bpms[] = { 60, 97, 120, 150, 220 };
    static const int ranges[][2] = { { 0, 32 }, { 0, 16 }, { 16, 32 }, { 5, 12 }, { 31, 32 }, { 7, 8 } };
    static DoomoSong s;
    int b, r, sw;
    test_reset();
    for (b = 0; b < 5; b++)
        for (sw = 0; sw <= 8; sw += 4)
            for (r = 0; r < 6; r++) {
                int from = ranges[r][0], to = ranges[r][1], k, bad = 0;
                song_clear(&s);
                s.bpm = (uint8_t)bpms[b];
                s.swing = (uint8_t)sw;
                song_play_range(&s, 1, from, to);
                CHECK_EQ(song_pos_step(), from);
                for (k = 0; k < 700 && !bad; k++) {
                    int frac = -1, st;
                    st = play_loop_pos(&s, from, to, song_pos_ticks(), &frac);
                    if (st != song_pos_step() || frac < 0 || frac > 255) bad = 1;
                    CHECK_MSG(!bad, "bpm %d swing %d range %d..%d tick %d: pos %d, player at %d", bpms[b], sw, from,
                              to, k, st, song_pos_step());
                    song_tick();
                    CHECK_MSG(song_playing(), "a range loop stopped by itself");
                    CHECK_MSG(song_pos_step() >= from && song_pos_step() < to, "left the range: step %d",
                              song_pos_step());
                }
                song_stop();
            }
}

TEST(studio_play_range_loops_only_its_steps_and_the_whole_pattern_is_unchanged)
{
    static DoomoSong s;
    int k, seen_lo = 99, seen_hi = -1, wraps = 0, last = -1;
    test_reset();
    song_clear(&s);
    s.bpm = 220;
    song_play_range(&s, 2, 8, 12);
    for (k = 0; k < 300; k++) {
        int st;
        song_tick();
        st = song_pos_step();
        if (st < seen_lo) seen_lo = st;
        if (st > seen_hi) seen_hi = st;
        if (st < last) wraps++;
        last = st;
    }
    CHECK_EQ(seen_lo, 8);
    CHECK_EQ(seen_hi, 11);
    CHECK(wraps >= 5);
    song_stop();
    /* song_play_pattern is still the whole pattern: 0..31 then back to 0 */
    song_play_pattern(&s, 1, 0);
    seen_lo = 99; seen_hi = -1; wraps = 0; last = -1;
    for (k = 0; k < 400; k++) {
        int st;
        song_tick();
        st = song_pos_step();
        if (st < seen_lo) seen_lo = st;
        if (st > seen_hi) seen_hi = st;
        if (st < last) wraps++;
        last = st;
    }
    CHECK_EQ(seen_lo, 0);
    CHECK_EQ(seen_hi, 31);
    CHECK(wraps >= 1);
    song_stop();
}

TEST(studio_play_jam_holds_and_releases_a_note)
{
    static DoomoSong s;
    int k;
    test_reset();
    song_clear(&s);
    song_jam_on(&s, 0, 72);
    CHECK_EQ(song_jam_note(0), 72);
    for (k = 0; k < 5; k++) song_tick();
    CHECK_EQ(song_jam_note(0), 72);                       /* held for as long as the pad is */
    song_jam_off(0);
    CHECK_EQ(song_jam_note(0), 0);
    song_jam_on(&s, 1, 60); song_jam_on(&s, 2, 48);
    song_jam_off(-1);
    CHECK_EQ(song_jam_note(1) + song_jam_note(2), 0);
    song_jam_on(&s, 0, 1);                                /* a tie or a rest is no note */
    song_jam_on(&s, 0, 0);
    CHECK_EQ(song_jam_note(0), 0);
    song_stop();
}

TEST(studio_play_jam_stays_in_the_scale)
{
    static DoomoSong s;
    int sc, key, trk, oct, d, pc;
    test_reset();
    song_clear(&s);
    for (sc = 0; sc < SONG_SCALE_COUNT; sc++)
        for (key = 0; key < 12; key++) {
            s.scale = (uint8_t)sc;
            s.key = (uint8_t)key;
            for (trk = 0; trk < 3; trk++)
                for (oct = -2; oct <= 2; oct++) {
                    for (d = 0; d < 8; d++) {
                        int n = play_degree_note(&s, trk, oct, d);
                        CHECK_MSG(in_scale(&s, n), "scale %d key %d track %d oct %d degree %d: note %d is out of key",
                                  sc, key, trk, oct, d, n);
                        CHECK(n >= SONG_NOTE_MIN && n <= SONG_NOTE_MAX);
                    }
                    CHECK_MSG(play_degree_note(&s, trk, oct, 0) % 12 == key || sc == SONG_SCALE_COUNT,
                              "degree 1 is not the key (scale %d key %d)", sc, key);
                    for (pc = 0; pc < 12; pc++) {
                        int c[3], i;
                        if (!in_scale(&s, 60 + pc)) continue;
                        play_chord_notes(&s, pc, oct, c);
                        for (i = 0; i < 3; i++)
                            CHECK_MSG(in_scale(&s, c[i]), "scale %d key %d chord root %d: note %d out of key", sc, key,
                                      pc, c[i]);
                        CHECK_EQ(c[0] % 12, pc);
                    }
                }
        }
}

TEST(studio_play_chords_come_from_the_songs_bass)
{
    static DoomoSong s;
    uint8_t r[4];
    int i, j;
    test_reset();
    song_clear(&s);
    s.order_len = 2;
    s.order[0] = 0; s.order[1] = 1;
    /* bass: A, F in pattern 0 (bar starts); C, G in pattern 1 */
    s.notes[0][2][0] = 45; s.notes[0][2][16] = 41;
    s.notes[1][2][0] = 48; s.notes[1][2][17] = 43;      /* the first note of the bar, not its start */
    play_song_roots(&s, 0, r);
    CHECK_EQ(r[0], 9); CHECK_EQ(r[1], 5); CHECK_EQ(r[2], 0); CHECK_EQ(r[3], 7);
    /* the plain set is I IV V vi of the key */
    s.key = 0;
    play_song_roots(&s, 1, r);
    CHECK_EQ(r[0], 0); CHECK_EQ(r[1], 5); CHECK_EQ(r[2], 7); CHECK_EQ(r[3], 9);
    /* a song with no bass gets the plain set too; every root is in the scale, none repeats in a 7-note scale */
    memset(s.notes, 0, sizeof(s.notes));
    play_song_roots(&s, 0, r);
    CHECK_EQ(r[0], 0); CHECK_EQ(r[1], 5); CHECK_EQ(r[2], 7); CHECK_EQ(r[3], 9);
    s.key = 2; s.scale = SONG_SCALE_MINOR;
    play_song_roots(&s, 1, r);
    for (i = 0; i < 4; i++) {
        CHECK(in_scale(&s, 60 + r[i]));
        for (j = i + 1; j < 4; j++) CHECK(r[i] != r[j]);
    }
}

TEST(studio_play_pads_sound_the_songs_notes_and_chords)
{
    int i, n[3];
    open_play();
    studio.song.key = 7;                                  /* G */
    studio.song.scale = SONG_SCALE_MINOR;
    frames(1, 0);
    CHECK_EQ(studio.tab, STAB_PLAY);
    frames(1, BTN_A);                                     /* the first pad: degree 1 on the lead channel */
    CHECK_EQ(song_jam_note(0), play_degree_note(&studio.song, 0, 0, 0));
    CHECK_EQ(song_jam_note(0) % 12, 7);
    frames(1, 0);
    CHECK_EQ(song_jam_note(0), 0);                        /* let go: silence */
    frames(1, BTN_B);
    CHECK_EQ(song_jam_note(0), play_degree_note(&studio.song, 0, 0, 1));
    frames(1, 0);
    frames(1, BTN_R);                                     /* the fourth pad */
    CHECK_EQ(song_jam_note(0), play_degree_note(&studio.song, 0, 0, 3));
    CHECK(in_scale(&studio.song, song_jam_note(0)));
    frames(1, 0);
    /* a chord: three channels, all in the scale */
    frames(1, BTN_LEFT);
    for (i = 0; i < 3; i++) {
        CHECK(song_jam_note(i) > 0);
        CHECK(in_scale(&studio.song, song_jam_note(i)));
    }
    play_chord_notes(&studio.song, song_jam_note(2) % 12, 0, n);
    CHECK_EQ(song_jam_note(2), n[0]);
    frames(1, 0);
    CHECK_EQ(song_jam_note(0) + song_jam_note(1) + song_jam_note(2), 0);
    /* the octave: SELECT + UP */
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_UP);
    frames(1, 0);
    frames(1, BTN_A);
    CHECK_EQ(song_jam_note(0), play_degree_note(&studio.song, 0, 1, 0));
    CHECK_EQ(play_degree_note(&studio.song, 0, 1, 0), play_degree_note(&studio.song, 0, 0, 0) + 12);
    frames(1, 0);
}

TEST(studio_play_drum_pads_hit_the_kit)
{
    open_play();
    menu_pick(0);                                         /* Notes / Drums */
    frames(1, BTN_A);
    CHECK_EQ(studio.song.notes[0][3][0], 0);              /* not recording: nothing is written */
    frames(1, 0);
    frames(1, BTN_RIGHT);
    frames(1, 0);
    CHECK_EQ(song_jam_note(0) + song_jam_note(1) + song_jam_note(2), 0);
    menu_pick(0);                                         /* back to notes */
    frames(1, BTN_A);
    CHECK(song_jam_note(0) > 0);
    frames(1, 0);
}

TEST(studio_play_record_quantizes_and_a_take_is_one_undo_step)
{
    int steps0, i;
    uint8_t row0[SONG_STEPS];
    open_play();
    studio.song.notes[0][0][20] = 60;                     /* an old note: overdub keeps it */
    memcpy(row0, studio.song.notes[0][0], SONG_STEPS);
    ed_reset();
    steps0 = ed_undo_steps();
    rec_start_keys();
    CHECK(!song_playing());                               /* the count-in first */
    CHECK(run_count_in() >= 55);                          /* four beats at 120 bpm: 15 ticks each */
    CHECK(song_playing());
    CHECK_EQ(song_pos_ticks(), 0u);
    /* tick 19: step 5 + 17/256 -> step 5.  tick 21: step 6 - 4320/7200 into 5 -> 1/16 rounds up to step 6 */
    hit_at(19, BTN_A);
    frames(1, 0);
    hit_at(21, BTN_B);
    frames(1, 0);
    hit_at(40, BTN_R);
    frames(1, 0);
    CHECK_EQ(studio.song.notes[0][0][5], play_degree_note(&studio.song, 0, 0, 0));
    CHECK_EQ(studio.song.notes[0][0][6], play_degree_note(&studio.song, 0, 0, 1));
    /* tick 40: units 76800 = step 10 + 4800 -> 10 + 0.67 -> step 11 */
    CHECK_EQ(studio.song.notes[0][0][11], play_degree_note(&studio.song, 0, 0, 3));
    CHECK_EQ(studio.song.notes[0][0][20], 60);            /* overdub: the old note stays */
    /* nothing was written outside the take's three notes (and the held cells) */
    press(BTN_START);                                     /* stop: the take ends */
    CHECK(!song_playing());
    CHECK_EQ(ed_undo_steps() - steps0, 1);                /* ONE step for the whole take */
    CHECK_EQ(studio.dirty, 1);
    CHECK(ed_undo());
    CHECK_EQ(ed_undo_steps(), steps0);
    for (i = 0; i < SONG_STEPS; i++)
        CHECK_MSG(studio.song.notes[0][0][i] == row0[i], "undo left step %d at %d", i, studio.song.notes[0][0][i]);
}

TEST(studio_play_quantize_off_and_eighths_change_where_a_note_lands)
{
    int i;
    open_play();
    menu_pick(2);                                         /* quantize: 1/16 -> off */
    rec_start_keys();
    run_count_in();
    hit_at(21, BTN_A);                                    /* step 5 + 153/256: off keeps step 5 */
    frames(1, 0);
    CHECK_EQ(studio.song.notes[0][0][5], play_degree_note(&studio.song, 0, 0, 0));
    CHECK_EQ(studio.song.notes[0][0][6], 0);
    press(BTN_START);
    menu_pick(2);                                         /* quantize: 1/4 */
    menu_pick(2);                                         /* 1/8 */
    menu_pick(2);                                         /* 1/16 again: so the menu cycles through all four */
    menu_pick(2);                                         /* off */
    menu_pick(2);                                         /* 1/4 */
    menu_pick(2);                                         /* 1/8 */
    memset(studio.song.notes, 0, sizeof(studio.song.notes));
    rec_start_keys();
    run_count_in();
    hit_at(21, BTN_B);                                    /* step 6 - ish (6.0 - 0.4 = 5.6): 1/8 -> step 6 */
    frames(1, 0);
    hit_at(33, BTN_R);                                    /* 33 * 1920 = 63360 = step 8 + 5760 -> 8.8 -> step 8 */
    frames(1, 0);
    press(BTN_START);
    CHECK_EQ(studio.song.notes[0][0][6], play_degree_note(&studio.song, 0, 0, 1));
    CHECK_EQ(studio.song.notes[0][0][8], play_degree_note(&studio.song, 0, 0, 3));
    for (i = 0; i < SONG_STEPS; i++)
        if (i != 6 && i != 8 && i != 9 && i != 10) CHECK_EQ(studio.song.notes[0][0][i], 0);
}

TEST(studio_play_a_held_pad_writes_tie_cells)
{
    int n, i, lead = 0;
    open_play();
    rec_start_keys();
    run_count_in();
    hit_at(4, BTN_A);                                     /* 4 * 1920 = 7680: step 1 + a bit */
    for (i = 0; i < 7; i++) frames(1, BTN_A);             /* held 8 ticks in all: about two steps */
    frames(1, 0);
    lead = play_degree_note(&studio.song, 0, 0, 0);
    CHECK_EQ(studio.song.notes[0][0][1], lead);
    n = studio.song.notes[0][0][2] == SONG_HOLD;
    CHECK(n);                                             /* the note and one tie: two steps */
    CHECK_EQ(studio.song.notes[0][0][3], 0);
    press(BTN_START);
}

TEST(studio_play_record_then_play_back_gives_the_recorded_notes)
{
    int exp[SONG_STEPS], k, last_step = -1, cur = 0;
    open_play();
    rec_start_keys();
    run_count_in();
    hit_at(2, BTN_A);                                     /* step 0.5 -> 1/16 half up: step 1 */
    frames(1, 0);
    hit_at(19, BTN_B);                                    /* step 5 */
    frames(1, 0);
    hit_at(41, BTN_L);                                    /* 41 * 1920 = 78720 = step 10 + 6720 -> 10.93 -> 11 */
    frames(1, 0);
    press(BTN_START);
    CHECK(!song_playing());
    memset(exp, 0, sizeof(exp));
    exp[1] = play_degree_note(&studio.song, 0, 0, 0);
    exp[5] = play_degree_note(&studio.song, 0, 0, 1);
    exp[11] = play_degree_note(&studio.song, 0, 0, 2);
    for (k = 0; k < SONG_STEPS; k++)
        CHECK_MSG(studio.song.notes[0][0][k] == exp[k], "cell %d is %d, expected %d", k, studio.song.notes[0][0][k],
                  exp[k]);
    /* and the player plays exactly those notes, one onset per step, over the whole pattern */
    {
        int onsets = 0;
        song_play_pattern(&studio.song, 0, 0);
        for (k = 0; k < 140; k++) {
            int st = song_pos_step();
            if (st != last_step) {
                last_step = st;
                cur = exp[st];
                if (cur) { CHECK_EQ(song_pos_note(0), cur); onsets++; }
                else CHECK_EQ(song_pos_note(0), 0);
            }
            song_tick();
            if (song_pos_step() == 0 && last_step == 31) break;
        }
        CHECK_EQ(onsets, 3);
        song_stop();
    }
}

TEST(studio_play_chords_and_other_tracks_record)
{
    open_play();
    menu_pick(1);                                         /* Next track: Harmony */
    rec_start_keys();
    run_count_in();
    hit_at(4, BTN_LEFT);                                  /* a chord records its root */
    frames(1, 0);
    press(BTN_START);
    CHECK_EQ(studio.song.notes[0][0][1], 0);              /* not the lead */
    CHECK(studio.song.notes[0][1][1] >= SONG_NOTE_MIN);
    CHECK(in_scale(&studio.song, studio.song.notes[0][1][1]));
    {
        uint8_t r[4];
        play_song_roots(&studio.song, 0, r);
        CHECK_EQ(studio.song.notes[0][1][1] % 12, r[0]);
    }
}

TEST(studio_play_drums_record_into_the_drum_track)
{
    int steps0;
    open_play();
    menu_pick(0);                                         /* drums */
    ed_reset();
    steps0 = ed_undo_steps();
    rec_start_keys();
    run_count_in();
    hit_at(2, BTN_A);                                     /* kick, step 1 */
    frames(1, 0);
    hit_at(19, BTN_B);                                    /* snare, step 5 */
    frames(1, 0);
    hit_at(27, BTN_RIGHT);                                /* crash on the d-pad: 27 * 1920 = 51840 = step 7 + 1440 -> 7 */
    frames(1, 0);
    press(BTN_START);
    CHECK_EQ(studio.song.notes[0][3][1], SONG_DRUM_KICK);
    CHECK_EQ(studio.song.notes[0][3][5], SONG_DRUM_SNARE);
    CHECK_EQ(studio.song.notes[0][3][7], SONG_DRUM_CRASH);
    CHECK_EQ(studio.song.notes[0][0][1], 0);              /* the melody tracks are untouched */
    CHECK_EQ(ed_undo_steps() - steps0, 1);
    CHECK(ed_undo());
    CHECK_EQ(studio.song.notes[0][3][5], 0);
}

TEST(studio_play_replace_clears_the_loop_and_an_empty_take_changes_nothing)
{
    int i, steps0;
    open_play();
    for (i = 0; i < SONG_STEPS; i += 4) studio.song.notes[0][0][i] = 64;
    menu_pick(3);                                         /* Replace */
    ed_reset();
    steps0 = ed_undo_steps();
    rec_start_keys();
    run_count_in();
    frames(2, 0);
    for (i = 0; i < SONG_STEPS; i += 4) CHECK_EQ(studio.song.notes[0][0][i], 0);     /* cleared when the loop starts */
    press(BTN_START);                                     /* ... and nothing recorded: as it was */
    for (i = 0; i < SONG_STEPS; i += 4) CHECK_EQ(studio.song.notes[0][0][i], 64);
    CHECK_EQ(ed_undo_steps(), steps0);
    /* a real take replaces */
    rec_start_keys();
    run_count_in();
    hit_at(19, BTN_A);
    frames(1, 0);
    press(BTN_START);
    CHECK_EQ(studio.song.notes[0][0][5], play_degree_note(&studio.song, 0, 0, 0));
    CHECK_EQ(studio.song.notes[0][0][0], 0);
    CHECK_EQ(studio.song.notes[0][0][8], 0);
    CHECK_EQ(ed_undo_steps() - steps0, 1);
    CHECK(ed_undo());
    for (i = 0; i < SONG_STEPS; i += 4) CHECK_EQ(studio.song.notes[0][0][i], 64);
    CHECK_EQ(studio.song.notes[0][0][5], 0);
}

TEST(studio_play_loop_part_limits_the_take_to_a_bar)
{
    int i;
    open_play();
    menu_pick(4);                                         /* Loop part: bar 1 */
    rec_start_keys();
    run_count_in();
    for (i = 0; i < 40; i++) {
        CHECK(song_pos_step() >= 0 && song_pos_step() < 16);
        frames(1, 0);
    }
    hit_at(19 + 0, BTN_A);                                /* within bar 1's loop: wraps, still a step < 16 */
    frames(1, 0);
    press(BTN_START);
    for (i = 16; i < SONG_STEPS; i++) CHECK_EQ(studio.song.notes[0][0][i], 0);
    menu_pick(4);                                         /* bar 2 */
    rec_start_keys();
    run_count_in();
    CHECK_EQ(song_pos_step(), 16);
    frames(1, 0);
    CHECK(song_pos_step() >= 16);
    press(BTN_START);
}

TEST(studio_play_leaving_the_tab_ends_the_take_and_stops_the_sound)
{
    int steps0;
    open_play();
    ed_reset();
    steps0 = ed_undo_steps();
    rec_start_keys();
    run_count_in();
    hit_at(19, BTN_A);
    frames(1, 0);
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_R);                        /* SELECT + R: the next tab */
    frames(1, 0);
    CHECK(studio.tab != STAB_PLAY);
    CHECK(!song_playing());
    CHECK_EQ(song_jam_note(0), 0);
    CHECK_EQ(ed_undo_steps() - steps0, 1);
    CHECK_EQ(studio.song.notes[0][0][5], play_degree_note(&studio.song, 0, 0, 0));
}

TEST(studio_play_count_in_can_be_stopped_and_start_loops_the_part)
{
    open_play();
    rec_start_keys();
    frames(20, 0);
    CHECK(!song_playing());
    press(BTN_START);                                     /* stop the count-in */
    frames(80, 0);
    CHECK(!song_playing());                               /* it did not start the loop later */
    press(BTN_START);                                     /* START: listen to the loop */
    CHECK(song_playing());
    frames(50, 0);
    CHECK(song_playing());
    CHECK_EQ(studio.song.notes[0][0][0] + studio.song.notes[0][0][5], 0);
    press(BTN_START);
    CHECK(!song_playing());
}

TEST(studio_play_the_tab_state_is_scratch_and_the_other_tabs_still_work)
{
    int fresh = 0;
    uint8_t *a, *b;
    open_play();
    a = (uint8_t *)st_tab_state(&fresh);
    CHECK(a != NULL);
    CHECK_EQ(fresh, 0);                                   /* the tab claimed it already */
    b = (uint8_t *)st_tab_state(&fresh);
    CHECK(a == b);
    /* the other tabs are as before */
    st_set_tab(STAB_NOTES);
    frames(1, 0);
    press(BTN_A);
    CHECK_EQ(studio.song.notes[0][0][0], 72);
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    st_set_tab(STAB_PLAY);
    frames(2, 0);
    frames(1, BTN_A);
    CHECK(song_jam_note(0) > 0);
    frames(1, 0);
}

TEST(studio_play_menu_items_do_what_they_say)
{
    int i;
    uint8_t basic[4], r[4];
    open_play();
    /* Next pattern: the take, the loop and the ruler follow studio.pat */
    CHECK_EQ(studio.pat, 0);
    menu_pick(5);
    CHECK_EQ(studio.pat, 1);
    CHECK_STR(studio.msg, "Pattern B");
    menu_pick(5); menu_pick(5); menu_pick(5);
    CHECK_EQ(studio.pat, 0);                              /* A..D and round again */
    /* Next track: the pads play and record on that track's channel */
    menu_pick(1);
    CHECK_STR(studio.msg, "Pads play and record Harmony");
    frames(1, BTN_A);
    CHECK_EQ(song_jam_note(0), 0);
    CHECK_EQ(song_jam_note(1), play_degree_note(&studio.song, 1, 0, 0));
    frames(1, 0);
    menu_pick(1);
    CHECK_STR(studio.msg, "Pads play and record Bass");
    frames(1, BTN_A);
    CHECK_EQ(song_jam_note(2), play_degree_note(&studio.song, 2, 0, 0));
    CHECK(song_jam_note(2) < song_jam_note(1) || song_jam_note(2) < 60);
    frames(1, 0);
    menu_pick(1);
    CHECK_STR(studio.msg, "Pads play and record Lead");
    /* Quantize cycles through all four settings and says which */
    menu_pick(2); CHECK_STR(studio.msg, "Quantize: off");
    menu_pick(2); CHECK_STR(studio.msg, "Quantize: 1/4");
    menu_pick(2); CHECK_STR(studio.msg, "Quantize: 1/8");
    menu_pick(2); CHECK_STR(studio.msg, "Quantize: 1/16");
    /* Overdub / Replace and the loop part say what they are now */
    menu_pick(3); CHECK_STR(studio.msg, "Replace: a take clears the loop");
    menu_pick(3); CHECK_STR(studio.msg, "Overdub: keeps the old notes");
    menu_pick(4); CHECK_STR(studio.msg, "Loop: bar 1");
    menu_pick(4); CHECK_STR(studio.msg, "Loop: bar 2");
    menu_pick(4); CHECK_STR(studio.msg, "Loop: whole pattern");
    /* Chord set: the song's own chords, then I IV V vi */
    memset(studio.song.notes, 0, sizeof(studio.song.notes));
    studio.song.notes[0][2][0] = 45;                      /* the bass starts on A: the d-pad's first chord is A */
    frames(1, BTN_LEFT);
    CHECK_EQ(song_jam_note(2) % 12, 9);
    frames(1, 0);
    menu_pick(6);
    CHECK_STR(studio.msg, "Chords: I IV V vi");
    play_song_roots(&studio.song, 1, basic);
    frames(1, BTN_LEFT);
    CHECK_EQ(song_jam_note(2) % 12, basic[0]);
    frames(1, 0);
    menu_pick(6);
    CHECK_STR(studio.msg, "Chords: from your song");
    play_song_roots(&studio.song, 0, r);
    CHECK_EQ(r[0], 9);
    /* B closes the menu and changes nothing */
    frames(1, BTN_SELECT);
    frames(1, 0);
    CHECK_EQ(st_popup_open(), 1);
    press(BTN_B);
    CHECK_EQ(st_popup_open(), 0);
    for (i = 0; i < 3; i++) CHECK_EQ(studio.pat, 0);
}

TEST(studio_play_select_shift_octave_chords_and_limits)
{
    int i, base;
    open_play();
    base = play_degree_note(&studio.song, 0, 0, 0);
    /* octave: up to +2, then it says so and stays */
    for (i = 0; i < 2; i++) {
        frames(1, BTN_SELECT);
        frames(1, BTN_SELECT | BTN_UP);
        frames(1, 0);
        CHECK_EQ(st_popup_open(), 0);                     /* a chord with SELECT is not a menu tap */
    }
    CHECK_STR(studio.msg, "Octave +2");
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_UP);
    frames(1, 0);
    CHECK_STR(studio.msg, "Highest octave");
    frames(1, BTN_A);
    CHECK_EQ(song_jam_note(0), base + 24);
    frames(1, 0);
    for (i = 0; i < 4; i++) {
        frames(1, BTN_SELECT);
        frames(1, BTN_SELECT | BTN_DOWN);
        frames(1, 0);
    }
    CHECK_STR(studio.msg, "Octave -2");
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_DOWN);
    frames(1, 0);
    CHECK_STR(studio.msg, "Lowest octave");
    frames(1, BTN_A);
    CHECK_EQ(song_jam_note(0), base - 24);
    frames(1, 0);
    /* SELECT + A: the chord set */
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_A);
    frames(1, 0);
    CHECK_STR(studio.msg, "Chords: I IV V vi");
    CHECK_EQ(st_popup_open(), 0);
}

TEST(studio_play_undo_during_a_take_takes_back_what_was_recorded_so_far)
{
    int steps0;
    open_play();
    ed_reset();
    steps0 = ed_undo_steps();
    rec_start_keys();
    run_count_in();
    hit_at(19, BTN_A);
    frames(1, 0);
    CHECK_EQ(studio.song.notes[0][0][5], play_degree_note(&studio.song, 0, 0, 0));
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_B);                        /* SELECT + B: undo, with the take still running */
    frames(1, 0);
    CHECK_EQ(studio.song.notes[0][0][5], 0);
    CHECK_EQ(ed_undo_steps(), steps0);
    CHECK(song_playing());                                /* the loop goes on */
    hit_at(song_pos_ticks() + 25, BTN_B);                 /* a new note starts a new take step */
    frames(1, 0);
    press(BTN_START);
    CHECK_EQ(ed_undo_steps() - steps0, 1);
    CHECK(studio.song.notes[0][0][5] == 0);
}

TEST(studio_play_record_from_the_menu_alone_for_a_pad_without_select_start)
{
    /* the Dreamcast pad makes SELECT from START+Y, so SELECT+START cannot be pressed there: the menu starts the take */
    int steps0;
    open_play();
    ed_reset();
    steps0 = ed_undo_steps();
    menu_pick(7);                                         /* Record a take */
    CHECK(!song_playing());                               /* the count-in */
    CHECK(run_count_in() >= 55);
    hit_at(19, BTN_A);                                    /* step 5 */
    frames(1, 0);
    press(BTN_START);                                     /* START alone stops it: no chord needed */
    CHECK(!song_playing());
    CHECK_EQ(studio.song.notes[0][0][5], play_degree_note(&studio.song, 0, 0, 0));
    CHECK_EQ(ed_undo_steps() - steps0, 1);
    frames(1, BTN_SELECT);                                /* undo is named in the shift prompts */
    {
        UiPrompt pr[UI_PROMPTS_MAX];
        int n = tab_prompts(STAB_PLAY, pr), i, has = 0;
        for (i = 0; i < n; i++) if (pr[i].btn == BTN_B && !strcmp(pr[i].verb, "Undo")) has = 1;
        CHECK(has);
    }
    frames(1, 0);
}

TEST(studio_play_prompt_bar_names_the_pad_buttons)
{
    UiPrompt pr[UI_PROMPTS_MAX];
    int n, i, has_record = 0;
    open_play();
    n = tab_prompts(STAB_PLAY, pr);
    CHECK(n >= 3);
    for (i = 0; i < n; i++) if (pr[i].btn == (BTN_SELECT | BTN_START)) has_record = 1;
    CHECK(has_record);
    rec_start_keys();
    n = tab_prompts(STAB_PLAY, pr);
    CHECK_EQ(pr[0].btn, BTN_START);
    CHECK_STR(pr[0].verb, "Stop");
    press(BTN_START);
}

TEST(studio_notes_loop_this_bar_loops_the_bar_under_the_cursor_and_hears_edits)
{
    int i, heard = 0;
    open_play();
    st_set_tab(STAB_NOTES);
    frames(2, 0);
    studio.stp = 20;                                      /* the cursor is in bar 2 */
    frames(1, BTN_SELECT);
    frames(1, 0);
    CHECK_EQ(st_popup_open(), 1);
    for (i = 0; i < 7; i++) press(BTN_DOWN);              /* "Loop this bar" is the last item of the pattern tools */
    press(BTN_A);
    CHECK(song_playing());
    CHECK_STR(studio.msg, "Looping bar 2 of A");
    for (i = 0; i < 200; i++) {
        CHECK(song_pos_step() >= 16 && song_pos_step() < 32);
        frames(1, 0);
    }
    /* a note placed while it loops is heard on the next pass */
    studio.song.notes[0][0][18] = 67;
    for (i = 0; i < 100; i++) {
        frames(1, 0);
        if (song_pos_step() == 18 && song_pos_note(0) == 67) heard = 1;
    }
    CHECK(heard);
    press(BTN_START);                                     /* START stops it as before */
    CHECK(!song_playing());
}

/* The PLAY tab's header and chord texts are built without doomo_snprintf (the draw ran it six times a frame). They must
 * stay the text the old formats made, character for character, for every state the tab can be in. */
TEST(studio_play_draw_texts_match_the_old_snprintf_formats)
{
    static const char *const qn[4] = { "off", "1/4", "1/8", "1/16" };
    static const char *const rn[3] = { "whole pattern", "bar 1", "bar 2" };
    char ref[44], got[44], tmp[16];
    int q, wide, drums, oct, row, keys4, rep, rng, pc, third, b;
    for (q = 0; q < 4; q++)
        for (wide = 0; wide < 2; wide++) {
            doomo_snprintf(ref, sizeof(ref), wide ? "Quantize %s" : "Q %s", qn[q]);
            CHECK_STR(play_q_text(q, wide), ref);
        }
    for (b = 0; b <= 12; b++) {
        doomo_snprintf(ref, sizeof(ref), "Count %d", b);
        CHECK_STR(play_count_text(b, tmp, (int)sizeof(tmp)), ref);
    }
    for (drums = 0; drums < 2; drums++)
        for (oct = -2; oct <= 2; oct++)
            for (row = 0; row < 2; row++)
                for (keys4 = 0; keys4 < 2; keys4++)
                    for (rep = 0; rep < 2; rep++)
                        for (rng = 0; rng < 3; rng++) {
                            if (drums) doomo_snprintf(ref, sizeof(ref), "%s  Loop %s", rep ? "Replace" : "Overdub", rn[rng]);
                            else doomo_snprintf(ref, sizeof(ref), "Oct %s%d  %s%s  Loop %s", oct > 0 ? "+" : "", oct,
                                                keys4 ? (row ? "Notes 5-8  " : "Notes 1-4  ") : "",
                                                rep ? "Replace" : "Overdub", rn[rng]);
                            play_head_text(got, drums, oct, row, keys4, rep, rng);
                            CHECK_STR(got, ref);
                        }
    for (pc = 0; pc < 12; pc++)
        for (third = 0; third < 12; third++) {
            doomo_snprintf(ref, sizeof(ref), "%s%s", doomo_song_key_names[pc], third == 3 ? "m" : (third == 6 ? "o" : ""));
            play_chord_text(got, pc, third);
            CHECK_STR(got, ref);
        }
}
