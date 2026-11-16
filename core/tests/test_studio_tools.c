/* test_studio_tools.c - ST-TOOLS (owner: studio): the range tools and the seven helper tools as pure functions. */
#include "test.h"
#include "../src/studio_int.h"

static void fresh(DoomoSong *s)
{
    song_clear(s);
    s->key = 0;
    s->scale = SONG_SCALE_MAJOR;
}

static void put(DoomoSong *s, int p, int t, int st, int v) { s->notes[p][t][st] = (uint8_t)v; }

TEST(tools_range_is_the_mark_to_the_cursor_or_the_bar)
{
    int f, t;
    tl_range(-1, 5, &f, &t);   CHECK_EQ(f, 0);  CHECK_EQ(t, 16);
    tl_range(-1, 20, &f, &t);  CHECK_EQ(f, 16); CHECK_EQ(t, 32);
    tl_range(4, 9, &f, &t);    CHECK_EQ(f, 4);  CHECK_EQ(t, 10);
    tl_range(9, 4, &f, &t);    CHECK_EQ(f, 4);  CHECK_EQ(t, 10);     /* the mark may be after the cursor */
}

TEST(tools_shift_moves_scale_steps_and_octaves_only_inside_the_range)
{
    DoomoSong s;
    fresh(&s);
    put(&s, 0, 0, 0, 60); put(&s, 0, 0, 4, 64); put(&s, 0, 0, 8, 67); put(&s, 0, 0, 5, SONG_HOLD);
    CHECK_EQ(tl_shift(&s, 0, 0, 0, 5, 1), 2);                 /* C -> D, E -> F; the G at step 8 is outside */
    CHECK_EQ(s.notes[0][0][0], 62);
    CHECK_EQ(s.notes[0][0][4], 65);
    CHECK_EQ(s.notes[0][0][8], 67);
    CHECK_EQ(s.notes[0][0][5], SONG_HOLD);                    /* ties stay ties */
    CHECK_EQ(tl_octave(&s, 0, 0, 0, 32, 1), 3);
    CHECK_EQ(s.notes[0][0][0], 74);
    CHECK_EQ(tl_octave(&s, 0, 0, 0, 32, -1), 3);
    CHECK_EQ(s.notes[0][0][0], 62);
    CHECK_EQ(tl_shift(&s, 0, 3, 0, 32, 1), 0);                /* drums have no pitch */
    put(&s, 0, 0, 12, SONG_NOTE_MAX);
    CHECK_EQ(tl_octave(&s, 0, 0, 12, 13, 1), 0);             /* it would leave the keyboard: left alone, not wrapped */
    CHECK_EQ(s.notes[0][0][12], SONG_NOTE_MAX);
}

TEST(tools_shift_stays_in_the_scale_of_the_song)
{
    DoomoSong s;
    int i;
    fresh(&s);
    s.scale = SONG_SCALE_PENTATONIC;
    put(&s, 1, 1, 0, 60);
    for (i = 0; i < 7; i++) {
        tl_shift(&s, 1, 1, 0, 1, 1);
        CHECK_EQ(song_scale_snap(&s, s.notes[1][1][0]), s.notes[1][1][0]);
    }
}

TEST(tools_repeat_fills_to_the_end_and_refuses_when_nothing_follows)
{
    DoomoSong s;
    fresh(&s);
    put(&s, 0, 0, 0, 60); put(&s, 0, 0, 2, 64);
    CHECK_EQ(tl_repeat(&s, 0, 0, 0, 4), 14);                  /* two notes, seven more copies of the 4 step range */
    CHECK_EQ(s.notes[0][0][4], 60);
    CHECK_EQ(s.notes[0][0][6], 64);
    CHECK_EQ(s.notes[0][0][28], 60);
    CHECK_EQ(s.notes[0][0][30], 64);
    CHECK_EQ(tl_repeat(&s, 0, 0, 0, 32), 0);                  /* the range already reaches the end */
    CHECK_EQ(tl_repeat(&s, 0, 0, 0, 4), 0);                   /* nothing changes the second time: nothing to say */
}

TEST(tools_reverse_keeps_long_notes_long)
{
    DoomoSong s;
    fresh(&s);
    /* C (3 steps), rest, E, G (2 steps) */
    put(&s, 0, 0, 0, 60); put(&s, 0, 0, 1, SONG_HOLD); put(&s, 0, 0, 2, SONG_HOLD);
    put(&s, 0, 0, 4, 64);
    put(&s, 0, 0, 5, 67); put(&s, 0, 0, 6, SONG_HOLD);
    CHECK(tl_reverse(&s, 0, 0, 0, 7) > 0);
    CHECK_EQ(s.notes[0][0][0], 67); CHECK_EQ(s.notes[0][0][1], SONG_HOLD);
    CHECK_EQ(s.notes[0][0][2], 64);
    CHECK_EQ(s.notes[0][0][4], 60); CHECK_EQ(s.notes[0][0][5], SONG_HOLD); CHECK_EQ(s.notes[0][0][6], SONG_HOLD);
    /* twice is the original */
    CHECK(tl_reverse(&s, 0, 0, 0, 7) > 0);
    CHECK_EQ(s.notes[0][0][0], 60); CHECK_EQ(s.notes[0][0][4], 64); CHECK_EQ(s.notes[0][0][5], 67);
    /* drums mirror */
    put(&s, 0, 3, 0, SONG_DRUM_KICK); put(&s, 0, 3, 3, SONG_DRUM_SNARE);
    CHECK(tl_reverse(&s, 0, 3, 0, 4) > 0);
    CHECK_EQ(s.notes[0][3][0], SONG_DRUM_SNARE); CHECK_EQ(s.notes[0][3][3], SONG_DRUM_KICK);
    CHECK_EQ(tl_reverse(&s, 0, 2, 0, 16), 0);                 /* an empty track: nothing to do */
}

TEST(tools_stretch_halves_and_doubles)
{
    DoomoSong s;
    fresh(&s);
    put(&s, 0, 0, 0, 60); put(&s, 0, 0, 2, 64); put(&s, 0, 0, 3, SONG_HOLD);
    CHECK(tl_stretch(&s, 0, 0, 0, 8, 0) > 0);                 /* half speed: every cell takes two steps */
    CHECK_EQ(s.notes[0][0][0], 60); CHECK_EQ(s.notes[0][0][1], SONG_HOLD);
    CHECK_EQ(s.notes[0][0][4], 64); CHECK_EQ(s.notes[0][0][5], SONG_HOLD); CHECK_EQ(s.notes[0][0][6], SONG_HOLD);
    CHECK(tl_stretch(&s, 0, 0, 0, 16, 1) > 0);                /* twice as fast: back */
    CHECK_EQ(s.notes[0][0][0], 60); CHECK_EQ(s.notes[0][0][2], 64);
    put(&s, 0, 3, 0, SONG_DRUM_KICK); put(&s, 0, 3, 4, SONG_DRUM_SNARE);
    CHECK(tl_stretch(&s, 0, 3, 0, 8, 1) > 0);
    CHECK_EQ(s.notes[0][3][0], SONG_DRUM_KICK); CHECK_EQ(s.notes[0][3][2], SONG_DRUM_SNARE); CHECK_EQ(s.notes[0][3][4], 0);
    CHECK_EQ(tl_stretch(&s, 0, 3, 0, 1, 1), 0);               /* one step cannot go faster */
    put(&s, 0, 3, 0, SONG_DRUM_KICK);
    tl_stretch(&s, 0, 3, 0, 4, 0);
    CHECK_EQ(s.notes[0][3][1], 0);                            /* a drum hit stretched is a hit then a gap */
}

TEST(tools_clear_range_clears_only_the_range)
{
    DoomoSong s;
    fresh(&s);
    put(&s, 0, 0, 1, 60); put(&s, 0, 0, 20, 62);
    CHECK_EQ(tl_clear(&s, 0, 0, 0, 16), 1);
    CHECK_EQ(s.notes[0][0][1], 0); CHECK_EQ(s.notes[0][0][20], 62);
    CHECK_EQ(tl_clear(&s, 0, 0, 0, 16), 0);
}

TEST(tools_chord_suggester_follows_the_chord_before_and_stays_diatonic)
{
    DoomoSong s;
    uint8_t out[4];
    int n, i, j;
    fresh(&s);
    n = tl_suggest_chords(&s, 0, 0, 4, out);                  /* nothing before it: the starting chords */
    CHECK_EQ(n, 4); CHECK_EQ(out[0], 0);
    tl_chord_write(&s, 0, 0, 0);                              /* I */
    CHECK_EQ(tl_chord_at(&s, 0, 0), 0);
    n = tl_suggest_chords(&s, 0, 1, 4, out);
    CHECK_EQ(n, 4);
    CHECK_EQ(out[0], 3); CHECK_EQ(out[1], 4);                 /* IV then V after I */
    for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) CHECK(out[i] != out[j]);
    n = tl_suggest_chords(&s, 0, 1, 3, out);                  /* the lighter version: three */
    CHECK_EQ(n, 3);
    s.scale = SONG_SCALE_PENTATONIC;                          /* 5 notes: no degree past 4 */
    tl_chord_write(&s, 0, 0, 4);
    n = tl_suggest_chords(&s, 0, 1, 4, out);
    for (i = 0; i < n; i++) CHECK(out[i] < 5);
    CHECK(n >= 2);
}

TEST(tools_chord_write_puts_root_and_third_and_names_them)
{
    DoomoSong s;
    char nm[8];
    fresh(&s);
    CHECK(tl_chord_write(&s, 1, 2, 5) > 0);                   /* vi in C major = A minor */
    CHECK_EQ(s.notes[1][2][16] % 12, 9);                      /* the bass plays A */
    CHECK_EQ(s.notes[1][1][16] % 12, 0);                      /* the harmony a third above: C */
    CHECK_EQ(s.notes[1][2][17], SONG_HOLD);
    CHECK_EQ(tl_chord_at(&s, 1, 2), 5);
    CHECK_EQ(tl_chord_write(&s, 1, 2, 5), 0);                 /* the same chord twice changes nothing */
    CHECK_EQ(tl_chord_write(&s, 1, 4, 0), 0);                 /* there is no fifth half bar */
    tl_chord_name(&s, 5, nm, sizeof(nm)); CHECK_STR(nm, "Am");
    tl_chord_name(&s, 0, nm, sizeof(nm)); CHECK_STR(nm, "C");
    tl_chord_name(&s, 4, nm, sizeof(nm)); CHECK_STR(nm, "G");
    s.key = 2;                                                /* D major: the names follow the key */
    tl_chord_name(&s, 0, nm, sizeof(nm)); CHECK_STR(nm, "D");
    tl_chord_name(&s, 3, nm, sizeof(nm)); CHECK_STR(nm, "G");
    s.key = 1;
    tl_chord_name(&s, 0, nm, sizeof(nm)); CHECK_STR(nm, "C#");
}

