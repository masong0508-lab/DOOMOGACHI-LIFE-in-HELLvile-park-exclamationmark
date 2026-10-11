/* sim_care.c - the player's care actions and the residents' reactions
 * (owner: life).
 *
 * Every reaction depends on who you're caring for: their favourite and
 * hated food, hobby, fears, species and personality "tone" (bubbly,
 * sweet, blunt or odd). Feeding someone their favourite food makes them
 * ecstatic; their hated food makes them gag. The possessed answer with
 * the demon's voice.
 *
 * Items are taken from the inventory here (OPT_CHEAT_INFINITE_CANDY
 * skips that). Reaction strings live in two rotating static buffers.
 */
#include "sim_int.h"

enum { TONE_SWEET, TONE_BUBBLY, TONE_BLUNT, TONE_ODD, TONE_COUNT };

static int tone_of(const DoomoResident *r)
{
    if (r->persona[PERS_QUIRK] >= 13) return TONE_ODD;
    if (r->persona[PERS_SPEECH] >= 11) return TONE_BLUNT;
    if (r->persona[PERS_EXPRESSION] >= 10) return TONE_BUBBLY;
    return TONE_SWEET;
}

static char g_buf[2][96];
static int g_which;

static const char *say(const char *fmt, const char *arg)
{
    char *b = g_buf[g_which];
    g_which ^= 1;
    doomo_snprintf(b, (int)sizeof(g_buf[0]), fmt, arg ? arg : "");
    return b;
}

static const char *pick_tone(const DoomoResident *r, const char *const t[TONE_COUNT],
                             const char *arg)
{
    return say(t[tone_of(r)], arg);
}

/* The demon talks back. */
static const char *demon_line(int id)
{
    static const char *const lines[] = {
        "\"%s IS NOT HOME RIGHT NOW.\"",
        "\"MORE... SPRINKLES... FOR THE PIT...\"",
        "*growls in a voice that isn't %s's*",
        "\"YOUR KINDNESS TICKLES. STOP IT.\"",
        "\"FATHER FUZZ CAN'T SAVE THIS ONE, HEHEHE.\"",
        "*the eyes glow red. %s twitches.*"
    };
    return say(lines[sim_rand(DOOMO_COUNTOF(lines))], sim_name(id));
}

static DoomoResident *care_target(int id, const char **fail)
{
    DoomoResident *r = town_res(id);
    sim_ensure_init();
    *fail = NULL;
    if (!r || r->status == RES_EMPTY) { *fail = "...there's nobody here."; return NULL; }
    if (r->status == RES_DEAD) { *fail = "...only silence, and a few flowers."; return NULL; }
    return r;
}

static void cared(int id, DoomoResident *r, int level, int bond)
{
    r->level = (uint8_t)DOOMO_MIN(99, r->level + level);
    if (bond) rel_adjust(id, DOOMO_PLAYER_ID, bond);
    sim_mood_of(id);
}

static int take_item(int item)
{
    if (opt_get(OPT_CHEAT_INFINITE_CANDY)) return 1;
    return inv_take(item, 1);
}

static void apply_item(DoomoResident *r, int item, int pct)
{
    const DoomoItemInfo *it = &doomo_items[item];
    sim_stat_add(r, STAT_HUNGER, it->hunger * pct / 100);
    sim_stat_add(r, STAT_HAPPY, it->happy * pct / 100);
    sim_stat_add(r, STAT_HYGIENE, it->hygiene * pct / 100);
    sim_stat_add(r, STAT_ENERGY, it->energy * pct / 100);
    sim_stat_add(r, STAT_HEALTH, it->health * pct / 100);
    sim_stat_add(r, STAT_SANITY, it->sanity * pct / 100);
    if (r->status == RES_ALIVE) sim_stat_add(r, STAT_CORRUPTION, it->corruption * pct / 100);
}

static void resolve_kind(int id, int kind)
{
    int pi = sim_problem_find(id, kind);
    if (pi >= 0) sim_problem_remove(pi);
}

static const char *missing(int item)
{
    return say("You don't have any %s!", doomo_items[item].name);
}

