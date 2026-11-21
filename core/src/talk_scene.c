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
#include "doomo/sim.h"
#include "doomo/items.h"
#include "doomo/talk_traits.h"

#ifdef DOOMO_HAS_TALK

/* The typed line needs a keyboard and the free-text parser (talk_lex / talk_parse: native and
 * the Dreamcast). The GBA has neither: it plays Talk from the quick replies and the topic list. */
#if !defined(DOOMO_PLATFORM_GBA) && !defined(DOOMO_PLATFORM_PS1) && !defined(DOOMO_PLATFORM_PS2)
#define TALK_TYPING 1
#endif

enum { PG_MAIN = 0, PG_ACTS, PG_TOPIC, PG_INVITE, PG_MORE, PG_OUTING, PG_SHOW, PG_GIVE, PG_CARE, PG_FEED, PG_HEAL, PG_HISTORY, PG_CONCERN };
enum { K_CAT = 0, K_ACT, K_TOPIC, K_TYPE, K_TO, K_INVITE, K_BYE, K_PICK, K_NOTOPIC, K_MORE, K_QUICK, K_OUT, K_OUTING, K_PLAY, K_SHOW, K_SHOWITEM, K_GIVE, K_GIVEITEM, K_REMEMBER, K_HELP, K_COMFORT, K_CLEAN, K_FEED, K_HEAL, K_FEEDITEM, K_HEALITEM, K_HISTORY, K_HISTORY_INFO, K_HISTORY_QUEST, K_HISTORY_TOPIC, K_HISTORY_EVENT, K_HISTORY_REPEAT, K_TOPIC_LINK, K_HISTORY_TONE, K_CONCERN, K_CONCERN_INFO, K_CONCERN_ACT, K_MEET_PLAN, K_QUIRK, K_HISTORY_LIKE };
enum { C_CHAT = 0, C_NICE, C_COOL, C_ASK, C_TELL, C_OTHERS, C_GESTURE, C_ROMANCE, C_COUNT };

typedef struct TRow { uint8_t kind, arg; uint16_t v; } TRow;
#define TROWS 40
#define TSTOCK 24