TEST(tools_melody_helper_gives_four_in_scale_phrases_that_differ_and_repeat)
{
    DoomoSong s;
    uint8_t a[4][16], again[16];
    int v, i, differ = 0, notes;
    fresh(&s);
    tl_chord_write(&s, 0, 0, 0);
    tl_chord_write(&s, 0, 1, 3);
    for (v = 0; v < 4; v++) {
        tl_phrase(&s, 0, 0, v, a[v]);
        notes = 0;
        for (i = 0; i < 16; i++)
            if (a[v][i] >= 2) {
                notes++;
                CHECK_EQ(song_scale_snap(&s, a[v][i]), a[v][i]);
                CHECK(a[v][i] >= SONG_NOTE_MIN && a[v][i] <= SONG_NOTE_MAX);
            }
        CHECK(notes >= 2);
        CHECK(a[v][0] >= 2);                                  /* a phrase starts on the beat */
    }
    tl_phrase(&s, 0, 0, 2, again);
    CHECK(memcmp(again, a[2], 16) == 0);                             /* the same song and variant: the same phrase */
    for (v = 1; v < 4; v++) if (memcmp(a[0], a[v], 16)) differ++;
    CHECK(differ >= 2);
    CHECK(tl_phrase_write(&s, 0, 1, a[0]) > 0);               /* into bar 2 */
    CHECK_EQ(s.notes[0][0][16], a[0][0]);
    CHECK_EQ(s.notes[0][0][0], 0);                            /* bar 1 untouched */
    CHECK_EQ(tl_phrase_write(&s, 0, 1, a[0]), 0);
}

TEST(tools_groove_picker_writes_each_groove_and_the_fill_is_optional)
{
    DoomoSong s;
    int g, n = studio_groove_count(), i, fills = 0;
    fresh(&s);
    CHECK_EQ(n, 16);                                          /* silence + the 15 composer grooves */
    for (g = 0; g < n; g++) CHECK(studio_groove_name(g)[0]);
    CHECK(tl_groove_write(&s, 0, 1, 1) > 0);                  /* pop */
    CHECK_EQ(s.notes[0][3][0], SONG_DRUM_KICK);
    CHECK_EQ(s.notes[0][3][31], SONG_DRUM_SNARE);             /* its fill */
    CHECK(tl_groove_write(&s, 0, 1, 0) > 0);                  /* no fill: the last bar is the groove again */
    for (i = 28; i < 32; i++) if (s.notes[0][3][i] == SONG_DRUM_TOM) fills++;
    CHECK_EQ(fills, 0);
    CHECK_EQ(tl_groove_write(&s, 0, 1, 0), 0);
    CHECK(tl_groove_write(&s, 0, 0, 0) > 0);                  /* silence clears the drums */
    for (i = 0; i < SONG_STEPS; i++) CHECK_EQ(s.notes[0][3][i], 0);
}

TEST(tools_harmonize_writes_a_scale_line_under_the_lead)
{
    DoomoSong s;
    int i;
    fresh(&s);
    CHECK_EQ(tl_harmonize(&s, 0, 0), 0);                      /* no lead: nothing to follow */
    put(&s, 0, 0, 0, 72); put(&s, 0, 0, 1, SONG_HOLD); put(&s, 0, 0, 4, 76);
    CHECK(tl_harmonize(&s, 0, 0) > 0);
    CHECK_EQ(s.notes[0][1][0], 69);                           /* a third below C5 in C major: two scale steps, A4 */
    CHECK_EQ(s.notes[0][1][1], SONG_HOLD);
    CHECK_EQ(s.notes[0][1][4], 72);
    tl_harmonize(&s, 0, 1);                                   /* a fifth below */
    CHECK_EQ(s.notes[0][1][0], 65);
    tl_harmonize(&s, 0, 2);
    CHECK_EQ(s.notes[0][1][0], 60);
    CHECK_EQ(tl_harmonize(&s, 0, 2), 0);
    for (i = 0; i < SONG_STEPS; i++) CHECK_EQ(s.notes[0][1][i] >= 2, s.notes[0][0][i] >= 2);
}

TEST(tools_section_copier_copies_shifts_and_swaps_drums)
{
    DoomoSong s;
    fresh(&s);
    put(&s, 0, 0, 0, 60); put(&s, 0, 2, 0, 36); put(&s, 0, 3, 0, SONG_DRUM_KICK);
    CHECK_EQ(tl_section_copy(&s, 0, 0, 0, -1), 0);            /* onto itself: nothing */
    CHECK_EQ(tl_section_copy(&s, 0, 2, 0, -1), 3);
    CHECK_EQ(s.notes[2][0][0], 60); CHECK_EQ(s.notes[2][3][0], SONG_DRUM_KICK);
    CHECK(tl_section_copy(&s, 0, 2, 2, 8) > 0);               /* two scale steps up, Disco drums */
    CHECK_EQ(s.notes[2][0][0], 64);
    CHECK_EQ(s.notes[2][3][0], SONG_DRUM_KICK);
    CHECK(s.notes[2][3][4] != 0 || s.notes[2][3][2] != 0);    /* a Disco bar has more than the one kick */
    CHECK_EQ(s.notes[0][0][0], 60);                           /* the source is not touched */
    s.order_len = 3; s.order[0] = 0; s.order[1] = 1; s.order[2] = 3;
    CHECK_EQ(tl_free_pattern(&s, 0), 2);
    s.order[2] = 2;
    CHECK_EQ(tl_free_pattern(&s, 0), 3);
    s.order_len = 4; s.order[3] = 3;
    CHECK_EQ(tl_free_pattern(&s, 0), 1);                      /* all used: the one after the source */
}

TEST(tools_mix_helper_balances_by_role_and_flags_a_track_that_hides_the_lead)
{
    DoomoSong s;
    int t;
    fresh(&s);
    for (t = 0; t < 4; t++) { put(&s, 0, t, 0, t == 3 ? SONG_DRUM_KICK : 60); s.vol[t] = 12; }
    CHECK(tl_mix_flags(&s) & (1 << 1));                       /* harmony as loud as the lead hides it */
    CHECK(tl_mix_balance(&s) > 0);
    CHECK(s.vol[0] > s.vol[1]); CHECK(s.vol[0] >= s.vol[2]); CHECK(s.vol[0] > s.vol[3]);
    CHECK_EQ(tl_mix_flags(&s), 0);
    CHECK_EQ(tl_mix_balance(&s), 0);                          /* balanced already: nothing to say */
    s.vol[2] = 3;
    memset(s.notes[0][2], 0, SONG_STEPS);                     /* an empty track keeps its volume */
    tl_mix_balance(&s);
    CHECK_EQ(s.vol[2], 3);
}

TEST(tools_fit_status_says_how_words_sit_on_the_notes)
{
    DoomoSong s;
    int w, n;
    fresh(&s);
    s.order_len = 1; s.order[0] = 0;
    put(&s, 0, 0, 0, 60); put(&s, 0, 0, 4, 62); put(&s, 0, 0, 8, 64);
    CHECK_EQ(tl_fit_status(&s, 0, &w, &n), -1);               /* no words: not sung */
    strcpy(s.lyrics[0], "la la la");
    CHECK_EQ(tl_fit_status(&s, 0, &w, &n), 0); CHECK_EQ(w, 3); CHECK_EQ(n, 3);
    strcpy(s.lyrics[0], "la la");
    CHECK_EQ(tl_fit_status(&s, 0, &w, &n), 1);
    strcpy(s.lyrics[0], "la la la la");
    CHECK_EQ(tl_fit_status(&s, 0, &w, &n), 2);
    CHECK(studio_autofit(&s, 0));                             /* the existing fix makes it fit */
    CHECK_EQ(tl_fit_status(&s, 0, &w, &n), 0);
}

/* ---- the tools through the studio scene (menus, undo, messages) ------------------------------------------------- */
static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}
static void press(uint32_t b) { frames(1, b); frames(1, 0); }
static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

static void open_notes(void)
{
    int i;
    studio_stats_reset();                      /* the classic ORDER screen: some other test may have left the level on Advanced */
    test_reset();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_STUDIO, -2);
    for (i = 0; i < 12; i++) frames(1, 0);
    st_level_set(0);                           /* (the scratch copy of the level may be left on Advanced by an earlier test) */
    song_clear(&studio.song);
    studio.song.key = 0;
    studio.song.scale = SONG_SCALE_MAJOR;
    studio.song.bpm = 120;
    studio.pat = 0;
    studio.trk = 0;
    studio.stp = 0;
    studio.sub = SUB_NONE;
    studio.lane = 0;
    studio.rng_a = -1;
    ed_reset();
    st_set_tab(STAB_NOTES);
    frames(2, 0);
}

/* the message bubble is 31 glyphs at most (Studio.msg) and has to fit the narrowest screen (240 px, 8 px of margin, the
 * bubble's own 16) */
static void msg_fits(void)
{
    CHECK(strlen(studio.msg) < sizeof(studio.msg) - 1);
    CHECK(ui_text_width(studio.msg) + 16 <= 240 - 8);
}

static void pick(int item)
{
    int i;
    CHECK_EQ(st_popup_open(), 1);
    for (i = 0; i < item; i++) press(BTN_DOWN);
    press(BTN_A);
    msg_fits();
}

/* SELECT tap, then the items a, b (b < 0: only a) */
static void tools(int a, int b)
{
    frames(1, BTN_SELECT);
    frames(1, 0);
    pick(a);
    if (b >= 0) pick(b);
}

TEST(tools_the_pattern_tools_menu_reaches_range_and_helpers)
{
    open_notes();
    frames(1, BTN_SELECT);
    frames(1, 0);
    CHECK_EQ(st_popup_open(), 1);
    CHECK_EQ(studio.pop_n, 10);
    CHECK_STR(studio.pop_items[8], "Range tools...");
    CHECK_STR(studio.pop_items[9], "Helper tools...");
    press(BTN_B);
    CHECK_EQ(st_popup_open(), 0);
}

TEST(tools_range_octave_is_one_undo_step_and_nothing_to_change_makes_none)
{
    open_notes();
    studio.song.notes[0][0][0] = 60;
    studio.song.notes[0][0][4] = 64;
    studio.song.notes[0][0][20] = 67;                         /* bar 2: outside the cursor's bar */
    tools(8, 3);                                              /* Range tools: Up an octave (the bar under the cursor) */
    CHECK_EQ(studio.song.notes[0][0][0], 72);
    CHECK_EQ(studio.song.notes[0][0][4], 76);
    CHECK_EQ(studio.song.notes[0][0][20], 67);
    CHECK_STR(studio.msg, "Up an octave");
    CHECK_EQ(ed_undo_steps(), 1);
    tools(8, 6);                                              /* Reverse */
    CHECK_STR(studio.msg, "Reversed");
    CHECK_EQ(ed_undo_steps(), 2);
    tools(8, 9);                                              /* Clear range */
    CHECK_EQ(studio.song.notes[0][0][0], 0);
    tools(8, 9);                                              /* nothing left to clear */
    CHECK_STR(studio.msg, "Nothing to change there");
    CHECK_EQ(ed_undo_steps(), 3);                             /* no step for a no-op */
    CHECK(ed_undo());                                         /* clear undone */
    CHECK(ed_undo());                                         /* reverse undone */
    CHECK(ed_undo());                                         /* octave undone */
    CHECK_EQ(studio.song.notes[0][0][0], 60);
    CHECK_EQ(studio.song.notes[0][0][4], 64);
}

TEST(tools_mark_makes_the_range_the_marked_steps)
{
    open_notes();
    studio.song.notes[0][0][2] = 60; studio.song.notes[0][0][6] = 62; studio.song.notes[0][0][12] = 64;
    studio.stp = 2;
    tools(8, 0);                                              /* Mark the start */
    CHECK_EQ(studio.rng_a, 2);
    studio.stp = 6;
    tools(8, 1);                                              /* up a scale step: steps 3..7 only */
    CHECK_EQ(studio.song.notes[0][0][2], 62);
    CHECK_EQ(studio.song.notes[0][0][6], 64);
    CHECK_EQ(studio.song.notes[0][0][12], 64);                /* outside the range: unchanged */
    tools(8, 0);                                              /* the second tap clears the mark */
    CHECK_EQ(studio.rng_a, -1);
    studio.stp = 2; studio.trk = 3;
    tools(8, 1);                                              /* drums have no pitch: say so */
    CHECK_STR(studio.msg, "Drums have no pitch");
}