/* ---------------------------------------------------------------------- */
const char *sim_feed(int id, int item)
{
    static const char *const fav[TONE_COUNT] = {
        "My favourite! %s! You remembered!",
        "!!! %s !!! MY FAVOURITE!! I love you!!",
        "...%s. Okay. You win. Best day ever.",
        "%s!! My tummy is doing a happy dance!"
    };
    static const char *const hate[TONE_COUNT] = {
        "Oh... %s. *gulp* ...thank you anyway.",
        "BLEH! %s?! *gag* Why would you DO this?!",
        "%s. Gross. Are you trying to poison me?",
        "*gag* %s tastes like a sad Tuesday!"
    };
    static const char *const ok[TONE_COUNT] = {
        "Yummy! Thank you for the %s.",
        "Nom nom nom! %s is SO good!",
        "Hm. %s. Not bad.",
        "I shall name this %s 'Gerald'. *chomp*"
    };
    static const char *const full[TONE_COUNT] = {
        "I'm so full... but %s is nice.",
        "One more bite of %s and I'll POP!",
        "I'm stuffed. Save the %s for later.",
        "My belly has filed a complaint. %s..."
    };
    const char *fail, *food;
    DoomoResident *r = care_target(id, &fail);
    int fi;
    if (!r) return fail;
    if (item <= ITEM_NONE || item >= ITEM_COUNT || doomo_items[item].category != ICAT_FOOD)
        return "I can't eat that!";
    if (!take_item(item)) return missing(item);
    food = doomo_items[item].name;
    progress_add(PROG_MEALS_SERVED, 1);
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        apply_item(r, item, 30);
        return demon_line(id);
    }
    fi = item - ITEM_CUPCAKE;                         /* doomo_food_names[] */
    resolve_kind(id, PROB_HUNGRY);
    if (r->status == RES_GHOST) {
        sim_stat_add(r, STAT_HAPPY, 8);
        cared(id, r, 1, 1);
        return say("The %s falls right through me... it's the thought!", food);
    }
    if (fi == r->like_food) {
        apply_item(r, item, 150);
        sim_stat_add(r, STAT_HAPPY, 30);
        sim_set_transient(id, TR_ECSTATIC, 6);
        cared(id, r, 4, 6);
        r->mood = MOOD_ECSTATIC;
        return pick_tone(r, fav, food);
    }
    if (fi == r->hate_food) {
        apply_item(r, item, 50);
        sim_stat_add(r, STAT_HAPPY, -18);
        sim_set_transient(id, r->persona[PERS_SPEECH] >= 10 ? TR_ANGRY : TR_SAD, 2);
        cared(id, r, 0, -3);
        return pick_tone(r, hate, food);
    }
    if (r->stat[STAT_HUNGER] >= 92) {
        apply_item(r, item, 40);
        sim_stat_add(r, STAT_ENERGY, -3);
        cared(id, r, 0, 0);
        return pick_tone(r, full, food);
    }
    apply_item(r, item, 100);
    cared(id, r, 1, 2);
    if (doomo_species_info[r->look.species % SPECIES_COUNT].flags & SPF_MECHANICAL)
        return say("%s: fuel accepted. Beep boop! Thank you.", food);
    return pick_tone(r, ok, food);
}

/* Does this gift match what they love? 2 = adore, 1 = like, -1 = fear */
static int gift_taste(const DoomoResident *r, int item)
{
    if (doomo_items[item].category == ICAT_FOOD) {
        int fi = item - ITEM_CUPCAKE;
        return fi == r->like_food ? 2 : fi == r->hate_food ? -1 : 1;
    }
    switch (item) {
    case ITEM_VIDEO_GAME: return r->like_hobby == 5 ? 2 : 1;          /* Video Games   */
    case ITEM_BOARD_GAME: return r->like_hobby == 14 ? 2 : 1;         /* Board Games   */
    case ITEM_SKULL_TRINKET:
        if (r->like_hobby == 31) return 2;                            /* Skull Collecting */
        return (r->flags & RF_HELLBORN) ? 2 : (r->fear == 4 ? -1 : 1);
    case ITEM_PLUSHIE:
        if (r->fear == 22) return -1;                                 /* Cute Things   */
        return r->like_hobby == 20 ? 2 : 1;                           /* Plush Making  */
    case ITEM_FLOWERS: return r->like_hobby == 2 ? 2 : (r->fear == 8 ? -1 : 1); /* bees! */
    case ITEM_HOLY_WATER: case ITEM_ROSARY: case ITEM_PLUSH_CROSS: case ITEM_HYMN_BOOK:
        if (r->fear == 17 || r->fear == 19 || r->fear == 20) return -1;
        return (r->flags & RF_HELLBORN) ? 0 : 1;
    case ITEM_DEMON_HORN: case ITEM_HELLSTEEL:
        return r->like_hobby == 30 ? 2 : (r->flags & RF_HELLBORN) ? 1 : 0;   /* Welding */
    case ITEM_GLITTER: return r->like_hobby == 6 ? 2 : 1;             /* Painting      */
    default: return 1;
    }
}

