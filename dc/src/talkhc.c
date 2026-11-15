/* talkhc.c - the Talk screen's eyes for the harness (owner: dreamcast; TD2, docs/DREAMCAST.md "Talk").
 *
 * dcrun's talk-* commands (the native runner's T8 set: talk-enter, talk-quick, talk-say, talk-type,
 * expect-talk-row ...) press real pad buttons and step real ticks on the host side; what they need from
 * the game is a read-only view of the screen, which talk_scene_debug() gives (core/src/talk_scene.c,
 * built here with DOOMO_TALK_DEBUG). DC_HC_TALKQ posts one question per call: arg = sub-op in the low
 * byte, the rest packed per op, name = a text argument (the first 19 characters). Nothing here changes
 * the conversation, except TQ_ENTER (home_talk_enter, as the house's A press does) and TQ_PARTY.
 */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "dc_plat.h"
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/town.h"
#include "doomo/ui.h"
#include "doomo/lang.h"
#include "../../core/src/ui_internal.h"     /* UiDialogDebug */
#include "../harness/dc_mailbox.h"

#ifdef DOOMO_TALK_DEBUG

static const char *const kinds[] = { "cat", "act", "topic", "type", "to", "invite", "bye", "pick", "notopic", "more", "quick" };
#define NKINDS ((int)(sizeof kinds / sizeof kinds[0]))

static const struct { const char *name; int id; } acts[] = {
    { "greet", TA_GREET }, { "farewell", TA_FAREWELL }, { "thank", TA_THANK }, { "apologize", TA_APOLOGIZE },
    { "introduce", TA_INTRODUCE }, { "ask_about", TA_ASK_ABOUT }, { "ask_opinion", TA_ASK_OPINION },
    { "ask_feeling", TA_ASK_FEELING }, { "ask_fact", TA_ASK_FACT }, { "ask_who", TA_ASK_WHO },
    { "ask_advice", TA_ASK_ADVICE }, { "ask_favor", TA_ASK_FAVOR }, { "tell", TA_TELL },
    { "opine_pos", TA_OPINE_POS }, { "opine_neg", TA_OPINE_NEG }, { "agree", TA_AGREE },
    { "disagree", TA_DISAGREE }, { "yes", TA_YES }, { "no", TA_NO }, { "compliment", TA_COMPLIMENT },
    { "praise_other", TA_PRAISE_OTHER }, { "comfort", TA_COMFORT }, { "encourage", TA_ENCOURAGE },
    { "sympathize", TA_SYMPATHIZE }, { "joke", TA_JOKE }, { "laugh", TA_LAUGH }, { "offer", TA_OFFER },
    { "invite", TA_INVITE }, { "promise", TA_PROMISE }, { "insult", TA_INSULT }, { "criticize", TA_CRITICIZE },
    { "tease", TA_TEASE }, { "mock", TA_MOCK }, { "complain", TA_COMPLAIN }, { "blame", TA_BLAME },
    { "dismiss", TA_DISMISS }, { "threaten", TA_THREATEN }, { "gossip", TA_GOSSIP }, { "defend", TA_DEFEND },
    { "accuse", TA_ACCUSE }, { "take_side", TA_TAKE_SIDE }, { "mediate", TA_MEDIATE },
    { "change_topic", TA_CHANGE_TOPIC }, { "flirt", TA_FLIRT }, { "confess", TA_CONFESS },
    { "ask_out", TA_ASK_OUT }, { "wave", TA_WAVE }, { "nod", TA_NOD }, { "shake_head", TA_SHAKE_HEAD },
    { "hug", TA_HUG }, { "give", TA_GIVE }, { "walk_away", TA_WALK_AWAY }, { "high_five", TA_HIGH_FIVE }
};

/* the native runner's tk_who: a number, "everyone" (-1), a full name or a first name; -2 when nobody */
static int who_of(const char *nm)
{
    int i;
    size_t n = strlen(nm);
    if (!strcmp(nm, "everyone") || !strcmp(nm, "all")) return -1;
    if (isdigit((unsigned char)nm[0])) return atoi(nm);
    if ((i = town_find_by_name(nm)) >= 0) return i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status != RES_EMPTY && !strncmp(doomo_town.res[i].name, nm, n) &&
            (doomo_town.res[i].name[n] == ' ' || !doomo_town.res[i].name[n])) return i;
    return -2;
}

static int act_of(const char *nm)
{
    char low[24];
    size_t k;
    int i;
    for (k = 0; nm[k] && k < sizeof low - 1; k++)
        low[k] = (char)(nm[k] == '-' ? '_' : tolower((unsigned char)nm[k]));
    low[k] = 0;
    for (i = 0; i < (int)(sizeof acts / sizeof acts[0]); i++)
        if (!strcmp(low, acts[i].name)) return acts[i].id;
    return -1;
}

