/* story_grumble.c - Grumbleguts' story-only state and his dating ticker
 * (owner: S5; docs/design/story/SPEC.md R22, C13; story_final.md sec.4).
 *
 * He is a real citizen, roster resident 729: friendship, romance, death
 * and burial all go through the sim (BEFRIEND, sim_social.c, the grave).
 * STRY.grumble keeps only what the sim has no field for:
 *     bits 0-3  dishes fed (chain T, S183/S207; capped at 15)
 *     bit  4    pacifist (S174: you never hit him)
 *     bits 5-7  exes met (S174: 0..7)
 *     bit  8    (S5) the "deceased" profile line has been posted
 * and the SF_GRUMBLE_DEAD / SF_GRUMBLE_NO_RITE flags (the Feast, S186).
 *
 * The ticker ("Hell's Biggest Flirt", S173's reward): once a day at 13:00
 * the Hellvile Herald runs one line about him - a flirt with an ADULT
 * citizen (never a KID or a TEEN: the sim's own romance age gate, plus the
 * roster traits checked again here), the Gate, Father Fuzz's horror, his
 * little black book - and he is turned down kindly every single time
 * (story_final.md: "he is rejected kindly every time").
 *
 * Story group on the PS1 (the name matches story(_\w+)?); the two getters
 * the graveyard and the credits read stay resident through the planner's
 * closure. 0 B of RAM beyond one 32-bit day.
 */
#include "story_int.h"
#include "doomo/roster.h"
#include "doomo/sim.h"

#define GRB_DISH_MASK   0x000Fu
#define GRB_PACIFIST_B  0x0010u
#define GRB_EXES_SHIFT  5
#define GRB_EXES_MASK   0x00E0u
#define GRB_OBIT_B      0x0100u

int story_grumble_dishes(void)   { return story_stry()->grumble & GRB_DISH_MASK; }
int story_grumble_pacifist(void) { return (story_stry()->grumble & GRB_PACIFIST_B) != 0; }
int story_grumble_exes_met(void) { return (story_stry()->grumble & GRB_EXES_MASK) >> GRB_EXES_SHIFT; }
int story_grumble_no_rite(void)  { return story_flag(SF_GRUMBLE_NO_RITE); }

/* Dead the Feast's way (the flag) or the sim's (a grave like anyone's). */
int story_grumble_dead(void)
{
    const DoomoResident *r = town_res(ROSTER_ID_GRUMBLEGUTS);
    return story_flag(SF_GRUMBLE_DEAD) || (r && r->status == RES_DEAD);
}

/* The graveyard's clause (C13): the rite may not raise him after the Feast. */
int story_rite_blocker(int resident_id)
{
    return resident_id == ROSTER_ID_GRUMBLEGUTS && story_flag(SF_GRUMBLE_NO_RITE);
}

/* `native CB_SIDE_GRUMBLE` (story_side.c): GRB_* with its value. */
void story_grumble_count(int op, int value)
{
    DoomoStry *s = story_stry();
    unsigned g = s->grumble;
    switch (op) {
    case GRB_DISH:
        if ((g & GRB_DISH_MASK) < GRB_DISH_MASK) g++;
        break;
    case GRB_EXES:
        g = (g & ~GRB_EXES_MASK) | (((unsigned)value & 7u) << GRB_EXES_SHIFT);
        break;
    case GRB_PACIFIST:
        g |= GRB_PACIFIST_B;
        break;
    default:
        return;
    }
    s->grumble = (uint16_t)g;
}

/* ---- the ticker ---------------------------------------------------------- */
/* %s is an adult's name (<= 11 chars); every line fits the Herald's 63
 * (test_side_b.c measures them all with the longest name). */
static const char *const flirt_line[] = {
    "Grumbleguts asked %s out. %s said no, nicely.",
    "Grumbleguts called %s 'sugar'. A kind no.",
    "%s turned Grumbleguts down. He sent a fruit basket.",
    "Grumbleguts winked at %s. %s smiled: 'No.'",
    "%s let Grumbleguts down gently. He took a bow.",
    "Grumbleguts left %s a note: 'Gate's open.' Returned.",
    "%s: 'He's very sweet. No.' Grumbleguts: 'Open late.'",
    "Grumbleguts dropped a spoon near %s. Twice. Still no.",
};
enum { SOLO_FUZZ = 2 };
static const char *const solo_line[] = {
    "Grumbleguts asked the Gate out again. It creaked.",
    "The Gate and Grumbleguts: still 'complicated'.",
    "Grumbleguts called Father Fuzz 'padre'. Fuzz prayed.",
    "Grumbleguts' little black book: now in two volumes.",
    "Grumbleguts' profile: 0 matches, 212 winks sent.",
    "Grumbleguts admired himself in a puddle. It said no.",
};
static const char partner_line[] = "Grumbleguts and %s: still on. Gate's shut.";
static const char obit_line[] = "Grumbleguts' dating profile now reads 'deceased'.";