TEST(tools_chord_suggester_menu_writes_the_chosen_chord_and_undoes)
{
    open_notes();
    studio.stp = 0;
    tools(9, 0);                                              /* Helper tools: Suggest a chord */
    CHECK_EQ(studio.pop_n, 6);                                /* four that fit, all chords, clear */
    CHECK_STR(studio.pop_items[0], "C");                      /* nothing before it: start on the tonic */
    press(BTN_A);
    CHECK_EQ(tl_chord_at(&studio.song, 0, 0), 0);
    CHECK(strstr(studio.msg, "Chord C") != NULL);
    studio.stp = 8;
    tools(9, 0);
    CHECK_STR(studio.pop_items[0], "F");                      /* IV after I */
    press(BTN_DOWN); press(BTN_A);                            /* the second: V */
    CHECK_EQ(tl_chord_at(&studio.song, 0, 1), 4);
    CHECK(ed_undo());
    CHECK_EQ(tl_chord_at(&studio.song, 0, 1), -1);
    CHECK_EQ(tl_chord_at(&studio.song, 0, 0), 0);
}

TEST(tools_phrase_groove_and_harmony_menus_write_and_undo)
{
    int i, lead = 0;
    open_notes();
    tools(9, 1);                                              /* Suggest a phrase: Idea 1 */
    press(BTN_A);
    for (i = 0; i < 16; i++) lead += studio.song.notes[0][0][i] >= 2;
    CHECK(lead >= 2);
    CHECK_EQ(studio.song.notes[0][0][20], 0);                 /* bar 1 only: the cursor is in bar 1 */
    tools(9, 3);                                              /* Harmonize: a third below */
    press(BTN_A);
    CHECK(studio.song.notes[0][1][0] >= 2 && studio.song.notes[0][1][0] < studio.song.notes[0][0][0]);
    tools(9, 2);                                              /* Pick a groove: Pop */
    press(BTN_A);
    CHECK_EQ(studio.song.notes[0][3][0], SONG_DRUM_KICK);
    CHECK(strstr(studio.msg, "Pop") != NULL);
    tools(9, 2);                                              /* More grooves... -> Silence */
    for (i = 0; i < 8; i++) press(BTN_DOWN);
    press(BTN_A);
    CHECK_EQ(st_popup_open(), 1);
    for (i = 0; i < 7; i++) press(BTN_DOWN);
    press(BTN_A);
    CHECK_EQ(studio.song.notes[0][3][0], 0);
    CHECK_EQ(ed_undo_steps(), 4);
    for (i = 0; i < 4; i++) CHECK(ed_undo());
    CHECK_EQ(studio.song.notes[0][0][0], 0);
    CHECK_EQ(studio.song.notes[0][1][0], 0);
    CHECK_EQ(studio.song.notes[0][3][0], 0);
}

TEST(tools_harmonize_without_a_lead_says_so_and_changes_nothing)
{
    open_notes();
    tools(9, 3);
    press(BTN_A);
    CHECK_STR(studio.msg, "Write a lead melody first");
    CHECK_EQ(ed_undo_steps(), 0);
}

TEST(tools_balance_the_mix_from_the_song_tools_and_the_helper_menu)
{
    int t;
    open_notes();
    for (t = 0; t < 4; t++) { studio.song.notes[0][t][0] = (uint8_t)(t == 3 ? SONG_DRUM_KICK : 60); studio.song.vol[t] = 12; }
    st_set_tab(STAB_SONG);
    frames(2, 0);
    st_card_close();                                          /* the Start card is not what is tested here */
    frames(2, 0);
    tools(4, -1);                                             /* Song tools: Balance the mix */
    CHECK(studio.song.vol[0] > studio.song.vol[1]);
    CHECK_EQ(ed_undo_steps(), 1);
    CHECK(strstr(studio.msg, "balanced") != NULL || strstr(studio.msg, "Balanced") != NULL);
    tools(4, -1);
    CHECK_STR(studio.msg, "The mix is already balanced");
    CHECK_EQ(ed_undo_steps(), 1);                             /* nothing to do, no step */
    CHECK(ed_undo());
    CHECK_EQ(studio.song.vol[1], 12);
}

TEST(tools_section_copier_in_arrange_adds_a_pass_with_a_free_part_in_one_undo_step)
{
    int before_len;
    open_notes();
    studio.song.order_len = 2; studio.song.order[0] = 0; studio.song.order[1] = 1;
    studio.song.notes[0][0][0] = 60;
    studio.song.notes[0][3][0] = SONG_DRUM_KICK;
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    studio.osel = 0;
    before_len = studio.song.order_len;
    tools(5, -1);                                             /* Copy as new part */
    CHECK_EQ(studio.song.order_len, before_len + 1);
    CHECK_EQ(studio.song.order[1], 2);                        /* the first free part */
    CHECK_EQ(studio.song.notes[2][0][0], 60);
    CHECK_EQ(studio.osel, 1);
    CHECK_STR(studio.msg, "Part C copied below");
    CHECK_EQ(ed_undo_steps(), 1);
    CHECK(ed_undo());
    CHECK_EQ(studio.song.order_len, before_len);
    CHECK_EQ(studio.song.notes[2][0][0], 0);
    studio.osel = 0;
    tools(6, -1);                                             /* a step higher */
    CHECK_EQ(studio.song.notes[2][0][0], 62);
    studio.osel = 0;
    tools(7, -1);                                             /* other drums */
    CHECK(studio.song.notes[3 - 0][3][0] != 0 || studio.song.notes[2][3][0] != 0);
}

TEST(tools_choir_auto_harmony_writes_thirds_for_every_used_pattern_in_one_step)
{
    int i;
    open_notes();
    studio.song.order_len = 2; studio.song.order[0] = 0; studio.song.order[1] = 1;
    studio.song.notes[0][0][0] = 72; studio.song.notes[1][0][4] = 76;
    st_set_tab(STAB_CHOIR);
    frames(2, 0);
    tools(8, -1);                                             /* Quick fill: Harmony: thirds */
    CHECK_EQ(studio.song.notes[0][1][0], 69);
    CHECK_EQ(studio.song.notes[1][1][4], 72);
    CHECK_EQ(ed_undo_steps(), 1);
    CHECK_STR(studio.msg, "Harmony: thirds under the lead");
    tools(8, -1);                                             /* again: nothing changes, no new step */
    CHECK_EQ(ed_undo_steps(), 1);
    CHECK(ed_undo());
    for (i = 0; i < SONG_STEPS; i++) CHECK_EQ(studio.song.notes[0][1][i], 0);
    studio.song.notes[0][0][0] = 0; studio.song.notes[1][0][4] = 0;
    tools(8, -1);
    CHECK_STR(studio.msg, "Write a lead melody first");
}

TEST(tools_lyric_fit_report_says_words_against_notes)
{
    open_notes();
    studio.song.order_len = 1; studio.song.order[0] = 0;
    studio.song.notes[0][0][0] = 60; studio.song.notes[0][0][4] = 62;
    strcpy(studio.song.lyrics[0], "la la la");
    st_set_tab(STAB_LYRICS);
    frames(2, 0);
    studio.lsel = 0;
    tools(6, -1);
    CHECK_STR(studio.msg, "3 words, 2 notes: too many");
    strcpy(studio.song.lyrics[0], "la");
    tools(6, -1);
    CHECK_STR(studio.msg, "1 word, 2 notes: la la fills");
    strcpy(studio.song.lyrics[0], "la la");
    tools(6, -1);
    CHECK_STR(studio.msg, "2 words, 2 notes: a fit!");
    CHECK_EQ(ed_undo_steps(), 0);                             /* a report changes nothing */
}


/* ======================================================================================================================
 * ST-TOOLS review (the second pass): edge cases of the pushed tools, and the new screens
 * ==================================================================================================================== */
TEST(tools_every_tool_leaves_an_empty_song_alone_and_clamps_a_range_past_the_end)
{
    DoomoSong s;
    uint8_t out[4];
    int t;
    fresh(&s);
    for (t = 0; t < 4; t++) {
        CHECK_EQ(tl_shift(&s, 0, t, 0, 32, 1), 0);
        CHECK_EQ(tl_octave(&s, 0, t, 0, 32, 1), 0);
        CHECK_EQ(tl_repeat(&s, 0, t, 0, 16), 0);
        CHECK_EQ(tl_clear(&s, 0, t, 0, 32), 0);
        CHECK_EQ(tl_reverse(&s, 0, t, 0, 32), 0);
        CHECK_EQ(tl_stretch(&s, 0, t, 0, 32, 1), 0);
        CHECK_EQ(tl_stretch(&s, 0, t, 0, 16, 0), 0);
    }
    CHECK_EQ(tl_harmonize(&s, 0, 0), 0);
    CHECK_EQ(tl_mix_balance(&s), 0);
    CHECK_EQ(tl_section_copy(&s, 0, 1, 1, -1), 0);            /* nothing to copy: nothing changes */
    CHECK_EQ(tl_chord_clear(&s, 0, 0), 0);
    CHECK_EQ(tl_chord_repeat(&s, 0, 0), 0);
    CHECK_EQ(tl_drum_lane_clear(&s, 0, SONG_DRUM_KICK), 0);
    CHECK(tl_suggest_chords(&s, 0, 0, 4, out) >= 3);
    /* a range that runs past the end is cut to the pattern; one that starts past it does nothing */
    put(&s, 0, 0, 30, 60); put(&s, 0, 0, 31, SONG_HOLD);
    CHECK_EQ(tl_shift(&s, 0, 0, 30, 99, 1), 1);
    CHECK_EQ(s.notes[0][0][30], 62);
    CHECK_EQ(tl_shift(&s, 0, 0, 32, 40, 1), 0);
    CHECK_EQ(tl_clear(&s, 0, 0, -5, 3), 0);                   /* before the start: clamped, nothing in it */
    CHECK_EQ(tl_clear(&s, 0, 0, 40, 50), 0);
    CHECK_EQ(tl_reverse(&s, 0, 0, 31, 32), 0);                /* one cell: nothing to turn round */
    CHECK_EQ(s.notes[0][0][31], SONG_HOLD);
}

TEST(tools_a_full_song_transposes_to_the_keyboard_edge_and_stops_there)
{
    DoomoSong s;
    int i, n, guard = 0;
    fresh(&s);
    for (i = 0; i < SONG_STEPS; i++) put(&s, 0, 0, i, SONG_NOTE_MAX - 3 + i % 4);   /* the top of the keyboard */
    while ((n = tl_octave(&s, 0, 0, 0, 32, 1)) > 0 && guard++ < 4) {}
    for (i = 0; i < SONG_STEPS; i++) CHECK(s.notes[0][0][i] >= SONG_NOTE_MIN && s.notes[0][0][i] <= SONG_NOTE_MAX);
    guard = 0;
    while ((n = tl_shift(&s, 0, 0, 0, 32, 1)) > 0 && guard++ < 40) {}
    for (i = 0; i < SONG_STEPS; i++) CHECK(s.notes[0][0][i] <= SONG_NOTE_MAX);
    CHECK_EQ(tl_shift(&s, 0, 0, 0, 32, 1), 0);                /* the top: a press that cannot move anything says 0 */
    for (i = 0; i < SONG_STEPS; i++) put(&s, 1, 2, i, SONG_NOTE_MIN + i % 3);
    guard = 0;
    while ((n = tl_shift(&s, 1, 2, 0, 32, -1)) > 0 && guard++ < 40) {}
    for (i = 0; i < SONG_STEPS; i++) CHECK(s.notes[1][2][i] >= SONG_NOTE_MIN);
}

