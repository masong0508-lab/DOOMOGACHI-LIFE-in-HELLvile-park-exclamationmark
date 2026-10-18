/* story_talk.c - small talk (owner: story).
 *
 * Every one of the 1000 citizens can chat: a greeting shaped by their
 * personality sliders, then a topic drawn from who they are (job,
 * catchphrase, favourite food / hobby / music, fear, bio) and from what is
 * happening (chapter 2's purple weirdness, the invasion, Hellvile's freed
 * prisoners, the festival, night time, possession, ghosts). The story
 * cast has its own idle chatter per chapter. Talking to someone new makes
 * a friend.
 */
#include "story_int.h"
#include "story_ch12.h"

void script_ambient_talked(int id, int new_friend);

/* ---- {token} formatting for small talk ---------------------------------------
 *   {N} name  {J} job  {F} favourite food  {H} hobby  {M} music  {X} fear
 *   {C} catchphrase  {B} bio  {Y} years of vapor duty  {P} player  {E} pet */
static int put(char *o, int cap, int n, const char *s)
{
    while (s && *s && n < cap - 1) o[n++] = *s++;
    return n;
}

static void tfmt(char *o, int cap, const char *t, int id)
{
    const DoomoResident *r = town_res(id);
    int n = 0;
    char yrs[8];
    while (*t && n < cap - 1) {
        if (t[0] == '{' && t[1] && t[2] == '}') {
            const char *s = "";
            switch (t[1]) {
            case 'N': s = r ? r->name : ""; break;
            case 'J': s = (id < DOOMO_MAX_RESIDENTS && roster_job(id)[0] && !(r->flags & RF_CUSTOM)) ? roster_job(id) : "neighbour"; break;
            case 'F': s = r ? doomo_food_names[r->like_food % FOOD_COUNT] : ""; break;
            case 'H': s = r ? doomo_hobby_names[r->like_hobby % HOBBY_COUNT] : ""; break;
            case 'M': s = r ? doomo_music_names[r->like_music % MUSIC_COUNT] : ""; break;
            case 'X': s = r ? doomo_fear_names[r->fear % FEAR_COUNT] : ""; break;
            case 'C': s = r ? doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT] : ""; break;
            case 'B': s = (id < DOOMO_MAX_RESIDENTS && !(r->flags & RF_CUSTOM)) ? roster_bio(id) : "A proud citizen of Hellvile Park."; break;
            case 'Y': doomo_snprintf(yrs, sizeof(yrs), "%d", 3 + (int)(doomo_hash32((uint32_t)id) % 40)); s = yrs; break;
            case 'P': s = story_player_name(); break;
            case 'E': s = story_pet_name(); break;
            default: break;
            }
            n = put(o, cap, n, s);
            t += 3;
            continue;
        }
        o[n++] = *t++;
    }
    o[n] = 0;
}

/* ---- the lines ------------------------------------------------------------------- */
static const char *const greet_polite[] = {
    "Oh, hello there, {P}!", "Good day to you, {P}!", "Why, if it isn't {P}! How lovely.",
    "Welcome, welcome! Mind the flowers.", "Hello, dear! Isn't it a fine day?"
};
static const char *const greet_plain[] = {
    "Hey, {P}!", "Hi hi!", "Oh! Hello!", "Heya, neighbour!", "Hi! I'm {N}!"
};
static const char *const greet_blunt[] = {
    "Yo.", "What do YOU want? ...Kidding. Hi.", "Sup.", "Make it quick, I'm busy being fabulous.",
    "Oh. It's you. The new one."
};
static const char *const greet_quirky[] = {
    "Bonjour, gravity!", "Greetings, fellow carbon blob!", "HELLO. I am a normal citizen.",
    "Shh! The pigeons are listening.", "Did you hear that? That was my tummy. It says hi."
};

static const char *const topic_life[] = {
    "I'm the {J} around here. {C}",
    "I could eat {F} every single day. Every. Single. Day.",
    "Do you like {H}? It's basically my whole personality.",
    "{M} is the only music that truly understands me.",
    "Don't tell anyone, but I'm scared of {X}. Shh!",
    "{C} ...Sorry, I say that a lot.",
    "If you ever need a {J}, you know where to find me!",
    "My dream? A big plate of {F} and a whole day of {H}.",
    "Have you met {E}? Your Doomogachi is SO cute!",
    "Being a {J} is hard work, but somebody has to do it. {C}"
};

static const char *const topic_weird[] = {
    "Have you tried the new vapor shop? I had one puff. Then eleven.",
    "Is it just me or is everyone humming the same note? Hmmmmmmmmm...",
    "I dreamed the Wall was breathing. In. Out. In. Out.",
    "Vex says I have a 'very possessable aura'. Isn't that sweet of him?",
    "My {F} tastes like grape now. Everything tastes like grape.",
    "I keep hearing giggles from the drains. Grape-flavoured giggles."
};

