/* concert_lyrics.c - song titles, lyrics, band names and reviews from
 * templates (owner: concert).
 *
 * A song is about its singer: their name, job, favourite food, the thing
 * they fear, their catchphrase, their favourite music, and the people in
 * their life (best friend, crush, sweetheart, rival). Verse 1 introduces
 * them, the chorus is the hook (catchphrase + genre + someone special +
 * the genre's ad-lib), and verse 2 is a tiny four-line story that starts
 * cute and ends... less cute:
 *
 *     I love cupcakes so much / But I fear the dark
 *     My best friend Grimbert / Ate my heart / (not literally)
 *
 * Hellvile citizens get the edgy template bank. Every line is at most 27
 * characters (the karaoke pill and the shared DoomoSong format); a
 * template that does not fit (long names) or needs someone the singer
 * does not have (a crush, a rival) is skipped for the next candidate.
 *
 * Template tokens: %n name  %f food  %h hated food  %b hobby  %m music
 * %c colour  %x fear  %p catchphrase  %j job  %J "a/an job"  %F friend
 * %C crush  %R rival  %P sweetheart  %s species  %a genre ad-lib
 * %X fear as a band name ("Spiders", "Dark")  %Y a fear that is a thing
 * (spiders, clowns - not "heights")  %t take number  %% percent
 */
#include "concert_int.h"

/* ====================================================================== */
/* the singer's world                                                       */
typedef struct LyCast {
    int id;
    const char *name;
    int food, hate, hobby, music, color, fear, phrase, species, edgy;
    int friend_id, crush, rival, partner;
    char job[24];
    int take;
} LyCast;

static const char *const hobby_jobs[HOBBY_COUNT] = {
    "baker", "knitter", "gardener", "stargazer", "skater", "gamer", "painter",
    "singer", "dancer", "bookworm", "fisher", "pro napper", "cosplayer",
    "karaoke star", "game master", "bug collector", "birdwatcher", "yogi",
    "potter", "paper folder", "plush maker", "cloud watcher", "bubble artist",
    "poet", "tarot reader", "spirit medium", "ghost hunter", "drummer",
    "prankster", "street artist", "welder", "skull collector"
};

/* fears that are things that can come and get you (bit = FEAR index):
 * spiders, the dark, thunder, clowns, ghosts, vacuum cleaners, bees,
 * needles, mirrors, dentists, holy water, church bells, hugs, cute things */
#define FEAR_THINGS ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 7) | \
                     (1u << 8) | (1u << 9) | (1u << 10) | (1u << 14) | (1u << 17) | (1u << 19) | \
                     (1u << 21) | (1u << 22))

