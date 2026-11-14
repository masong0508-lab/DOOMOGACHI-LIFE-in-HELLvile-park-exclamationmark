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

#ifdef DOOMO_HAS_TALK

/* The typed line needs a keyboard and the free-text parser (talk_lex / talk_parse: native and
 * the Dreamcast). The GBA has neither: it plays Talk from the quick replies and the topic list. */
#ifndef DOOMO_PLATFORM_GBA
#define TALK_TYPING 1
#endif

enum { PG_MAIN = 0, PG_ACTS, PG_TOPIC, PG_INVITE, PG_MORE };
enum { K_CAT = 0, K_ACT, K_TOPIC, K_TYPE, K_TO, K_INVITE, K_BYE, K_PICK, K_NOTOPIC, K_MORE };
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

static void phrase(int act, char *out, int cap)
{
    const char *src = act > 0 && act < TA_COUNT && act_text[act] ? act_text[act] : "...";
    int n = 0;
    while (*src && n < cap - 1) {
        if (src[0] == '%' && src[1] == 't') {
            char tn[20];
            int k;
            topic_name(T.topic, tn, (int)sizeof tn);
            for (k = 0; tn[k] && n < cap - 1; k++) out[n++] = tn[k];
            src += 2;
        } else {
            out[n++] = *src++;
        }
    }
    out[n] = 0;
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

static void build_main(void)
{
    int c;
    T.page = PG_MAIN;
    T.nrows = 0;
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