static struct TalkScn {
    TalkState s;
    int       ids[TALK_PARTS_MAX];
    int       n, avatar;
    int8_t    to;                       /* slot spoken to, -1 everyone               */
    uint8_t   page, cat, active, busy, after, leaving, typed_ok, gen;   /* gen: TD2, steps still to run (talk_scene_slice) */
    uint8_t   pending, handin;          /* pending topic; real quest payout after thanks */
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

/* Real places with a real simulation action, never a random conversation
 * topic. Resolve the bank IDs on entry to the page; no per-frame reads,
 * extra resident state or venue-specific allocations. */
static const struct { const char *key; const char *label; uint8_t venue; } outing[] = {
    { "park", "the park", ACT_PARK },
    { "Brimstone Bites", "Brimstone Bites", ACT_CAFE },
    { "shop", "the shop", ACT_SHOP },
    { "St. Cuddles", "St. Cuddles", ACT_CHURCH },
    { "workshop", "the workshop", ACT_WORKSHOP },
    { "beach", "the beach", ACT_BEACH },
    { "concert hall", "the concert hall", ACT_CONCERT },
    { "Cloud Nine Vapor", "Cloud Nine Vapor", ACT_VAPE_SHOP }
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

/* Small deliberative reply policy: predict how the *actual* resident would
 * receive each eligible act, using the DM's existing personality/relationship/
 * mood/trait/topic model, then discount acts the player just repeated. It
 * never invents lines, outcomes or feelings: every option still passes q_text
 * and the normal dialogue/safety gate. This runs on a menu refresh, not each
 * frame; <= 7 candidates, <= 6 history entries and zero allocations. */
static int q_candidate_score(const QCtx *q, int slot, int act, uint32_t h)
{
    const TalkState *s = q->s;
    int i, reception, score;
    reception = talk_dm_expect(s, slot, act, q_tone(act), q_topic_of(q, act));
    /* Negative reactions should matter: don't recommend a jab to somebody
     * who has repeatedly taken it badly just because it changes the text. */
    score = reception * 2 + (reception < 0 ? reception : 0);
    for (i = 0; i < s->nhist && i < 6; i++) {
        const TalkHist *old = &s->hist[i];
        if (!(old->flags & THF_PLAYER)) continue;
        if (old->act == act) score -= i < 2 ? 24 : i < 4 ? 13 : 5;
    }
    if (act == s->last_act) score -= 8;
    /* The current talk also has emotional momentum. A relationship that
     * deteriorated in THIS session is not fixed just because an old saved
     * bond is good. Suggest a repair attempt before another provocation.
     * These are preferences within already-safe, contextual candidates. */
    if (slot >= 0 && slot < s->nparts) {
        const TalkPart *person = &s->part[slot];
        if (person->rel_d_player <= -3 || person->engage < 30) {
            if (act == TA_APOLOGIZE || act == TA_SYMPATHIZE ||
                act == TA_COMFORT || act == TA_ASK_FEELING) score += 7;
            if (act == TA_JOKE || act == TA_TEASE || act == TA_DISAGREE ||
                act == TA_OPINE_NEG) score -= 7;
        }
        if (person->engage >= TALK_ENGAGE_LEAN &&
            (act == TA_ASK_ABOUT || act == TA_ASK_OPINION ||
             act == TA_CONTINUE)) score += 2;
    }
    if (s->tension >= TALK_TENSION_ARGUE) {
        if (act == TA_APOLOGIZE || act == TA_SYMPATHIZE ||
            act == TA_COMFORT || act == TA_MEDIATE) score += 8;
        if (act == TA_JOKE || act == TA_TEASE ||
            act == TA_DISAGREE || act == TA_OPINE_NEG) score -= 8;
    }
    /* A related follow-up is preferable to abruptly abandoning an active
     * thread; the memory bridge below is the deliberate exception. */
    if (q->topic && q->topic == s->topic_stack[0] &&
        (act == TA_ASK_ABOUT || act == TA_ASK_OPINION ||
         act == TA_TELL || act == TA_CONTINUE))
        score += 4;
    /* Salt resolves close scores; it does not override a bad social read. */
    score += (int)(doomo_hash32(h ^ (uint32_t)act * 0x9E3779B1u) & 3u);
    return score;
}

/* First acceptable act on a cold opening; after two real turns, rank a tiny
 * allowed candidate set by the ongoing relationship and short-term memory. */
static int q_from(QCtx *q, const uint8_t *list, int n, int slot, uint32_t h, TalkSugg *out)
{
    int i, best = -1, value = -10000;
    if (q->s->turn >= 2 && q->s->nhist > 0) {
        for (i = 0; i < n; i++) {
            int act = list[i], score;
            if (!q_text(q, act, h)) continue;
            score = q_candidate_score(q, slot, act, h);
            if (score > value) { value = score; best = act; }
        }
        if (best >= 0) return q_add(q, NULL, best, h, out);
    }
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
/* A topic-dive is not a random topic change. Keep a question or disclosure
 * alive long enough to be meaningful; the resident's real answer still
 * comes from talk_dm_step and the stateful language generator. */
static const uint8_t ql_dive[]  = { TA_ASK_ABOUT, TA_ASK_OPINION, TA_ASK_FACT, TA_TELL,
                                    TA_CONTINUE, TA_AGREE, TA_DISAGREE };
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
    /* A character's actual simulated needs shape how one engages with
     * them. These are actions by the player, not canned lines falsely
     * attributed to the resident. No extra resident state is stored. */
    static const uint8_t hungry[] = { TA_OFFER, TA_ASK_FEELING, TA_SYMPATHIZE };
    static const uint8_t tired[] = { TA_ASK_FEELING, TA_OFFER, TA_COMFORT };
    static const uint8_t ill[] = { TA_OFFER, TA_COMFORT, TA_ASK_FEELING };
    static const uint8_t lonely[] = { TA_INVITE, TA_ASK_FEELING, TA_COMPLIMENT };
    static const uint8_t scared[] = { TA_COMFORT, TA_SYMPATHIZE, TA_OFFER };
    static const uint8_t easy[] = { TA_JOKE, TA_LAUGH };
    static const uint8_t serious[] = { TA_ASK_ABOUT, TA_ASK_ADVICE, TA_ANSWER };
    static const uint8_t expr[] = { TA_COMPLIMENT, TA_HUG, TA_HIGH_FIVE };
    static const uint8_t blunt[] = { TA_DISAGREE, TA_OPINE_NEG, TA_TELL };
    static const uint8_t polite[] = { TA_THANK, TA_APOLOGIZE, TA_ASK_FEELING };
    static const uint8_t mid[] = { TA_ASK_OPINION, TA_TELL, TA_ENCOURAGE };
    static const uint8_t reconcile[] = { TA_APOLOGIZE, TA_ASK_FEELING, TA_SYMPATHIZE };
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
    if (mood == MOOD_SICK || (r && r->stat[STAT_HEALTH] < 30)) return QSET(ill);
    if (mood == MOOD_HUNGRY || (r && r->stat[STAT_HUNGER] < 20)) return QSET(hungry);
    if (mood == MOOD_SLEEPY || (r && r->stat[STAT_ENERGY] < 15)) return QSET(tired);
    if (mood == MOOD_LONELY || (r && r->stat[STAT_SOCIAL] < 20)) return QSET(lonely);
    if (mood == MOOD_SCARED) return QSET(scared);
    /* Only after urgent needs are addressed: a resident you hurt on the
     * previous visit gets a chance for a real apology, even if today's
     * topic and their permanent personality are otherwise cheerful.
     * This is backed by the current TALK save chunk, not made-up dialogue. */
    if (slot >= 0 && slot < s->nparts &&
        (s->conv.mem_flags[slot] & TMF_INSULTED))
        return QSET(reconcile);
    /* Explicit creator traits must have visible consequences beyond a
     * different voice. Keep the five saved persona sliders as the default
     * and let a custom trait steer only the second, suggested response.
     * Urgent needs and genuine distress above always take precedence.
     * Filter edgy traits by stage and the whole room before using them. */
    if (r && talk_trait_is_custom(id)) {
        TalkTraitSet ts;
        if (talk_trait_get(id, &ts)) {
            talk_trait_room(&ts, RES_STAGE(r), s->conv.aud.any_minor ||
                            (s->flags & TDF_KIDSAFE));
            if (talk_traitset_has(&ts, TT_SHY) || talk_traitset_has(&ts, TT_KIND))
                return QSET(polite);
            if (talk_traitset_has(&ts, TT_GOOFBALL) ||
                talk_traitset_has(&ts, TT_CHEERFUL) ||
                talk_traitset_has(&ts, TT_OUTGOING))
                return QSET(easy);
            if (talk_traitset_has(&ts, TT_BOOKWORM) ||
                talk_traitset_has(&ts, TT_CURIOUS) ||
                talk_traitset_has(&ts, TT_AMBITIOUS))
                return QSET(serious);
            if (talk_traitset_has(&ts, TT_MEAN) ||
                talk_traitset_has(&ts, TT_HOTHEADED))
                return QSET(blunt);
            if (talk_traitset_has(&ts, TT_CREATIVE))
                return QSET(expr);
        }
    }
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

/* Only suggest a new subject when the current concept is linked by the
 * curated association graph. Never substitute random word adjacency for
 * a conversational segue. One 16 B bank read, no persistent allocation. */
static int q_linked(uint16_t active, uint16_t candidate)
{
    uint16_t ids[TALK_ASSOC_TOP];
    int i, n;
    if (!candidate || candidate == active) return 0;
    if (!active) return 1;
    n = talk_db_assoc(active, ids, NULL);
    for (i = 0; i < n; i++) if (ids[i] == candidate) return 1;
    return 0;
}

int talk_quick_replies(const TalkState *s, int slot, uint16_t topic, uint16_t fresh, uint32_t salt, TalkSugg *out)
{
    QCtx q;
    TalkSugg tmp[TALK_QUICK_MAX];
    const TalkLine *th;
    const uint8_t *fl = NULL, *sl = NULL;
    int tslot = -1, act = TA_NONE, b = -1, i, fn, sn, argue, bridge = 0;
    uint32_t h;
    if (!s || !out || (s->flags & TDF_OVER)) return 0;
    if (s->q_kind && (s->q_st & TQ_ST_ASKED)) return talk_quest_quick(s, out);   /* TQ: a request is on the table: accept, decline, haggle, ask more */
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
    /* Current conversational context wins. Only on a neutral opening
     * may an otherwise empty thread resume the resident's saved subject.
     * This comes from the saved per-port TALK memory; no extra
     * long-lived tokens or sentence buffers per resident are allocated. */
    q.topic = th && th->topic ? th->topic : topic ? topic : s->topic_stack[0];
    if (!q.topic && slot >= 0 && slot < s->nparts &&
        (!th || act == TA_GREET || act == TA_WAVE || act == TA_BACKCHANNEL)) {
        q.topic = s->conv.mem_topic[slot];
    }
    if (q.topic && (!talk_gen_concept_ok(q.topic, &s->conv.aud) || q_grief(q.topic))) q.topic = TC_NONE;
    q.fresh = fresh && !q.sincere && q_linked(q.topic, fresh) &&
              talk_gen_concept_ok(fresh, &s->conv.aud) && !q_grief(fresh)
              ? fresh : TC_NONE;
    /* An associative callback must be earned: a remembered *person's*
     * subject linked in the lexicon to today's actual thread, not a random
     * nearby word. It is optional, kept separate from the ongoing replies. */
    if (!q.sincere && th && th->topic && q.topic == th->topic &&
        s->turn >= 3 && slot >= 0 && slot < s->nparts &&
        s->conv.mem_topic[slot] != TC_NONE &&
        q_linked(q.topic, s->conv.mem_topic[slot]) &&
        talk_gen_concept_ok(s->conv.mem_topic[slot], &s->conv.aud) &&
        !q_grief(s->conv.mem_topic[slot])) {
        q.fresh = s->conv.mem_topic[slot];
        bridge = 1;
    }
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
        q_from(&q, fl, fn, slot, h >> 2, tmp);                           /* 1: the thread */
    }
    if (b >= 0) {                                                  /* 2: the one that suits them */
        q.used[b >> 3] &= (uint8_t)~(1u << (b & 7));
        q_add(&q, NULL, b, h >> 4, tmp);
    }
    if (q.sincere) {                                               /* more of the same kindness */
        q_from(&q, ql_sinc, (int)sizeof ql_sinc, slot, h >> 9, tmp);
        q_from(&q, ql_sinc, (int)sizeof ql_sinc, slot, h >> 11, tmp);
    } else {
        if (q.topic && th) {
            /* Continue an existing thought first; changing subjects is a
             * visible, deliberate choice only for a linked candidate. */
            q_from(&q, ql_dive, (int)sizeof ql_dive, slot, h >> 9, tmp);
            /* Reserve the final option for a personal semantic bridge
             * only after the conversation has earned it. Never keep
             * pushing a new subject right after a topic change. */
            if (!(bridge && s->last_act != TA_CHANGE_TOPIC &&
                  q_add(&q, NULL, TA_CHANGE_TOPIC, h >> 11, tmp)))
                q_from(&q, ql_dive, (int)sizeof ql_dive, slot, h >> 11, tmp);
        } else {
            q_from(&q, ql_chg, (int)sizeof ql_chg, slot, h >> 9, tmp);
            q_from(&q, ql_wild, (int)sizeof ql_wild, slot, h >> 11, tmp);
        }
        q_from(&q, ql_wild, (int)sizeof ql_wild, slot, h >> 13, tmp);
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
    /* Deliberate shared-life actions are below the old categories so the
     * classic quick-reply and four-press compliment budgets stay intact. */
    add_row(K_OUT, 0, 0);
    add_row(K_PLAY, 0, 0);
    add_row(K_SHOW, 0, 0);
    add_row(K_GIVE, 0, 0);
    add_row(K_HELP, 0, 0);
    if (T.to >= 0 && T.to < T.n && T.ids[T.to] < DOOMO_MAX_RESIDENTS)
        add_row(K_QUIRK, 0, 0);
    /* A genuine sim thought bubble, not random chatter. Keep it below
     * quick replies so the one-press response budget stays unchanged.
     * It is refreshed on page entry/target switch, never each frame. */
    if (T.to >= 0 && T.to < T.n) {
        int pi = sim_problem_for(T.ids[T.to]);
        const DoomoProblem *p = sim_problem(pi);
        if (p && p->kind > PROB_NONE && p->kind < PROB_COUNT)
            add_row(K_CONCERN, p->kind, 0);
    }
    /* An accepted outing is not imaginary: show where to meet this
     * person today. The target is sampled on menu build, not on draw. */
    if (T.to >= 0 && T.to < T.n) {
        int venue = talk_outcome_meet_plan(T.ids[T.to]);
        if (venue >= 0) add_row(K_MEET_PLAN, 0, venue);
    }
    /* Make an *existing* request directly reachable from the main Talk
     * screen. Entering it opens the actual history/quest objective rather
     * than inventing a promise to hand something in. The row appears only
     * when this selected resident has an unfinished favor. */
    if (T.to >= 0 && T.to < T.n) {
        int kind, st;
        if (talk_quest_state(T.ids[T.to], &kind, &st, NULL, NULL) &&
            (st == TQS_ACTIVE || st == TQS_READY))
            add_row(K_HISTORY, 1, talk_quest_ready(T.ids[T.to]) ? TQS_READY : st);
    }
    /* A shared-memory callback is offered only for a real prior event,
     * for this particular person. It cannot invent a past relationship. */
    if (T.to >= 0 && T.to < T.n &&
        (talk_mem_shared(T.ids[T.to], TALK_SHARED_PLAYED) ||
         talk_mem_shared(T.ids[T.to], TALK_SHARED_GIFTED) ||
         talk_mem_shared(T.ids[T.to], TALK_SHARED_INVITED)))
        add_row(K_REMEMBER, 0, 0);
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
    /* The history is deliberately below Topic and Invite: existing
     * conversation shortcuts and journey button budgets remain intact. */
    if (T.to >= 0 && T.to < T.n) add_row(K_HISTORY, 0, 0);
    list_reset();
}

/* No new save bytes or heap allocations: this page presents the recorded
 * per-person facts already in TALK. Counts are snapshotted on entering,
 * not recomputed each video frame. Nothing here invents an outing. */
static void build_history(void)
{
    const TalkMem *m;
    int id, kind, st, days;
    if (T.to < 0 || T.to >= T.n) {
        ui_toast("Choose one person first.");
        build_more();
        return;
    }
    id = T.ids[T.to];
    m = talk_mem_find(id);
    T.page = PG_HISTORY;
    T.nrows = 0;
    /* Count real unfinished favors, not memory pins: a shared experience
     * can keep a resident in memory without creating a request. */
    add_row(K_HISTORY_INFO, talk_quest_open_count(), talk_mem_count());
    if (talk_mem_quest_get(id, &kind, &st, &days, NULL)) {
        /* Fetch and toy requests become ready as soon as the player has
         * the real item. Do not wait for a save or another day's tick. */
        int shown = talk_quest_ready(id) ? TQS_READY : st;
        add_row(K_HISTORY_QUEST, kind, (shown << 8) | days);
    }
    /* An observable explanation of the living relationship state. Players
     * should not need to decode numerical engagement or tension flags to
     * know whether an apology or a casual question makes sense. This is
     * sampled only on opening History, not drawn/calculated each frame. */
    add_row(K_HISTORY_TONE, T.s.part[T.to].engage, T.s.tension);
    {
        int venue = talk_outcome_meet_plan(id);
        if (venue >= 0) add_row(K_MEET_PLAN, 1, venue);
    }
    if (m) {
        /* Knowledge is revealed only by a real earlier conversation.
         * These are saved bits in TalkMem, not a menu-visit reward. */
        for (kind = 0; kind < 3; kind++)
            if (talk_mem_like_known(id, kind))
                add_row(K_HISTORY_LIKE, kind, talk_gen_like(id, kind));
        if (m->topic_a && talk_gen_concept_ok(m->topic_a, &T.s.conv.aud) &&
            !q_grief(m->topic_a))
            add_row(K_HISTORY_TOPIC, 0, m->topic_a);
        if (m->topic_b && m->topic_b != m->topic_a &&
            talk_gen_concept_ok(m->topic_b, &T.s.conv.aud) && !q_grief(m->topic_b))
            add_row(K_HISTORY_TOPIC, 1, m->topic_b);
        if (m->likes_known & TALK_SHARED_PLAYED) {
            add_row(K_HISTORY_EVENT, TALK_SHARED_PLAYED, 0);
            add_row(K_HISTORY_REPEAT, TALK_SHARED_PLAYED, 0);
        }
        if (m->likes_known & TALK_SHARED_GIFTED) {
            add_row(K_HISTORY_EVENT, TALK_SHARED_GIFTED, 0);
            add_row(K_HISTORY_REPEAT, TALK_SHARED_GIFTED, 0);
        }
        if (m->likes_known & TALK_SHARED_INVITED) {
            add_row(K_HISTORY_EVENT, TALK_SHARED_INVITED, 0);
            add_row(K_HISTORY_REPEAT, TALK_SHARED_INVITED, 0);
        }
    }
    list_reset();
}

/* One resident's actual outstanding thought: two obvious choices, with
 * the current problem checked again on every press. No saved state,
 * invented concern or duplicate copy of the simulation problem queue. */
static void build_concern(void)
{
    int pi;
    const DoomoProblem *p;
    if (T.to < 0 || T.to >= T.n) {
        ui_toast("Choose one person first.");
        build_main();
        return;
    }
    pi = sim_problem_for(T.ids[T.to]);
    p = sim_problem(pi);
    if (!p || p->kind <= PROB_NONE || p->kind >= PROB_COUNT) {
        ui_toast("That concern is no longer active.");
        build_main();
        return;
    }
    T.page = PG_CONCERN;
    T.nrows = 0;
    add_row(K_CONCERN_INFO, p->kind, 0);
    add_row(K_CONCERN_ACT, p->kind, 0);
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

/* Outings require a specific, available destination and one selected person.
 * "Invite someone" on the Options page is a distinct room-membership action.
 * The underlying DM accepts/refuses this invitation by character personality
 * and applies an accepted change to the person's real activity. */
static void build_outings(void)
{
    int i, target, minor;
    if (T.to < 0 || T.to >= T.n) {
        ui_toast("Choose one person first.");
        build_main();
        return;
    }
    target = T.ids[T.to];
    minor = T.s.conv.aud.any_minor || talk_safe_minor(target);
    T.page = PG_OUTING;
    T.nrows = 0;
    for (i = 0; i < (int)(sizeof outing / sizeof outing[0]); i++) {
        uint16_t concept = TC_NONE, wf = 0;
        uint32_t tags[4];
        if (!talk_act_invite_ok(TA_INVITE, outing[i].venue, minor, town_hour()) ||
            !sim_nudge_ok(target, outing[i].venue))
            continue;
        if (talk_lex_find(outing[i].key, &concept, &wf, tags) < 0 || !concept ||
            talk_act_venue_of(concept) != outing[i].venue)
            continue;
        add_row(K_OUTING, i, concept);
    }
    if (!T.nrows) {
        ui_toast("No outings available right now.");
        build_main();
        return;
    }
    list_reset();
}

/* Only the player's actually-held, safe non-key items appear. Keys are
 * story artifacts, not props to disclose or hand to a resident. This list
 * is bounded by the existing 40-row Talk UI; no inventory or string copy. */
static void build_show_items(void)
{
    int item, minor;
    if (T.to < 0 || T.to >= T.n || T.ids[T.to] < 0 ||
        T.ids[T.to] >= DOOMO_MAX_RESIDENTS || !town_alive(T.ids[T.to])) {
        ui_toast("Choose a resident first.");
        build_main();
        return;
    }
    minor = T.s.conv.aud.any_minor || talk_safe_minor(T.ids[T.to]);
    T.page = PG_SHOW;
    T.nrows = 0;
    for (item = ITEM_NONE + 1; item < ITEM_COUNT && T.nrows < TROWS; item++) {
        if (!inv_count(item) || doomo_items[item].category == ICAT_KEY ||
            (minor && talk_act_item_adult(item)))
            continue;
        add_row(K_SHOWITEM, item, 0);
    }
    if (!T.nrows) {
        ui_toast("No items to show right now.");
        build_main();
        return;
    }
    list_reset();
}

/* Giving and showing are intentionally separate inventory actions.
 * Only legitimate gifts owned at the time are selectable; the full
 * gift logic will recheck and consume one item at activation. */
static void build_give_items(void)
{
    int item, minor, target;
    if (T.to < 0 || T.to >= T.n || T.ids[T.to] < 0 ||
        T.ids[T.to] >= DOOMO_MAX_RESIDENTS || !town_alive(T.ids[T.to])) {
        ui_toast("Choose a living resident first.");
        build_main();
        return;
    }
    target = T.ids[T.to];
    minor = T.s.conv.aud.any_minor || talk_safe_minor(target) ||
            talk_safe_minor(T.avatar);
    T.page = PG_GIVE;
    T.nrows = 0;
    for (item = ITEM_NONE + 1; item < ITEM_COUNT && T.nrows < TROWS; item++) {
        if (inv_count(item) &&
            talk_act_gift_ok(item, talk_safe_minor(T.avatar), minor))
            add_row(K_GIVEITEM, item, 0);
    }
    if (!T.nrows) {
        ui_toast("No suitable gifts in your bag.");
        build_main();
        return;
    }
    list_reset();
}

/* The live needs, not a random text topic, define the help actions.
 * Show only useful care actions; inventory-based aid is selectable only
 * when the player owns an eligible item. No extra per-resident data or
 * frame-time scan (these lists are built on button press). */
static int care_target_id(void)
{
    int id = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS || !town_alive(id) ||
        town_res(id)->status != RES_ALIVE) return -1;
    return id;
}

static int care_item_ok(int item, int category)
{
    int minor = T.s.conv.aud.any_minor || talk_safe_minor(T.avatar);
    if (item <= ITEM_NONE || item >= ITEM_COUNT || !inv_count(item) ||
        doomo_items[item].category != category) return 0;
    if (minor && talk_act_item_adult(item)) return 0;
    return 1;
}

static int care_has_items(int category)
{
    int item;
    for (item = ITEM_NONE + 1; item < ITEM_COUNT; item++)
        if (care_item_ok(item, category)) return 1;
    return 0;
}

static void build_care(void)
{
    int target = care_target_id();
    const DoomoResident *r;
    if (target < 0) {
        ui_toast("This person can't receive care.");
        build_main();
        return;
    }
    r = town_res(target);
    T.page = PG_CARE;
    T.nrows = 0;
    add_row(K_COMFORT, 0, 0); /* supportive care always has a real effect */
    if (r->stat[STAT_HYGIENE] < 95) add_row(K_CLEAN, 0, 0);
    if (care_has_items(ICAT_FOOD)) add_row(K_FEED, 0, 0);
    if (care_has_items(ICAT_CARE) || care_has_items(ICAT_EXORCISM))
        add_row(K_HEAL, 0, 0);
    list_reset();
}

static void build_care_items(int is_med)
{
    int item;
    if (care_target_id() < 0) { build_main(); return; }
    T.page = is_med ? PG_HEAL : PG_FEED;
    T.nrows = 0;
    for (item = ITEM_NONE + 1; item < ITEM_COUNT && T.nrows < TROWS; item++) {
        if ((is_med && (care_item_ok(item, ICAT_CARE) || care_item_ok(item, ICAT_EXORCISM))) ||
            (!is_med && care_item_ok(item, ICAT_FOOD)))
            add_row(is_med ? K_HEALITEM : K_FEEDITEM, item, 0);
    }
    if (!T.nrows) { ui_toast("No suitable supplies in your bag."); build_care(); return; }
    list_reset();
}

/* The care menu appears earlier than the shared dialogue callback below.
 * Declare it here for strict C99 and the ARM/Dreamcast compilers. */
static void cb_after(void);

static void perform_care(int kind, int item)
{
    int target = care_target_id();
    const char *reply;
    if (target < 0) { ui_toast("This person can't receive care."); build_main(); return; }
    if (kind == K_CLEAN && town_res(target)->stat[STAT_HYGIENE] >= 95) {
        ui_toast("They're already clean.");
        build_care();
        return;
    }
    if (kind == K_FEEDITEM && !care_item_ok(item, ICAT_FOOD)) {
        ui_toast("You don't have that food.");
        build_care();
        return;
    }
    if (kind == K_HEALITEM && !care_item_ok(item, ICAT_CARE) &&
        !care_item_ok(item, ICAT_EXORCISM)) {
        ui_toast("You don't have that medicine.");
        build_care();
        return;
    }
    switch (kind) {
    case K_COMFORT:
        reply = sim_comfort(target);
        doomo_snprintf(T.you, sizeof T.you, "I'm here for you.");
        break;
    case K_CLEAN:
        reply = sim_clean(target);
        doomo_snprintf(T.you, sizeof T.you, "Let me help you clean up.");
        break;
    case K_FEEDITEM:
        reply = sim_feed(target, item);
        doomo_snprintf(T.you, sizeof T.you, "Would you like some %s?", doomo_items[item].name);
        break;
    case K_HEALITEM:
        reply = sim_heal(target, item);
        doomo_snprintf(T.you, sizeof T.you, "Let's try some %s.", doomo_items[item].name);
        break;
    default: build_care(); return;
    }
    ui_say(target, reply);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
    build_main();
}

static void topic_add(uint16_t c, int kind)
{
    int i;
    if (c == TC_NONE) return;
    for (i = 0; i < T.nrows; i++)
        if (T.row[i].v == c && (T.row[i].kind == K_PICK || T.row[i].kind == K_TOPIC_LINK)) return;
    add_row(kind, 0, c);
}

static void build_topics(void)
{
    static const char *const everyday[] = { "band", "fishing", "gardening" };
    uint16_t links[TALK_ASSOC_TOP], concept, form;
    uint32_t tags[4];
    const TalkLine *thread = talk_quick_thread(&T.s, NULL);
    uint16_t active = T.topic ? T.topic : T.s.topic_stack[0];
    int stock_start = T.nstock > 0;
    int stock_remaining = T.nstock - stock_start;
    int i, w, count;
    T.page = PG_TOPIC;
    T.nrows = 0;
    add_row(K_NOTOPIC, 0, 0);
    for (i = 0; i < TALK_STACK_MAX; i++) topic_add(T.s.topic_stack[i], K_PICK);
    if (T.to >= 0 && T.to < T.n)
        for (w = 0; w < 5; w++)
            if (talk_mem_like_known(T.ids[T.to], w)) topic_add(talk_gen_like(T.ids[T.to], w), K_PICK);
    /* Preserve the shortest path to the default topic. */
    if (stock_start) topic_add(T.stock[0], K_PICK);
    /* Read suggestions once on entering the picker. Leave room for the
     * existing stock topics and keep sensitive threads on their subject. */
    if (!q_grief(active) &&
        (!thread || (!(thread->flags & TLF_SINCERE) && !TA_IS_SAFETY(thread->act) && !q_grief(thread->topic)))) {
        for (i = 0; i < (int)(sizeof everyday / sizeof everyday[0]) && T.nrows + stock_remaining < TROWS; i++)
            if (talk_lex_find(everyday[i], &concept, &form, tags) >= 0)
                topic_add(concept, K_PICK);
        count = talk_db_assoc(active, links, NULL);
        for (i = 0; i < count && T.nrows + stock_remaining < TROWS; i++)
            if (links[i] != active && !q_grief(links[i]) &&
                talk_gen_concept_ok(links[i], &T.s.conv.aud))
                topic_add(links[i], T.pending ? K_PICK : K_TOPIC_LINK);
    }
    for (i = stock_start; i < T.nstock; i++) topic_add(T.stock[i], K_PICK);
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
    case K_INVITE: doomo_snprintf(buf, 48, "Add someone to chat >"); break;
    case K_OUT: doomo_snprintf(buf, 48, "Ask to go somewhere >"); break;
    case K_PLAY: doomo_snprintf(buf, 48, "Play together"); break;
    case K_SHOW: doomo_snprintf(buf, 48, "Show an item >"); break;
    case K_SHOWITEM: doomo_snprintf(buf, 48, "Show %s", doomo_items[r->arg].name); break;
    case K_GIVE: doomo_snprintf(buf, 48, "Give a gift >"); break;
    case K_GIVEITEM: doomo_snprintf(buf, 48, "Give %s", doomo_items[r->arg].name); break;
    case K_REMEMBER: doomo_snprintf(buf, 48, "Remember when we...?"); break;
    case K_HISTORY:
        if (r->arg == 1)
            doomo_snprintf(buf, 48, r->v == TQS_READY ?
                           "Favor ready: check request >" : "Your favor: check goal >");
        else doomo_snprintf(buf, 48, "Our history >");
        break;
    case K_HISTORY_INFO:
        doomo_snprintf(buf, 48, "Town memory: %d/%d, %d open",
                       (int)r->v, talk_mem_capacity(), (int)r->arg);
        break;
    case K_HISTORY_TONE:
        doomo_snprintf(buf, 48, "This talk: %s",
                       r->v >= TALK_TENSION_ARGUE ? "argument heating up" :
                       r->arg < 30 ? "keeping their distance" :
                       r->arg < 60 ? "warming up cautiously" :
                                     "comfortable together");
        break;
    case K_HISTORY_QUEST: {
        int st = r->v >> 8, days = r->v & 0xff;
        const char *phase = st == TQS_READY ? "ready" :
                            st == TQS_ACTIVE ? "active" : "settled";
        doomo_snprintf(buf, 48, "Favor: %s (%s, %dd)",
                       talk_quest_kind_name(r->arg), phase, days);
        break;
    }
    case K_HISTORY_TOPIC:
        topic_name(r->v, tn, (int)sizeof tn);
        doomo_snprintf(buf, 48, "%s: %s >", r->arg ? "Earlier topic" : "Last topic", tn);
        break;
    case K_HISTORY_EVENT:
        doomo_snprintf(buf, 48, "%s",
                       r->arg == TALK_SHARED_PLAYED ? "Recall playing together" :
                       r->arg == TALK_SHARED_GIFTED ? "Recall their gift" :
                       "Recall an invitation");
        break;
    case K_HISTORY_REPEAT:
        doomo_snprintf(buf, 48, "%s",
                       r->arg == TALK_SHARED_PLAYED ? "Play together again" :
                       r->arg == TALK_SHARED_GIFTED ? "Give another gift >" :
                       "Invite them out again >");
        break;
    case K_QUIRK: doomo_snprintf(buf, 48, "Tell me one little oddity"); break;
    case K_HISTORY_LIKE: {
        int id = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
        const DoomoResident *who = id >= 0 ? town_res(id) : NULL;
        if (!who) { doomo_snprintf(buf, 48, "A remembered preference"); break; }
        switch (r->arg) {
        case 0: doomo_snprintf(buf, 48, "Favorite food: %s >",
                                doomo_food_names[RES_LIKE_FOOD(who) % FOOD_COUNT]); break;
        case 1: doomo_snprintf(buf, 48, "Hobby: %s >",
                                doomo_hobby_names[RES_LIKE_HOBBY(who) % HOBBY_COUNT]); break;
        default: doomo_snprintf(buf, 48, "Music: %s >",
                                doomo_music_names[RES_LIKE_MUSIC(who) % MUSIC_COUNT]); break;
        }
        break;
    }
    case K_MEET_PLAN:
        doomo_snprintf(buf, 48, "Meet today: %s >", r->v < ACT_COUNT ?
                       doomo_activity_names[r->v] : "unknown place");
        break;
    case K_CONCERN:
        doomo_snprintf(buf, 48, "On their mind: %s >", doomo_problem_names[r->arg]);
        break;
    case K_CONCERN_INFO:
        doomo_snprintf(buf, 48, "Hear what happened");
        break;
    case K_CONCERN_ACT: {
        const char *action = "Help with this";
        switch (r->arg) {
        case PROB_HUNGRY: action = "Offer some food"; break;
        case PROB_DIRTY: action = "Help them wash"; break;
        case PROB_SICK: case PROB_CORRUPTION: action = "Offer treatment"; break;
        case PROB_BORED: action = "Play together"; break;
        case PROB_LONELY: case PROB_NIGHTMARE: action = "Comfort them"; break;
        case PROB_WANTS_FRIEND: action = "Help them make a friend"; break;
        case PROB_FIGHT: action = "Help them make peace"; break;
        case PROB_WANTS_ITEM: action = "Hand over their requested item"; break;
        case PROB_CRUSH: case PROB_CONFESS_HELP:
        case PROB_BREAKUP: case PROB_PROPOSAL: action = "Give relationship advice"; break;
        case PROB_POSSESSED: action = "How to help them"; break;
        }
        doomo_snprintf(buf, 48, "%s >", action);
        break;
    }
    case K_HELP: doomo_snprintf(buf, 48, "Help with their needs >"); break;
    case K_COMFORT: doomo_snprintf(buf, 48, "Comfort them"); break;
    case K_CLEAN: doomo_snprintf(buf, 48, "Help them wash"); break;
    case K_FEED: doomo_snprintf(buf, 48, "Offer something to eat >"); break;
    case K_HEAL: doomo_snprintf(buf, 48, "Use medicine >"); break;
    case K_FEEDITEM: doomo_snprintf(buf, 48, "Offer %s", doomo_items[r->arg].name); break;
    case K_HEALITEM: doomo_snprintf(buf, 48, "Use %s", doomo_items[r->arg].name); break;
    case K_OUTING:
        doomo_snprintf(buf, 48, "Visit %s", outing[r->arg].label);
        break;
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
    case K_TOPIC_LINK:
        topic_name(r->v, tn, (int)sizeof tn);
        doomo_snprintf(buf, 48, "Ask about %s", tn);
        break;
    case K_PICK:
        if (r->arg) doomo_snprintf(buf, 48, "%s", res_name(r->v));
        else topic_name(r->v, buf, 48);
        break;
    default: break;
    }
    return buf;
}

/* ---- running the talk ----------------------------------------------------------------------- */
static void begin_talk(void);

static void cb_after(void)
{
    if (T.handin) {
        TalkOutcome out;
        char msg[48];
        /* The genuine, generated thank-you is finished. Commit the
         * TEV_QUEST_DONE through the same outcome ledger as any other
         * conversation (real inventory, reward, bond, needs and news),
         * then seamlessly resume Talk in a fresh session. */
        T.handin = 0;
        talk_outcome_end(&T.s, &out, NULL);
        begin_talk();
        build_main();
        if (out.quests_done)
            doomo_snprintf(msg, sizeof msg, "Favor complete! H$%d earned", (int)out.reward);
        else doomo_snprintf(msg, sizeof msg, "Favor was not completed.");
        ui_toast(msg);
        T.after = 1;
        return;
    }
    T.busy = 0;
    T.after = 1;
}

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
    char msg[48], sgn[8];
    int i, bond = 0;
    talk_outcome_end(&T.s, &o, &sum);
    for (i = 0; i < T.s.nparts; i++) bond += o.rel[i];
    /* the game's snprintf has no "%+d": the sign goes in by hand (T8: the toast showed "Bond %+d") */
    doomo_snprintf(sgn, sizeof sgn, "%s%d", bond > 0 ? "+" : "", bond);
    if (o.invited) {
        int v = -1, k;
        for (k = 0; k < sum.nev; k++)
            if (sum.ev[k].kind == TEV_ACT_INVITE) { v = sum.ev[k].b; break; }
        if (v >= 0 && v < ACT_COUNT)
            doomo_snprintf(msg, sizeof msg, "Meet today: %s (City Map)", doomo_activity_names[v]);
        else doomo_snprintf(msg, sizeof msg, "Outing accepted! Check City Map.");
    }
    else if (o.made_up) doomo_snprintf(msg, sizeof msg, "They made up! Bond %s", sgn);
    else if (o.dating) doomo_snprintf(msg, sizeof msg, "A new couple! Bond %s", sgn);
    else if (sum.flags & TDF_K7) doomo_snprintf(msg, sizeof msg, "The chat ended.");
    else if (o.quests_done) doomo_snprintf(msg, sizeof msg, "Favor completed! Bond %s", sgn);
    else if (o.learned) doomo_snprintf(msg, sizeof msg, "Learned something new! Bond %s", sgn);
    else if (o.problems) doomo_snprintf(msg, sizeof msg, "You helped them! Bond %s", sgn);
    else doomo_snprintf(msg, sizeof msg, "Chat over. Bond %s", sgn);
    T.active = 0;
    scene_pop();
    ui_toast(msg);
}

static void begin_talk(void)
{
    uint32_t seed = talk_gen_seed(T.avatar, T.ids, T.n) + (++sessions) * 2654435761u;
    talk_outcome_begin(&T.s, T.avatar, T.ids, T.n, seed);
    if (T.to >= T.n) T.to = 0;
    T.busy = T.after = T.leaving = T.pending = T.gen = 0;
    T.idle = 0;
    T.you[0] = 0;
}

/* The generator's steps for the lines just asked for, then the lines go into the dialogue box. A step is 3 to 17 ms on
 * the Dreamcast's SH4 (the first one of "Ask about pie" 17 ms, the three of an answer 26 ms in all, on top of the
 * 8 ms the screen costs), which is a tick over 33.3 ms: with talk_scene_slice(1) (TD2: the Dreamcast, and the unit
 * test) the steps run one a tick from talk_update, the input is dead meanwhile, and the lines are the very same.
 * T.gen: 1 an answer to the player (a farewell sets leaving), 2 the opener or the idle chatter. Not on the GBA. */
#ifndef DOOMO_PLATFORM_GBA
static uint8_t slice_on;
void talk_scene_slice(int on) { slice_on = (uint8_t)(on != 0); }
#endif

static void gen_done(void)
{
    int k;
    if (T.handin && !T.s.nlines) {
        /* No generated thank-you means no completion event was emitted.
         * Never pay on an empty or missing language-bank response. */
        T.handin = 0;
        T.gen = 0;
        ui_toast("That favor cannot be handed in right now.");
        build_history();
        return;
    }
    if (T.gen == 1 && (T.s.flags & TDF_FAREWELL)) T.leaving = 1;
    /* Once an outing has been *accepted* and the answer generated, close
     * the encounter after its dialogue box: talk_outcome_end commits the
     * promised venue change. A refusal keeps the conversation open. */
    if (T.gen == 1 && T.s.in.act[0] == TA_INVITE)
        for (k = 0; k < T.s.nev; k++)
            if (T.s.ev[k].kind == TEV_ACT_INVITE) { T.leaving = 1; break; }
    T.gen = 0;
    show_lines();
}

static void gen_run(int kind)
{
    T.gen = (uint8_t)kind;
#ifndef DOOMO_PLATFORM_GBA
    if (slice_on) return;                /* talk_update takes the steps, one a tick */
#endif
    while (talk_dm_step(&T.s, NULL)) {}
    gen_done();
}

static void say_utter(TalkUtter *u)
{
    doomo_snprintf(T.you, sizeof T.you, "%s", u->shown);
    talk_dm_say(&T.s, u);
    gen_run(1);
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

static void play_together(void)
{
    int target = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
    const char *reply;
    if (target < 0 || target >= DOOMO_MAX_RESIDENTS || !town_alive(target)) {
        ui_toast("Choose a living resident first.");
        return;
    }
    /* sim_play applies Energy, Joy, Social and care/relationship changes.
     * Unlike a generic Talk utterance this really plays the activity. */
    {
        const DoomoResident *r = town_res(target);
        int can_play = r && (r->status == RES_ALIVE || r->status == RES_GHOST) &&
                       (r->status == RES_GHOST || r->stat[STAT_ENERGY] >= 15);
        reply = sim_play(target);
        if (can_play) talk_mem_note_shared(target, TALK_SHARED_PLAYED);
    }
    doomo_snprintf(T.you, sizeof T.you, "Let's play together!");
    ui_say(target, reply);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
}

static void show_item(int item)
{
    int target = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
    const char *reply;
    if (target < 0 || target >= DOOMO_MAX_RESIDENTS || !town_alive(target) ||
        item <= ITEM_NONE || item >= ITEM_COUNT ||
        !inv_count(item) || doomo_items[item].category == ICAT_KEY ||
        ((T.s.conv.aud.any_minor || talk_safe_minor(target)) && talk_act_item_adult(item))) {
        ui_toast("That item can't be shown here.");
        build_main();
        return;
    }
    reply = sim_show(target, item);
    doomo_snprintf(T.you, sizeof T.you, "Look at my %s!", doomo_items[item].name);
    ui_say(target, reply);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
    build_main();
}

static void give_item(int item)
{
    int target = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
    const char *reply;
    int minor;
    if (target < 0 || target >= DOOMO_MAX_RESIDENTS || !town_alive(target)) {
        ui_toast("Choose a living resident first.");
        build_main();
        return;
    }
    minor = T.s.conv.aud.any_minor || talk_safe_minor(target) ||
            talk_safe_minor(T.avatar);
    if (item <= ITEM_NONE || item >= ITEM_COUNT || !inv_count(item) ||
        !talk_act_gift_ok(item, talk_safe_minor(T.avatar), minor)) {
        ui_toast("This gift isn't available.");
        build_main();
        return;
    }
    /* Unlike sim_show, sim_gift consumes one real item and applies the
     * resident's actual preferences, needs, friendship and progression.
     * There is no imaginary item and no duplicated inventory. */
    reply = sim_gift(target, item);
    /* The care system handles the actual transfer. Only recorded as a
     * gift if a real recipient accepted its item-care code path. */
    talk_mem_note_shared(target, TALK_SHARED_GIFTED);
    doomo_snprintf(T.you, sizeof T.you, "This %s is for you!", doomo_items[item].name);
    ui_say(target, reply);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
    build_main();
}

/* A Tomodachi-style thought bubble is actionable because it delegates to
 * the *existing* care/problem machinery. No free hunger/medicine rewards;
 * romance advice still goes through sim's age, consent and partner gates. */
/* An odd but honest admission rooted in one resident's actual tastes.
 * It is deliberately separate from the broad persona archetypes: a shy
 * resident can love rowdy music, and a demon can adore baking. No RNG,
 * assets, heap, new save fields, or per-frame processing. */
static void tell_oddity(void)
{
    static const char *const food[4] = {
        "I put a place setting out for %s. It looked lonely.",
        "I dreamed a cloud tasted like %s. I woke up hungry.",
        "I tried to share %s with my shadow. It wasn't hungry.",
        "I wish my front door smelled like %s. Is that possible?"
    };
    static const char *const hobby[4] = {
        "I showed a little flower how to do %s. It just stared.",
        "I keep %s notes under my pillow. It gets lumpy.",
        "I practiced %s in a dream. I forgot the rules.",
        "I made a tiny medal for %s. I gave it to a spoon."
    };
    static const char *const music[4] = {
        "My shoes tap to %s even when I tell them not to.",
        "The floorboards hum %s after dark. Quietly.",
        "I think the moon likes %s. It always looks pleased.",
        "I played %s for a houseplant. It stood up straighter."
    };
    char line[144];
    int id, which, start, i, flavor;
    const DoomoResident *who;
    if (T.to < 0 || T.to >= T.n) { ui_toast("Choose a resident first."); return; }
    id = T.ids[T.to];
    who = id >= 0 && id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    if (!who || who->status != RES_ALIVE) {
        ui_toast("They can't discuss their habits right now.");
        return;
    }
    if (T.s.tension >= TALK_TENSION_ARGUE || q_grief(T.topic) ||
        who->mood == MOOD_SAD || who->mood == MOOD_SCARED ||
        who->mood == MOOD_ANGRY) {
        ui_toast("Not the moment for jokes. Try listening or comforting.");
        return;
    }
    start = (int)((((uint32_t)id * 11u) + doomo_town.day) % 3u);
    which = start;
    for (i = 0; i < 3; i++) {
        which = (start + i) % 3;
        if (!talk_mem_like_known(id, which)) break;
    }
    flavor = ((int)RES_CATCHPHRASE(who) + id) & 3;
    switch (which) {
    case 0:
        doomo_snprintf(line, sizeof line, food[flavor],
                       doomo_food_names[RES_LIKE_FOOD(who) % FOOD_COUNT]);
        break;
    case 1:
        doomo_snprintf(line, sizeof line, hobby[flavor],
                       doomo_hobby_names[RES_LIKE_HOBBY(who) % HOBBY_COUNT]);
        break;
    default:
        doomo_snprintf(line, sizeof line, music[flavor],
                       doomo_music_names[RES_LIKE_MUSIC(who) % MUSIC_COUNT]);
        break;
    }
    /* A spoken preference is real knowledge and becomes a History row
     * and a follow-up subject in the existing Talk topic picker. */
    talk_mem_know_like(id, which);
    doomo_snprintf(T.you, sizeof T.you, "What's one odd little thing about you?");
    ui_say(id, line);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
    build_main();
}

static void concern_reply(int id, const char *reply, const char *you)
{
    doomo_snprintf(T.you, sizeof T.you, "%s", you);
    ui_say(id, reply);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
    build_main();
}

static void concern_info(void)
{
    int id, pi;
    char message[96];
    if (T.to < 0 || T.to >= T.n) { build_main(); return; }
    id = T.ids[T.to];
    pi = sim_problem_for(id);
    if (pi < 0) { build_concern(); return; }
    sim_problem_text(pi, message, (int)sizeof message);
    doomo_snprintf(T.you, sizeof T.you, "What's on your mind?");
    ui_say(id, message);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
}

static void concern_choice(int c)
{
    int id, pi;
    const DoomoProblem *p;
    const char *reply;
    if (T.to < 0 || T.to >= T.n) { build_main(); return; }
    id = T.ids[T.to];
    pi = sim_problem_for(id);
    p = sim_problem(pi);
    if (!p) { build_concern(); return; }
    switch (p->kind) {
    case PROB_CRUSH: case PROB_CONFESS_HELP:
        if (p->other == 0xFFFF) { build_concern(); return; }
        reply = sim_advise_confess(id, p->other, c == 0);
        break;
    case PROB_BREAKUP: reply = sim_advise_breakup(id, c == 0); break;
    case PROB_PROPOSAL: reply = sim_advise_propose(id, c == 0); break;
    default: build_concern(); return;
    }
    concern_reply(id, reply, c == 0 ? "I think you should try." : "I'd take it slowly.");
}

static void concern_action(void)
{
    int id, pi, minor;
    const DoomoProblem *p;
    if (T.to < 0 || T.to >= T.n) { build_main(); return; }
    id = T.ids[T.to];
    pi = sim_problem_for(id);
    p = sim_problem(pi);
    if (!p) { build_concern(); return; } /* expired while the page was open */
    switch (p->kind) {
    case PROB_HUNGRY: build_care_items(0); return;
    case PROB_DIRTY: perform_care(K_CLEAN, 0); return;
    case PROB_SICK: case PROB_CORRUPTION: build_care_items(1); return;
    case PROB_BORED: play_together(); build_main(); return;
    case PROB_LONELY: case PROB_NIGHTMARE: perform_care(K_COMFORT, 0); return;
    case PROB_WANTS_FRIEND:
        sim_problem_resolve(pi);
        concern_reply(id, sim_last_reaction(), "Maybe I can introduce you.");
        return;
    case PROB_FIGHT:
        if (p->other == 0xFFFF) { ui_toast("Their friend is no longer available."); return; }
        sim_problem_resolve(pi);
        concern_reply(id, sim_last_reaction(), "Let's sort this out.");
        return;
    case PROB_WANTS_ITEM:
        minor = T.s.conv.aud.any_minor || talk_safe_minor(id);
        if (p->item <= ITEM_NONE || p->item >= ITEM_COUNT || !inv_count(p->item) ||
            !talk_act_gift_ok(p->item, talk_safe_minor(T.avatar), minor)) {
            ui_toast("Find a suitable requested item first.");
            return;
        }
        /* This routine consumes the item once and grants the real sim reward;
         * selecting the informational row never touches inventory. */
        sim_problem_resolve(pi);
        concern_reply(id, sim_last_reaction(), "I brought the thing you wanted.");
        return;
    case PROB_CRUSH: case PROB_CONFESS_HELP: {
        static const char *const choices[2] = { "Tell them how you feel", "Not yet" };
        ui_choice(id, "Should I tell them how I feel?", choices, 2, concern_choice);
        return;
    }
    case PROB_BREAKUP: {
        static const char *const choices[2] = { "Work it out", "Let them go" };
        ui_choice(id, "What about our relationship?", choices, 2, concern_choice);
        return;
    }
    case PROB_PROPOSAL: {
        static const char *const choices[2] = { "Pop the question!", "Take it slowly" };
        ui_choice(id, "Should I propose?", choices, 2, concern_choice);
        return;
    }
    case PROB_POSSESSED:
        ui_toast("An exorcism is needed. Open their Care screen.");
        return;
    default:
        ui_toast("You can help from the resident Care screen.");
        return;
    }
}

static void reminisce(unsigned selected)
{
    int target = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
    const DoomoResident *r;
    const char *reply = NULL;
    if (target < 0 || target >= DOOMO_MAX_RESIDENTS || !town_alive(target))
        return;
    r = town_res(target);
    /* Existing one-tap callback chooses the first known memory; History
     * supplies a specific event, which is rechecked at activation time. */
    if (!selected) {
        if (talk_mem_shared(target, TALK_SHARED_PLAYED)) selected = TALK_SHARED_PLAYED;
        else if (talk_mem_shared(target, TALK_SHARED_GIFTED)) selected = TALK_SHARED_GIFTED;
        else if (talk_mem_shared(target, TALK_SHARED_INVITED)) selected = TALK_SHARED_INVITED;
    }
    if (selected == TALK_SHARED_PLAYED && talk_mem_shared(target, selected))
        reply = RES_PERSONA(r, PERS_SPEECH) >= 10
            ? "That day we played together? I still remember the fun!"
            : "I remember playing together. That was nice.";
    else if (selected == TALK_SHARED_GIFTED && talk_mem_shared(target, selected))
        reply = RES_PERSONA(r, PERS_EXPRESSION) >= 9
            ? "That gift you gave me! I remember it so clearly!"
            : "I remember your present. Thanks for thinking of me.";
    else if (selected == TALK_SHARED_INVITED && talk_mem_shared(target, selected))
        reply = RES_PERSONA(r, PERS_SPEECH) >= 10
            ? "You invited me out once. That caught me by surprise!"
            : "I remember you inviting me somewhere.";
    if (!reply) { build_main(); return; }
    doomo_snprintf(T.you, sizeof T.you, "Remember our last time?");
    ui_say(target, reply);
    T.busy = 1;
    T.idle = 0;
    ui_say_done(cb_after);
}

static void say_quick(int i)
{
    TalkUtter u;
    const TalkSugg *g;
    if (i < 0 || i >= T.nquick) return;
    g = &T.quick[i];
    /* A suggested invitation with no specified real venue must open the
     * destination picker. Never promise an arbitrary nearby activity. */
    if (g->act == TA_INVITE) { build_outings(); return; }
    if (g->act == TA_OFFER)  { build_care(); return; }
    if (g->act == TA_GIVE)   { build_give_items(); return; }
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
    /* Spoken intentions that promise an actual game action must not
     * turn into empty gestures. Use exactly the same verified target,
     * inventory and activity menus as the controller actions. */
    if (u.target >= 0 && u.target < T.n) T.to = u.target;
    if (u.act[0] == TA_INVITE) { build_outings(); return; }
    if (u.act[0] == TA_OFFER) { build_care(); return; }
    if (u.act[0] == TA_GIVE) { build_give_items(); return; }
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
        if (r->arg == TA_INVITE) { build_outings(); break; }
        if (r->arg == TA_OFFER) { build_care(); break; }
        if (r->arg == TA_GIVE) { build_give_items(); break; }
        if (wants_topic(r->arg) && T.topic == TC_NONE) { T.pending = r->arg; build_topics(); }
        else say_act(r->arg);
        break;
    case K_QUICK: say_quick(r->arg); break;
    case K_OUT: build_outings(); break;
    case K_PLAY: play_together(); break;
    case K_SHOW: build_show_items(); break;
    case K_SHOWITEM: show_item(r->arg); break;
    case K_GIVE: build_give_items(); break;
    case K_GIVEITEM: give_item(r->arg); break;
    case K_REMEMBER: reminisce(0); break;
    case K_HISTORY: {
        int shortcut = r->arg;
        build_history();
        /* The main-page shortcut opens with the real favor already
         * selected, not the generic table-capacity status above it. */
        if (shortcut && T.nrows > 1 && T.row[1].kind == K_HISTORY_QUEST)
            T.list.sel = 1;
        break;
    }
    case K_HISTORY_INFO:
        ui_toast("Open favors are protected from eviction.");
        break;
    case K_HISTORY_TONE:
        ui_toast(r->v >= TALK_TENSION_ARGUE ?
                 "Listen or apologize before this becomes a fight." :
                 r->arg < 30 ? "Ask how they feel; give them room." :
                 r->arg < 60 ? "Take an interest in what matters to them." :
                               "They seem comfortable talking to you.");
        break;
    case K_HISTORY_QUEST: {
        int id, kind, state, days, i;
        TalkQuestRow q;
        if (T.to < 0 || T.to >= T.n) { build_more(); break; }
        id = T.ids[T.to];
        /* A saved row can be stale after inventory changes, expiration, or
         * another Talk. Re-check live state only when the player selects it. */
        if (!talk_quest_state(id, &kind, &state, &days, NULL)) {
            ui_toast("There is no outstanding request.");
            build_history();
            break;
        }
        if (talk_quest_ready(id)) {
            int queued = 0;
            /* Hand-in rewards flow through talk_outcome_end, where they
             * correctly apply the relationship and news ledger. Never
             * award money merely for inspecting the journal. */
            for (i = 0; i < T.s.nev; i++)
                if (T.s.ev[i].kind == TEV_QUEST_DONE &&
                    T.s.ev[i].a == T.to) queued = 1;
            if (!queued && talk_dm_hand_in(&T.s, T.to)) {
                /* Generate the resident's real TQP_DONE response; the
                 * callback commits the event after the player reads it.
                 * Prevent double claiming and preserve cross-port slicing. */
                T.handin = 1;
                doomo_snprintf(T.you, sizeof T.you, "I've finished your favor.");
                gen_run(2);
            } else {
                ui_toast(queued ? "Finish this chat to receive the reward." :
                                  "The favor cannot be handed in yet.");
            }
        } else if (state == TQS_ACTIVE) {
            /* The same specific instruction shown in the Side quest log,
             * including the target or required item. No fake task text. */
            int found = 0;
            for (i = 0; i < talk_quest_log_count(); i++)
                if (talk_quest_log_row(i, &q) && q.giver == id) {
                    ui_toast(q.goal);
                    found = 1;
                    break;
                }
            if (!found) ui_toast("Check the Side quest log for details.");
        } else {
            ui_toast(days ? "This request has been settled." :
                            "That request expired.");
        }
        break;
    }
    case K_TOPIC_LINK:
    case K_HISTORY_TOPIC:
        if (talk_gen_concept_ok(r->v, &T.s.conv.aud) && !q_grief(r->v)) {
            T.topic = r->v;
            say_act(TA_ASK_ABOUT); /* A genuine Talk question, not just a label. */
        }
        break;
    case K_HISTORY_EVENT: reminisce(r->arg); break;
    case K_HISTORY_REPEAT:
        /* Never allow a forged/stale row to manufacture a memory. These
         * dispatch into the *real* activity, inventory and venue paths. */
        if (T.to < 0 || T.to >= T.n ||
            !talk_mem_shared(T.ids[T.to], r->arg)) { build_history(); break; }
        if (r->arg == TALK_SHARED_PLAYED) play_together();
        else if (r->arg == TALK_SHARED_GIFTED) build_give_items();
        else if (r->arg == TALK_SHARED_INVITED) build_outings();
        break;
    case K_QUIRK: tell_oddity(); break;
    case K_HISTORY_LIKE:
        if (T.to >= 0 && T.to < T.n &&
            talk_mem_like_known(T.ids[T.to], r->arg) && r->v &&
            talk_gen_concept_ok(r->v, &T.s.conv.aud) && !q_grief(r->v)) {
            T.topic = r->v;
            say_act(TA_ASK_ABOUT); /* actually converse about the discovery */
        } else ui_toast("Ask about this preference another time.");
        break;
    case K_MEET_PLAN: {
        char hint[80];
        int venue, id = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
        venue = id >= 0 ? talk_outcome_meet_plan(id) : -1;
        if (venue < 0) {
            ui_toast("This invitation is no longer active.");
        } else if (talk_outcome_meet_ready(venue) != id) {
            ui_toast("They have moved on; try another invitation tomorrow.");
        } else {
            doomo_snprintf(hint, sizeof(hint),
                           "Visit %s on the City Map today.",
                           doomo_activity_names[venue]);
            ui_toast(hint);
        }
        break;
    }
    case K_CONCERN: build_concern(); break;
    case K_CONCERN_INFO: concern_info(); break;
    case K_CONCERN_ACT: concern_action(); break;
    case K_HELP: build_care(); break;
    case K_COMFORT: case K_CLEAN: perform_care(r->kind, 0); break;
    case K_FEED: build_care_items(0); break;
    case K_HEAL: build_care_items(1); break;
    case K_FEEDITEM: case K_HEALITEM: perform_care(r->kind, r->arg); break;
    case K_OUTING:
        T.to = T.to < 0 ? 0 : T.to;
        T.topic = r->v;
        say_act(TA_INVITE);
        break;
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
        gen_run(2);
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

/* who you speak to (SELECT, or a click on a name): the page that shows it is built again */
static void change_to(int to)
{
    T.to = (int8_t)to;
    if (T.page == PG_MORE) { int sel = T.list.sel; build_more(); T.list.sel = sel; }
    else if (T.page == PG_HISTORY) build_history();
    else if (T.page == PG_MAIN) {
        int c;
        build_main();
        /* You turned to someone else: the quick replies answer what was said before, so the cursor
         * waits on the first category instead. A kind word to them is then 4 presses (SELECT, down to
         * "Be nice", A, A): the budget of docs/design/talk/TALK_DESIGN.md 2.3 (T8). */
        for (c = 0; c < T.nrows; c++)
            if (T.row[c].kind == K_CAT) { T.list.sel = c; break; }
    }
}

/* the panel: header 16 px + the list rows (the draw and the pointer share it) */
static int layout(int *rows_out)
{
    int sh = plat_screen_h(), rows = 6, ph = 18 + rows * UI_LIST_ROW_H;
    if (ph > sh - 26) { ph = sh - 26; rows = (ph - 18) / UI_LIST_ROW_H; }
    T.list.visible = rows < 1 ? 1 : rows;
    if (rows_out) *rows_out = rows;
    return sh - ph;
}

#if DOOMO_POINTER
/* T9: a click is exactly the pad press it stands for (docs/TALK.md "The pointer"):
 *   a name in the people strip   SELECT until you speak to them (only with more than one in the room)
 *   a row of the panel           the row is selected and A is pressed (quick reply, category, act,
 *                                typed line, topic, invite, Options, Say goodbye)
 *   the panel's header           B on a lower page (back to the replies); nothing on the main page, so a stray
 *                                click never says goodbye
 * Returns 1 when the click was used. Never needed: the pad does all of it. */
static int pointer_click(int x, int y)
{
    int py = layout(NULL), sw = plat_screen_w(), n = T.s.nparts > 0 ? T.s.nparts : 1;
    if (y < 24) {
        int i = x / (sw / n);
        if (i >= n) i = n - 1;
        if (T.n > 1 && i < T.n && i != T.to && !(T.s.part[i].flags & TPF_LEFT)) change_to(i);
        return 1;
    }
    if (y < py) return 0;
    if (y < py + 17) {
        if (T.page != PG_MAIN) { T.idle = 0; T.pending = 0; if (T.page == PG_HISTORY) build_more(); else build_main(); }
        return 1;
    }
    if (ui_list_click(&T.list, 6, py + 17, sw - 12, x, y) == UI_SELECT) { T.idle = 0; activate(T.list.sel); }
    return 1;
}

/* a small arrow where the pointer is (only while a pointer is present) */
static void pointer_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int x, y, i;
    if (!input_pointer(&x, &y)) return;
    for (i = 0; i < 7; i++) plat_rect(x + 1, y + 1 + i, i + 1, 1, st->fill);
    for (i = 0; i < 7; i++) plat_rect(x, y + i, i + 1, 1, st->text);
}
#endif

static void talk_update(void)
{
    int r;
    if (!T.active) { scene_pop(); return; }
    if (T.busy) return;                  /* the dialogue box is up (we are not on top then) */
    if (T.gen) {                         /* sliced generation: one step a tick, no input meanwhile */
        if (!talk_dm_step(&T.s, NULL)) gen_done();
        return;
    }
#ifdef TALK_TYPING
    if (T.typed_ok) {
        int ok = T.typed_ok == 1;
        T.typed_ok = 0;
        if (ok && T.typed[0]) { say_typed(); return; }
    }
#endif
    if (T.after) {
        T.after = 0;
        if (T.s.call_to && !T.leaving && !talk_dm_over(&T.s) && T.n < TALK_PARTS_MAX) {
            invite((int)T.s.call_to - 1);                 /* TM2e: "I'll get someone": they come in (the event is applied by invite's talk_outcome_end) */
            return;
        }
        if (T.leaving || talk_dm_over(&T.s)) { finish(); return; }
        if (T.page == PG_MAIN) build_main();          /* new replies for what was just said */
    }
    if (T.page == PG_MAIN && ++T.idle % 8 == 0 && talk_dm_tick(&T.s, 8)) {
        gen_run(2);
        return;
    }
    if (btn_pressed(BTN_SELECT) && T.n > 1) {
        change_to(T.to + 1 >= T.n ? -1 : T.to + 1);
        return;
    }
#if DOOMO_POINTER
    { int cx, cy; if (input_pointer_click(&cx, &cy) && pointer_click(cx, cy)) return; }
#endif
    r = ui_list_update(&T.list);
    if (r == UI_SELECT) { T.idle = 0; activate(T.list.sel); }
    else if (r == UI_CANCEL) {
        T.idle = 0;
        if (T.page != PG_MAIN) { T.pending = 0; if (T.page == PG_HISTORY) build_more(); else build_main(); }
        else bye();
    }
}

/* "calm" is the usual state and says nothing; only a tense or heated room is named (less on screen) */
static const char *mood_word(void)
{
    if (T.s.tension >= TALK_TENSION_ARGUE + 2) return "heated";
    if (T.s.tension >= TALK_TENSION_ARGUE - 2) return "tense";
    return NULL;
}

/* One calm panel: a single instruction line (what you are choosing; the room's mood on the right only when it is
 * not calm), then the list. The people strip marks who you are speaking to with a '>' and shows the engage bar of
 * that person only (everyone's when you speak to the room). The pad's buttons are one small card above the panel
 * (TUI-TALK): A picks, B goes back (on the replies page B says goodbye). No window glint moves here. */
static const char *panel_title(char *buf, int cap)
{
    const char *t = NULL;
    switch (T.page) {
    case PG_TOPIC:  t = "Talk about..."; break;
    case PG_INVITE: t = "Who joins in?"; break;
    case PG_OUTING: t = "Where shall we go?"; break;
    case PG_SHOW: t = "Show something..."; break;
    case PG_GIVE: t = "Choose a gift..."; break;
    case PG_CARE: t = "How can you help?"; break;
    case PG_FEED: t = "Offer what food?"; break;
    case PG_HEAL: t = "Choose a treatment"; break;
    case PG_HISTORY: t = "Our history"; break;
    case PG_CONCERN: t = "What's on their mind?"; break;
    case PG_MORE:   t = "Options"; break;
    case PG_ACTS:   t = cats[T.cat].name; break;
    default: break;
    }
    if (t) {
#if DOOMO_POINTER
        if (input_pointer(NULL, NULL)) { doomo_snprintf(buf, (size_t)cap, "< %s", t); return buf; }   /* T9: click = back */
#endif
        return t;
    }
    if (T.you[0]) { doomo_snprintf(buf, (size_t)cap, "You: %s", T.you); return buf; }
    return "Say something";
}

static void talk_card(int y)
{
    UiPrompt p[2];
    p[0].btn = BTN_A; p[0].prio = 0; p[0].verb = "Pick";
    p[1].btn = BTN_B; p[1].prio = 1; p[1].verb = T.page == PG_MAIN ? "Goodbye" : "Back";
    ui_prompts_at(p, 2, WIN_CUTE, y, ui_prompts_maxw());
}

static void talk_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), sh = plat_screen_h(), i, ph, py, colw, rows, mw = 0;
    char ttl[48];
    const char *mood = mood_word();
    ui_glint_hold(1);
    /* the people: a name and (for who you speak to) how much they are into it */
    ui_window(0, 0, sw, 24, WIN_CUTE);
    colw = sw / (T.s.nparts > 0 ? T.s.nparts : 1);
    for (i = 0; i < T.s.nparts; i++) {
        const TalkPart *p = &T.s.part[i];
        int x = i * colw + 6;
        if (i == T.to) ui_text_clip(x - 5, 4, ">", st->text, x - 5, x);
        ui_text_clip(x, 4, p->id == TALK_PET_ID ? "Pet" : res_name(p->id), st->text, x, x + colw - 8);
        if (p->flags & TPF_LEFT) ui_text_clip(x, 14, "(left)", st->text_dim, x, x + colw - 8);
        else if (T.to < 0 || i == T.to) {
#if UI_PANELS
            ui_bar_still(x, 15, colw - 14, 5, p->engage, 100, st->accent);
#else
            ui_bar(x, 15, colw - 14, 5, p->engage, 100, st->accent);
#endif
        }
    }
    py = layout(&rows);
    ph = sh - py;
    ui_window(0, py, sw, ph, WIN_CUTE);
    ui_glint_hold(0);
    if (mood) {
        mw = ui_text_width(mood);
        ui_text_clip(sw - 8 - mw, py + 5, mood, st->text_dim, sw - 8 - mw, sw - 8);
    }
    ui_text_clip(8, py + 5, panel_title(ttl, (int)sizeof ttl), st->text, 8, sw - 12 - mw);
    ui_list_draw(&T.list, 6, py + 17, sw - 12, WIN_CUTE);
    if (!T.busy && py - 14 >= 26) talk_card(py - 14);   /* not under an open dialogue box (A advances the line) */
#if DOOMO_POINTER
    pointer_draw();
#endif
}