uint32_t dc_talk_hcmd(uint32_t arg, const char *name)
{
    TalkSceneDebug d;
    int on = talk_scene_debug(&d), i;
    switch (arg & 0xFFu) {
    case DC_TQ_ENTER: {
        int who = who_of(name);
        if (who < 0) return 1;
        return home_talk_enter(who) ? 3u : 2u;
    }
    case DC_TQ_STATE: {
        int pg = 0;
        static const char *const pages[] = { "main", "acts", "topic", "invite", "more" };
        if (!on) return 0;
        for (i = 0; i < 5; i++) if (!strcmp(d.page, pages[i])) pg = i;
        return 1u | (uint32_t)(d.busy != 0) << 1 | (uint32_t)pg << 2 | (uint32_t)(d.sel & 63) << 5 |
               (uint32_t)(d.nrows & 63) << 11 | (uint32_t)((d.to_id + 1) & 2047) << 17 | (uint32_t)(d.gate & 7) << 28;
    }
    case DC_TQ_FIND: {                  /* kind<<8 | (n+1)<<12 | (v+1)<<22; name: the row's exact text, "" any */
        int kind = (int)(arg >> 8 & 15), n = (int)(arg >> 12 & 1023) - 1, v = (int)(arg >> 22 & 1023) - 1;
        if (!on || kind >= NKINDS) return 0;
        for (i = 0; i < d.nrows; i++) {
            const char *k = "";
            int a = 0, vv = 0;
            char tx[64];
            talk_scene_row_debug(i, &k, &a, &vv, tx);
            if (strcmp(k, kinds[kind]) == 0 && (n < 0 || a == n) && (v < 0 || vv == v) && (!name[0] || !strcmp(tx, name)))
                return (uint32_t)i + 1u;
        }
        return 0;
    }
    case DC_TQ_ROWSAY:                  /* some row of the page shows the needle: 2; none: 1; no talk: 0 */
        if (!on) return 0;
        for (i = 0; i < d.nrows; i++) {
            char tx[64];
            talk_scene_row_debug(i, NULL, NULL, NULL, tx);
            if (strstr(tx, name)) return 2;
        }
        return 1;
    case DC_TQ_YOU:
        return !on ? 0u : strstr(d.you, name) ? 2u : 1u;
    case DC_TQ_REPLY:                   /* 1 | act<<1 | sincere<<8 | care<<9 */
        return !on ? 0u : 1u | (uint32_t)(d.reply_act & 127) << 1 | (uint32_t)(d.reply_sincere != 0) << 8 | (uint32_t)(d.care != 0) << 9 |
                          (uint32_t)(TA_IS_ROMANCE(d.reply_act) != 0) << 10;
    case DC_TQ_ACT: {                   /* name -> id + 1 (0: no such act); arg bit 8: -> category + 1 of act arg >> 16 */
        int a;
        if (arg & 0x100u) return (uint32_t)(talk_scene_act_cat((int)(arg >> 16)) + 1);
        a = act_of(name);
        return a < 0 ? 0u : (uint32_t)a + 1u;
    }
    case DC_TQ_DIALOG: {                /* glyphs shown of the dialogue's page + 1; 0 when no dialogue box is up */
        UiDialogDebug dd;
        memset(&dd, 0, sizeof dd);
        ui_dialog_debug(&dd);
        return dd.text ? (uint32_t)dd.shown + 1u : 0u;
    }
    case DC_TQ_WHO: {
        int who = who_of(name);
        return who == -2 ? 0u : (uint32_t)(who + 2);
    }
    case DC_TQ_PARTY: {                 /* arg bit 8: forget the list first; name: one more person */
        static int ids[3], n;
        int who;
        if (arg & 0x100u) n = 0;
        if (!name[0]) { talk_scene_party(ids, n); return 1; }
        who = who_of(name);
        if (who < 0 || n >= 3) return 0;
        ids[n++] = who;
        talk_scene_party(ids, n);
        return 1;
    }
    case DC_TQ_TOAST:                   /* TD3: expect-toast / expect-no-toast */
        for (i = 0; ui_toast_text(i); i++)
            if (strstr(ui_toast_text(i), name)) return 2;
        return 1;
    case DC_TQ_READY:                   /* TD3: talk-ready (a poll: the open answers LOADING until the last slice is in) */
        return talk_db_is_open() || talk_db_open(lang_find("en")) == TALK_DB_OK ? 2u : 1u;
    default:
        return 0;
    }
}

#else
uint32_t dc_talk_hcmd(uint32_t arg, const char *name) { (void)arg; (void)name; return 0; }
#endif