TEST(tools_half_speed_spills_into_the_steps_after_the_range_and_stops_at_the_pattern_end)
{
    DoomoSong s;
    fresh(&s);
    put(&s, 0, 0, 0, 60); put(&s, 0, 0, 1, 62); put(&s, 0, 0, 2, 64); put(&s, 0, 0, 3, 65);
    put(&s, 0, 0, 6, 72);                                      /* after the range: overwritten by the longer notes */
    CHECK(tl_stretch(&s, 0, 0, 0, 4, 0) > 0);                  /* steps 1-4 take 8 steps: 1-8 */
    CHECK_EQ(s.notes[0][0][0], 60); CHECK_EQ(s.notes[0][0][1], SONG_HOLD);
    CHECK_EQ(s.notes[0][0][2], 62); CHECK_EQ(s.notes[0][0][6], 65);
    put(&s, 0, 0, 20, 70); put(&s, 0, 0, 21, 71);
    CHECK(tl_stretch(&s, 0, 0, 20, 32, 0) > 0);                /* the range is cut at step 32: nothing is written past it */
    CHECK_EQ(s.notes[0][0][20], 70); CHECK_EQ(s.notes[0][0][21], SONG_HOLD); CHECK_EQ(s.notes[0][0][22], 71);
}

TEST(tools_chord_suggester_starts_a_part_after_the_part_before_it_in_the_order)
{
    DoomoSong s;
    uint8_t out[4];
    fresh(&s);
    s.order_len = 2; s.order[0] = 1; s.order[1] = 0;
    tl_chord_write(&s, 1, 3, 3);                              /* part B ends on IV; it plays before part A */
    CHECK_EQ(tl_suggest_chords(&s, 0, 0, 4, out), 4);
    CHECK_EQ(out[0], 4);                                      /* V after IV: from the order list, not from the letter */
    CHECK_EQ(tl_suggest_chords(&s, 1, 0, 4, out), 4);         /* B opens the song: the tonic and its friends */
    CHECK_EQ(out[0], 0);
    s.scale = SONG_SCALE_PENTATONIC;                          /* a five note scale has no sixth chord to start on */
    CHECK(tl_suggest_chords(&s, 1, 0, 4, out) >= 3);
    {
        int i, j, k = tl_suggest_chords(&s, 1, 0, 4, out);
        for (i = 0; i < k; i++) { CHECK(out[i] < 5); for (j = i + 1; j < k; j++) CHECK(out[i] != out[j]); }
    }
}

TEST(tools_mix_balance_is_stable_and_never_moves_the_track_it_is_measured_by)
{
    DoomoSong s;
    int mask, v;
    for (mask = 1; mask < 16; mask++)
        for (v = 0; v < 3; v++) {
            int t, before[4], first;
            fresh(&s);
            for (t = 0; t < 4; t++) {
                if (mask & (1 << t)) put(&s, 0, t, 0, t == 3 ? SONG_DRUM_KICK : 60);
                s.vol[t] = (uint8_t)(v == 0 ? 15 : v == 1 ? 9 : 4 + t * 3);
                before[t] = s.vol[t];
            }
            first = tl_mix_balance(&s);
            (void)first;
            CHECK_EQ(tl_mix_balance(&s), 0);                  /* a second press says "already balanced": no drift */
            for (t = 0; t < 4; t++) {
                CHECK(s.vol[t] >= 1 && s.vol[t] <= 15);
                if (!(mask & (1 << t))) CHECK_EQ(s.vol[t], before[t]);      /* an empty track keeps its volume */
            }
        }
    /* no lead: the bass is the measure and keeps its volume; the others follow it */
    fresh(&s);
    put(&s, 0, 2, 0, 40); put(&s, 0, 1, 0, 60);
    s.vol[2] = 10; s.vol[1] = 14; s.vol[0] = 15;
    CHECK(tl_mix_balance(&s) > 0);
    CHECK_EQ(s.vol[2], 10);
    CHECK(s.vol[1] < s.vol[2]);
    CHECK_EQ(s.vol[0], 15);                                   /* the empty lead is left alone */
}

TEST(tools_phrase_write_and_chord_helpers_refuse_bad_places_without_writing)
{
    DoomoSong s;
    uint8_t ph[16];
    int i;
    fresh(&s);
    for (i = 0; i < 16; i++) ph[i] = (uint8_t)(60 + i);
    CHECK_EQ(tl_phrase_write(&s, 0, -1, ph), 0);
    CHECK_EQ(tl_phrase_write(&s, 0, 2, ph), 0);
    CHECK_EQ(tl_phrase_write(&s, 0, 1, ph), 16);
    CHECK_EQ(s.notes[0][0][16], 60); CHECK_EQ(s.notes[0][0][0], 0);
    CHECK_EQ(tl_chord_clear(&s, 0, 4), 0);
    CHECK_EQ(tl_chord_write(&s, 0, -1, 0), 0);
}

TEST(tools_chord_clear_and_repeat_work_on_half_bars)
{
    DoomoSong s;
    int i;
    fresh(&s);
    CHECK(tl_chord_write(&s, 0, 1, 4) > 0);                   /* V in half bar 2 */
    CHECK(tl_chord_repeat(&s, 0, 1) > 0);                     /* ... on to the end */
    CHECK_EQ(tl_chord_at(&s, 0, 0), -1);
    CHECK_EQ(tl_chord_at(&s, 0, 2), 4); CHECK_EQ(tl_chord_at(&s, 0, 3), 4);
    CHECK_EQ(tl_chord_repeat(&s, 0, 1), 0);                   /* again: nothing changes */
    CHECK_EQ(tl_chord_repeat(&s, 0, 0), 0);                   /* no chord to copy */
    CHECK(tl_chord_clear(&s, 0, 2) > 0);
    CHECK_EQ(tl_chord_at(&s, 0, 2), -1);
    CHECK_EQ(tl_chord_at(&s, 0, 1), 4); CHECK_EQ(tl_chord_at(&s, 0, 3), 4);
    for (i = 16; i < 24; i++) { CHECK_EQ(s.notes[0][2][i], 0); CHECK_EQ(s.notes[0][1][i], 0); }
    CHECK_EQ(tl_chord_clear(&s, 0, 2), 0);
}

TEST(tools_drum_toggle_replaces_the_drum_on_a_step_and_lane_clear_takes_one_drum_out)
{
    DoomoSong s;
    fresh(&s);
    CHECK_EQ(tl_drum_toggle(&s, 0, 4, SONG_DRUM_KICK), SONG_DRUM_KICK);
    CHECK_EQ(tl_drum_toggle(&s, 0, 4, SONG_DRUM_SNARE), SONG_DRUM_SNARE);   /* one drum per step: replaced */
    CHECK_EQ(tl_drum_toggle(&s, 0, 4, SONG_DRUM_SNARE), 0);                 /* the same drum again: off */
    CHECK_EQ(tl_drum_toggle(&s, 0, 4, 0), 0);
    CHECK_EQ(tl_drum_toggle(&s, 0, 4, SONG_DRUM_COUNT), 0);                 /* no such drum: nothing happens */
    CHECK_EQ(tl_drum_toggle(&s, 0, 36, SONG_DRUM_HAT), SONG_DRUM_HAT);      /* the step wraps into the pattern */
    CHECK_EQ(s.notes[0][3][4], SONG_DRUM_HAT);
    put(&s, 0, 3, 0, SONG_DRUM_KICK); put(&s, 0, 3, 8, SONG_DRUM_KICK);
    CHECK_EQ(tl_drum_lane_clear(&s, 0, SONG_DRUM_KICK), 2);
    CHECK_EQ(s.notes[0][3][4], SONG_DRUM_HAT);                              /* the other lanes stay */
    CHECK_EQ(tl_drum_lane_clear(&s, 0, SONG_DRUM_KICK), 0);
    CHECK_EQ(tl_drum_lane_clear(&s, 0, 0), 0);
}

static int sum_notes(void)
{
    int i, n = 0;
    const uint8_t *b = (const uint8_t *)studio.song.notes;
    for (i = 0; i < (int)sizeof(studio.song.notes); i++) n = n * 31 + b[i];
    return n;
}

static void busy_pattern(void)
{
    int i;
    for (i = 0; i < SONG_STEPS; i += 2) {
        studio.song.notes[0][0][i] = (uint8_t)(60 + i % 7);
        studio.song.notes[0][1][i] = (uint8_t)(55 + i % 5);
        studio.song.notes[0][2][i] = (uint8_t)(40 + i % 3);
        studio.song.notes[0][3][i] = (uint8_t)(1 + i % 8);
    }
}

TEST(tools_every_range_tool_on_every_track_is_one_undo_step_and_undoes_to_the_exact_bytes)
{
    int item, trk;
    for (trk = 0; trk < 4; trk++)
        for (item = 1; item <= 9; item++) {
            int before, steps;
            open_notes();
            busy_pattern();
            studio.trk = (uint8_t)trk;
            studio.stp = 21;
            before = sum_notes();
            tools(8, item);
            steps = ed_undo_steps();
            if (steps) {
                CHECK(sum_notes() != before);                 /* a step means something really changed */
                CHECK_EQ(steps, 1);
                CHECK(ed_undo());
            } else {
                CHECK_EQ(sum_notes(), before);                /* no step means nothing changed */
            }
            CHECK_EQ(sum_notes(), before);
            CHECK(ed_redo() == (steps ? 1 : 0));
            if (steps) CHECK(sum_notes() != before);
        }
}

TEST(tools_the_mark_follows_the_pattern_it_was_set_in)
{
    open_notes();
    studio.stp = 31;
    tools(8, 0);
    CHECK_EQ(studio.rng_a, 31);
    CHECK_STR(studio.msg, "Start at step 32. Now move");
    msg_fits();
    press(BTN_R);                                             /* another pattern: the mark is not carried over */
    CHECK_EQ(studio.rng_a, -1);
}

TEST(tools_the_pattern_tools_move_a_track_with_a_message_and_no_step_when_it_cannot)
{
    open_notes();
    studio.song.notes[0][0][0] = 60;
    tools(5, -1);                                             /* Track up a step */
    CHECK_EQ(studio.song.notes[0][0][0], 62);
    CHECK_STR(studio.msg, "Track up a scale step");
    CHECK_EQ(ed_undo_steps(), 1);
    studio.song.notes[0][0][0] = SONG_NOTE_MAX;
    tools(5, -1);
    CHECK_STR(studio.msg, "Nothing to change there");
    CHECK_EQ(ed_undo_steps(), 1);
    studio.trk = 3;
    tools(5, -1);
    CHECK_STR(studio.msg, "Drums have no pitch");
}

