/* talk_scene.c - SCENE_TALK: the playable Talk screen (owner: TK; docs/TALK.md 6h).
 *
 * Native host and the GBA game ROM (DOOMO_HAS_TALK, the link-time twin of PLATF_TALK; the
 * PS1 builds none of it yet). The GBA reads the bank out of the ROM and has no typed line. It puts the whole of
 * Talk in the player's hands:
 *   - quick replies in eight groups (Chat, Be nice, Be cool, Ask, Tell, Others,
 *     Gestures, Romance), every one a TA_* act the drama manager understands;
 *   - the topic list (recent topics, what you learned they like, stock topics);
 *   - a typed line, read by the parser (talk_parse: its safety gate runs first);
 *   - "Talking to": one resident or everyone; invite a third and fourth person in
 *     (the talk starts again with the bigger room; what the first did is applied);
 *   - the idle chatter and the prod (talk_dm_tick), arguments, walk-outs, mediation;
 *   - the outcomes into the sim (talk_outcome_end: the daily caps of design 5.5).
 * Every resident line comes from talk_dm_step() and is shown with ui_say(): the
 * portrait, the voice babble and the face are the dialogue box's. Nothing the player
 * typed is ever shown back except the parser's masked `shown` text.
 *
 * Reached through home_talk_enter(who) (the house's visit and the resident screen's
 * Talk action; weak default "not available" in weak_talk.c) or `push TALK <id>` in
 * the runner. SAFETY is not decided here: romance rows are listed only when
 * talk_safe_romance_ok() says so, and the gate, the lint and the sim stay the
 * authority (docs/TALK.md section 2).
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/playas.h"
#include "doomo/family.h"
#include "doomo/resident_acc.h"
#include "doomo/rng.h"

#ifdef DOOMO_HAS_TALK

/* The typed line needs a keyboard and the free-text parser (talk_lex / talk_parse: native and
 * the Dreamcast). The GBA has neither: it plays Talk from the quick replies and the topic list. */
#ifndef DOOMO_PLATFORM_GBA
#define TALK_TYPING 1
#endif

enum { PG_MAIN = 0, PG_ACTS, PG_TOPIC, PG_INVITE, PG_MORE };
enum { K_CAT = 0, K_ACT, K_TOPIC, K_TYPE, K_TO, K_INVITE, K_BYE, K_PICK, K_NOTOPIC, K_MORE, K_QUICK };
enum { C_CHAT = 0, C_NICE, C_COOL, C_ASK, C_TELL, C_OTHERS, C_GESTURE, C_ROMANCE, C_COUNT };

typedef struct TRow { uint8_t kind, arg; uint16_t v; } TRow;
#define TROWS 40
#define TSTOCK 24

static struct TalkScn {
    TalkState s;
    int       ids[TALK_PARTS_MAX];
    int       n, avatar;
    int8_t    to;                       /* slot spoken to, -1 everyone               */
    uint8_t   page, cat, active, busy, after, leaving, typed_ok, pad;
    uint8_t   pending;                  /* a topic act waiting for its topic         */
    uint16_t  topic, idle;
    int       nrows, nstock;
    TRow      row[TROWS];
    uint16_t  stock[TSTOCK];
    TalkSugg  quick[TALK_QUICK_MAX];     /* the replies to what was just said (top of the main page) */
    uint8_t   nquick;
    uint32_t  qsalt;
    UiList    list;
    char      you[TALK_SHOWN_MAX];
#ifdef TALK_TYPING
    char      typed[TALK_INPUT_MAX + 1];
#endif
    char      note[40];
} T;

static int extra_ids[TALK_PARTS_MAX], extra_n;       /* the others the caller wants in the room */
static uint32_t sessions;                            /* a new seed for every talk of a day      */

/* ---- the text of the quick replies ------------------------------------------------------ */
static const char *const act_text[TA_COUNT] = {
    [TA_GREET] = "Hi there!", [TA_FAREWELL] = "I have to go. Bye!", [TA_THANK] = "Thank you!",
    [TA_APOLOGIZE] = "I'm sorry.", [TA_INTRODUCE] = "Let me introduce myself.",
    [TA_ASK_ABOUT] = "Tell me about %t.", [TA_ASK_OPINION] = "What do you think of %t?",
    [TA_ASK_FEELING] = "How are you feeling?", [TA_ASK_FACT] = "Do you know something?",
    [TA_ASK_WHO] = "Who is that?", [TA_ASK_ADVICE] = "Any advice for me?",
    [TA_ASK_FAVOR] = "Could you do me a favour?",
    [TA_TELL] = "Guess what: %t!", [TA_OPINE_POS] = "I like %t.", [TA_OPINE_NEG] = "I don't like %t.",
    [TA_AGREE] = "I agree.", [TA_DISAGREE] = "I don't think so.", [TA_YES] = "Yes.", [TA_NO] = "No.",
    [TA_COMPLIMENT] = "You're great.", [TA_PRAISE_OTHER] = "The others are great too.",
    [TA_COMFORT] = "It'll be okay.", [TA_ENCOURAGE] = "You can do it!",
    [TA_SYMPATHIZE] = "That sounds hard.", [TA_JOKE] = "Here's a good one...",
    [TA_LAUGH] = "Ha ha ha!", [TA_OFFER] = "Want a hand?", [TA_INVITE] = "Want to do %t?",
    [TA_PROMISE] = "I promise.",
    [TA_INSULT] = "You're awful.", [TA_CRITICIZE] = "That could be better.",
    [TA_TEASE] = "Oh, come on.", [TA_MOCK] = "Nice one. Not.", [TA_COMPLAIN] = "Ugh, this again.",
    [TA_BLAME] = "That was your fault.", [TA_DISMISS] = "Whatever.", [TA_THREATEN] = "Don't push it.",
    [TA_GOSSIP] = "Did you hear...?", [TA_DEFEND] = "Leave them alone.", [TA_ACCUSE] = "It was you.",
    [TA_TAKE_SIDE] = "I'm on their side.", [TA_MEDIATE] = "Let's all calm down.",
    [TA_CHANGE_TOPIC] = "Anyway, about %t...", [TA_FLIRT] = "You look nice tonight.",
    [TA_CONFESS] = "I really like you.", [TA_ASK_OUT] = "Would you go out with me?",
    [TA_WAVE] = "*waves*", [TA_NOD] = "*nods*", [TA_SHAKE_HEAD] = "*shakes head*",
    [TA_HUG] = "*hugs*", [TA_GIVE] = "*holds something out*", [TA_WALK_AWAY] = "*walks away*",
    [TA_HIGH_FIVE] = "*high five*"
};