static uint32_t mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

static void copy(char *out, int cap, const char *s)
{
    int i;
    for (i = 0; i < cap - 1 && s[i]; i++) out[i] = s[i];
    out[i] = 0;
}

/* An adult he may flirt with: alive, not him, never a kid or a teen (the
 * sim's romance gate says the same; the traits are checked again here so a
 * change there can never put a child in his ticker). */
static int adult_ok(int id)
{
    const DoomoResident *r = town_res(id);
    if (id == ROSTER_ID_GRUMBLEGUTS || !r || r->status != RES_ALIVE) return 0;
    if (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) return 0;
    return sim_romance_ok(ROSTER_ID_GRUMBLEGUTS, id);
}

static int partner(void)
{
    uint16_t ids[8];
    int n = rel_list(ROSTER_ID_GRUMBLEGUTS, ids, 8), i, k;
    for (i = 0; i < n; i++) {
        k = rel_kind(ROSTER_ID_GRUMBLEGUTS, ids[i]);
        if ((k == REL_DATING || k == REL_MARRIED) && adult_ok(ids[i])) return ids[i];
    }
    return -1;
}

int story_grumble_headline(uint32_t day, char *out, int cap)
{
    uint32_t h = mix(day * 0x9E3779B9u ^ doomo_town.seed ^ 0x729u);
    int who = -1, i, p, k;
    const char *nm;
    if (!out || cap <= 0) return -1;
    out[0] = 0;
    if (story_grumble_dead()) return -1;
    if ((p = partner()) >= 0 && (h & 3u) == 0) {
        doomo_snprintf(out, cap, partner_line, town_res(p)->name);
        return p;
    }
    if (((h >> 2) % 3u) == 0) {
        k = (int)((h >> 4) % (uint32_t)DOOMO_COUNTOF(solo_line));
        if (k == SOLO_FUZZ && !adult_ok(ROSTER_ID_FATHER_FUZZ)) k = 0;
        copy(out, cap, solo_line[k]);
        return k == SOLO_FUZZ ? ROSTER_ID_FATHER_FUZZ : -1;
    }
    /* a scan from a hashed start (7919 is prime: one pass visits everyone) */
    for (i = 0; i < DOOMO_MAX_RESIDENTS && who < 0; i++) {
        int id = (int)(((h >> 8) + (uint32_t)i * 7919u) % DOOMO_MAX_RESIDENTS);
        if (adult_ok(id)) who = id;
    }
    if (who < 0) {
        copy(out, cap, solo_line[0]);
        return -1;
    }
    nm = town_res(who)->name;
    doomo_snprintf(out, cap, flirt_line[(h >> 4) % (uint32_t)DOOMO_COUNTOF(flirt_line)], nm, nm);
    return who;
}

/* Once per game minute on the overworld (story_vm_minute): the line of the
 * day, posted at the first quiet minute between 13:00 and 14:00 once S173
 * ("Profile Picture") is done; after the Feast's Branch B, one last line. */
void story_grumble_minute(void)
{
    static uint16_t posted;             /* (day + 1) of the last line; 0 = none (2 B .bss, not .data) */
    char line[NEWS_LEN];
    DoomoStry *s;
    uint16_t today = (uint16_t)(doomo_town.day + 1u);
    if (doomo_town.minute < 13 * 60 || doomo_town.minute >= 14 * 60 || posted == today) return;
    if (story_side2_state(73) != 3) return;
    posted = today;
    if (story_grumble_dead()) {
        s = story_stry();
        if (s->grumble & GRB_OBIT_B) return;
        s->grumble = (uint16_t)(s->grumble | GRB_OBIT_B);
        sim_news_push(obit_line);
        return;
    }
    story_grumble_headline(doomo_town.day, line, (int)sizeof line);
    if (!line[0]) return;
    sim_news_push(line);
    ui_toast("Herald: Hell's Biggest Flirt!");
}