/* the card: every menu of the tools has a help line for every item, and it fits */
static void menu_has_help_that_fits(const char *what)
{
    int i, ok = 1;
    if (!st_popup_open()) { printf("    menu %s is not open\n", what); ok = 0; }
    CHECK(ok);
    if (!ok) return;
    CHECK(studio.pop_help != NULL);
    if (!studio.pop_help) return;
    for (i = 0; i < studio.pop_n; i++) {
        CHECK(studio.pop_help[i] != NULL);
        if (!studio.pop_help[i]) continue;
        CHECK(strlen(studio.pop_help[i]) <= 36);
        CHECK(ui_text_width(studio.pop_help[i]) + 16 <= 240 - 8);        /* the card is as wide as the longest help */
        CHECK(ui_text_width(studio.pop_items[i]) + 30 <= 240 - 8);
    }
    CHECK(ui_text_width(studio.pop_title) + 20 <= 240 - 8);
    CHECK(studio.pop_hw + 16 <= 240 - 8);
}

TEST(tools_every_menu_item_has_a_help_line_that_fits_the_card)
{
    open_notes();
    frames(1, BTN_SELECT); frames(1, 0);
    menu_has_help_that_fits("Pattern tools");
    CHECK_EQ(studio.pop_n, 10);
    pick(8);
    menu_has_help_that_fits("Range tools");
    press(BTN_B);
    tools(9, -1);
    menu_has_help_that_fits("Helper tools");
    CHECK_EQ(studio.pop_n, 7);
    pick(0);
    menu_has_help_that_fits("Next chord");
    pick(4);                                                  /* All chords... */
    menu_has_help_that_fits("All chords");
    press(BTN_B);
    tools(9, 1);
    menu_has_help_that_fits("Phrase");
    press(BTN_B);
    tools(9, 2);
    menu_has_help_that_fits("Groove");
    pick(8);
    menu_has_help_that_fits("More grooves");
    press(BTN_B);
    tools(9, 3);
    menu_has_help_that_fits("Harmony");
    press(BTN_B);
    st_set_tab(STAB_SONG); frames(2, 0); st_card_close(); frames(2, 0);
    frames(1, BTN_SELECT); frames(1, 0);
    menu_has_help_that_fits("Song tools");
    press(BTN_B);
    st_set_tab(STAB_ORDER); frames(2, 0);
    frames(1, BTN_SELECT); frames(1, 0);
    menu_has_help_that_fits("Arrange");
    press(BTN_B);
    st_set_tab(STAB_LYRICS); frames(2, 0);
    frames(1, BTN_SELECT); frames(1, 0);
    menu_has_help_that_fits("Lyric tools");
    press(BTN_B);
    st_set_tab(STAB_CHOIR); frames(2, 0);
    frames(1, BTN_SELECT); frames(1, 0);
    menu_has_help_that_fits("Quick fill");
    press(BTN_B);
}

TEST(tools_the_help_line_changes_with_the_highlighted_item_and_a_plain_menu_has_none)
{
    open_notes();
    frames(1, BTN_SELECT); frames(1, 0);
    CHECK(studio.pop_help != NULL);
    CHECK_EQ(studio.pop_sel, 0);
    CHECK(strstr(studio.pop_help[studio.pop_sel], "pattern") != NULL);
    press(BTN_DOWN);
    CHECK_EQ(studio.pop_sel, 1);
    CHECK(strstr(studio.pop_help[studio.pop_sel], "copied pattern") != NULL);
    press(BTN_B);
    st_popup("Plain", (const char *const[]){ "One", "Two" }, 2, 0, NULL);
    CHECK(studio.pop_help == NULL);                           /* a menu that gives none shows none (and no stale one) */
    press(BTN_B);
}

TEST(tools_drum_editor_puts_one_drum_per_step_in_its_lane_and_undoes_each_press)
{
    open_notes();
    tools(9, 5);                                              /* Helper tools: Drum editor */
    CHECK_EQ(studio.sub, SUB_DRUMS);
    CHECK(strstr(studio.msg, "Drum editor") != NULL);
    msg_fits();
    press(BTN_A);                                             /* lane 0 = Kick, step 1 */
    CHECK_EQ(studio.song.notes[0][3][0], SONG_DRUM_KICK);
    CHECK_EQ(ed_undo_steps(), 1);
    press(BTN_DOWN);                                          /* lane 1 = Snare */
    press(BTN_A);
    CHECK_EQ(studio.song.notes[0][3][0], SONG_DRUM_SNARE);    /* the step holds one drum: replaced */
    CHECK_EQ(ed_undo_steps(), 2);
    press(BTN_A);                                             /* the same lane again: off */
    CHECK_EQ(studio.song.notes[0][3][0], 0);
    press(BTN_RIGHT); press(BTN_RIGHT);
    press(BTN_A);
    CHECK_EQ(studio.stp, 2);
    CHECK_EQ(studio.song.notes[0][3][2], SONG_DRUM_SNARE);
    CHECK_EQ(studio.song.notes[0][0][2], 0);                  /* only the drums track is touched */
    press(BTN_R);                                             /* another pattern */
    CHECK_EQ(studio.pat, 1);
    press(BTN_A);
    CHECK_EQ(studio.song.notes[1][3][2], SONG_DRUM_SNARE);
    CHECK_EQ(studio.song.notes[0][3][2], SONG_DRUM_SNARE);
    CHECK(ed_undo());
    CHECK_EQ(studio.song.notes[1][3][2], 0);
    press(BTN_UP); press(BTN_UP);                             /* the lanes wrap: Snare up twice = Cow (the last) */
    CHECK_EQ(studio.lane, 7);
    press(BTN_L);
    CHECK_EQ(studio.pat, 0);
    press(BTN_B);                                             /* back to the grid */
    CHECK_EQ(studio.sub, SUB_NONE);
    press(BTN_A);                                             /* and A is a note again */
    CHECK(studio.song.notes[0][0][2] >= 2);
}

TEST(tools_drum_editor_menu_grooves_clears_and_repeats_with_one_step_each)
{
    int i, kicks = 0;
    open_notes();
    tools(9, 5);
    tools(1, -1);                                             /* Clear this lane: nothing there */
    CHECK_STR(studio.msg, "Nothing to change there");
    CHECK_EQ(ed_undo_steps(), 0);
    tools(0, 0);                                              /* Pick a groove: Pop */
    CHECK_EQ(studio.song.notes[0][3][0], SONG_DRUM_KICK);
    CHECK_EQ(ed_undo_steps(), 1);
    tools(1, -1);                                             /* the lane is Kick: its hits go */
    CHECK_STR(studio.msg, "Lane cleared");
    for (i = 0; i < SONG_STEPS; i++) kicks += studio.song.notes[0][3][i] == SONG_DRUM_KICK;
    CHECK_EQ(kicks, 0);
    CHECK(studio.song.notes[0][3][4] != 0);                   /* the snare stays */
    CHECK_EQ(ed_undo_steps(), 2);
    tools(3, -1);                                             /* Repeat bar 1 in bar 2 */
    CHECK_STR(studio.msg, "Bar 1 repeated in bar 2");
    for (i = 0; i < 16; i++) CHECK_EQ(studio.song.notes[0][3][16 + i], studio.song.notes[0][3][i]);
    tools(3, -1);
    CHECK_STR(studio.msg, "Nothing to change there");
    tools(2, -1);                                             /* Clear all drums */
    for (i = 0; i < SONG_STEPS; i++) CHECK_EQ(studio.song.notes[0][3][i], 0);
    CHECK_STR(studio.msg, "Drums cleared");
    CHECK_EQ(ed_undo_steps(), 4);
    for (i = 0; i < 4; i++) CHECK(ed_undo());
    for (i = 0; i < SONG_STEPS; i++) CHECK_EQ(studio.song.notes[0][3][i], 0);
    tools(4, -1);                                             /* Loop this bar */
    CHECK(strstr(studio.msg, "Looping bar 1") != NULL);
    CHECK(song_playing());
}

TEST(tools_the_screens_close_with_the_tab_and_keep_the_notes_grid_prompts)
{
    UiPrompt pr[UI_PROMPTS_MAX];
    int n;
    open_notes();
    n = tab_prompts(STAB_NOTES, pr);
    CHECK(n >= 5);
    tools(9, 6);                                              /* Chord track */
    CHECK_EQ(studio.sub, SUB_CHORDS);
    n = tab_prompts(STAB_NOTES, pr);
    CHECK(n >= 4 && n <= UI_PROMPTS_MAX);
    st_set_tab(STAB_PLAY);
    CHECK_EQ(studio.sub, SUB_NONE);                           /* a tab opens on its main screen */
    st_set_tab(STAB_NOTES);
    CHECK_EQ(studio.sub, SUB_NONE);
}

TEST(tools_chord_track_shows_every_part_picks_a_chord_and_undoes)
{
    open_notes();
    tools(9, 6);
    CHECK_EQ(studio.sub, SUB_CHORDS);
    press(BTN_RIGHT);                                         /* half bar 2 */
    CHECK_EQ(studio.stp, 8);
    press(BTN_A);                                             /* the chord menu: four that fit, all, clear */
    CHECK_EQ(st_popup_open(), 1);
    CHECK_EQ(studio.pop_n, 6);
    press(BTN_A);                                             /* the first: the tonic starts a song, but not in half bar 2 */
    CHECK(tl_chord_at(&studio.song, 0, 1) >= 0);
    CHECK_EQ(ed_undo_steps(), 1);
    CHECK(strstr(studio.msg, "half bar 2") != NULL);
    press(BTN_DOWN);                                          /* part B */
    CHECK_EQ(studio.pat, 1);
    CHECK_EQ(studio.stp, 8);
    press(BTN_A);
    pick(4);                                                  /* All chords... */
    CHECK_EQ(studio.pop_n, 7);
    CHECK_STR(studio.pop_items[0], "C");
    CHECK_STR(studio.pop_items[5], "Am");
    pick(5);
    CHECK_EQ(tl_chord_at(&studio.song, 1, 1), 5);
    CHECK_EQ(ed_undo_steps(), 2);
    press(BTN_A);                                             /* the same chord again: nothing to change */
    pick(4); pick(5);
    CHECK_EQ(ed_undo_steps(), 2);
    CHECK(strstr(studio.msg, "already there") != NULL);
    press(BTN_A);
    pick(5);                                                  /* Clear this chord */
    CHECK_EQ(tl_chord_at(&studio.song, 1, 1), -1);
    CHECK_STR(studio.msg, "Chord cleared");
    press(BTN_A);
    pick(5);
    CHECK_STR(studio.msg, "Nothing to change there");         /* clearing an empty half bar makes no step */
    CHECK_EQ(ed_undo_steps(), 3);
    CHECK(ed_undo()); CHECK(ed_undo()); CHECK(ed_undo());
    CHECK_EQ(tl_chord_at(&studio.song, 1, 1), -1);
    CHECK_EQ(tl_chord_at(&studio.song, 0, 1), -1);
    press(BTN_B);
    CHECK_EQ(studio.sub, SUB_NONE);
}

TEST(tools_chord_track_menu_repeats_clears_and_hears_the_part)
{
    int i;
    open_notes();
    tools(9, 6);
    tools(0, -1);                                             /* SELECT: Pick a chord (opens the chord menu) ... */
    press(BTN_A);                                             /* ... the first suggestion into half bar 1 */
    CHECK_EQ(tl_chord_at(&studio.song, 0, 0), 0);
    tools(2, -1);                                             /* Repeat to the end */
    for (i = 1; i < 4; i++) CHECK_EQ(tl_chord_at(&studio.song, 0, i), 0);
    CHECK_STR(studio.msg, "Chord repeated to the end");
    tools(2, -1);
    CHECK_STR(studio.msg, "Nothing to change there");
    tools(1, -1);                                             /* Clear this chord (half bar 1) */
    CHECK_EQ(tl_chord_at(&studio.song, 0, 0), -1);
    tools(3, -1);                                             /* Hear this part */
    CHECK(song_playing());
    CHECK(strstr(studio.msg, "Looping part A") != NULL);
}