static const uint8_t cat_chat[]  = { TA_GREET, TA_THANK, TA_APOLOGIZE, TA_AGREE, TA_DISAGREE, TA_YES, TA_NO, TA_LAUGH, TA_CHANGE_TOPIC };
static const uint8_t cat_nice[]  = { TA_COMPLIMENT, TA_COMFORT, TA_ENCOURAGE, TA_SYMPATHIZE, TA_JOKE, TA_OFFER, TA_INVITE, TA_PROMISE, TA_PRAISE_OTHER };
static const uint8_t cat_cool[]  = { TA_TEASE, TA_CRITICIZE, TA_COMPLAIN, TA_DISMISS, TA_MOCK, TA_BLAME, TA_INSULT, TA_THREATEN };
static const uint8_t cat_ask[]   = { TA_ASK_ABOUT, TA_ASK_OPINION, TA_ASK_FEELING, TA_ASK_ADVICE, TA_ASK_FAVOR, TA_ASK_FACT, TA_ASK_WHO };
static const uint8_t cat_tell[]  = { TA_TELL, TA_OPINE_POS, TA_OPINE_NEG, TA_INTRODUCE };
static const uint8_t cat_oth[]   = { TA_GOSSIP, TA_DEFEND, TA_ACCUSE, TA_TAKE_SIDE, TA_MEDIATE };
static const uint8_t cat_ges[]   = { TA_WAVE, TA_NOD, TA_SHAKE_HEAD, TA_HUG, TA_HIGH_FIVE, TA_GIVE, TA_WALK_AWAY };
static const uint8_t cat_rom[]   = { TA_FLIRT, TA_CONFESS, TA_ASK_OUT };
static const struct { const char *name; const uint8_t *acts; uint8_t n; } cats[C_COUNT] = {
    { "Chat", cat_chat, sizeof cat_chat },       { "Be nice", cat_nice, sizeof cat_nice },
    { "Be cool", cat_cool, sizeof cat_cool },    { "Ask", cat_ask, sizeof cat_ask },
    { "Tell", cat_tell, sizeof cat_tell },       { "Others", cat_oth, sizeof cat_oth },
    { "Gestures", cat_ges, sizeof cat_ges },     { "Romance", cat_rom, sizeof cat_rom }
};
static const char *const stock_key[] = {
    "pie", "music", "weather", "food", "work", "ghosts", "monsters", "sleep", "friends", "pets", "games",
    "dinner", "dreams", "hellvile", "hillvile", "school", "money", "books", "coffee", "dancing", "stars",
    "family", "the park", "cake"
};

/* ---- quick replies (T6b): what to say to what was just said ------------------------------------
 * Every reply is a TA_* act in a sentence. The sentences are static (a pointer is the identity of a
 * reply: the tests compare them); "%t" in the q_top table stands for the topic. A reply is chosen
 * from short lists by the act of the line it answers, the resident's persona and mood, and a salt
 * (the same salt gives the same replies). */
#define QV 6
static const char *const q_say[TA_COUNT][QV] = {
    [TA_GREET]    = { "Hi there!", "Hey, good to see you.", "Hello!", "Oh hey, it's you!", "Hi! What's new?" },
    [TA_FAREWELL] = { "I have to go. Bye!", "Gotta run. See you!", "Catch you later!", "Take care!" },
    [TA_THANK]    = { "Thank you!", "Thanks, that's kind.", "Aw, thanks!", "You're too nice.", "That means a lot." },
    [TA_APOLOGIZE] = { "I'm sorry.", "Sorry, my bad.", "Did I say something wrong?", "Oops. Sorry about that." },
    [TA_ASK_ABOUT] = { "Tell me more.", "Go on, I'm listening.", "How come?", "What happened?", "Really? Then what?" },
    [TA_ASK_OPINION] = { "What do you think?", "Where do you stand on it?", "Is that a yes or a no?", "Be honest. Thoughts?" },
    [TA_ASK_FEELING] = { "How are you feeling?", "You okay?", "How's your day going?", "What's on your mind?" },
    [TA_ASK_FACT] = { "Do you know why?", "Are you sure about that?", "Is that really true?" },
    [TA_ASK_ADVICE] = { "Any advice for me?", "What would you do?", "What should I do?" },
    [TA_TELL]     = { "Guess what happened to me.", "Funny thing, actually.", "I had a strange day.", "Can I tell you something?" },
    [TA_OPINE_POS] = { "I like it, actually.", "I'm a big fan.", "Honestly? I love it.", "It's growing on me." },
    [TA_OPINE_NEG] = { "Not my thing, honestly.", "I can't stand it.", "I'm not sold on it.", "It's not for me." },
    [TA_AGREE]    = { "I agree.", "Yeah, same here.", "Exactly!", "You said it.", "That's what I think too." },
    [TA_DISAGREE] = { "I don't think so.", "Hmm, not really.", "I see it differently.", "Respectfully, no." },
    [TA_ANSWER]   = { "Pretty good, thanks.", "Can't complain. Mostly.", "Same old, same old.", "No problem at all!", "Any time!" },
    [TA_YES]      = { "Yes.", "Sure thing.", "You bet.", "Of course." },
    [TA_NO]       = { "No.", "Not really.", "I'd rather not.", "Nope." },
    [TA_COMPLIMENT] = { "You're great.", "You always know what to say.", "I like your style.", "You're good at this.", "You've got a good heart." },
    [TA_COMFORT]  = { "It'll be okay.", "I'm here for you.", "Take your time.", "It's not as bad as it seems." },
    [TA_ENCOURAGE] = { "You can do it!", "I believe in you.", "You've got this.", "Don't give up." },
    [TA_SYMPATHIZE] = { "That sounds hard.", "I'm sorry you're going through that.", "That must hurt.", "I'm listening." },
    [TA_JOKE]     = { "Here's a good one...", "I'd make a joke, but...", "Funny you say that.", "Stop me if you've heard this.", "Plot twist!" },
    [TA_LAUGH]    = { "Ha ha ha!", "Heh. Good one.", "Okay, that was funny.", "Hahaha, stop it!" },
    [TA_OFFER]    = { "Want a hand?", "Can I help?", "Let me help you with that.", "Need anything?" },
    [TA_INVITE]   = { "Want to hang out?", "Let's do something fun." },
    [TA_PROMISE]  = { "I promise.", "I'll be there.", "You can count on me." },
    [TA_TEASE]    = { "Oh, come on.", "Is that the best you've got?", "You're so dramatic.", "Look who's talking." },
    [TA_BACKCHANNEL] = { "Mm-hm.", "Huh. Okay.", "I see.", "Right, right." },
    [TA_CONTINUE] = { "And then?", "Keep going." },
    [TA_WAVE]     = { "*waves*" },
    [TA_NOD]      = { "*nods*" },
    [TA_SHAKE_HEAD] = { "*shakes head*" },
    [TA_HUG]      = { "*hugs*" },
    [TA_HIGH_FIVE] = { "*high five*" },
    [TA_MEDIATE]  = { "Let's all calm down.", "Can we just talk this through?", "Come on, you two." },
    [TA_TAKE_SIDE] = { "I'm with you on this.", "You've got a point." },
    [TA_DEFEND]   = { "Leave them alone.", "Hey, go easy on them." },
};
static const char *const q_top[TA_COUNT][QV] = {
    [TA_ASK_ABOUT] = { "Tell me about %t.", "What's the deal with %t?", "So, what about %t?", "I want to hear about %t." },
    [TA_ASK_OPINION] = { "What do you think of %t?", "Are you into %t?", "%t: love it or hate it?" },
    [TA_TELL]     = { "I've been thinking about %t.", "Guess what: %t!", "I ran into %t today." },
    [TA_OPINE_POS] = { "I like %t.", "I'm into %t.", "%t is great, honestly." },
    [TA_OPINE_NEG] = { "I'm not a fan of %t.", "%t? Not for me.", "I can take or leave %t." },
    [TA_AGREE]    = { "Yeah, %t is like that.", "Right? %t, exactly." },
    [TA_DISAGREE] = { "I'm not so sure about %t.", "I'd say %t is overrated." },
    [TA_JOKE]     = { "%t? Don't get me started.", "Say %t one more time..." },
    [TA_CHANGE_TOPIC] = { "Anyway, about %t...", "Speaking of which: %t?", "Can we talk about %t?", "Random, but... %t?" },
    [TA_INVITE]   = { "Want to do %t?", "Fancy some %t?" },
};

