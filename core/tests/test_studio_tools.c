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