static char lower_c(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

void concert_job_of(int id, char *buf, int cap)
{
    const DoomoResident *r = town_res(id);
    const char *j = NULL;
    int i;
    if (cap <= 0) return;
    buf[0] = 0;
    if (!r) return;
    if (id < DOOMO_MAX_RESIDENTS && !(r->flags & RF_CUSTOM)) {
        j = roster_job(id);
        if (j && j[0]) {
            /* only if the premade citizen still lives in that slot */
            const char *rn = roster_name(id);
            if (!rn || strncmp(rn, r->name, DOOMO_NAME_LEN) != 0) j = NULL;
        } else {
            j = NULL;
        }
    }
    if (!j) j = hobby_jobs[r->like_hobby % HOBBY_COUNT];
    /* lowercase, except place names ("mayor of Hillvile") */
    {
        int keep = 0;
        for (i = 0; j[i] && i < cap - 1; i++) {
            if (i == 0 || j[i - 1] == ' ')
                keep = strncmp(j + i, "Hillvile", 8) == 0 || strncmp(j + i, "Hellvile", 8) == 0 ||
                       strncmp(j + i, "HaHa", 4) == 0 || strncmp(j + i, "Cloud Nine", 10) == 0 ||
                       (keep && strncmp(j + i, "Nine", 4) == 0);
            buf[i] = keep ? j[i] : lower_c(j[i]);
        }
    }
    buf[i] = 0;
}

const char *concert_name(int id)
{
    const DoomoResident *r = town_res(id);
    return (r && r->status != RES_EMPTY && r->name[0]) ? r->name : "???";
}

static int alive_other(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status != RES_EMPTY && r->status != RES_DEAD;
}

static void cast_of(int id, int genre, LyCast *c)
{
    const DoomoResident *r = town_res(id);
    uint16_t rel[24];
    int n, i;
    memset(c, 0, sizeof(*c));
    c->id = id;
    c->friend_id = c->crush = c->rival = c->partner = -1;
    c->music = genre;
    if (!r) { c->name = "Someone"; strcpy(c->job, "citizen"); return; }
    c->name = r->name[0] ? r->name : "Someone";
    c->food = r->like_food % FOOD_COUNT;
    c->hate = r->hate_food % FOOD_COUNT;
    c->hobby = r->like_hobby % HOBBY_COUNT;
    c->color = r->like_color % PAL_PAINT_COUNT;
    c->fear = r->fear % FEAR_COUNT;
    c->phrase = r->catchphrase % CATCHPHRASE_COUNT;
    c->species = r->look.species % SPECIES_COUNT;
    concert_job_of(id, c->job, (int)sizeof(c->job));
    c->partner = sim_partner_of(id);
    if (c->partner >= 0 && !alive_other(c->partner)) c->partner = -1;
    n = rel_list(id, rel, DOOMO_COUNTOF(rel));
    for (i = 0; i < n; i++) {
        int o = rel[i], v = rel_value(id, o), k = rel_kind(id, o);
        if (!alive_other(o) || o == id) continue;
        if (c->crush < 0 && o != c->partner && rel_has_crush(id, o)) c->crush = o;
        if (c->friend_id < 0 && o != c->partner && v > 15 &&
            (k == REL_FRIEND || k == REL_BEST_FRIEND || k == REL_FAMILY || k == REL_ACQUAINTANCE))
            c->friend_id = o;
        if (v < -15 || k == REL_RIVAL || k == REL_NEMESIS) c->rival = o;  /* worst last */
    }
    if (c->crush == c->friend_id) c->crush = -1;
}

/* ====================================================================== */
/* the template filler                                                      */
static const char *str_lower_into(const char *s, char *buf, int cap)
{
    int i;
    for (i = 0; s[i] && i < cap - 1; i++) buf[i] = lower_c(s[i]);
    buf[i] = 0;
    return buf;
}

static int is_vowel_c(char c)
{
    c = lower_c(c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

/* Expand template `t` into out (cap bytes incl. NUL, at most `maxlen`
 * chars). Returns 0 if a needed person is missing or it does not fit. */
static int fill(const char *t, const LyCast *c, char *out, int maxlen)
{
    int n = 0;
    char tmp[28];
    while (*t) {
        const char *ins = NULL;
        if (*t != '%') {
            if (n >= maxlen) return 0;
            out[n++] = *t++;
            continue;
        }
        t++;
        switch (*t) {
        case 'n': ins = c->name; break;
        case 'f': ins = str_lower_into(doomo_food_names[c->food], tmp, sizeof(tmp)); break;
        case 'h': ins = str_lower_into(doomo_food_names[c->hate], tmp, sizeof(tmp)); break;
        case 'b': ins = str_lower_into(doomo_hobby_names[c->hobby], tmp, sizeof(tmp)); break;
        case 'm': ins = doomo_music_names[c->music % MUSIC_COUNT]; break;
        case 'c': ins = str_lower_into(doomo_paint_names[c->color], tmp, sizeof(tmp)); break;
        case 'x': ins = str_lower_into(doomo_fear_names[c->fear], tmp, sizeof(tmp)); break;
        case 'Y':                       /* a fear that can come and get you */
            if (!((FEAR_THINGS >> c->fear) & 1u)) return 0;
            ins = str_lower_into(doomo_fear_names[c->fear], tmp, sizeof(tmp));
            break;
        case 'X': {
            const char *f = doomo_fear_names[c->fear];
            if (strncmp(f, "The ", 4) == 0) f += 4;
            ins = f;
            break; }
        case 'p':
            ins = doomo_catchphrases[c->phrase];
            /* mid-line: "Oh-oh, mochi mochi!" */
            if (n > 1 && !strchr(".!?", out[n - 2]) && ins[0] >= 'A' && ins[0] <= 'Z' &&
                ins[1] >= 'a' && ins[1] <= 'z') {
                doomo_snprintf(tmp, sizeof(tmp), "%s", ins);
                tmp[0] = lower_c(tmp[0]);
                ins = tmp;
            }
            break;
        case 'j': ins = c->job; break;
        case 'J':
            doomo_snprintf(tmp, sizeof(tmp), "%s %s", is_vowel_c(c->job[0]) ? "an" : "a", c->job);
            ins = tmp;
            break;
        case 'F': if (c->friend_id < 0) return 0; ins = concert_name(c->friend_id); break;
        case 'C': if (c->crush < 0) return 0; ins = concert_name(c->crush); break;
        case 'R': if (c->rival < 0) return 0; ins = concert_name(c->rival); break;
        case 'P': if (c->partner < 0) return 0; ins = concert_name(c->partner); break;
        case 's': ins = str_lower_into(doomo_species_names[c->species], tmp, sizeof(tmp)); break;
        case 'a': ins = concert_genres[c->music % MUSIC_COUNT].adlib; break;
        case 't': tmp[0] = (char)('1' + (c->take & 7)); tmp[1] = 0; ins = tmp; break;
        case '%': ins = "%"; break;
        default: return 0;
        }
        t++;
        while (*ins) {
            if (n >= maxlen) return 0;
            out[n++] = *ins++;
        }
    }
    out[n] = 0;
    return 1;
}

/* Try the templates of a NULL-terminated bank starting at a random one;
 * the first that fits wins. Returns 0 if none fits. */
static int fill_from(const char *const *bank, DoomoRng *rng, const LyCast *c, char *out,
                     int maxlen)
{
    int n = 0, i, k;
    while (bank[n]) n++;
    if (!n) return 0;
    k = rng_range(rng, n);
    for (i = 0; i < n; i++)
        if (fill(bank[(k + i) % n], c, out, maxlen)) return 1;
    return 0;
}

static void cap_first(char *s)
{
    if (s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 32);
}

/* ====================================================================== */
/* the banks                                                                */
#define LINE_MAX_CH (CONCERT_LINE_LEN - 1)
#define TITLE_MAX_CH (CONCERT_TITLE_LEN - 1)

/* verse 1: who I am */
static const char *const v_intro[] = {
    "My name is %n", "Hello, I'm %n!", "Hi hi! It's me, %n", "They call me %n",
    "%n is here!", "Everybody, it's %n!", "Little old me, %n", "Knock knock! It's %n!",
    NULL };
static const char *const v_intro_edgy[] = {
    "I am %n. Fear me.", "%n has arrived.", "Bow down to %n", "Yo, it's %n, punks",
    "%n crawled out of bed", "The one, the only %n", "%n rises again", NULL };
static const char *const v_job[] = {
    "I'm %J by day", "I work as %J", "Just %J", "A humble %j", "I'm into %b",
    "My thing? %b!", "I live for %b", "Professional %j", NULL };
static const char *const v_job_edgy[] = {
    "%J by trade", "I'm %J, so what?", "%b. Keeps me sane.", "I do %b at midnight",
    "Certified %j", "Evil %j for hire", NULL };
static const char *const v_food[] = {
    "I love %f so much", "Give me %f!", "%f? Yes please!", "I dream of %f",
    "%f all day long", "Nothing beats %f", "Mmm, %f, mmm!", NULL };
static const char *const v_food_edgy[] = {
    "I eat %f with claws", "%f, raw, at midnight", "%f or I bite", "Feed me %f now",
    "Bring me %f. NOW.", "I'd sell my soul for %f", NULL };
static const char *const v_fear[] = {
    "But I fear %x", "But %x? Eek!", "Just not %x, please", "Hide me from %Y",
    "%x? Nope! Nope!", "But I'm scared of %x", NULL };
static const char *const v_fear_edgy[] = {
    "%Y? I'll bite it", "I fear only %x", "%x... my weakness", "Keep %Y away",
    "Don't say %x. Ever.", NULL };

/* chorus */
static const char *const c_hook[] = { "%p", "Oh-oh, %p", "%p %p", "Say it: %p", NULL };
static const char *const c_hook_edgy[] = { "%p", "Scream it: %p", "All say: %p", NULL };
static const char *const c_genre[] = {
    "%m all night long", "This is %m!", "%m forever", "Dance to %m!",
    "Turn up the %m", "Sweet sweet %m", "My heart beats %m", NULL };
static const char *const c_genre_edgy[] = {
    "%m till we drop", "%m from the pit", "Crank the %m to 11", "%m or death!",
    "%m melts my face", NULL };
static const char *const c_someone[] = {
    "%C, you make me blush", "%C, look at me!", "%P, my sweetie pie", "%P, hold my paw",
    "%F, sing with me!", "%F is my bestie", "Me and %F, la la!", NULL };
static const char *const c_someone_edgy[] = {
    "%C, be my minion", "%C, haunt me please", "%P, my dark sweetheart",
    "%F, howl with me!", "%F, my partner in crime", NULL };
static const char *const c_alone[] = {
    "You can be my friend!", "Sing with me, okay?", "Clap your paws with me!", NULL };
static const char *const c_alone_edgy[] = {
    "I need no friends!", "Join my cult! (choir)", "Clap. Or else.", NULL };
static const char *const c_adlib[] = {
    "%a %a", "%a", "La la la la la!", "Woah-oh-oh-oh!", "Doo-be-doo-be-doo!", NULL };
static const char *const c_adlib_edgy[] = {
    "%a %a", "%a", "Woah-oh-OH-OH!", "Rawr rawr rawr!", NULL };

/* verse 2: tiny four-line stories, cute then... less cute */
typedef struct LyStory { const char *l[4]; } LyStory;
static const LyStory stories[] = {
    { { "My best friend %F", "Ate my heart", "(Not literally)", "%f for two!" } },
    { { "My best friend %F", "Holds my paw at night", "When I dream of %Y", "...wait, whose paw?" } },
    { { "I saw %C at the cafe", "My tail went wag wag wag", "I spilled my %f", "...on %C. Oops." } },
    { { "%C smiled at me", "My ears went boing!", "I hid in a bush", "For three whole days" } },
    { { "%R stole my %f", "So I stole their socks", "Now we're even", "...or are we?" } },
    { { "%P holds my paw", "We share our %f", "Forever and ever", "(And ever and ever)" } },
    { { "One night came %Y", "I hid under my bed", "%Y hid there too", "Now we're roommates" } },
    { { "They served me %h", "I made a sad face", "I fed it to a ghost", "The ghost said thanks" } },
    { { "I tried some %b", "It did not go well", "The fire truck came", "They loved it, though" } },
    { { "A purple cloud came by", "It smelled like grape", "Father Fuzz said no", "So I said no too!" } },
    { { "I clocked in at nine", "I clocked out at five", "In between? %f", "And a teeny nap" } },
    { { "I baked a %c cake", "It started to sing", "It sang this song!", "...then I ate it" } },
    { { NULL, NULL, NULL, NULL } }
};
static const LyStory stories_edgy[] = {
    { { "%R crossed my path", "I cursed their %f", "Now it screams at night", "Serves them right" } },
    { { "They say I fear %Y", "That's a filthy lie", "...it's in my room", "MOM! MOOOOM!" } },
    { { "%C, my dark delight", "I'd bring you a soul", "But I ate it first", "Here's the wrapper" } },
    { { "%F and me", "We raised a little hell", "Then we cleaned it up", "We're polite like that" } },
    { { "Down in Hellvile", "The lava's warm and nice", "Bring your own %f", "And a fire hose" } },
    { { "Vex sold me a cloud", "Unholy Grape flavour", "Now my horns tingle", "Don't vape, kids!" } },
    { { "I do %b at night", "With candles and bones", "The neighbours moved out", "More room for bones" } },
    { { "%P, my nightmare", "You stole my black heart", "Keep it in a jar", "Feed it %f" } },
    { { "I'm %J", "I file souls in triplicate", "Overtime in the pit", "Paid in %f" } },
    { { NULL, NULL, NULL, NULL } }
};

/* titles (15 chars max) */
static const char *const t_titles[] = {
    "Ode to %f", "Not Literally", "Sweet %f", "Scared of %x", "%m Heart", "%p",
    "Little %n", "%n's Song", "Hug O'Clock", "Me and %F", "Dear %C", "Lost in %f",
    "Paws Up, %n!", "%c Sky", "Take %t", NULL };
static const char *const t_titles_edgy[] = {
    "Bite Me, %R", "Hellvile Nights", "%X Must Die", "Doom Snack", "%n Unleashed",
    "Lava Lullaby", "Raw %f", "%p", "Born to Haunt", "Sulfur Kisses", "Bloody %f", NULL };

/* band names */
static const char *const b_names[] = {
    "%n & the %X", "The %f Gang", "%n's Fluffy Four", "Sticky %f", "%n & Friends",
    "The %c Paws", "Purrfect %X", "%F, %n & Co.", NULL };
static const char *const b_names_edgy[] = {
    "%n & the Damned", "Death by %f", "The Screaming %X", "%n & the Gremlins",
    "Hellvile %X", "%c Coffin Club", NULL };

/* ====================================================================== */
void concert_lyrics_make(ConcertSong *s, uint32_t seed)
{
    LyCast c;
    DoomoRng rng;
    int edgy = s->edgy, i, k, nst;
    const LyStory *bank;
    cast_of(s->singer, s->genre, &c);
    c.take = s->take;
    c.edgy = edgy;
    rng_seed(&rng, seed);
    /* verse 1 */
    fill_from(edgy ? v_intro_edgy : v_intro, &rng, &c, s->lines[0], LINE_MAX_CH);
    fill_from(edgy ? v_job_edgy : v_job, &rng, &c, s->lines[1], LINE_MAX_CH);
    fill_from(edgy ? v_food_edgy : v_food, &rng, &c, s->lines[2], LINE_MAX_CH);
    fill_from(edgy ? v_fear_edgy : v_fear, &rng, &c, s->lines[3], LINE_MAX_CH);
    /* chorus */
    fill_from(edgy ? c_hook_edgy : c_hook, &rng, &c, s->lines[4], LINE_MAX_CH);
    fill_from(edgy ? c_genre_edgy : c_genre, &rng, &c, s->lines[5], LINE_MAX_CH);
    if (!fill_from(edgy ? c_someone_edgy : c_someone, &rng, &c, s->lines[6], LINE_MAX_CH))
        fill_from(edgy ? c_alone_edgy : c_alone, &rng, &c, s->lines[6], LINE_MAX_CH);
    fill_from(edgy ? c_adlib_edgy : c_adlib, &rng, &c, s->lines[7], LINE_MAX_CH);
    /* verse 2: a story that fits */
    bank = edgy ? stories_edgy : stories;
    for (nst = 0; bank[nst].l[0]; nst++) {}
    k = rng_range(&rng, nst);
    for (i = 0; i < nst; i++) {
        const LyStory *st = &bank[(k + i) % nst];
        int j, ok = 1;
        for (j = 0; j < 4 && ok; j++) ok = fill(st->l[j], &c, s->lines[8 + j], LINE_MAX_CH);
        if (ok) break;
    }
    if (i == nst) {
        for (k = 0; k < 4; k++) fill(v_food[k], &c, s->lines[8 + k], LINE_MAX_CH);
    }
    for (i = 0; i < CONCERT_LINES; i++) {
        if (!s->lines[i][0]) doomo_snprintf(s->lines[i], CONCERT_LINE_LEN, "La la la la!");
        cap_first(s->lines[i]);
    }
    /* the title */
    if (!fill_from(edgy ? t_titles_edgy : t_titles, &rng, &c, s->title, TITLE_MAX_CH))
        doomo_snprintf(s->title, CONCERT_TITLE_LEN, edgy ? "Doom Song" : "La La Life");
    cap_first(s->title);
}

void concert_band_name(int lead, int genre, uint32_t seed, char *buf, int cap)
{
    LyCast c;
    DoomoRng rng;
    const DoomoResident *r = town_res(lead);
    int edgy = r && lead < DOOMO_MAX_RESIDENTS && (RES_DISTRICT(lead) || (r->flags & RF_HELLBORN));
    char out[40];
    cast_of(lead, genre, &c);
    rng_seed(&rng, seed ^ 0xBA4Du);
    if (concert_genres[genre % MUSIC_COUNT].edgy) edgy = 1;
    if (!fill_from(edgy ? b_names_edgy : b_names, &rng, &c, out, DOOMO_MIN(cap - 1, 18)))
        doomo_snprintf(out, sizeof(out), "%s & Co.", c.name);
    /* Title Case (but "the", "of", "by" stay small) */
    {
        int i;
        for (i = 0; out[i]; i++) {
            if ((i == 0 || out[i - 1] == ' ') && out[i] >= 'a' && out[i] <= 'z' &&
                strncmp(out + i, "the ", 4) != 0 && strncmp(out + i, "of ", 3) != 0 &&
                strncmp(out + i, "by ", 3) != 0)
                out[i] = (char)(out[i] - 32);
        }
    }
    doomo_snprintf(buf, cap, "%s", out);
}

/* ====================================================================== */
/* reviews                                                                  */
static const char *const rev_cute[5][3] = {
    { "My ears filed a complaint.", "Bold. Loud. Mostly loud.", "I have notes. All of them." },
    { "Pitchy, but plucky!", "The cupcakes enjoyed it.", "A for effort! C for pitch." },
    { "Solid! I tapped a paw.", "My grandma would dance.", "Hummable. Very hummable." },
    { "Catchy! It's stuck in my head.", "Hearts were thrown!", "I cried a little. Happy tears." },
    { "A masterpiece. I wept glitter.", "10/10, my tail won't stop.", "Ascended. I saw a rainbow." }
};
static const char *const rev_edgy[5][3] = {
    { "Even the imps left.", "My horns fell off.", "Banned from the pit." },
    { "Tolerable. Barely.", "I've heard worse screams.", "Meh. Needs more fire." },
    { "Decent headbang value.", "My skull nodded. Twice.", "Spooky enough. Ok." },
    { "Shredded my soul. Nice.", "The lava bubbled along!", "Wicked riffs, dude." },
    { "My ears are bleeding (good).", "Hellvile is SHAKING.", "10/10, would be possessed." }
};

const char *concert_review(int stars, int genre, int edgy, uint32_t seed)
{
    int s = DOOMO_CLAMP(stars, 1, 5) - 1;
    int k = (int)(doomo_hash32(seed + (uint32_t)genre) % 3u);
    return (edgy || concert_genres[genre % MUSIC_COUNT].edgy) ? rev_edgy[s][k] : rev_cute[s][k];
}

/* ====================================================================== */
/* syllables: vowel groups ("cup-cakes", "Grim-bert", "lit-er-al-ly"); a
 * silent final e does not count, every word is at least one syllable and
 * punctuation rides along with its word */
static int is_alpha_c(char c) { c = lower_c(c); return c >= 'a' && c <= 'z'; }

static int vowelish(const char *w, int i)
{
    char c = lower_c(w[i]);
    if (is_vowel_c(c)) return 1;
    return c == 'y' && i > 0 && is_alpha_c(w[i - 1]) && !is_vowel_c(w[i - 1]);
}

/* where to split the consonants between two vowel groups */
static int split_at(const char *w, int c0, int c1)
{
    static const char *const clusters[] = { "th", "sh", "ch", "ph", "wh", "ck",
        "pl", "bl", "cl", "fl", "gl", "pr", "br", "tr", "dr", "cr", "gr", "fr", "st", "tl", "dl",
        NULL };
    int k, len = c1 - c0;
    for (k = c1 - 1; k >= c0; k--)
        if (!is_alpha_c(w[k])) return k + 1;          /* "oh-|oh", "o'|clock" */
    if (len <= 1) return c0;
    for (k = 0; clusters[k]; k++)
        if (lower_c(w[c1 - 2]) == clusters[k][0] && lower_c(w[c1 - 1]) == clusters[k][1])
            return (k < 6 && len == 2) ? c0 : c1 - 2;  /* "Fa|ther", "pur|ple" */
    return c1 - 1;                                    /* "cup|cakes", "Hel|lo" */
}

int concert_syllables(const char *line, uint8_t *starts, int max)
{
    int i = 0, n = 0;
    while (line[i] && n < max) {
        int ws, we, k, ng = 0, gs[12], ge[12];
        while (line[i] == ' ') i++;
        if (!line[i]) break;
        ws = i;
        while (line[i] && line[i] != ' ') i++;
        we = i;                                       /* [ws, we) */
        for (k = ws; k < we && ng < 12; k++) {
            if (!is_alpha_c(line[k]) || !vowelish(line, k)) continue;
            gs[ng] = k;
            while (k + 1 < we && is_alpha_c(line[k + 1]) && vowelish(line, k + 1)) k++;
            ge[ng++] = k + 1;
        }
        /* silent e: "cakes", "smile", "loved" (but "lit-tle", "star-ted") */
        if (ng > 1 && ge[ng - 1] - gs[ng - 1] == 1 && lower_c(line[gs[ng - 1]]) == 'e') {
            int e = gs[ng - 1], tail = e + 1, prev = lower_c(line[e - 1]);
            char t0 = tail < we ? lower_c(line[tail]) : 0;
            int end_after = tail + 1 >= we || !is_alpha_c(line[tail + 1]);
            if (!is_alpha_c(t0) && prev == 'l' && e - 2 >= ws && !is_vowel_c(line[e - 2])) {
                /* "-ble", "-tle": a syllable of its own */
            } else if (!is_alpha_c(t0) || (t0 == 's' && end_after) ||
                       (t0 == 'd' && end_after && prev != 't' && prev != 'd')) {
                ng--;
            }
        }
        starts[n++] = (uint8_t)ws;
        for (k = 1; k < ng && n < max; k++)
            starts[n++] = (uint8_t)split_at(line, ge[k - 1], gs[k]);
    }
    return n;
}