typedef struct QCtx {
    const TalkState *s;
    uint32_t salt;
    uint16_t topic, fresh;               /* the topic the replies stay on; one to steer to        */
    uint8_t  kid, sincere, n;
    uint8_t  used[(TA_COUNT + 7) / 8];
} QCtx;

static int q_used(const QCtx *q, int act) { return (q->used[act >> 3] >> (act & 7)) & 1; }

static int q_count(const char *const *v)
{
    int n = 0;
    while (n < QV && v[n]) n++;
    return n;
}

static int q_sincere_act(int act)
{
    return act == TA_SYMPATHIZE || act == TA_COMFORT || act == TA_OFFER || act == TA_ENCOURAGE;
}

static uint8_t q_tone(int act)
{
    switch (act) {
    case TA_JOKE: case TA_TEASE: return TONE_CHEEKY;
    case TA_COMPLIMENT: case TA_COMFORT: case TA_ENCOURAGE: case TA_THANK: case TA_SYMPATHIZE: case TA_OFFER:
    case TA_HUG: case TA_HIGH_FIVE: return TONE_WARM;
    default: return TONE_PLAIN;
    }
}

/* the topic an act stays on; the steer is its own */
static uint16_t q_topic_of(const QCtx *q, int act) { return act == TA_CHANGE_TOPIC ? q->fresh : q->topic; }

/* may this act be offered here, and which sentence (NULL: it cannot) */
static const char *q_text(const QCtx *q, int act, uint32_t h)
{
    int ng, nt, i;
    uint16_t tp;
    if (act <= TA_NONE || act >= TA_COUNT || TA_IS_ROMANCE(act) || TA_IS_SAFETY(act) || q_used(q, act)) return NULL;
    if (q->sincere && !q_sincere_act(act)) return NULL;
    if (q->kid && (act == TA_TEASE || act == TA_DISMISS || act == TA_COMPLAIN)) return NULL;
    tp = q_topic_of(q, act);
    if (act == TA_CHANGE_TOPIC && !tp) return NULL;
    ng = q_count(q_say[act]);
    nt = tp ? q_count(q_top[act]) : 0;
    if (ng + nt == 0) return NULL;
    i = (int)((h >> 8) % (uint32_t)(ng + nt));
    return i < ng ? q_say[act][i] : q_top[act][i - ng];
}

static int q_add(QCtx *q, const int8_t *act_to, int act, uint32_t h, TalkSugg *out)
{
    const char *t;
    if (q->n >= TALK_QUICK_MAX || !(t = q_text(q, act, h))) return 0;
    out[q->n].act = (uint8_t)act;
    out[q->n].tone = q_tone(act);
    out[q->n].topic = q_topic_of(q, act);
    out[q->n].to = act_to ? *act_to : -1;
    out[q->n].text = t;
    q->used[act >> 3] |= (uint8_t)(1u << (act & 7));
    q->n++;
    return 1;
}

/* the first of a list (from a salted start) that can be offered */
static int q_from(QCtx *q, const uint8_t *list, int n, uint32_t h, TalkSugg *out)
{
    int i;
    for (i = 0; i < n; i++)
        if (q_add(q, NULL, list[(h % (uint32_t)n + (uint32_t)i) % (uint32_t)n], h, out)) return 1;
    return 0;
}

static const uint8_t ql_hello[] = { TA_GREET, TA_ASK_FEELING, TA_WAVE, TA_ASK_ABOUT };
static const uint8_t ql_bye[]   = { TA_FAREWELL, TA_THANK, TA_PROMISE, TA_WAVE };
static const uint8_t ql_opin[]  = { TA_OPINE_POS, TA_OPINE_NEG, TA_ANSWER, TA_ASK_OPINION };
static const uint8_t ql_feel[]  = { TA_ANSWER, TA_TELL, TA_ASK_FEELING };
static const uint8_t ql_fact[]  = { TA_YES, TA_NO, TA_ANSWER, TA_ASK_ABOUT };
static const uint8_t ql_tell[]  = { TA_AGREE, TA_ASK_ABOUT, TA_OPINE_POS, TA_OPINE_NEG, TA_LAUGH, TA_DISAGREE };
static const uint8_t ql_kind[]  = { TA_THANK, TA_COMPLIMENT };
static const uint8_t ql_thx[]   = { TA_ANSWER, TA_JOKE, TA_ASK_FEELING };
static const uint8_t ql_sorry[] = { TA_COMFORT, TA_ANSWER, TA_THANK, TA_JOKE };
static const uint8_t ql_warm[]  = { TA_THANK, TA_YES, TA_ANSWER, TA_JOKE };
static const uint8_t ql_fun[]   = { TA_LAUGH, TA_JOKE, TA_AGREE, TA_ASK_ABOUT };
static const uint8_t ql_cool[]  = { TA_APOLOGIZE, TA_DISAGREE, TA_JOKE, TA_SYMPATHIZE };
static const uint8_t ql_moan[]  = { TA_SYMPATHIZE, TA_AGREE, TA_COMFORT, TA_JOKE };
static const uint8_t ql_third[] = { TA_AGREE, TA_DISAGREE, TA_ASK_ABOUT, TA_JOKE };
static const uint8_t ql_hug[]   = { TA_HUG, TA_THANK, TA_LAUGH };
static const uint8_t ql_five[]  = { TA_HIGH_FIVE, TA_LAUGH, TA_JOKE };
static const uint8_t ql_nod[]   = { TA_ASK_ABOUT, TA_NOD, TA_TELL, TA_BACKCHANNEL };
static const uint8_t ql_start[] = { TA_GREET, TA_ASK_FEELING, TA_ASK_ABOUT, TA_JOKE, TA_COMPLIMENT, TA_TELL, TA_WAVE };
static const uint8_t ql_sinc[]  = { TA_SYMPATHIZE, TA_COMFORT, TA_OFFER, TA_ENCOURAGE };
static const uint8_t ql_chg[]   = { TA_CHANGE_TOPIC, TA_ASK_OPINION, TA_DISAGREE, TA_TEASE, TA_ASK_ABOUT, TA_TELL, TA_ASK_FEELING };
static const uint8_t ql_wild[]  = { TA_COMPLIMENT, TA_ENCOURAGE, TA_HIGH_FIVE, TA_ASK_FEELING, TA_NOD, TA_BACKCHANNEL,
                                    TA_ANSWER, TA_ASK_ABOUT, TA_THANK };