static const char *const topic_invaded[] = {
    "The sky is RED! Why is the sky RED?!",
    "A demon ate my {F}! The whole thing! In one bite!",
    "I'm hiding. You didn't see me. I'm a bush. Rustle rustle.",
    "Thank you for fighting for us, {P}! Go get 'em!",
    "My {H} stuff is all singed. But I'm alive! Yay?",
    "I'm a {J}, not a demon hunter! Please hurry!"
};

static const char *const topic_hellvile[] = {
    "Vapula made me pump vapor at the gate for {Y} years. My lungs are purple.",
    "Is it true Hillvile has flowers that smell nice? On PURPOSE?",
    "I'm the {J} here. Well, I was supposed to be. Vapula made everyone a gate laborer.",
    "We were never allowed near the Wall. Never. And now you just... walked through it.",
    "I've always wanted to try {F}. Real {F}, from the good side.",
    "Do Hillvile folks really like {H}? Me too! Can we be friends?",
    "{C} ...That's how we say hello here. Sort of."
};

static const char *const topic_softie[] = {
    "I baked you something. It's burnt. It's fine. It's love.",
    "Please don't tell anyone I'm nice. I have a reputation.",
    "I knitted a tiny scarf for every bat on our street. They love it.",
    "Shh... I water the little plants by the gate when nobody's looking."
};

static const char *const topic_loyalist[] = {
    "Hmph. Vapula was... ugh. Fine. He was AWFUL. Happy now?",
    "I served the Duke for decades. Now I don't know what to do with my hands.",
    "Don't expect a hug, Hillvile. ...Maybe a handshake. A small one."
};

static const char *const topic_festival[] = {
    "I'm from Hillvile and I just hugged an imp! He was so WARM!",
    "Someone gave me a cupcake and I didn't even have to sign a contract!",
    "Best. Festival. EVER.",
    "Have you seen the fireworks? Hearts AND skulls! Together!",
    "{F} AND bone soup on one table! At last!",
    "My new best friend lives on the other side of the river. Isn't that wild?",
    "I'm going to dance until my horns fall off. Figuratively. Mostly."
};

static const char *const topic_night[] = {
    "*yawn* It's late. Even the ducks are asleep.",
    "The stars are so pretty tonight. Hellvile's stars look the same, did you know?",
    "Shouldn't you be in bed, {P}? {E} looks sleepy."
};

static const char *const topic_possessed[] = {
    "...{P}... the cloud wants you too...",
    "{N} isn't here right now. Leave a message after the scream.",
    "heh. heh heh. Don't look at my eyes. LOOK AT MY EYES.",
    "We moved in. It's cosy in here. So many memories to chew on."
};

static const char *const topic_ghost[] = {
    "Boo! ...Sorry. Old habit.", "Being a ghost is fine. I just float through my problems now.",
    "I can see the graveyard from up here. They kept my flowers fresh."
};

static const char *pick(const char *const *a, int n, uint32_t h) { return a[h % (uint32_t)n]; }