#if DOOMO_POINTER
/* T9: what the screen shows, for the runner's expect-talk and the pointer test (native only) */
int talk_scene_view(int *page, int *sel, int *top, int *to, int *nrows, int *busy, char *you)
{
    if (!T.active) return 0;
    if (page) *page = T.page;
    if (sel) *sel = T.list.sel;
    if (top) *top = T.list.top;
    if (to) *to = T.to;
    if (nrows) *nrows = T.nrows;
    if (busy) *busy = T.busy;
    if (you) doomo_snprintf(you, TALK_SHOWN_MAX, "%s", T.you);
    return 1;
}

/* the text of row i (what the list draws), "" past the end */
const char *talk_scene_row(int i, char *buf48) { return row_text(i, buf48); }

/* the row whose text starts with `prefix`, or -1 */
int talk_scene_find(const char *prefix)
{
    char buf[48];
    int i, n = (int)strlen(prefix);
    for (i = 0; i < T.nrows; i++)
        if (strncmp(row_text(i, buf), prefix, (size_t)n) == 0) return i;
    return -1;
}

/* the player's name of the people strip is drawn at the column x of person i: where to click */
int talk_scene_person_x(int i)
{
    int sw = plat_screen_w(), n = T.s.nparts > 0 ? T.s.nparts : 1;
    return (sw / n) * i + 6 + 4;
}
#endif