#define QSET(a) (*list = (a), (int)sizeof(a))
/* the list that answers a line of this act */
static int q_follow(int act, const uint8_t **list)
{
    switch (act) {
    case TA_GREET: case TA_WAVE: case TA_INTRODUCE: return QSET(ql_hello);
    case TA_FAREWELL: case TA_WALK_AWAY: return QSET(ql_bye);
    case TA_ASK_ABOUT: case TA_ASK_OPINION: case TA_ASK_ADVICE: case TA_ASK_WHO: return QSET(ql_opin);
    case TA_ASK_FEELING: case TA_ASK_REPEAT: return QSET(ql_feel);
    case TA_ASK_FACT: case TA_ASK_FAVOR: return QSET(ql_fact);
    case TA_COMPLIMENT: case TA_PRAISE_OTHER: case TA_GIVE: return QSET(ql_kind);
    case TA_THANK: return QSET(ql_thx);
    case TA_APOLOGIZE: return QSET(ql_sorry);
    case TA_COMFORT: case TA_ENCOURAGE: case TA_SYMPATHIZE: case TA_OFFER: case TA_INVITE: case TA_PROMISE:
        return QSET(ql_warm);
    case TA_JOKE: case TA_LAUGH: return QSET(ql_fun);
    case TA_COMPLAIN: return QSET(ql_moan);
    case TA_INSULT: case TA_CRITICIZE: case TA_TEASE: case TA_MOCK: case TA_BLAME: case TA_DISMISS: case TA_THREATEN:
        return QSET(ql_cool);
    case TA_GOSSIP: case TA_DEFEND: case TA_ACCUSE: case TA_TAKE_SIDE: case TA_MEDIATE: return QSET(ql_third);
    case TA_HUG: return QSET(ql_hug);
    case TA_HIGH_FIVE: return QSET(ql_five);
    case TA_NOD: case TA_SHAKE_HEAD: return QSET(ql_nod);
    default: return QSET(ql_tell);
    }
}

/* what a resident's persona, mood and the second slot say: the reply that suits them */
static int q_suit(const TalkState *s, int slot, int sincere, const uint8_t **list)
{
    static const uint8_t sad[] = { TA_SYMPATHIZE, TA_COMFORT, TA_OFFER };
    static const uint8_t angry[] = { TA_APOLOGIZE, TA_ASK_FEELING };
    static const uint8_t easy[] = { TA_JOKE, TA_LAUGH };
    static const uint8_t serious[] = { TA_ASK_ABOUT, TA_ASK_ADVICE, TA_ANSWER };
    static const uint8_t expr[] = { TA_COMPLIMENT, TA_HUG, TA_HIGH_FIVE };
    static const uint8_t blunt[] = { TA_DISAGREE, TA_OPINE_NEG, TA_TELL };
    static const uint8_t polite[] = { TA_THANK, TA_APOLOGIZE, TA_ASK_FEELING };
    static const uint8_t mid[] = { TA_ASK_OPINION, TA_TELL, TA_ENCOURAGE };
    DoomoResident buf;
    const DoomoResident *r = NULL;
    int id, sp = 6, ex = 6, at = 6, qk = 6, mood = MOOD_OK;
    if (slot >= 0 && slot < s->nparts) {
        id = s->part[slot].id;
        if (id >= 0 && id <= DOOMO_PLAYER_ID) r = town_res(id);
        else if (id != TALK_PET_ID && family_resident(id, &buf)) r = &buf;
    }
    if (r) {
        sp = RES_PERSONA(r, PERS_SPEECH); ex = RES_PERSONA(r, PERS_EXPRESSION);
        at = RES_PERSONA(r, PERS_ATTITUDE); qk = RES_PERSONA(r, PERS_QUIRK);
        mood = r->mood;
    }
    if (sincere) {
        if (mood == MOOD_SAD) return QSET(sad);
        return QSET(ql_sinc);
    }
    if (mood == MOOD_SAD) return QSET(sad);
    if (mood == MOOD_ANGRY) return QSET(angry);
    if (at >= 10 || qk >= 12) return QSET(easy);
    if (at <= 5) return QSET(serious);
    if (ex >= 10) return QSET(expr);
    if (sp >= 10) return QSET(blunt);
    if (sp <= 5) return QSET(polite);
    return QSET(mid);
}

const TalkLine *talk_quick_thread(const TalkState *s, int *slot)
{
    int i;
    if (!s || s->nlines == 0) return NULL;
    for (i = s->nlines - 1; i >= 0; i--)
        if (s->line[i].to < 0 && s->line[i].who >= 0) { if (slot) *slot = s->line[i].who; return &s->line[i]; }
    if (slot) *slot = s->line[s->nlines - 1].who;
    return &s->line[s->nlines - 1];
}

static int q_grief(uint16_t topic)
{
    TalkConcept tc;
    return topic && talk_db_concept(topic, &tc) && (tc.flags & TCF_GRIEF);
}