const char *sim_gift(int id, int item)
{
    static const char *const adore[TONE_COUNT] = {
        "A %s! It's exactly what I wanted!",
        "A %s?!?! AAAAH! I'm gonna CRY!",
        "...A %s. Okay, you actually get me.",
        "A %s! It shall sit upon my throne of socks!"
    };
    static const char *const like[TONE_COUNT] = {
        "A %s? How thoughtful, thank you!",
        "Ooh, a %s! Thank you thank you!",
        "A %s. Thanks, I guess.",
        "A %s! I will whisper secrets to it."
    };
    static const char *const fear[TONE_COUNT] = {
        "A %s... um. I'm scared of those...",
        "EEK! A %s! Get it AWAY!",
        "A %s. Seriously? You know I hate that.",
        "The %s is staring at me. Make it stop."
    };
    static const char *const meh[TONE_COUNT] = {
        "A %s? Oh! Um... thanks.",
        "A %s? Huh! Interesting!",
        "What am I supposed to do with a %s?",
        "A %s. I shall eat it. Wait, no."
    };
    const char *fail, *name;
    DoomoResident *r = care_target(id, &fail);
    int t;
    if (!r) return fail;
    if (item <= ITEM_NONE || item >= ITEM_COUNT || doomo_items[item].category == ICAT_KEY)
        return "That's not really a gift...";
    if (!take_item(item)) return missing(item);
    name = doomo_items[item].name;
    progress_add(PROG_GIFTS_GIVEN, 1);
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        if (doomo_items[item].holy) {
            sim_stat_add(r, STAT_SANITY, 6);
            return say("\"HSSSS! TAKE YOUR %s AWAY!\"", name);
        }
        return demon_line(id);
    }
    {
        int pi = sim_problem_find(id, PROB_WANTS_ITEM);
        if (pi >= 0 && sim_problem(pi)->item == item) {
            sim_problem_remove(pi);
            sim_stat_add(r, STAT_HAPPY, 20);
        }
    }
    if (item == ITEM_RING) {
        apply_item(r, item, 100);
        cared(id, r, 6, 10);
        if (sim_partner_of(id) >= 0)
            return "A ring?! I'll give it to my sweetheart... or keep it?";
        return r->persona[PERS_EXPRESSION] >= 8 ? "A RING?! For ME?! *faints happily*"
                                                : "A ring. ...I'll treasure it. Forever.";
    }
    t = gift_taste(r, item);
    if (t == 2) {
        apply_item(r, item, 140);
        sim_stat_add(r, STAT_HAPPY, 20);
        sim_set_transient(id, TR_ECSTATIC, 5);
        cared(id, r, 5, 8);
        return pick_tone(r, adore, name);
    }
    if (t < 0) {
        sim_stat_add(r, STAT_HAPPY, -10);
        sim_stat_add(r, STAT_SANITY, -6);
        sim_set_transient(id, TR_SCARED, 2);
        cared(id, r, 0, -2);
        return pick_tone(r, fear, name);
    }
    if (t == 0) {
        apply_item(r, item, 40);
        cared(id, r, 1, 1);
        return pick_tone(r, meh, name);
    }
    apply_item(r, item, 100);
    cared(id, r, 2, 4);
    return pick_tone(r, like, name);
}