const char *story_small_talk(int id, int variant, char *buf, int cap)
{
    const DoomoResident *r = town_res(id);
    const char *t;
    uint32_t h = doomo_hash32((uint32_t)id * 2654435761u + (uint32_t)variant * 40503u + doomo_town.day);
    int ch = story_chapter(), b = story_beat(), map = story_map();
    int traits = id < DOOMO_MAX_RESIDENTS ? roster_traits(id) : 0;
    if (!r || r->status == RES_EMPTY) { buf[0] = 0; return buf; }
    if (variant == 0) {
        int sp = r->persona[PERS_SPEECH], q = r->persona[PERS_QUIRK];
        if (r->status == RES_POSSESSED) t = pick(topic_possessed, DOOMO_COUNTOF(topic_possessed), h);
        else if (q >= 12) t = pick(greet_quirky, DOOMO_COUNTOF(greet_quirky), h);
        else if (sp <= 4) t = pick(greet_polite, DOOMO_COUNTOF(greet_polite), h);
        else if (sp >= 11) t = pick(greet_blunt, DOOMO_COUNTOF(greet_blunt), h);
        else t = pick(greet_plain, DOOMO_COUNTOF(greet_plain), h);
    } else if (r->status == RES_POSSESSED) {
        t = pick(topic_possessed, DOOMO_COUNTOF(topic_possessed), h);
    } else if (r->status == RES_GHOST) {
        t = pick(topic_ghost, DOOMO_COUNTOF(topic_ghost), h);
    } else if (map == MAPID_PARK_UNITED) {
        t = (h & 1) ? pick(topic_festival, DOOMO_COUNTOF(topic_festival), h >> 1)
                    : pick(topic_life, DOOMO_COUNTOF(topic_life), h >> 1);
    } else if (map == MAPID_HAHALAND_INVADED || (ch == STORY_CH3 && map != MAPID_HELLVILE)) {
        t = pick(topic_invaded, DOOMO_COUNTOF(topic_invaded), h);
    } else if (RES_DISTRICT(id) == DISTRICT_HELLVILE && ch <= STORY_CH4 && b < BEAT_C4_FESTIVAL) {
        if ((traits & ROSTER_T_SOFTIE) && (h & 1)) t = pick(topic_softie, DOOMO_COUNTOF(topic_softie), h >> 1);
        else if ((traits & ROSTER_T_LOYALIST) && (h & 1)) t = pick(topic_loyalist, DOOMO_COUNTOF(topic_loyalist), h >> 1);
        else t = pick(topic_hellvile, DOOMO_COUNTOF(topic_hellvile), h);
    } else if (b == BEAT_C2_WEIRD || (ch == STORY_CH2 && (h % 3) == 0)) {
        t = pick(topic_weird, DOOMO_COUNTOF(topic_weird), h);
    } else if (town_is_night() && (h % 3) == 0) {
        t = pick(topic_night, DOOMO_COUNTOF(topic_night), h);
    } else {
        t = pick(topic_life, DOOMO_COUNTOF(topic_life), h);
    }
    tfmt(buf, cap, t, id);
    return buf;
}

/* ---- the cast's idle chatter ---------------------------------------------------------- */
typedef struct { int16_t id; uint8_t ch; const char *text; } CastLine;
static const CastLine cast_lines[] = {
    { ID_MAYOR, 3, "I'm not scared! I'm hiding BRAVELY. There's a difference!" },
    { ID_MAYOR, 4, "A summit! With Hellvile! I've never summited anything in my life!" },
    { ID_MAYOR, 5, "Oh my stars, look at everyone! Together! I'm crying again! Happy crying!" },
    { ID_NANA, 3, "If a demon touches my sourdough starter, I will become the demon." },
    { ID_NANA, 4, "I hear Hellvile has a baker who burns everything. I must meet her. For science." },
    { ID_NANA, 5, "Charlotte and I are writing a cookbook! 'Burnt and Sweet'. It'll be a hit." },
    { ID_PIP, 3, "Father Fuzz says if we hold hands and hum, the demons can't hear us. Hmmm..." },
    { ID_PIP, 4, "Do kids in Hellvile have plushies? I'm going to bring them ALL plushies." },
    { ID_PIP, 5, "I made a friend from Hellvile! Her name is Morguelet and she likes BUGS too!" },
    { ID_FUZZ, 3, "We'll keep everyone safe in here. Go on, @P. We'll pray in rhythm for you." },
    { ID_FUZZ, 4, "Every soul over the Wall has a name. Learn them, @P. That is how peace begins." },
    { ID_FUZZ, 5, "Mother Nox and I are co-hosting the potluck. Hymns and bone soup. A fine pair." },
    { ID_RIVET, 3, "Bring me scrap and I'll make your weapon sing. Or scream. Your call." },
    { ID_RIVET, 4, "Mum's forge still smells the same. Hot metal and burnt toast. Home." },
    { ID_RIVET, 5, "Teaching Hellvile kids AND Hillvile kids to weld. Safety goggles for all!" },
    { ID_BLOOM, 3, "Bandages, antiseptic, deep breaths. We'll get through this." },
    { ID_BLOOM, 4, "Hellvile's lungs need help after all that vapor. I'm opening a clinic over there." },
    { ID_BLOOM, 5, "Everyone's healthy, happy and dancing. Best day of my afterlife." },
    { ID_VEX, 4, "...I lost everything. The shop, the Duke, the cloud. And I feel... lighter?" },
    { ID_VEX, 5, "Plain water vapes! No flavour! No demons! ...No customers, either." },
    { ID_LULLA, 5, "Hellvile kids and Hillvile kids in one daycare! The naps are chaos. I love it." },
    { ID_STEAMY, 4, "I'm opening a second bath house in Hellvile. Lava-heated! Very eco." },
    { ID_TALLYWORN, 4, "Day one of freedom. Then day two. I'm going to count these days forever." },
    { ID_TALLYWORN, 5, "I stopped counting days. Now I count friends. It's a much bigger number." },
    { ID_MOTHER_NOX, 4, "Have some soup. Everyone who comes to my door gets soup. That is the law." },
    { ID_MOTHER_NOX, 5, "The potluck feeds both sides now. We'll need a bigger pot. A MUCH bigger pot." },
    { ID_SOOTKIN, 4, "Look! The sunflower turned toward the gate. It's looking for Hillvile." },
    { ID_ANVIL, 4, "Rivet's room is still just how she left it. Posters of welding torches. Bless her." },
    { ID_LADY_SMOG, 4, "I'm taking up gardening. Real air for real plants. Smells divine." },
    { ID_HAZE, 4, "I hated that shop. I watered its plants every day so SOMETHING there was alive." },
    { ID_VAPULA, 5, "*tiny wheeze* I was a Duke. Now I'm a decorative cloud. It's... relaxing, honestly." },
};