int talk_quick_replies(const TalkState *s, int slot, uint16_t topic, uint16_t fresh, uint32_t salt, TalkSugg *out)
{
    QCtx q;
    TalkSugg tmp[TALK_QUICK_MAX];
    const TalkLine *th;
    const uint8_t *fl = NULL, *sl = NULL;
    int tslot = -1, act = TA_NONE, b = -1, i, fn, sn, argue;
    uint32_t h;
    if (!s || !out || (s->flags & TDF_OVER)) return 0;
    memset(&q, 0, sizeof q);
    memset(tmp, 0, sizeof tmp);
    q.s = s;
    q.salt = salt;
    th = talk_quick_thread(s, &tslot);
    if (slot < 0 || slot >= s->nparts || (s->part[slot].flags & (TPF_LEFT | TPF_PET)))
        slot = tslot >= 0 && tslot < s->nparts ? tslot : 0;
    if (th) act = th->act;
    q.kid = s->conv.aud.any_minor || (s->flags & TDF_KIDSAFE) ? 1 : 0;
    /* grief and sincerity: only comfort, and the topic is not picked up */
    q.sincere = (th && (th->flags & TLF_SINCERE)) || TA_IS_SAFETY(act) || q_grief(th ? th->topic : TC_NONE) ||
                q_grief(topic) || q_grief(s->topic_stack[0]);
    /* the topic: what the line is about, else what the player has in play, else the last one said */
    q.topic = th && th->topic ? th->topic : topic ? topic : s->topic_stack[0];
    if (q.topic && (!talk_gen_concept_ok(q.topic, &s->conv.aud) || q_grief(q.topic))) q.topic = TC_NONE;
    q.fresh = fresh && !q.sincere && fresh != q.topic && talk_gen_concept_ok(fresh, &s->conv.aud) && !q_grief(fresh)
              ? fresh : TC_NONE;
    h = doomo_hash32(salt * 0x9E3779B1u + (uint32_t)act * 0x85EBCA6Bu + 0x7A1Bu);

    /* the reply that suits them is reserved first (the first of its list that can be said), so the thread's
     * reply never takes its act; it goes in the second place */
    sn = q_suit(s, slot, q.sincere, &sl);
    for (i = 0; i < sn && b < 0; i++) if (q_text(&q, sl[i], h >> 4)) b = sl[i];
    if (b >= 0) q.used[b >> 3] |= (uint8_t)(1u << (b & 7));

    /* a row between two residents is running: the player can step in */
    argue = !q.sincere && s->arg_step > 0 && th && s->arg_a >= 0 && s->arg_b >= 0 &&
            (th->who == s->arg_a || th->who == s->arg_b);
    if (argue) {
        int8_t sp = th->who, other = th->who == s->arg_a ? s->arg_b : s->arg_a;
        q_add(&q, NULL, TA_MEDIATE, h, tmp);
        q_add(&q, &sp, TA_TAKE_SIDE, h >> 3, tmp);
        q_add(&q, &other, TA_DEFEND, h >> 5, tmp);
    } else {
        if (!th) { fl = ql_start; fn = (int)sizeof ql_start; }
        else if (q.sincere) { fl = ql_sinc; fn = (int)sizeof ql_sinc; }
        else fn = q_follow(act, &fl);
        q_from(&q, fl, fn, h >> 2, tmp);                           /* 1: the thread */
    }
    if (b >= 0) {                                                  /* 2: the one that suits them */
        q.used[b >> 3] &= (uint8_t)~(1u << (b & 7));
        q_add(&q, NULL, b, h >> 4, tmp);
    }
    if (q.sincere) {                                               /* more of the same kindness */
        q_from(&q, ql_sinc, (int)sizeof ql_sinc, h >> 9, tmp);
        q_from(&q, ql_sinc, (int)sizeof ql_sinc, h >> 11, tmp);
    } else {
        q_from(&q, ql_chg, (int)sizeof ql_chg, h >> 9, tmp);       /* 3: change something */
        q_from(&q, ql_wild, (int)sizeof ql_wild, h >> 11, tmp);    /* 4: a wildcard */
        q_from(&q, ql_wild, (int)sizeof ql_wild, h >> 13, tmp);
    }
    for (i = 0; i < q.n; i++) out[i] = tmp[i];
    return q.n;
}

/* ---- helpers ------------------------------------------------------------------------------ */
static const char *res_name(int id)
{
    const DoomoResident *r = town_res(id);
    return r ? r->name : "?";
}

static void topic_name(uint16_t c, char *out, int cap)
{
    out[0] = 0;
    if (c == TC_NONE || talk_db_form(c, TF_LEMMA, out, cap) < 0) { doomo_snprintf(out, (size_t)cap, "that"); }
}

static void expand(const char *src, uint16_t topic, char *out, int cap)
{
    int n = 0;
    while (*src && n < cap - 1) {
        if (src[0] == '%' && src[1] == 't') {
            char tn[20];
            int k;
            topic_name(topic, tn, (int)sizeof tn);
            for (k = 0; tn[k] && n < cap - 1; k++) out[n++] = tn[k];
            src += 2;
        } else {
            out[n++] = *src++;
        }
    }
    out[n] = 0;
}

static void phrase(int act, char *out, int cap)
{
    expand(act > 0 && act < TA_COUNT && act_text[act] ? act_text[act] : "...", T.topic, out, cap);
}

static int wants_topic(int act)
{
    switch (act) {
    case TA_ASK_ABOUT: case TA_ASK_OPINION: case TA_TELL: case TA_OPINE_POS: case TA_OPINE_NEG:
    case TA_CHANGE_TOPIC: case TA_INVITE: return 1;
    default: return 0;
    }
}

static int needs_two(int act)
{
    return act == TA_DEFEND || act == TA_ACCUSE || act == TA_TAKE_SIDE || act == TA_MEDIATE || act == TA_PRAISE_OTHER;
}

static int romance_open(void)
{
    return T.to >= 0 && T.to < T.n && T.ids[T.to] != TALK_PET_ID &&
           talk_safe_romance_ok(&T.s.conv.aud, T.avatar, T.ids[T.to]);
}

static int act_listed(int act)
{
    if (needs_two(act) && T.n < 2) return 0;
    if (TA_IS_ROMANCE(act) && !romance_open()) return 0;
    return 1;
}

static int speaker_of(int slot)
{
    if (slot < 0 || slot >= T.s.nparts) return -1;
    return T.s.part[slot].id == TALK_PET_ID ? -1 : T.s.part[slot].id;
}

/* ---- the rows ------------------------------------------------------------------------------ */
static void add_row(int kind, int arg, int v)
{
    if (T.nrows >= TROWS) return;
    T.row[T.nrows].kind = (uint8_t)kind;
    T.row[T.nrows].arg = (uint8_t)arg;
    T.row[T.nrows].v = (uint16_t)v;
    T.nrows++;
}

static void list_reset(void)
{
    T.list.items = NULL;
    T.list.count = T.nrows;
    T.list.sel = 0;
    T.list.top = 0;
}

