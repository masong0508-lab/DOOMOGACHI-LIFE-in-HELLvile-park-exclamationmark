/* studio_easy.c - ST-EASY: START plays, and says what it plays; the cursor's cell described (owner: studio).
 *
 * START on every tab plays, and a second press stops. The prompt bar names the target:
 *   "Play from here"  NOTES (the pattern from the cursor step, looping), ORDER (from the selected pass),
 *                     LYRICS (from the pass that sings the selected line)
 *   "Play song"       every other tab (and a LYRICS line that no pass sings)
 *   "Stop"            while anything plays
 * One function decides the target (target()), so the words and the action cannot drift apart.
 *
 * st_describe_text() is NOTES' one-line description of the cell under the cursor, e.g.
 *   "Bar 2, beat 3, Lead: E4 (3rd of C)"; it falls back to shorter forms until it fits maxw pixels.
 */
#include "studio_int.h"

enum { TG_SONG, TG_PATTERN, TG_ORDER };

static int target(int *pos)
{
    *pos = 0;
    switch (studio.tab) {
    case STAB_NOTES: return TG_PATTERN;
    case STAB_ORDER: *pos = studio.osel; return TG_ORDER;
    case STAB_LYRICS: {
        int p = studio_line_pos(SS, studio.lsel);
        if (p >= 0) { *pos = p; return TG_ORDER; }
        return TG_SONG;
    }
    default: return TG_SONG;
    }
}

const char *st_play_verb(void)
{
    int pos;
    if (song_playing()) return "Stop";
    return target(&pos) == TG_SONG ? "Play song" : "Play from here";
}

void st_start(void)
{
    int pos, k;
    if (song_playing()) {
        song_stop();
        audio_sfx(SFX_CANCEL);
        return;
    }
    k = target(&pos);
    if (k == TG_PATTERN) song_play_pattern(SS, studio.pat, studio.stp);
    else song_play_at(SS, -1, pos, 0);
}

/* ---- the cursor's cell ----------------------------------------------------------------------------- */
static const char *const track_name[SONG_TRACKS] = { "Lead", "Harmony", "Bass", "Drums" };
static const char *const interval_name[12] = {
    "root", "flat 2nd", "2nd", "flat 3rd", "3rd", "4th", "flat 5th", "5th", "flat 6th", "6th", "flat 7th", "7th"
};

int st_describe_text(char *out, int cap, int maxw, int pat, int trk, int stp)
{
    const DoomoSong *s = SS;
    int v, bar, beat, k;
    char what[8], note[28], full[STUDIO_DESC_LEN];
    const char *tn;
    pat &= 3; trk &= 3; stp &= 31;
    v = s->notes[pat][trk][stp];
    bar = stp / 16 + 1;
    beat = (stp % 16) / 4 + 1;
    tn = track_name[trk];
    what[0] = 0;
    note[0] = 0;
    if (trk == 3) {
        if (v > 0 && v < SONG_DRUM_COUNT) doomo_snprintf(what, sizeof(what), "%s", doomo_song_drum_names[v]);
        else doomo_snprintf(what, sizeof(what), "rest");
    } else if (v >= 2) {
        int iv = ((v - s->key) % 12 + 12) % 12;
        song_note_name(v, what);
        if (song_scale_snap(s, v) == v)
            doomo_snprintf(note, sizeof(note), "%s of %s", interval_name[iv], doomo_song_key_names[s->key % 12]);
        else
            doomo_snprintf(note, sizeof(note), "off the scale");
    } else {
        doomo_snprintf(what, sizeof(what), v == SONG_HOLD ? "hold" : "rest");
    }
    /* longest form first; each shorter one drops something */
    for (k = 0; k < 5; k++) {
        switch (k) {
        case 0: doomo_snprintf(full, sizeof(full), note[0] ? "Bar %d, beat %d, %s: %s (%s)" : "Bar %d, beat %d, %s: %s%s",
                               bar, beat, tn, what, note); break;
        case 1: {
            /* without "of C" */
            char *of = strstr(note, " of ");
            if (of) *of = 0;
            doomo_snprintf(full, sizeof(full), note[0] ? "Bar %d, beat %d, %s: %s (%s)" : "Bar %d, beat %d, %s: %s%s",
                           bar, beat, tn, what, note);
            break; }
        case 2: doomo_snprintf(full, sizeof(full), note[0] ? "Bar %d.%d %s %s (%s)" : "Bar %d.%d %s %s%s",
                               bar, beat, tn, what, note); break;
        case 3: doomo_snprintf(full, sizeof(full), note[0] ? "%s %s (%s)" : "%s %s%s", tn, what, note); break;
        default: doomo_snprintf(full, sizeof(full), "%s %s", tn, what); break;
        }
        if (ui_text_width(full) <= maxw || k == 4) break;
    }
    doomo_snprintf(out, cap, "%s", full);
    return ui_text_width(out);
}

/* the same, cached in the scene scratch: rebuilt only when the cell, the key / scale or the width change */
const char *st_describe_cell(int pat, int trk, int stp)
{
    StEasy *ez = st_easy();
    int maxw = st_content().w - 12, v;
    uint32_t key;
    if (!ez) return "";
    pat &= 3; trk &= 3; stp &= 31;
    v = SS->notes[pat][trk][stp];
    key = (uint32_t)pat | (uint32_t)trk << 2 | (uint32_t)stp << 4 | (uint32_t)v << 9 | (uint32_t)(SS->key % 12) << 17 |
          (uint32_t)SS->scale << 21 | (uint32_t)((maxw >> 1) & 255) << 24;
    key = key * 2654435761u + 1u;
    if (!ez->desc[0] || ez->desc_key != key) {
        (void)st_describe_text(ez->desc, (int)sizeof(ez->desc), maxw, pat, trk, stp);
        ez->desc_key = key;
    }
    return ez->desc;
}