const char *talk_cast_idle(int id)
{
    int i, ch = story_chapter(), n = 0, pickn;
    const char *found[8];
    static char idle[112];                             /* S2: Chapters 1-2 are pack text */
    if (ch == STORY_CH_DONE) ch = 5;
    if (story_map() == MAPID_PARK_UNITED) ch = 5;
    if (ch <= 2) return ch12_idle(id, ch, idle, (int)sizeof(idle)) ? idle : NULL;
    for (i = 0; i < DOOMO_COUNTOF(cast_lines) && n < 8; i++)
        if (cast_lines[i].id == id && cast_lines[i].ch == ch) found[n++] = cast_lines[i].text;
    if (!n) {
        /* nearest earlier chapter (S2: Chapters 1-2 from the packs) */
        for (i = 0; i < DOOMO_COUNTOF(cast_lines) && n < 8; i++)
            if (cast_lines[i].id == id && cast_lines[i].ch < ch) found[0] = cast_lines[i].text, n = 1;
        if (!n) return (ch12_idle(id, 2, idle, (int)sizeof(idle)) || ch12_idle(id, 1, idle, (int)sizeof(idle))) ? idle : NULL;
    }
    if (!n) return NULL;
    pickn = (int)((doomo_town.minute / 7 + doomo_town.day) % (uint32_t)n);
    return found[pickn];
}

/* ---- a chat ------------------------------------------------------------------------------ */
#define RECENT 24
static int16_t recent[RECENT];
static uint8_t recent_n, recent_i;

static int talked_recently(int id)
{
    int i;
    for (i = 0; i < recent_n; i++) if (recent[i] == id) return 1;
    return 0;
}

static void remember(int id)
{
    recent[recent_i] = (int16_t)id;
    recent_i = (uint8_t)((recent_i + 1) % RECENT);
    if (recent_n < RECENT) recent_n++;
}

void talk_ambient(int id)
{
    DoomoResident *r = town_res(id);
    char a[160], b[160];
    const char *cast;
    int before, newf = 0, fresh, castable;
    static uint16_t variant;
    if (!r || r->status == RES_EMPTY || id == DOOMO_PLAYER_ID) return;
    fresh = !talked_recently(id);
    before = r->level;
    castable = roster_npc_of(id) >= 0 || (r->flags & RF_STORY_NPC) || id == ID_LULLA || id == ID_STEAMY ||
               id == ID_TALLYWORN || id == ID_MOTHER_NOX || id == ID_SOOTKIN || id == ID_ANVIL ||
               id == ID_LADY_SMOG || id == ID_HAZE;
    cast = castable ? talk_cast_idle(id) : NULL;
    if (!cast && story_chapter() >= 1 && story_chapter() <= STORY_CH2 &&   /* S2: Hattie, Grampy, the mascot */
        ch12_idle(id, story_chapter(), a, (int)sizeof(a)))
        cast = a;
    if (cast && r->status != RES_POSSESSED) {
        story_say(id, cast);
    } else {
        variant++;
        story_small_talk(id, 0, a, sizeof(a));
        story_small_talk(id, 1 + (variant & 7), b, sizeof(b));
        if (before == 0 && fresh && id < DOOMO_MAX_RESIDENTS && r->status == RES_ALIVE) {
            char bio[120];
            tfmt(bio, sizeof(bio), "({N} the {J}: {B})", id);
            story_say(-1, bio);
        }
        story_say(id, a);
        story_say(id, b);
    }
    if (fresh && r->status == RES_ALIVE) {
        story_befriend(id, STORY_FRIEND_LEVEL);
        newf = before < STORY_FRIEND_LEVEL && r->level >= STORY_FRIEND_LEVEL;
        r->stat[STAT_SOCIAL] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SOCIAL] + 10);
        r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + 3);
        if (newf) ui_toast(STR_ICON_HEART " New friend!");
        remember(id);
    }
    script_ambient_talked(id, newf || (fresh && r->status == RES_ALIVE && story_beat() != BEAT_C1_FRIENDS));
}