const char *sim_clean(int id)
{
    static const char *const ok[TONE_COUNT] = {
        "Ahh, all fresh and fluffy. Thank you!",
        "Bubbles!! I smell like a CUPCAKE!",
        "Fine. I was getting a bit ripe.",
        "Scrub-a-dub! I am reborn as foam!"
    };
    static const char *const scared[TONE_COUNT] = {
        "N-not bath time... *shivers*",
        "NOOO! NOT BATH TIME! *flails*",
        "Ugh. Water. My nemesis.",
        "The tub has teeth! I KNOW it does!"
    };
    const char *fail;
    DoomoResident *r = care_target(id, &fail);
    int sp;
    if (!r) return fail;
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        sim_stat_add(r, STAT_HYGIENE, 40);
        return demon_line(id);
    }
    sp = r->look.species;
    resolve_kind(id, PROB_DIRTY);
    if (r->status == RES_GHOST) return "You scrub the air. The ghost giggles.";
    if (r->stat[STAT_HYGIENE] >= 95) {
        cared(id, r, 0, 0);
        return "I'm already sparkly clean!";
    }
    r->stat[STAT_HYGIENE] = 100;
    if (r->fear == 13) {                                  /* Bath Time */
        sim_stat_add(r, STAT_HAPPY, -8);
        sim_set_transient(id, TR_SCARED, 1);
        cared(id, r, 1, 1);
        return pick_tone(r, scared, NULL);
    }
    sim_stat_add(r, STAT_HAPPY, 6);
    cared(id, r, 1, 2);
    if (doomo_species_info[sp % SPECIES_COUNT].flags & SPF_SQUISHY)
        return "Squeaky clean! ...still slimy. That's just me!";
    if (doomo_species_info[sp % SPECIES_COUNT].flags & SPF_MECHANICAL)
        return "Polished to a shine. *happy whirr*";
    if (sp == SPECIES_SHEEPKIN || sp == SPECIES_CATFOLK || sp == SPECIES_RACCOONKIN ||
        sp == SPECIES_FOXKIN || sp == SPECIES_BEARKIN || sp == SPECIES_PUPKIN)
        return "*shakes off* My fur is SO fluffy now!";
    return pick_tone(r, ok, NULL);
}

const char *sim_play(int id)
{
    static const char *const fun[TONE_COUNT] = {
        "That was lovely! Let's do %s again soon!",
        "WHEEE! %s with you is the BEST!",
        "...Fine, %s was fun. Don't tell anyone.",
        "%s! I have become one with the fun!"
    };
    static const char *const tired[TONE_COUNT] = {
        "I'm a little too sleepy to play...",
        "Five more minutes of nap first! Pleeease?",
        "Too tired. Go away. ...Nicely.",
        "My legs have gone on strike."
    };
    const char *fail, *hobby;
    DoomoResident *r = care_target(id, &fail);
    if (!r) return fail;
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        sim_stat_add(r, STAT_SANITY, -2);
        return demon_line(id);
    }
    if (r->stat[STAT_ENERGY] < 15 && r->status != RES_GHOST) {
        cared(id, r, 0, 0);
        return pick_tone(r, tired, NULL);
    }
    hobby = doomo_hobby_names[r->like_hobby % HOBBY_COUNT];
    sim_stat_add(r, STAT_ENERGY, -10);
    sim_stat_add(r, STAT_HAPPY, 16);
    sim_stat_add(r, STAT_SOCIAL, 14);
    sim_stat_add(r, STAT_HUNGER, -4);
    resolve_kind(id, PROB_BORED);
    resolve_kind(id, PROB_LONELY);
    cared(id, r, 2, 4);
    if (r->status == RES_GHOST) return "Boo! Tag, you're it! *phases through a wall*";
    return pick_tone(r, fun, hobby);
}

