/* test_story_register.c - the PG-21 register over the COMPILED packs (owner:
 * E5; docs/design/story/SCRIPT_STYLE.md §5 R1-R3, §5b, §12).
 *
 * core/tools/story_register.py lints the .dsc sources (make -C core
 * story-check); this test holds the same rules over what ships: the packs
 * in core/gen, decoded through txt.c, the beat scripts the scripts run,
 * every beat script on its own (C code plays the cutscene ones directly)
 * and the side-mission rows. Two readers of one rule, so a compiler change
 * that loses a speaker or a LOVE face cannot hide adult text in a kid's
 * scene.
 *
 *   * core/data/story/register.tsv and kid_cast.txt parse; every kid_cast
 *     citizen's name matches the roster; Pip, Wisp and the pet are on it;
 *     the full f-word's cap is 1 (DECISIONS 23: twice in all, the other
 *     one cut off before the word is out);
 *   * the matcher: whole words, a leading or trailing *, a space matching a
 *     '/' line break, line codes and @-codes ignored ("Hell Breach" is not
 *     "what the hell"), a hyphen joining two words into one ("fuck-up",
 *     "piss-poor") and a cut-off dash ending one ("Shit- I mean");
 *   * K1/K2: no register word and no LOVE face in a script or beat script
 *     whose cast (speakers, VMD_WHO residents, the beat scripts' speakers
 *     and NPCs, the mission giver of an offer / active script) has a KID or
 *     TEEN citizen or a kid_cast citizen, or whose pack a kid_cast @scene
 *     names (P0); with only the pet in it, no sexual word. K4: every KID
 *     citizen a script names is on kid_cast.txt;
 *   * C1: the game-wide caps over every string of every English pack.
 *
 * Not here (the source lint has them): the @scene entries that name a
 * script or beat script (those names are not in the packs; pack names
 * are), the @scene globs matching something (S1), verb residents and E0's
 * jmat branches. */
#include <ctype.h>
#include <stdlib.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/beat.h"
#include "doomo/portrait.h"
#include "doomo/roster.h"

enum { C_STRONG, C_CRUDE, C_SEXUAL, C_N };
#define MAX_WORDS 160
typedef struct { char w[40]; uint8_t cls, head, tail; int cap; } RegWord;
static RegWord words[MAX_WORDS];
static int nwords = -1;
static uint8_t kid_cast[DOOMO_MAX_RESIDENTS];
static int kid_cast_pet, kid_cast_n, kid_cast_bad;
#define MAX_SCENES 32
static char kid_scene[MAX_SCENES][24];                 /* the @scene globs */
static int kid_scene_n;

static FILE *open_story(const char *name)
{
    char path[700];
    snprintf(path, sizeof(path), "%s/../../data/story/%s", test_golden_dir, name);
    return fopen(path, "r");
}

static void load_lists(void)
{
    char line[256];
    FILE *f;
    if (nwords >= 0) return;
    nwords = 0;
    if ((f = open_story("register.tsv")) != NULL) {
        while (fgets(line, sizeof(line), f) && nwords < MAX_WORDS) {
            char *t1, *t2, *t3;
            RegWord *w = &words[nwords];
            if (line[0] == '#' || line[0] == '\n') continue;
            line[strcspn(line, "\r\n")] = 0;
            t1 = strchr(line, '\t');
            if (!t1) continue;
            *t1++ = 0;
            t2 = strchr(t1, '\t');
            if (t2) *t2++ = 0;
            t3 = t2 && *t2 != '#' ? t2 : NULL;
            w->head = line[0] == '*';
            w->tail = line[strlen(line) - 1] == '*';
            snprintf(w->w, sizeof(w->w), "%.*s", (int)(strlen(line) - w->head - w->tail), line + w->head);
            w->cls = !strcmp(t1, "strong") ? C_STRONG : !strcmp(t1, "crude") ? C_CRUDE : !strcmp(t1, "sexual") ? C_SEXUAL : C_N;
            w->cap = t3 && isdigit((unsigned char)*t3) ? atoi(t3) : -1;
            nwords++;
        }
        fclose(f);
    }
    memset(kid_cast, 0, sizeof(kid_cast));
    kid_cast_pet = kid_cast_n = kid_cast_bad = kid_scene_n = 0;
    if ((f = open_story("kid_cast.txt")) != NULL) {
        while (fgets(line, sizeof(line), f)) {
            char *name, *e;
            int id;
            if (line[0] == '#' || line[0] == '\n') continue;
            line[strcspn(line, "#\r\n")] = 0;
            if (!strncmp(line, "@E\t", 3)) { kid_cast_pet = 1; continue; }
            if (!strncmp(line, "@scene\t", 7)) {
                char *g = line + 7;
                while (*g == '\t' || *g == ' ') g++;
                for (e = g + strlen(g); e > g && (e[-1] == ' ' || e[-1] == '\t'); e--) *(e - 1) = 0;
                if (*g && kid_scene_n < MAX_SCENES) snprintf(kid_scene[kid_scene_n++], sizeof(kid_scene[0]), "%s", g);
                continue;
            }
            if (!line[0]) continue;
            if (line[0] == '@' || !isdigit((unsigned char)line[0])) { kid_cast_bad++; continue; }
            id = atoi(line);
            name = strchr(line, '\t');
            if (!name || id < 0 || id >= DOOMO_MAX_RESIDENTS) { kid_cast_bad++; continue; }
            while (*name == '\t') name++;
            for (e = name + strlen(name); e > name && (e[-1] == ' ' || e[-1] == '\t'); e--) *(e - 1) = 0;
            for (e = name; *e; e++) if (*e == '_') *e = ' ';
            if (strcmp(roster_name(id), name) != 0) { kid_cast_bad++; continue; }
            kid_cast[id] = 1;
            kid_cast_n++;
        }
        fclose(f);
    }
}