/* the replies to what was just said: for the one you are talking to, staying on the topic; a stock topic
 * the talk is not on is offered to steer to */
static void quick_refresh(void)
{
    uint16_t fresh = TC_NONE;
    int i, slot = T.to >= 0 ? T.to : 0;
    uint32_t h = doomo_hash32(T.qsalt * 2654435761u + 0x51u);
    for (i = 0; i < T.nstock && !fresh; i++) {
        uint16_t c = T.stock[(h + (uint32_t)i) % (uint32_t)T.nstock];
        if (c != T.topic && c != T.s.topic_stack[0]) fresh = c;
    }
    T.nquick = (uint8_t)talk_quick_replies(&T.s, slot, T.topic, fresh, T.qsalt++, T.quick);
}

static void build_main(void)
{
    int c;
    T.page = PG_MAIN;
    T.nrows = 0;
    quick_refresh();
    for (c = 0; c < T.nquick; c++) add_row(K_QUICK, c, 0);
    for (c = 0; c < C_COUNT; c++) {
        if (c == C_ROMANCE && !romance_open()) continue;
        add_row(K_CAT, c, 0);
    }
#ifdef TALK_TYPING
    add_row(K_TYPE, 0, 0);
#endif
    add_row(K_MORE, 0, 0);
    add_row(K_BYE, 0, 0);
    list_reset();
}

/* The three setup rows live one page down, so the main page is just the replies, Options and Bye. */
static void build_more(void)
{
    T.page = PG_MORE;
    T.nrows = 0;
    add_row(K_TOPIC, 0, 0);
    if (T.n > 1) add_row(K_TO, 0, 0);
    if (T.n < TALK_PARTS_MAX && T.s.nparts < TALK_PARTS_MAX) add_row(K_INVITE, 0, 0);
    list_reset();
}

static void build_acts(int c)
{
    int i;
    T.page = PG_ACTS;
    T.cat = (uint8_t)c;
    T.nrows = 0;
    for (i = 0; i < cats[c].n; i++)
        if (act_listed(cats[c].acts[i])) add_row(K_ACT, cats[c].acts[i], 0);
    list_reset();
}

static void topic_add(uint16_t c)
{
    int i;
    if (c == TC_NONE) return;
    for (i = 0; i < T.nrows; i++) if (T.row[i].kind == K_PICK && T.row[i].v == c) return;
    add_row(K_PICK, 0, c);
}

static void build_topics(void)
{
    int i, w;
    T.page = PG_TOPIC;
    T.nrows = 0;
    add_row(K_NOTOPIC, 0, 0);
    for (i = 0; i < TALK_STACK_MAX; i++) topic_add(T.s.topic_stack[i]);
    if (T.to >= 0 && T.to < T.n)
        for (w = 0; w < 5; w++)
            if (talk_mem_like_known(T.ids[T.to], w)) topic_add(talk_gen_like(T.ids[T.to], w));
    for (i = 0; i < T.nstock; i++) topic_add(T.stock[i]);
    list_reset();
}

static void build_invite(void)
{
    int i, k, best, pick[6], np = 0;
    T.page = PG_INVITE;
    T.nrows = 0;
    for (k = 0; k < 6; k++) {
        best = -1;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            int j, used = (i == T.avatar) || !town_alive(i) || talk_safe_is_pet(i);
            for (j = 0; j < T.n && !used; j++) if (T.ids[j] == i) used = 1;
            for (j = 0; j < np && !used; j++) if (pick[j] == i) used = 1;
            if (used) continue;
            if (best < 0 || rel_value(T.ids[0], i) > rel_value(T.ids[0], best)) best = i;
        }
        if (best < 0) break;
        pick[np++] = best;
        add_row(K_PICK, 1, best);
    }
    list_reset();
}

static const char *row_text(int index, char *buf)
{
    const TRow *r;
    char tn[20];
    buf[0] = 0;
    if (index < 0 || index >= T.nrows) return buf;
    r = &T.row[index];
    switch (r->kind) {
    case K_CAT: doomo_snprintf(buf, 48, "%s >", cats[r->arg].name); break;
    case K_ACT: phrase(r->arg, buf, 48); break;
    case K_TOPIC:
        topic_name(T.topic, tn, (int)sizeof tn);
        doomo_snprintf(buf, 48, "Topic: %s >", T.topic == TC_NONE ? "any" : tn);
        break;
    case K_TYPE: doomo_snprintf(buf, 48, "Type your own line..."); break;
    case K_TO:
        doomo_snprintf(buf, 48, "To: %s <>", T.to < 0 ? "everyone" : res_name(T.ids[T.to]));
        break;
    case K_INVITE: doomo_snprintf(buf, 48, "Invite someone >"); break;
    case K_MORE:                         /* the live settings ride on the row: "Options (Nana, pie) >" */
        topic_name(T.topic, tn, (int)sizeof tn);
        if (T.n > 1 && T.topic != TC_NONE)
            doomo_snprintf(buf, 48, "Options (%s, %s) >", T.to < 0 ? "all" : res_name(T.ids[T.to]), tn);
        else if (T.n > 1) doomo_snprintf(buf, 48, "Options (%s) >", T.to < 0 ? "all" : res_name(T.ids[T.to]));
        else if (T.topic != TC_NONE) doomo_snprintf(buf, 48, "Options (%s) >", tn);
        else doomo_snprintf(buf, 48, "Options >");
        break;
    case K_QUICK: expand(T.quick[r->arg].text, T.quick[r->arg].topic, buf, 48); break;
    case K_BYE: doomo_snprintf(buf, 48, "Say goodbye"); break;
    case K_NOTOPIC: doomo_snprintf(buf, 48, "Any topic"); break;
    case K_PICK:
        if (r->arg) doomo_snprintf(buf, 48, "%s", res_name(r->v));
        else topic_name(r->v, buf, 48);
        break;
    default: break;
    }
    return buf;
}

/* ---- running the talk ----------------------------------------------------------------------- */
static void cb_after(void) { T.busy = 0; T.after = 1; }

static void show_lines(void)
{
    int i;
    for (i = 0; i < T.s.nlines; i++) {
        const TalkLine *l = &T.s.line[i];
        if (l->face) ui_say_ex(speaker_of(l->who), l->face - 1, 0, l->text);
        else ui_say(speaker_of(l->who), l->text);
    }
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
}