const char *sim_talk(int id)
{
    const char *fail;
    DoomoResident *r = care_target(id, &fail);
    int roll, pi;
    if (!r) return fail;
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) return demon_line(id);
    sim_stat_add(r, STAT_SOCIAL, 6);
    cared(id, r, 1, 1);
    if (r->status == RES_GHOST) return "Ooooo... I mean, hi! Being dead is chill.";
    pi = sim_problem_find(id, -1);
    if (pi >= 0) {
        static char pbuf[64];
        return sim_problem_text(pi, pbuf, sizeof(pbuf));
    }
    roll = sim_rand(8);
    switch (r->mood) {
    case MOOD_LOVESICK: return "Do you think... someone could like me back?";
    case MOOD_ANGRY: return r->persona[PERS_SPEECH] >= 10 ? "Not now. I'm FUMING." : "Hmph! I'm a bit cross.";
    case MOOD_SAD: return "I'm feeling a little blue today...";
    case MOOD_SCARED: return "Did you hear that?! Something's out there...";
    case MOOD_CORRUPTED: return "The purple fog... it sings to me...";
    case MOOD_SLEEPY: return "*yaaawn* Is it bedtime yet?";
    default: break;
    }
    switch (roll) {
    case 0: case 1:
        return say("%s", doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT]);
    case 2: return say("Have you tried %s? It's my favourite!",
                       doomo_food_names[r->like_food % FOOD_COUNT]);
    case 3: return say("I've been really into %s lately!",
                       doomo_hobby_names[r->like_hobby % HOBBY_COUNT]);
    case 4: return say("I can't stand %s. Just saying.",
                       doomo_food_names[r->hate_food % FOOD_COUNT]);
    case 5: return say("Don't tell anyone, but I'm scared of %s.",
                       doomo_fear_names[r->fear % FEAR_COUNT]);
    case 6: return say("Tonight's %s night at the concert hall!",
                       doomo_music_names[sim_concert_genre()]);
    default:
        if (r->flags & RF_HELLBORN) return "Hellvile's not so bad. We have snacks.";
        return "Hellvile Park is the best place ever!";
    }
}

const char *sim_heal(int id, int item)
{
    static const char *const ok[TONE_COUNT] = {
        "Thank you... I feel much better.",
        "I'm CURED! I could run a marathon!",
        "Tastes awful. Works, though. Thanks.",
        "My insides are doing a little jig!"
    };
    const char *fail;
    DoomoResident *r = care_target(id, &fail);
    if (!r) return fail;
    if (item <= ITEM_NONE || item >= ITEM_COUNT ||
        (doomo_items[item].category != ICAT_CARE && doomo_items[item].category != ICAT_EXORCISM))
        return "That's not medicine!";
    if (!take_item(item)) return missing(item);
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        apply_item(r, item, 50);
        if (doomo_items[item].holy) return "\"HSSS! That stings! You'll need more than that!\"";
        return demon_line(id);
    }
    apply_item(r, item, 100);
    resolve_kind(id, PROB_SICK);
    if (item == ITEM_CHILL_PILL) resolve_kind(id, PROB_NIGHTMARE);
    if (doomo_items[item].holy) resolve_kind(id, PROB_CORRUPTION);
    cared(id, r, 2, 3);
    if (r->fear == 9 && item == ITEM_MEDICINE) return "Is there a NEEDLE?! ...oh, it's syrup. Phew.";
    if (item == ITEM_ENERGY_DRINK) return "WOAH! I CAN SEE SOUNDS! THANK YOU!";
    if (item == ITEM_CHILL_PILL) return "Mmm... everything is soft and okay now.";
    return pick_tone(r, ok, NULL);
}

const char *sim_comfort(int id)
{
    static const char *const ok[TONE_COUNT] = {
        "Thank you... I needed that.",
        "*sniffle* You're the BEST! Group hug!",
        "...Okay. That helped. A little.",
        "Your hug has the texture of a cloud."
    };
    const char *fail;
    DoomoResident *r = care_target(id, &fail);
    int tr;
    if (!r) return fail;
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        sim_stat_add(r, STAT_SANITY, 4);
        return r->status == RES_SOUL_LOST ? "...the eyes stare through you. Nobody's home."
                                          : demon_line(id);
    }
    tr = sim_transient(id);
    sim_stat_add(r, STAT_SANITY, 15);
    sim_stat_add(r, STAT_HAPPY, 8);
    sim_stat_add(r, STAT_SOCIAL, 8);
    if (r->status == RES_ALIVE) sim_stat_add(r, STAT_CORRUPTION, -3);
    resolve_kind(id, PROB_NIGHTMARE);
    resolve_kind(id, PROB_LONELY);
    if (tr == TR_SAD || tr == TR_SCARED || tr == TR_ANGRY) sim_set_transient(id, TR_NONE, 0);
    cared(id, r, 2, 3);
    if (r->fear == 21) {                                   /* Hugs */
        sim_stat_add(r, STAT_HAPPY, -6);
        return "Ack! Too much hug! ...but thanks.";
    }
    if (tr == TR_GRIEF) return "I miss them so much... thank you for staying.";
    if (tr == TR_LOVESICK || bit_get(sim_s.crush, id)) return "My heart's a mess... but you help.";
    return pick_tone(r, ok, NULL);
}