/* the visible text, lowercased: line codes {..}, @P / @E and the like out,
 * '/' line breaks as spaces */
static void visible(const char *s, char *out, int cap)
{
    int n = 0;
    while (*s && n < cap - 1) {
        if (*s == '{') { const char *e = strchr(s, '}'); if (e) { s = e + 1; continue; } }
        if (*s == '@' && isalpha((unsigned char)s[1])) { s += 2; continue; }
        out[n++] = *s == '/' ? ' ' : (char)tolower((unsigned char)*s);
        s++;
    }
    out[n] = 0;
}

/* a word character at t[i]: a letter, digit or _, or a hyphen that joins two
 * words ("fuck-up", "mother-fucker"); a hyphen with no word character after
 * it is a cut-off dash ("Shit- I mean", "Oh, fuck-") and ends the word.
 * story_register.py's WCH is the same rule. */
static int wordc(char c) { return isalnum((unsigned char)c) || c == '_'; }
static int wordch_at(const char *t, int i) { return wordc(t[i]) || (t[i] == '-' && wordc(t[i + 1])); }

/* does glob g (* and ?) match the whole of s? */
static int glob_match(const char *g, const char *s)
{
    if (!*g) return !*s;
    if (*g == '*') return glob_match(g + 1, s) || (*s && glob_match(g, s + 1));
    return *s && (*g == '?' || *g == *s) && glob_match(g + 1, s + 1);
}

/* is pack p a kids' scene by a kid_cast.txt @scene glob (P0's crayon prologue)? */
static int scene_pack(int p)
{
    int i;
    for (i = 0; i < kid_scene_n; i++)
        if (glob_match(kid_scene[i], doomo_txt_pack_names[p])) return 1;
    return 0;
}

/* the word's letters (spaces match runs of spaces) from t[j]: the end, or -1 */
static int match_body(const RegWord *w, const char *t, int j)
{
    const char *p = w->w;
    while (*p) {
        if (*p == ' ') {
            if (t[j] != ' ') return -1;
            while (t[j] == ' ') j++;
            p++;
            continue;
        }
        if (t[j] != *p) return -1;
        j++;
        p++;
    }
    return j;
}

/* does w match the word that starts at t[i]? returns its end, or -1. A
 * leading * lets the letters start anywhere inside the word, a trailing *
 * lets the word run on after them. */
static int match_at(const RegWord *w, const char *t, int i)
{
    int j;
    for (j = i; t[j] && (j == i || wordch_at(t, j - 1)); j++) {
        int e = match_body(w, t, j);
        if (e >= 0) {
            if (w->tail) while (wordch_at(t, e)) e++;
            if (!wordch_at(t, e)) return e;
        }
        if (!w->head) break;
    }
    return -1;
}

/* how many times register word k is in the visible text v */
static int count_word(int k, const char *v)
{
    int i, n = 0;
    for (i = 0; v[i]; i++) {
        int e;
        if (i > 0 && wordch_at(v, i - 1)) continue;
        e = match_at(&words[k], v, i);
        if (e >= 0) { n++; i = e - 1; }
    }
    return n;
}