TEST(tools_the_new_screens_draw_without_overflowing_a_narrow_row)
{
    int sc;
    for (sc = 0; sc < 2; sc++) {
        open_notes();
        studio.song.notes[0][3][0] = SONG_DRUM_KICK;
        tl_chord_write(&studio.song, 0, 0, 5);
        tl_chord_write(&studio.song, 1, 3, 6);
        tools(9, sc ? 6 : 5);
        frames(3, 0);
        studio.stp = 31;                                      /* the last step / the last half bar */
        studio.lane = 7;
        studio.pat = 3;
        frames(3, 0);
        CHECK_EQ(studio.sub, sc ? SUB_CHORDS : SUB_DRUMS);
        press(BTN_B);
    }
}

TEST(tools_the_screens_have_a_help_line_and_a_control_card_that_fit)
{
    int sub, wide;
    for (sub = SUB_DRUMS; sub <= SUB_CHORDS; sub++) {
        const char *h, *g;
        int x, y, w, hh;
        open_notes();
        tools(9, sub == SUB_DRUMS ? 5 : 6);
        CHECK_EQ(studio.sub, sub);
        h = st_tab_help(STAB_NOTES);
        CHECK(ui_text_width(h) + 16 <= 240 - 8);
        studio.sub = SUB_NONE;
        g = st_tab_help(STAB_NOTES);
        CHECK(strcmp(h, g) != 0);                             /* the notes grid keeps its own line */
        studio.sub = (uint8_t)sub;
        frames(1, BTN_SELECT | BTN_UP); frames(2, 0);
        CHECK(st_help_is_open());
        (void)st_help_rect(&x, &y, &w, &hh);
        for (wide = 0; wide < 1; wide++) CHECK(y >= 0 && y + hh <= 160 && w <= 240);   /* the card on the GBA screen */
        press(BTN_A);
        CHECK(!st_help_is_open());
    }
}

/* ---- ST-ADV: the Advanced ORDER screen's helpers and its place in the studio (undo, take, save) -------------------- */
TEST(adv_from_order_plays_like_the_classic_order)
{
    DoomoSong s;
    SongExt x;
    int i, sec, pass;
    fresh(&s);
    s.order_len = 11;
    for (i = 0; i < 11; i++) s.order[i] = (uint8_t)((i * 3 + 1) & 3);
    songx_from_order(&x, &s);
    songx_sanitize(&x);                                        /* (fills the unused spare parts) */
    CHECK_EQ(songx_sanitize(&x), 0);                           /* ... and then nothing is left to fix */
    CHECK_EQ(x.sections, 3);                                   /* 4 + 4 + 3 passes */
    CHECK_EQ(songx_len(&x), 11);
    for (i = 0; i < 11; i++) {
        CHECK_EQ(songx_at(&x, i, &sec, &pass), s.order[i]);
        CHECK_EQ(x.sect[sec].line[pass & 3], SONG_LINE_OF(i));  /* the same lyric line */
    }
    CHECK_EQ(songx_ticks(&s, &x), song_length_ticks(&s));
}

TEST(adv_arrangement_insert_remove_and_entry_passes)
{
    DoomoSong s;
    SongExt x;
    int n;
    fresh(&s);
    s.order_len = 8;
    songx_from_order(&x, &s);                                  /* two parts of 4 passes */
    CHECK_EQ(songx_entry_pass(&x, 1), 4);
    CHECK_EQ(songx_entry_of_pass(&x, 3), 0);
    CHECK_EQ(songx_entry_of_pass(&x, 4), 1);
    CHECK(songx_arrange_insert(&x, 1, 0));                     /* part 0 again, between */
    CHECK_EQ(x.arrange_len, 3);
    CHECK_EQ(songx_entry_pass(&x, 2), 8);
    CHECK_EQ(songx_sect_users(&x, 0), 2);
    n = songx_section_dup(&x, 1);
    CHECK_EQ(n, 2);
    CHECK_EQ(x.sect[2].npat, x.sect[1].npat);
    CHECK(songx_arrange_remove(&x, 2));                        /* the last entry (part 1) goes... */
    CHECK_EQ(x.arrange_len, 2);
    CHECK_EQ(songx_sect_users(&x, 1), 0);
    CHECK(x.sections >= 2);
    CHECK(songx_arrange_remove(&x, 0));
    CHECK(!songx_arrange_remove(&x, 0));                       /* never the last entry */
    CHECK_EQ(x.arrange_len, 1);
    songx_sanitize(&x);
    CHECK_EQ(songx_sanitize(&x), 0);
    CHECK_EQ(songx_len(&x), x.sect[x.arrange[0]].npat);
}

#if STUDIO_ADV
TEST(adv_order_tab_opens_the_arrangement_once_and_undo_reaches_it)
{
    int key0;
    open_notes();
    studio.song.order_len = 8;
    st_level_set(1);
    studio.dirty = 0;
    CHECK(!st_adv_on());
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    CHECK(st_adv_on());
    CHECK_EQ(studio.dirty, 0);                                 /* opening is not an edit */
    CHECK_EQ(songx_len(&studio.xw), 8);
    key0 = studio.xw.sect[0].key;
    ed_touch(&studio.xw.sect[0].key, 1);                       /* an edit of the arrangement is one undo step */
    studio.xw.sect[0].key = 3;
    ed_commit();
    CHECK(ed_undo());
    CHECK_EQ(studio.xw.sect[0].key, key0);
    CHECK(ed_redo());
    CHECK_EQ(studio.xw.sect[0].key, 3);
    st_set_tab(STAB_NOTES);                                    /* leaving and coming back keeps it */
    st_set_tab(STAB_ORDER);
    CHECK(st_adv_on());
    CHECK_EQ(studio.xw.sect[0].key, 3);
    st_level_set(0);
}

TEST(adv_a_take_drops_the_arrangement_in_the_same_undo_step)
{
    open_notes();
    st_level_set(1);
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    CHECK(st_adv_on());
    ed_reset();
    ed_touch(SS, (int)sizeof(DoomoSong));
    st_adv_take();
    ed_commit();
    CHECK(!st_adv_on());
    CHECK(ed_undo());                                          /* one undo brings the arrangement back */
    CHECK(st_adv_on());
    st_level_set(0);
}

TEST(adv_the_page_is_saved_with_the_song_and_loads_back)
{
    DoomoSong *dst = &doomo_song_book[2];
    int key;
    open_notes();
    st_level_set(1);
    studio.song.order_len = 6;
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    ed_touch(&studio.xw.sect[0].key, 1);
    studio.xw.sect[0].key = 2;
    ed_commit();
    *dst = studio.song;
    dst->flags |= SONGF_VALID | SONGF_CUSTOM;
    CHECK(st_adv_saved(2));
    CHECK(songx_book_has(2));
    studio.xw.version = 0;                                     /* as after a new song */
    st_adv_enter(2);
    CHECK(st_adv_on());
    key = studio.xw.sect[0].key;
    CHECK_EQ(key, 2);
    st_adv_enter(-1);                                          /* a song not from the book is classic */
    CHECK(!st_adv_on());
    songx_book_drop(2);
    CHECK(st_adv_saved(2));                                    /* a classic song saves fine and keeps no page */
    CHECK(!songx_book_has(2));
    st_level_set(0);
}
#endif

#if STUDIO_ADV
/* ---- ST-ADV: the Start card's Epic length (64 bars) and the FILE fold line ------------------------------------------- */
TEST(adv_start_card_offers_epic_only_on_advanced)
{
    open_notes();
    st_level_set(0);
    st_set_tab(STAB_SONG);
    st_card_open();
    st_easy()->csel = 3;                                       /* Length */
    st_easy()->length = SLEN_LONG;
    press(BTN_RIGHT);
    CHECK_EQ(st_easy()->length, SLEN_SHORT);                   /* Easy stops at Long and wraps */
    st_level_set(1);
    st_easy()->length = SLEN_LONG;
    press(BTN_RIGHT);
    CHECK_EQ(st_easy()->length, SLEN_EPIC);
    press(BTN_RIGHT);
    CHECK_EQ(st_easy()->length, SLEN_SHORT);
    st_card_close();
    st_level_set(0);
}

TEST(adv_epic_make_lays_out_64_bars_and_undo_takes_it_back_in_two_steps)
{
    DoomoSong before;
    open_notes();
    st_level_set(1);
    ed_reset();
    before = studio.song;
    st_set_tab(STAB_SONG);
    st_card_open();
    st_easy()->length = SLEN_EPIC;
    st_easy()->csel = 5;                                       /* Make it! */
    press(BTN_A);
    CHECK(st_adv_on());
    CHECK_EQ(studio.song.order_len, SONG_ORDER_MAX);
    CHECK_EQ(songx_len(&studio.xw), 32);                       /* 32 passes of 2 bars = 64 bars */
    CHECK_EQ(studio.xw.arrange_len, 8);
    CHECK(st_adv_ticks() > song_length_ticks(&studio.song));   /* longer than the classic 16 passes */
    CHECK(ed_undo());                                          /* first the arrangement ... */
    CHECK(!st_adv_on());
    CHECK(ed_undo());                                          /* ... then the take */
    CHECK(memcmp(&studio.song, &before, sizeof before) == 0);
    CHECK(ed_redo());
    CHECK(ed_redo());
    CHECK(st_adv_on());
    CHECK_EQ(songx_len(&studio.xw), 32);
    song_stop();
    st_level_set(0);
}

TEST(adv_epic_on_easy_falls_back_to_long)
{
    open_notes();
    st_level_set(1);
    st_set_tab(STAB_SONG);
    st_card_open();
    st_easy()->length = SLEN_EPIC;
    st_level_set(0);                                           /* switched back before Make it! */
    st_easy()->csel = 5;
    press(BTN_A);
    CHECK(!st_adv_on());
    CHECK_EQ(studio.song.order_len, SONG_ORDER_MAX);
    song_stop();
}

TEST(adv_lyric_lines_follow_the_arrangement)
{
    int line, pass0;
    open_notes();
    st_level_set(1);
    studio.song.order_len = 8;
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    CHECK(st_adv_on());
    /* from_order: lyric line k is over pass k, so the arrangement answers the same as the classic order */
    for (line = 0; line < 8; line++) {
        CHECK_EQ(studio_line_pos(&studio.song, line), line);
        CHECK_EQ(studio_line_pat(&studio.song, line), studio.song.order[line] & 3);
    }
    /* move line 5 to the first pass of part 0: the line is now sung over that part's first pattern, on pass 0 */
    studio.xw.sect[studio.xw.arrange[0]].line[0] = 5;
    studio.xw.sect[studio.xw.arrange[0]].line[1] = SX_NO_LINE;
    pass0 = studio_line_pos(&studio.song, 5);
    CHECK_EQ(pass0, 0);
    CHECK_EQ(studio_line_pat(&studio.song, 5), st_adv_line_pattern(5));
    CHECK_EQ(studio_line_pos(&studio.song, 1), -1 + (st_adv_line_pass(1) + 1));   /* line 1 may be sung elsewhere or not at all */
    /* a classic song (another struct) is untouched by the open arrangement */
    {
        DoomoSong c = studio.song;
        CHECK_EQ(studio_line_pos(&c, 5), 5);
    }
    st_level_set(0);
}