const DoomoScene SCENE_TALK = { "TALK", talk_enter, talk_leave, talk_update, talk_draw, 1 };

/* ---- the runner's eyes (DOOMO_TALK_DEBUG: native host and tests only; TALK_DESIGN T8, docs/TALK.md 6k) ----
 * Read-only views of the screen so scripts can press the pad the way a player does (talk-quick, talk-say,
 * talk-type in core/native/runner_main.c) and the journeys can count the presses. Nothing here changes
 * the talk, and no console build defines the macro (no bytes in the GBA ROM, the PS1 EXE or the DC). */
#ifdef DOOMO_TALK_DEBUG
int talk_scene_debug(TalkSceneDebug *o)
{
    static const char *const pages[] = { "main", "acts", "topic", "invite", "more", "outing", "show", "give", "care", "feed", "heal", "history", "concern" };
    int i;
    memset(o, 0, sizeof *o);
    o->to_id = -1;
    if (!T.active) return 0;                       /* (the dialogue box is the top scene while a line is up) */
    o->active = 1;
    o->busy = T.busy || T.gen;                    /* (a sliced answer is still being made: the list is not up either) */
    o->page = pages[T.page < 13 ? T.page : 0];
    o->sel = T.list.sel;
    o->nrows = T.nrows;
    o->nparts = T.n;
    o->to = T.to;
    o->to_id = T.to >= 0 && T.to < T.n ? T.ids[T.to] : -1;
    o->tension = T.s.tension;
    o->gate = T.s.gate;
    o->reply_act = T.s.nlines > 0 ? T.s.line[T.s.nlines - 1].act : 0;
    o->nlines = T.s.nlines;
    o->care = (T.s.flags & TDF_CARE) != 0;
    o->reply_sincere = T.s.nlines > 0 && (T.s.line[T.s.nlines - 1].flags & TLF_SINCERE) != 0;
    for (i = 0; i < T.n && i < TALK_PARTS_MAX; i++) o->ids[i] = T.ids[i];
    doomo_snprintf(o->you, sizeof o->you, "%s", T.you);
    return 1;
}