/* the classes of the register words in a string (bit per class) */
static int classes_in(const char *s, char *first, int cap)
{
    char v[400];
    int k, m = 0;
    visible(s, v, (int)sizeof(v));
    for (k = 0; k < nwords; k++)
        if (count_word(k, v)) {
            if (!m && first) snprintf(first, (size_t)cap, "%.39s", words[k].w);
            m |= 1 << words[k].cls;
        }
    return m;
}

static int is_pet_line(const char *s)
{
    char v[400];
    const char *p;
    if (strstr(s, "@E")) return 1;
    visible(s, v, (int)sizeof(v));
    for (p = v; *p == ' ' || *p == '('; p++) {}
    return (!strncmp(p, "beep", 4) || !strncmp(p, "bep", 3)) && !isalpha((unsigned char)p[!strncmp(p, "beep", 4) ? 4 : 3]);
}

static int kid(int id)
{
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    return kid_cast[id] || (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) != 0;
}

TEST(story_register_lists)
{
    int k, fword = -1, classes = 0;
    nwords = -1;
    load_lists();
    CHECK_MSG(nwords >= 40, "register.tsv: %d words", nwords);
    for (k = 0; k < nwords; k++) {
        CHECK_MSG(words[k].cls < C_N, "register.tsv: %.39s has no class", words[k].w);
        classes |= 1 << words[k].cls;
        if (!strcmp(words[k].w, "fuck")) fword = k;
    }
    CHECK_EQ(classes, 7);
    CHECK(fword >= 0);
    CHECK_EQ(words[fword].cap, 1);                       /* DECISIONS 23: twice, one of them cut off */
    CHECK_EQ(kid_cast_bad, 0);                           /* every name matches the roster */
    CHECK(kid_cast_n >= 2);
    CHECK(kid_cast[ROSTER_ID_PIP]);
    CHECK(kid_cast[563]);                                /* Wisp */
    CHECK(kid_cast_pet);
    CHECK(kid_scene_n >= 1);
    for (k = 0; k < kid_scene_n && strcmp(kid_scene[k], "p0"); k++) {}
    CHECK_MSG(k < kid_scene_n, "kid_cast.txt: the prologue P0 is not a kids' scene");
    for (k = 0; k < txt_pack_count() && !scene_pack(k); k++) {}
    CHECK_MSG(k < txt_pack_count(), "kid_cast.txt: no @scene glob names a pack");
    for (k = 0; k < DOOMO_MAX_RESIDENTS; k++)            /* a kid-coded adult may be listed; a teen may not (LS0) */
        CHECK_MSG(!kid_cast[k] || !(roster_traits(k) & ROSTER_T_TEEN), "kid_cast.txt lists the teen %d", k);
}