TEST(adv_file_fold_message_only_when_tracks_exceed_the_console)
{
    char buf[96];
    open_notes();
    st_level_set(1);
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    CHECK(st_adv_on());
    CHECK_EQ(st_adv_folded(buf, (int)sizeof(buf)), 0);         /* 4 tracks: nothing to say */
    studio.xw.tracks = 8;
    CHECK_EQ(st_adv_folded(buf, (int)sizeof(buf)), 8 - songx_tracks_max());
    CHECK(strstr(buf, "mixed in") != NULL);
    studio.xw.tracks = 4;
    st_level_set(0);
}
#endif

#if STUDIO_ADV
/* ---- ST-ADV: the screen's own keys ---------------------------------------------------------------------------------- */
static void open_adv(int order_len)
{
    open_notes();
    studio.song.order_len = (uint8_t)order_len;
    st_level_set(1);
    st_set_tab(STAB_ORDER);
    frames(2, 0);
    studio.osel = 0;
}

TEST(adv_keys_insert_a_copy_unique_and_delete_are_undoable_steps)
{
    int sec0;
    open_adv(8);
    CHECK(st_adv_on());
    CHECK_EQ(studio.xw.arrange_len, 2);
    press(BTN_SELECT); pick(1);                                /* Insert a copy */
    CHECK_EQ(studio.xw.arrange_len, 3);
    CHECK_EQ(studio.osel, 1);
    CHECK_EQ(studio.xw.arrange[0], studio.xw.arrange[1]);
    sec0 = studio.xw.sections;
    press(BTN_SELECT); pick(2);                                /* Make it unique: the copy gets its own part */
    CHECK_EQ(studio.xw.sections, sec0 + 1);
    CHECK(studio.xw.arrange[0] != studio.xw.arrange[1]);
    CHECK(ed_undo());
    CHECK_EQ(studio.xw.sections, sec0);
    CHECK(ed_undo());
    CHECK_EQ(studio.xw.arrange_len, 2);
    press(BTN_SELECT); pick(4);                                /* Delete */
    CHECK_EQ(studio.xw.arrange_len, 1);
    press(BTN_SELECT); pick(4);                                /* the last entry stays */
    CHECK_EQ(studio.xw.arrange_len, 1);
    st_level_set(0);
}

TEST(adv_keys_part_view_edits_the_part_and_b_goes_back_not_out)
{
    int k0, t0;
    open_adv(4);
    k0 = studio.xw.sect[0].key;
    t0 = studio.xw.sect[0].tempo;
    press(BTN_A);                                              /* a tap opens the part */
    CHECK_EQ(studio.aview, 1);
    press(BTN_DOWN);                                           /* the key row */
    press(BTN_RIGHT); press(BTN_RIGHT);
    CHECK_EQ(studio.xw.sect[0].key, k0 + 2);
    press(BTN_DOWN);                                           /* the tempo row: steps of 5 % */
    press(BTN_RIGHT);
    CHECK_EQ(studio.xw.sect[0].tempo, t0 + 5);
    press(BTN_LEFT); press(BTN_LEFT); press(BTN_LEFT);
    CHECK_EQ(studio.xw.sect[0].tempo, t0 - 10);
    press(BTN_B);
    CHECK_EQ(studio.aview, 0);                                 /* back to the list ... */
    CHECK_EQ(studio.tab, STAB_ORDER);                          /* ... and still in the studio on the same tab */
    CHECK(st_adv_on());
    CHECK(ed_undo());                                          /* nudges of one setting made one step each */
    CHECK_EQ(studio.xw.sect[0].tempo, t0);
    st_level_set(0);
}

TEST(adv_keys_the_key_menu_and_the_pass_rows)
{
    int n;
    open_adv(4);
    press(BTN_SELECT); pick(5);                                /* Key change... */
    pick(2);                                                   /* Up 2 */
    CHECK_EQ(studio.xw.sect[0].key, 2);
    press(BTN_SELECT); pick(5);
    pick(6);                                                   /* Down 1 */
    CHECK_EQ(studio.xw.sect[0].key, -1);
    n = studio.xw.sect[0].npat;
    press(BTN_A);                                              /* the part view */
    press(BTN_DOWN); press(BTN_DOWN); press(BTN_DOWN); press(BTN_DOWN);   /* name, key, tempo, fade, then the first pass */
    press(BTN_RIGHT);                                          /* its pattern becomes the next one (A -> B) */
    CHECK_EQ(studio.xw.sect[0].pat[0], (studio.song.order[0] + 1) % SONG_PATTERNS);
    press(BTN_A);                                              /* A on a pass adds a copy after it */
    CHECK_EQ(studio.xw.sect[0].npat, n + 1);
    press(BTN_SELECT); pick(1);                                /* Delete this pass */
    CHECK_EQ(studio.xw.sect[0].npat, n);
    st_level_set(0);
}

TEST(adv_mix_view_edits_pan_sound_swing_and_brightness_one_undo_step_each)
{
    open_adv(4);
    press(BTN_A);                                              /* the part view, then its tools: Mix and sound... */
    press(BTN_SELECT); pick(8);
    CHECK_EQ(studio.aview, 2);
    CHECK_EQ(studio.xw.trk[0].pan, 1);
    press(BTN_RIGHT);                                          /* pan: middle -> right */
    CHECK_EQ(studio.xw.trk[0].pan, 2);
    press(BTN_RIGHT);                                          /* stays at the edge */
    CHECK_EQ(studio.xw.trk[0].pan, 2);
    press(BTN_A);                                              /* A moves to the Sound column */
    press(BTN_RIGHT);
    CHECK_EQ(studio.xw.trk[0].env, 1);
    press(BTN_LEFT); press(BTN_LEFT);                          /* wraps: 0 -> 3 */
    CHECK_EQ(studio.xw.trk[0].env, 3);
    press(BTN_A);                                              /* Swing column */
    press(BTN_DOWN);                                           /* harmony track */
    press(BTN_RIGHT); press(BTN_RIGHT);
    CHECK_EQ(studio.xw.trk[1].swing, 2);
    press(BTN_LEFT); press(BTN_LEFT); press(BTN_LEFT);         /* clamps at 0 = the song's own */
    CHECK_EQ(studio.xw.trk[1].swing, 0);
    press(BTN_DOWN); press(BTN_DOWN); press(BTN_DOWN);         /* the brightness row of the part */
    press(BTN_RIGHT);
    CHECK_EQ(studio.xw.sect[0].bright, 1);
    press(BTN_B);
    CHECK_EQ(studio.aview, 0);                                 /* back to the list, still in the studio */
    CHECK_EQ(studio.tab, STAB_ORDER);
    CHECK(ed_undo());                                          /* brightness */
    CHECK_EQ(studio.xw.sect[0].bright, 0);
    CHECK(ed_undo());                                          /* the swing nudges joined into steps; one more takes it back */
    CHECK(ed_undo());
    st_level_set(0);
}

TEST(adv_mix_settings_are_saved_with_the_page)
{
    uint8_t buf[2048];
    SongExt back;
    uint32_t n;
    open_adv(4);
    press(BTN_A);
    press(BTN_SELECT); pick(8);
    press(BTN_LEFT);                                           /* pan left */
    press(BTN_A); press(BTN_RIGHT); press(BTN_RIGHT);          /* sound: long */
    CHECK_EQ(studio.xw.trk[0].pan, 0);
    CHECK_EQ(studio.xw.trk[0].env, 2);
    n = songx_encode(&studio.song, &studio.xw, buf, sizeof(buf));
    CHECK(n > 0);
    memset(&back, 0, sizeof(back));
    CHECK(songx_decode(&studio.song, &back, buf, n));
    CHECK_EQ(back.trk[0].pan, 0);
    CHECK_EQ(back.trk[0].env, 2);
    st_level_set(0);
}

TEST(adv_expert_view_free_notes_switch_is_one_undo_step_and_stops_the_scale_snap)
{
    open_adv(4);
    CHECK(!st_adv_free());
    press(BTN_A);                                              /* the part view, its tools: Expert view... */
    press(BTN_SELECT); pick(9);
    CHECK_EQ(studio.aview, 3);
    press(BTN_A);                                              /* the switch */
    CHECK(st_adv_free());
    CHECK(studio.xw.flags & SX_FLAG_FREE);
    press(BTN_RIGHT);                                          /* and off again */
    CHECK(!st_adv_free());
    press(BTN_A);
    CHECK(st_adv_free());
    press(BTN_B);
    CHECK_EQ(studio.aview, 1);                                 /* back to the part, not out of the studio */
    CHECK_EQ(studio.tab, STAB_ORDER);
    CHECK(ed_undo());
    CHECK(!st_adv_free());
    st_level_set(0);
}

TEST(adv_expert_free_flag_is_saved_with_the_page)
{
    uint8_t buf[2048];
    SongExt back;
    uint32_t n;
    open_adv(4);
    press(BTN_A);
    press(BTN_SELECT); pick(9);
    press(BTN_A);
    n = songx_encode(&studio.song, &studio.xw, buf, sizeof(buf));
    CHECK(n > 0);
    memset(&back, 0, sizeof(back));
    CHECK(songx_decode(&studio.song, &back, buf, n));
    CHECK(back.flags & SX_FLAG_FREE);
    st_level_set(0);
}

/* ---- ST-ADV: the Note grid (Expert view, "Note grid") ------------------------------------------------------------------ */
static void open_grid(void)
{
    open_adv(4);
    press(BTN_A);                                              /* the part view, its tools: Notes, expert... */
    press(BTN_SELECT); pick(9);
    press(BTN_DOWN);                                           /* the Note grid row */
    press(BTN_A);
}

static int grid_pat(void) { return studio.xw.sect[studio.xw.arrange[0]].pat[0]; }
static int gcell(int pat, int trk, int step) { return songx_cell(&studio.song, &studio.xw, pat, trk, step); }

TEST(adv_note_grid_opens_from_expert_and_places_and_takes_off_a_note_one_undo_step_each)
{
    int p;
    open_grid();
    CHECK_EQ(studio.aview, 4);
    CHECK_EQ(studio.tab, STAB_ORDER);
    p = grid_pat();
    CHECK_EQ(gcell(p, 0, 0), 0);
    press(BTN_A);                                              /* the lead's pitch, on step 1 */
    CHECK_EQ(gcell(p, 0, 0), 72);
    CHECK_EQ(studio.song.notes[p][0][0], 72);                  /* a classic cell is the song's own */
    press(BTN_A);                                              /* the same pitch again: off */
    CHECK_EQ(gcell(p, 0, 0), 0);
    CHECK(ed_undo());
    CHECK_EQ(gcell(p, 0, 0), 72);
    CHECK(ed_undo());
    CHECK_EQ(gcell(p, 0, 0), 0);
    press(BTN_B);
    CHECK_EQ(studio.aview, 3);                                 /* back to Expert, not out of the studio */
    CHECK_EQ(studio.tab, STAB_ORDER);
    st_level_set(0);
}