static void finish(void)
{
    TalkOutcome o;
    TalkSummary sum;
    char msg[48];
    int i, bond = 0;
    talk_outcome_end(&T.s, &o, &sum);
    for (i = 0; i < T.s.nparts; i++) bond += o.rel[i];
    if (o.made_up) doomo_snprintf(msg, sizeof msg, "They made up! Bond %+d", bond);
    else if (o.dating) doomo_snprintf(msg, sizeof msg, "A new couple! Bond %+d", bond);
    else if (sum.flags & TDF_K7) doomo_snprintf(msg, sizeof msg, "The chat ended.");
    else doomo_snprintf(msg, sizeof msg, "Chat over. Bond %+d", bond);
    T.active = 0;
    scene_pop();
    ui_toast(msg);
}

static void begin_talk(void)
{
    uint32_t seed = talk_gen_seed(T.avatar, T.ids, T.n) + (++sessions) * 2654435761u;
    talk_outcome_begin(&T.s, T.avatar, T.ids, T.n, seed);
    if (T.to >= T.n) T.to = 0;
    T.busy = T.after = T.leaving = T.pending = 0;
    T.idle = 0;
    T.you[0] = 0;
}

static void say_utter(TalkUtter *u)
{
    doomo_snprintf(T.you, sizeof T.you, "%s", u->shown);
    talk_dm_say(&T.s, u);
    while (talk_dm_step(&T.s, NULL)) {}
    if (T.s.flags & TDF_FAREWELL) T.leaving = 1;
    show_lines();
}

static void say_act(int act)
{
    TalkUtter u;
    char buf[TALK_SHOWN_MAX];
    memset(&u, 0, sizeof u);
    u.act[0] = (uint8_t)act;
    u.topic[0] = wants_topic(act) ? T.topic : TC_NONE;
    u.target = T.to;
    u.conf = 15;
    u.tone = TA_IS_ROMANCE(act) ? TONE_FLIRTY
           : (act == TA_COMPLIMENT || act == TA_COMFORT || act == TA_ENCOURAGE || act == TA_THANK) ? TONE_WARM
           : (act == TA_TEASE || act == TA_JOKE) ? TONE_CHEEKY
           : (act == TA_MOCK || act == TA_CRITICIZE || act == TA_DISMISS) ? TONE_SNARKY : TONE_PLAIN;
    phrase(act, buf, (int)sizeof buf);
    doomo_snprintf(u.shown, sizeof u.shown, "%s", buf);
    say_utter(&u);
    build_main();
}

static void say_quick(int i)
{
    TalkUtter u;
    const TalkSugg *g;
    if (i < 0 || i >= T.nquick) return;
    g = &T.quick[i];
    memset(&u, 0, sizeof u);
    u.act[0] = g->act;
    u.topic[0] = g->topic;
    u.target = g->to >= 0 ? g->to : T.to;
    u.tone = g->tone;
    u.conf = 15;
    expand(g->text, g->topic, u.shown, (int)sizeof u.shown);
    if (g->topic) T.topic = g->topic;                 /* the talk stays on it */
    say_utter(&u);
    build_main();
}

#ifdef TALK_TYPING
static int person_lookup(const char *canon)
{
    int i;
    char key[24];
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        if (!town_alive(i) || i == T.avatar) continue;
        if (talk_lex_canon(res_name(i), key, (int)sizeof key) > 0 && strcmp(key, canon) == 0) return i;
    }
    return -1;
}

static void cb_typed(int ok) { T.typed_ok = (uint8_t)(ok ? 1 : 2); }

static void say_typed(void)
{
    TalkParseCtx pc;
    TalkParseInfo *info;
    TalkUtter u;
    int i;
    static TalkParseInfo pinfo;
    info = &pinfo;
    memset(&pc, 0, sizeof pc);
    pc.nparts = (uint8_t)T.n;
    pc.ring = -1;
    pc.hops = 1;
    pc.kid_room = T.s.conv.aud.any_minor;
    for (i = 0; i < T.n && i < TALK_PARTS_MAX; i++) pc.name[i] = res_name(T.ids[i]);
    for (i = 0; i < TALK_STACK_MAX && T.s.topic_stack[i]; i++) pc.stack[pc.nstack++] = T.s.topic_stack[i];
    pc.person = person_lookup;
    memset(&u, 0, sizeof u);
    if (!talk_parse(T.typed, &pc, &u, info)) { ui_toast("Typing isn't available here."); return; }
    if (u.act[0] == TA_NONE) u.act[0] = TA_UNKNOWN;
    if (u.target < 0 && T.n == 1) u.target = 0;
    say_utter(&u);
    build_main();
}
#endif /* TALK_TYPING */

static void bye(void)
{
    T.pending = 0;
    say_act(TA_FAREWELL);
    T.leaving = 1;
}

static void invite(int id)
{
    TalkOutcome o;
    char msg[40];
    if (T.n >= TALK_PARTS_MAX) return;
    talk_outcome_end(&T.s, &o, NULL);            /* what the first talk did counts */
    T.ids[T.n++] = id;
    begin_talk();
    doomo_snprintf(msg, sizeof msg, "%s joins in.", res_name(id));
    ui_toast(msg);
    ui_say(id, "Hey, what did I miss?");
    T.busy = 1;
    ui_say_done(cb_after);
    build_main();
}

static void pick_topic(uint16_t c)
{
    T.topic = c;
    if (T.pending) {
        int a = T.pending;
        T.pending = 0;
        say_act(a);
    } else {
        build_main();
    }
}

static void activate(int i)
{
    const TRow *r;
    if (i < 0 || i >= T.nrows) return;
    r = &T.row[i];
    switch (r->kind) {
    case K_CAT: build_acts(r->arg); break;
    case K_ACT:
        if (wants_topic(r->arg) && T.topic == TC_NONE && r->arg != TA_INVITE) { T.pending = r->arg; build_topics(); }
        else say_act(r->arg);
        break;
    case K_QUICK: say_quick(r->arg); break;
    case K_MORE: build_more(); break;
    case K_TOPIC: T.pending = 0; build_topics(); break;
    case K_NOTOPIC: pick_topic(TC_NONE); break;
#ifdef TALK_TYPING
    case K_TYPE:
        T.typed[0] = 0;
        T.typed_ok = 0;
        ui_text_entry("Say something:", T.typed, (int)sizeof T.typed, cb_typed);
        break;
#endif
    case K_TO:
        T.to = (int8_t)(T.to + 1 >= T.n ? -1 : T.to + 1);
        build_more();
        T.list.sel = i;
        break;
    case K_INVITE: build_invite(); break;
    case K_BYE: bye(); break;
    case K_PICK:
        if (r->arg) invite(r->v);
        else pick_topic(r->v);
        break;
    default: break;
    }
}