TEST(story_register_matcher)
{
    char w[40];
    load_lists();
    CHECK_EQ(classes_in("Oh, for fuck's sake.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("{.}Fucking marvellous.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("That's horseshit.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("What the / hell?", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("The Hell Breach! Hell Pits!", w, (int)sizeof(w)), 0);
    CHECK_EQ(classes_in("Essex is lovely. Sussex too.", w, (int)sizeof(w)), 0);
    CHECK_EQ(classes_in("Twenty shots and a striped / sock. Classic.", w, (int)sizeof(w)), 0);
    CHECK_EQ(classes_in("Sugar, darlin', a cookie.", w, (int)sizeof(w)), 0);
    CHECK_EQ(classes_in("(A sign: 'NAKED KARAOKE'.)", w, (int)sizeof(w)), 1 << C_SEXUAL);
    CHECK_STR(w, "naked");
    CHECK_EQ(classes_in("More booze, @P?", w, (int)sizeof(w)), 1 << C_CRUDE);
    /* the hyphen: a joining hyphen makes one word, a cut-off dash ends it */
    CHECK_EQ(classes_in("What a fuck-up.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("You mother-fucker!", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("Oh, fuck-", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("Shit- I mean, shoot.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("Shit-faced again.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("Piss-poor work.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_EQ(classes_in("Damn- no.", w, (int)sizeof(w)), 1 << C_STRONG);
    CHECK_STR(w, "damn");
    CHECK_EQ(classes_in("A first-class pass.", w, (int)sizeof(w)), 0);
    /* Hell's lore words are not oaths */
    CHECK_EQ(classes_in("The damned souls sing.", w, (int)sizeof(w)), 0);
    CHECK_EQ(classes_in("Damnation Pit!", w, (int)sizeof(w)), 0);
    CHECK(is_pet_line("beep."));
    CHECK(is_pet_line("(@E, very quietly: beep.)"));
    CHECK(!is_pet_line("Beeper repair, two coins."));
}

/* one script's (or beat script's) cast and its LOVE faces, from the compiled
 * ops; scene: its pack is a kids' scene by a kid_cast.txt @scene glob */
typedef struct { int kids[8], nk, pet, love, scene; } Cast;

static int has_kid(const Cast *c) { return c->nk > 0 || c->scene; }

static void add_who(Cast *c, int who)
{
    int i;
    if (who < 0 || who >= DOOMO_MAX_RESIDENTS || !kid(who)) return;
    for (i = 0; i < c->nk; i++) if (c->kids[i] == who) return;
    if (c->nk < 8) c->kids[c->nk++] = who;
}

static int bad_k1, bad_k2, bad_k4, kid_scripts, pet_scripts;

/* what: "script" or "beat" (k is its index in the pack) */
static const char *what = "script";

static void check_string(const Cast *c, uint16_t tid, const char *pack, int k, int pass)
{
    char buf[256], w[40];
    int m;
    if (tid == 0xFFFFu || txt_get(tid, buf, (int)sizeof(buf)) < 0) return;
    if (pass == 0) return;
    m = classes_in(buf, w, (int)sizeof(w));
    if (has_kid(c) && m) {
        bad_k1++;
        printf("    K1: pack %s %s %d: '%s' with kid %d in the cast%s: %s\n", pack, what, k, w, c->nk ? c->kids[0] : -1,
               c->scene ? " (an @scene pack)" : "", buf);
    } else if (c->pet && kid_cast_pet && (m & (1 << C_SEXUAL))) {
        bad_k1++;
        printf("    K1: pack %s %s %d: '%s' with the pet in the scene: %s\n", pack, what, k, w, buf);
    }
}

/* one beat script's events: pass 0 gathers the cast, pass 1 checks */
static void scan_beat(uint16_t beat_id, Cast *c, const char *pack, int k, int pass)
{
    BeatScript bs;
    char buf[256];
    int e;
    if (!txt_beat(beat_id, &bs)) return;
    for (e = 0; e < bs.n; e++) {
        const BeatEv *ev = &bs.ev[e];
        if (pass == 0 && (ev->op == BOP_SAY || ev->op == BOP_NPC || ev->op == BOP_HEADTURN))
            add_who(c, ev->a);
        if (pass == 0 && ev->op == BOP_EXPR && (ev->b & 0xFF) == EXPR_LOVE) c->love = 1;
        if (ev->op == BOP_SAY || ev->op == BOP_CAPTION) {
            if (pass == 0 && txt_get((uint16_t)ev->b, buf, (int)sizeof(buf)) >= 0 && is_pet_line(buf)) c->pet = 1;
            check_string(c, (uint16_t)ev->b, pack, k, pass);
        }
    }
}

/* pass 0 gathers the cast (and the pet), pass 1 checks the strings */
static void scan_script(int p, int k, Cast *c, int pass)
{
    int n, i;
    const StoryOp *o = txt_script((uint16_t)TXT_ID(p, k), &n);
    const char *pack = doomo_txt_pack_names[p];
    char buf[256];
    if (!o) return;
    for (i = 0; i < n; i++) {
        uint16_t tid = 0xFFFFu;
        switch (o[i].op) {
        case OP_SAY:
            if (o[i].a < VM_SAY_TARGET && pass == 0) add_who(c, o[i].a);
            tid = o[i].b;
            break;
        case OP_SAY_X:
            if (o[i].a == EXPR_LOVE && pass == 0) c->love = 1;
            tid = o[i].b;
            break;
        case OP_CHOICE: case OP_PLAYER_LINE:
            tid = o[i].b;
            break;
        case OP_DATA:
            if (o[i].a == VMD_WHO && pass == 0) add_who(c, o[i].b);
            if (o[i].a == VMD_LINE && (o[i].b >> 8) == EXPR_LOVE && pass == 0) c->love = 1;
            if (o[i].a == VMD_OPT) tid = o[i].b;
            break;
        case OP_BEAT:
            scan_beat(o[i].b, c, pack, k, pass);
            break;
        default:
            break;
        }
        if (tid != 0xFFFFu) {
            if (pass == 0 && txt_get(tid, buf, (int)sizeof(buf)) >= 0 && is_pet_line(buf)) c->pet = 1;
            check_string(c, tid, pack, k, pass);
        }
    }
}

static int kid_beats, pet_beats, all_beats;

/* K2 and K4 for one scanned script or beat script */
static void check_cast(const Cast *c, int p, int k)
{
    int i;
    if (has_kid(c) && c->love) {
        bad_k2++;
        printf("    K2: pack %s %s %d: a LOVE face with kid %d in the cast%s\n", doomo_txt_pack_names[p], what, k,
               c->nk ? c->kids[0] : -1, c->scene ? " (an @scene pack)" : "");
    }
    for (i = 0; i < c->nk; i++)
        if ((roster_traits(c->kids[i]) & ROSTER_T_KID) && !kid_cast[c->kids[i]]) {
            bad_k4++;
            printf("    K4: pack %s %s %d: KID citizen %d (%s) is not on kid_cast.txt\n", doomo_txt_pack_names[p], what,
                   k, c->kids[i], roster_name(c->kids[i]));
        }
}

TEST(story_register_no_adult_text_near_kids)
{
    int p, i;
    load_lists();
    CHECK(nwords > 0);
    bad_k1 = bad_k2 = bad_k4 = kid_scripts = pet_scripts = kid_beats = pet_beats = all_beats = 0;
    for (p = 0; p < txt_pack_count(); p++) {
        int k, ns = txt_pack_scripts(p), nb = txt_pack_beats(p);
        const char *pack = doomo_txt_pack_names[p];
        if (!strcmp(pack, "vmtest")) continue;
        what = "script";
        for (k = 1; k < ns; k++) {
            Cast c;
            uint16_t sid = (uint16_t)TXT_ID(p, k);
            memset(&c, 0, sizeof(c));
            c.scene = scene_pack(p);
            for (i = 0; i < STORY_SIDE2_ROWS; i++)          /* the giver runs the offer / active script */
                if (doomo_side_rows[i].kind && (doomo_side_rows[i].offer == sid || doomo_side_rows[i].active == sid))
                    add_who(&c, doomo_side_rows[i].giver);
            scan_script(p, k, &c, 0);
            scan_script(p, k, &c, 1);
            kid_scripts += has_kid(&c);
            pet_scripts += c.pet;
            check_cast(&c, p, k);
        }
        /* every beat script on its own too: C code plays the cutscene ones
         * (P0's crayon prologue, F2-F4, the summit, E1) with no script's
         * runbeat in between */
        what = "beat";
        for (k = 0; k < nb; k++) {
            Cast c;
            memset(&c, 0, sizeof(c));
            c.scene = scene_pack(p);
            scan_beat((uint16_t)TXT_ID(p, k), &c, pack, k, 0);
            scan_beat((uint16_t)TXT_ID(p, k), &c, pack, k, 1);
            all_beats++;
            kid_beats += has_kid(&c);
            pet_beats += c.pet;
            check_cast(&c, p, k);
        }
        what = "script";
    }
    printf("    %d scripts with a kid in the cast, %d with the pet; %d beat scripts, %d with a kid, %d with the pet\n",
           kid_scripts, pet_scripts, all_beats, kid_beats, pet_beats);
    CHECK(kid_scripts >= 50);            /* the scan sees the cast (77 scripts in the source lint, with @scene) */
    CHECK(pet_scripts >= 20);
    CHECK(all_beats >= 30);              /* every beat script, not only the runbeat ones */
    CHECK(kid_beats >= 1);               /* P0's p0_egg, a kids' scene by its pack (@scene p0) */
    CHECK(pet_beats >= 1);               /* E1's "@E: beep." */
    CHECK_EQ(bad_k1, 0);
    CHECK_EQ(bad_k2, 0);
    CHECK_EQ(bad_k4, 0);
}

TEST(story_register_caps)
{
    static int used[MAX_WORDS];
    int p, k;
    load_lists();
    memset(used, 0, sizeof(used));
    for (p = 0; p < txt_pack_count(); p++) {
        int i, n = txt_pack_strings(p);
        if (!strcmp(doomo_txt_pack_names[p], "vmtest")) continue;
        for (i = 0; i < n; i++) {
            char buf[256], v[400];
            if (txt_get((uint16_t)TXT_ID(p, i), buf, (int)sizeof(buf)) < 0) continue;
            visible(buf, v, (int)sizeof(v));
            for (k = 0; k < nwords; k++) used[k] += count_word(k, v);
        }
    }
    for (k = 0; k < nwords; k++)
        CHECK_MSG(words[k].cap < 0 || used[k] <= words[k].cap, "'%.39s' used %d times in the game (cap %d)", words[k].w,
                  used[k], words[k].cap);
}