TEST(adv_note_grid_pitch_follows_the_scale_unless_free_notes_is_on)
{
    int p;
    open_grid();
    p = grid_pat();
    press(BTN_UP);                                             /* C major: 72 -> 74 */
    press(BTN_A);
    CHECK_EQ(gcell(p, 0, 0), 74);
    press(BTN_RIGHT);
    studio.xw.flags |= SX_FLAG_FREE;
    press(BTN_UP);                                             /* free: one semitone */
    press(BTN_A);
    CHECK_EQ(gcell(p, 0, 1), 75);
    st_level_set(0);
}

TEST(adv_note_grid_extra_track_uses_a_block_and_undo_gives_it_back)
{
    int p;
    open_grid();
    p = grid_pat();
    CHECK_EQ(studio.xw.tracks, 4);
    press(BTN_SELECT); pick(7);                                /* Add a track */
    CHECK_EQ(studio.xw.tracks, 5);
    press(BTN_A);                                              /* a note on the new track (the cursor is on it) */
    CHECK_EQ(studio.xw.nblocks, 1);
    CHECK(gcell(p, 4, 0) >= 2);
    CHECK(studio.xw.map[p][4] == 1);
    press(BTN_A);                                              /* taken off again: its block is given back */
    CHECK_EQ(gcell(p, 4, 0), 0);
    CHECK_EQ(studio.xw.nblocks, 0);
    CHECK_EQ(studio.xw.map[p][4], 0);
    CHECK(ed_undo());                                          /* ... and undo puts it back whole */
    CHECK_EQ(studio.xw.nblocks, 1);
    CHECK(gcell(p, 4, 0) >= 2);
    CHECK(ed_undo());
    CHECK_EQ(studio.xw.nblocks, 0);
    CHECK_EQ(gcell(p, 4, 0), 0);
    st_level_set(0);
}

TEST(adv_note_grid_a_track_with_notes_cannot_be_taken_away)
{
    open_grid();
    press(BTN_SELECT); pick(7);
    press(BTN_A);
    press(BTN_SELECT); pick(8);                                /* Take the last track away: it has a note */
    CHECK_EQ(studio.xw.tracks, 5);
    press(BTN_A);                                              /* empty it */
    press(BTN_SELECT); pick(8);
    CHECK_EQ(studio.xw.tracks, 4);
    st_level_set(0);
}

TEST(adv_note_grid_new_pattern_copies_every_track_and_a_pass_can_pick_it)
{
    uint8_t buf[2048];
    SongExt back;
    uint32_t n;
    int p, c0, c4, bl;
    open_grid();
    p = grid_pat();
    press(BTN_A);                                              /* lead, step 1 */
    press(BTN_SELECT); pick(7);                                /* a fifth track */
    press(BTN_RIGHT);
    press(BTN_A);                                              /* a note on it, step 2 */
    c0 = gcell(p, 0, 0); c4 = gcell(p, 4, 1);
    bl = studio.xw.nblocks;
    press(BTN_SELECT); pick(2);                                /* New pattern (copy) */
    CHECK_EQ(studio.xw.patterns, 5);
    CHECK_EQ(gcell(4, 0, 0), c0);
    CHECK_EQ(gcell(4, 4, 1), c4);
    CHECK_EQ(studio.xw.nblocks, bl + 2);                       /* the copy lives wholly in the pool */
    press(BTN_B); press(BTN_B);                                /* Expert, then the part */
    CHECK_EQ(studio.aview, 1);
    studio.asel = 4;                                           /* pass 1 */
    for (n = 0; n < 5; n++) press(BTN_RIGHT);                  /* A B C D E: it can pick the new pattern */
    CHECK_EQ(studio.xw.sect[studio.xw.arrange[0]].pat[0], p == 0 ? 0 : (p + 5) % 5);
    n = songx_encode(&studio.song, &studio.xw, buf, sizeof(buf));
    CHECK(n > 0);
    memset(&back, 0, sizeof(back));
    CHECK(songx_decode(&studio.song, &back, buf, n));
    CHECK_EQ(back.patterns, 5);
    CHECK_EQ(songx_cell(&studio.song, &back, 4, 4, 1), c4);
    CHECK_EQ(songx_cell(&studio.song, &back, 4, 0, 0), c0);
    CHECK(ed_undo());                                          /* the pass change, then the new pattern */
    CHECK(ed_undo());
    CHECK_EQ(studio.xw.patterns, 4);
    CHECK_EQ(studio.xw.nblocks, bl);
    st_level_set(0);
}

TEST(adv_note_grid_drum_lanes_tie_and_octave)
{
    int p;
    open_grid();
    p = grid_pat();
    press(BTN_R); press(BTN_R); press(BTN_R);                  /* L / R pick the track: Drums */
    press(BTN_A);
    CHECK_EQ(gcell(p, 3, 0), SONG_DRUM_KICK);
    press(BTN_DOWN);                                           /* the next lane: snare */
    press(BTN_RIGHT);
    press(BTN_A);
    CHECK_EQ(gcell(p, 3, 1), SONG_DRUM_SNARE);
    press(BTN_SELECT); pick(5);                                /* drums have no octaves: nothing changes */
    CHECK_EQ(gcell(p, 3, 1), SONG_DRUM_SNARE);
    press(BTN_L); press(BTN_L); press(BTN_L);                  /* back to the lead */
    press(BTN_LEFT);
    press(BTN_A);                                              /* 72 on step 1 */
    press(BTN_RIGHT);
    press(BTN_SELECT); pick(3);                                /* Tie to the note before */
    CHECK_EQ(gcell(p, 0, 1), SONG_HOLD);
    press(BTN_SELECT); pick(3);                                /* again: the tie comes off */
    CHECK_EQ(gcell(p, 0, 1), 0);
    press(BTN_SELECT); pick(5);                                /* Octave up */
    CHECK_EQ(gcell(p, 0, 0), 84);
    press(BTN_SELECT); pick(6);
    CHECK_EQ(gcell(p, 0, 0), 72);
    press(BTN_SELECT); pick(4);                                /* Clear this track here */
    CHECK_EQ(gcell(p, 0, 0), 0);
    CHECK(ed_undo());
    CHECK_EQ(gcell(p, 0, 0), 72);
    st_level_set(0);
}

TEST(adv_note_grid_removing_a_note_takes_its_ties_with_it)
{
    int p;
    open_grid();
    p = grid_pat();
    press(BTN_A);
    press(BTN_RIGHT);
    press(BTN_SELECT); pick(3);
    press(BTN_RIGHT);
    press(BTN_SELECT); pick(3);
    CHECK_EQ(gcell(p, 0, 1), SONG_HOLD);
    CHECK_EQ(gcell(p, 0, 2), SONG_HOLD);
    press(BTN_LEFT); press(BTN_LEFT);
    press(BTN_A);                                              /* off: the ties would ring on nothing */
    CHECK_EQ(gcell(p, 0, 0), 0);
    CHECK_EQ(gcell(p, 0, 1), 0);
    CHECK_EQ(gcell(p, 0, 2), 0);
    CHECK(ed_undo());                                          /* one step brings all three back */
    CHECK_EQ(gcell(p, 0, 0), 72);
    CHECK_EQ(gcell(p, 0, 2), SONG_HOLD);
    st_level_set(0);
}

TEST(adv_note_grid_a_full_pool_refuses_a_note_and_says_so)
{
    open_grid();
    press(BTN_SELECT); pick(7);
    studio.xw.nblocks = SX_POOL;                               /* every block taken */
    press(BTN_A);
    CHECK_EQ(gcell(grid_pat(), 4, 0), 0);
    CHECK_EQ(studio.xw.nblocks, SX_POOL);
    CHECK(!studio.undo_open);
    studio.xw.nblocks = 0;
    st_level_set(0);
}

TEST(adv_note_grid_a_pass_on_a_new_pattern_plays_its_notes)
{
    open_grid();
    press(BTN_A);                                              /* lead 72 on step 1 of the part's pattern */
    press(BTN_SELECT); pick(2);                                /* a copy */
    press(BTN_LEFT); press(BTN_LEFT);                          /* (no-op: cursor only) */
    press(BTN_B); press(BTN_B);
    studio.asel = 4;
    press(BTN_LEFT);                                           /* A -> the last pattern (E) */
    CHECK_EQ(studio.xw.sect[studio.xw.arrange[0]].pat[0], 4);
    studio.song.notes[0][0][0] = 0;                            /* the classic cell is empty: it can only sound from the pool */
    studio.song.notes[studio.xw.sect[0].pat[0] == 4 ? 0 : 0][0][0] = 0;
    CHECK_EQ(gcell(4, 0, 0), 72);
    st_adv_play(0, -1);
    frames(3, 0);
    CHECK(song_playing());
    CHECK_EQ(song_pos_note(0), 72);
    song_stop();
    st_level_set(0);
}

TEST(adv_keys_start_plays_the_arrangement)
{
    open_adv(8);
    press(BTN_SELECT); pick(1);                                /* three entries: 12 passes, the classic order has 8 */
    CHECK_EQ(songx_len(&studio.xw), 12);
    press(BTN_START);
    frames(3, 0);
    CHECK(song_playing());
    song_stop();
    st_level_set(0);
}
#endif

#if STUDIO_ADV
/* ---- ST-ADV: PERFORM plays the arrangement ---------------------------------------------------------------------------- */
TEST(adv_perform_line_fit_follows_the_arrangement)
{
    open_adv(4);
    CHECK_EQ(st_adv_line_pattern(0), studio.song.order[0] & 3);      /* from_order: line k over pass k */
    studio.xw.sect[0].pat[2] = 3;
    studio.xw.sect[0].line[2] = 5;                                     /* the arrangement sings line 5 over pattern D */
    CHECK_EQ(st_adv_line_pattern(5), 3);
    studio.xw.sect[0].pat[1] = 9;                                      /* a pattern outside the classic four */
    studio.xw.sect[0].line[1] = 6;
    CHECK_EQ(st_adv_line_pattern(6), -1);
    CHECK_EQ(st_adv_line_pattern(7), -1);                              /* never sung */
    st_level_set(0);
}

TEST(adv_perform_length_is_the_arrangements_not_the_classic_orders)
{
    open_adv(4);
    CHECK_EQ(st_adv_ticks(), song_length_ticks(SS));
    studio.xw.sect[0].tempo = 200;                                     /* twice as fast */
    CHECK(st_adv_ticks() < song_length_ticks(SS) * 6 / 10);
    CHECK(st_adv_ext() != NULL);
    st_adv_enter(-1);
    CHECK(st_adv_ext() == NULL);
    st_level_set(0);
}

TEST(adv_perform_show_runs_the_arrangement_and_ends_with_it)
{
    int n = 0, playing_at = -1;
    open_adv(2);
    studio.xw.sect[0].npat = 1;                                        /* the arrangement is one pass, the classic order two */
    studio.xw.arrange_len = 1;
    st_set_tab(STAB_STAGE);
    frames(2, 0);
    press(BTN_A);                                                      /* the curtain goes up */
    CHECK_EQ(studio.show, 1);
    while (studio.show == 1 && n < 600) {
        frames(1, 0);
        n++;
        if (playing_at < 0 && song_playing()) playing_at = n;
    }
    CHECK(playing_at > 0);
    CHECK_EQ(studio.show, 2);                                          /* it finished (bows) */
    CHECK((uint32_t)(n - playing_at) <= st_adv_ticks() + 20);          /* ... when the arrangement ended, not the classic order */
    CHECK((uint32_t)(n - playing_at) < song_length_ticks(SS));
    st_stop();
    st_level_set(0);
}
#endif