int talk_scene_row_debug(int i, const char **kind, int *arg, int *v, char *text)
{
    static const char *const kinds[] = { "cat", "act", "topic", "type", "to", "invite", "bye", "pick", "notopic", "more", "quick", "out", "outing", "play", "show", "showitem", "give", "giveitem", "remember", "help", "comfort", "clean", "feed", "heal", "feeditem", "healitem", "history", "historyinfo", "historyquest", "historytopic", "historyevent", "historyrepeat", "topiclink", "historytone", "concern", "concerninfo", "concernact", "meetplan", "quirk", "historylike" };
    if (i < 0 || i >= T.nrows) return 0;
    if (kind) *kind = kinds[T.row[i].kind];
    if (arg) *arg = T.row[i].arg;
    if (v) *v = T.row[i].kind == K_QUICK && T.row[i].arg < T.nquick ? T.quick[T.row[i].arg].act : T.row[i].v;   /* a quick reply: its act */
    if (text) row_text(i, text);
    return 1;
}

int talk_scene_act_cat(int act)
{
    int c, i;
    for (c = 0; c < C_COUNT; c++)
        for (i = 0; i < cats[c].n; i++)
            if (cats[c].acts[i] == act) return c;
    return -1;
}
#endif /* DOOMO_TALK_DEBUG */

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

/* DC10B: 1 while the Talk screen is open; a platform with finer text (the Dreamcast's HD glyphs) draws with it. */
int talk_text_hd(void) { return T.active; }

/* DC10B: the bank is still being read off the disc. A short plain note (once; a second press adds none)
 * instead of silence; 1 = the note is up and the caller should do nothing else. Native: never loading. */
int home_talk_note(void)
{
    static const char msg[] = "Talk is warming up...";
    int i;
    if (talk_db_is_open() || talk_db_open(lang_find("en")) != TALK_DB_LOADING) return 0;
    for (i = 0; ui_toast_text(i); i++)
        if (!strcmp(ui_toast_text(i), msg)) return 1;
    ui_toast(msg);
    return 1;
}

int home_talk_enter(int who)
{
    if (who < 0 || who >= DOOMO_MAX_RESIDENTS || !town_alive(who)) return 0;
    if (who == playas_avatar() || !talk_ready()) return 0;
    scene_push(&SCENE_TALK, who);
    return 1;
}

#endif /* DOOMO_HAS_TALK */