/* ---- the scene -------------------------------------------------------------------------------- */
static void resolve_stock(void)
{
    uint32_t tags[4];
    uint16_t c, wf;
    size_t i;
    T.nstock = 0;
    for (i = 0; i < sizeof stock_key / sizeof stock_key[0] && T.nstock < TSTOCK; i++)
        if (talk_lex_find(stock_key[i], &c, &wf, tags) >= 0 && c != TC_NONE) T.stock[T.nstock++] = c;
}

static void talk_enter(int arg)
{
    int i;
    memset(&T, 0, sizeof T);
    T.avatar = playas_avatar();
    T.ids[0] = arg;
    T.n = 1;
    for (i = 0; i < extra_n && T.n < TALK_PARTS_MAX; i++)
        if (extra_ids[i] != arg && extra_ids[i] != T.avatar && town_alive(extra_ids[i])) T.ids[T.n++] = extra_ids[i];
    extra_n = 0;
    T.to = 0;
    T.topic = TC_NONE;
    T.list.visible = 6;
    T.list.item_fn = row_text;
    resolve_stock();
    T.active = 1;
    begin_talk();
    build_main();
    if (talk_dm_open(&T.s)) {            /* the resident opens the talk and hands it to the player */
        while (talk_dm_step(&T.s, NULL)) {}
        show_lines();
    } else {
        ui_say(arg, "Oh! Hi. What's up?");
        T.busy = 1;
        ui_say_done(cb_after);
    }
}

static void talk_leave(void)
{
    if (T.active) {                      /* popped from outside: still close the session */
        TalkOutcome o;
        talk_outcome_end(&T.s, &o, NULL);
        T.active = 0;
    }
}

static void talk_update(void)
{
    int r;
    if (!T.active) { scene_pop(); return; }
    if (T.busy) return;                  /* the dialogue box is up (we are not on top then) */
#ifdef TALK_TYPING
    if (T.typed_ok) {
        int ok = T.typed_ok == 1;
        T.typed_ok = 0;
        if (ok && T.typed[0]) { say_typed(); return; }
    }
#endif
    if (T.after) {
        T.after = 0;
        if (T.leaving || talk_dm_over(&T.s)) { finish(); return; }
        if (T.page == PG_MAIN) build_main();          /* new replies for what was just said */
    }
    if (T.page == PG_MAIN && ++T.idle % 8 == 0 && talk_dm_tick(&T.s, 8)) {
        while (talk_dm_step(&T.s, NULL)) {}
        show_lines();
        return;
    }
    if (btn_pressed(BTN_SELECT) && T.n > 1) {
        T.to = (int8_t)(T.to + 1 >= T.n ? -1 : T.to + 1);
        if (T.page == PG_MORE) { int sel = T.list.sel; build_more(); T.list.sel = sel; }
        else if (T.page == PG_MAIN) build_main();
        return;
    }
    r = ui_list_update(&T.list);
    if (r == UI_SELECT) { T.idle = 0; activate(T.list.sel); }
    else if (r == UI_CANCEL) {
        T.idle = 0;
        if (T.page != PG_MAIN) { T.pending = 0; build_main(); }
        else bye();
    }
}

static const char *mood_word(void)
{
    if (T.s.tension >= TALK_TENSION_ARGUE + 2) return "heated";
    if (T.s.tension >= TALK_TENSION_ARGUE - 2) return "tense";
    return "calm";
}

/* One calm panel: a single header line (what you are choosing; the mood of the room on the right),
 * then the list. The A/B hints are gone (the last row says what B does) and the old third line too.
 * The people strip marks who you are speaking to with a '>' and keeps the engage bar. */
static const char *panel_title(char *buf, int cap)
{
    switch (T.page) {
    case PG_TOPIC:  return "Talk about...";
    case PG_INVITE: return "Who joins in?";
    case PG_MORE:   return "Options";
    case PG_ACTS:   return cats[T.cat].name;
    default: break;
    }
    if (T.you[0]) { doomo_snprintf(buf, (size_t)cap, "You: %s", T.you); return buf; }
    return "Say something";
}

static void talk_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), sh = plat_screen_h(), i, ph, py, colw, rows, mw;
    char ttl[48];
    const char *mood = mood_word();
    /* the people: a name and how much they are into it */
    ui_window(0, 0, sw, 24, WIN_CUTE);
    colw = sw / (T.s.nparts > 0 ? T.s.nparts : 1);
    for (i = 0; i < T.s.nparts; i++) {
        const TalkPart *p = &T.s.part[i];
        int x = i * colw + 6;
        if (i == T.to) ui_text_clip(x - 5, 4, ">", st->text, x - 5, x);
        ui_text_clip(x, 4, p->id == TALK_PET_ID ? "Pet" : res_name(p->id), st->text, x, x + colw - 8);
        if (p->flags & TPF_LEFT) ui_text_clip(x, 14, "(left)", st->text_dim, x, x + colw - 8);
        else ui_bar(x, 15, colw - 14, 5, p->engage, 100, i == T.to ? st->accent : st->accent_d);
    }
    /* the panel: header 16 px + 6 list rows */
    rows = 6;
    ph = 18 + rows * UI_LIST_ROW_H;
    if (ph > sh - 26) { ph = sh - 26; rows = (ph - 18) / UI_LIST_ROW_H; }
    T.list.visible = rows < 1 ? 1 : rows;
    py = sh - ph;
    ui_window(0, py, sw, ph, WIN_CUTE);
    mw = ui_text_width(mood);
    ui_text_clip(sw - 8 - mw, py + 5, mood, st->text_dim, sw - 8 - mw, sw - 8);
    ui_text_clip(8, py + 5, panel_title(ttl, (int)sizeof ttl), st->text, 8, sw - 12 - mw);
    ui_list_draw(&T.list, 6, py + 17, sw - 12, WIN_CUTE);
}

const DoomoScene SCENE_TALK = { "TALK", talk_enter, talk_leave, talk_update, talk_draw, 1 };

/* ---- the house hook ------------------------------------------------------------------------------ */
static int talk_ready(void)
{
    if (!talk_db_is_open() && talk_db_open(lang_find("en")) != TALK_DB_OK) return 0;
    return talk_gen_ready();
}

/* Others to bring into the next talk (up to TALK_PARTS_MAX - 1; consumed by it). */
void talk_scene_party(const int *ids, int n)
{
    int i;
    extra_n = 0;
    for (i = 0; ids && i < n && extra_n < TALK_PARTS_MAX - 1; i++) extra_ids[extra_n++] = ids[i];
}

int home_talk_enter(int who)
{
    if (who < 0 || who >= DOOMO_MAX_RESIDENTS || !town_alive(who)) return 0;
    if (who == playas_avatar() || !talk_ready()) return 0;
    scene_push(&SCENE_TALK, who);
    return 1;
}

#endif /* DOOMO_HAS_TALK */
