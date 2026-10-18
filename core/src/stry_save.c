/* stry_save.c - the story's second save chunk 'STRY' v1 (owner: S2;
 * docs/design/story/SPEC.md sec.4, R14, C9, C19; story_final.md sec.17.2).
 *
 * Exactly 96 B (DoomoStry, story.h): episode bits, the 118 side-mission
 * rows x 2 bits, branches, counters, the spawner cooldowns and the four
 * pending "Later" callbacks. The live copy is the only one (96 B of .bss;
 * resident on the PS1: the VM, the house API and the overworld read it).
 *
 * Old saves have no STRY. save.c resets the missing chunks after the town
 * body is decoded, so reset() can tell a new town (quest 0, SF_STARTED
 * clear) from a migration and derive the episode bits and the house phase
 * from doomo_town.story.quest: the 28 legacy BEAT_C* ids alias onto
 * episodes (story_beat_episode) and are never reordered (SPEC C19).
 */
#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/home.h"

static DoomoStry stry;

DoomoStry *story_stry(void) { return &stry; }

/* The 28 legacy beats -> the episode each one now belongs to (SF sec.10-13:
 * "Ep 1.1 ... existing C1_ARRIVE + C1_MAYOR" and so on). */
static const uint8_t legacy_ep[BEAT_COUNT] = {
    BEAT_EP1_1 - BEAT_P0, BEAT_EP1_1 - BEAT_P0,                         /* ARRIVE MAYOR   */
    BEAT_EP1_2 - BEAT_P0, BEAT_EP1_2 - BEAT_P0,                         /* KEY HOME       */
    BEAT_EP1_3 - BEAT_P0, BEAT_EP1_3 - BEAT_P0, BEAT_EP1_3 - BEAT_P0,   /* HATCH PIP BUNNY */
    BEAT_EP1_3 - BEAT_P0, BEAT_EP1_4 - BEAT_P0, BEAT_EP1_5 - BEAT_P0,   /* B.RETURN FRIENDS CUPCAKE */
    BEAT_EP1_6 - BEAT_P0,                                               /* WALL           */
    BEAT_EP2_1 - BEAT_P0, BEAT_EP2_2 - BEAT_P0, BEAT_EP2_4 - BEAT_P0,   /* VAPOR WEIRD SCREAM */
    BEAT_EP2_4 - BEAT_P0, BEAT_EP2_5 - BEAT_P0, BEAT_EP2_6 - BEAT_P0,   /* FUZZ EXORCISE RIVET */
    BEAT_EP2_7 - BEAT_P0,                                               /* CELLAR         */
    BEAT_EP3_1 - BEAT_P0, BEAT_EP3_3 - BEAT_P0, BEAT_EP3_6 - BEAT_P0,   /* ERUPTION NESTS SPIRE */
    BEAT_EP4_1 - BEAT_P0, BEAT_EP4_2 - BEAT_P0, BEAT_EP4_3 - BEAT_P0,   /* GATE PRISONERS HELP */
    BEAT_EP4_6 - BEAT_P0, BEAT_EP4_7 - BEAT_P0, BEAT_EP4_7 - BEAT_P0,   /* MAYOR FESTIVAL STAGE */
    BEAT_E1 - BEAT_P0                                                   /* DONE           */
};

/* The story order of the 38 new ids (specials where they play, SF sec.9-13). */
static const uint8_t story_order[BEAT_NEW_COUNT] = {
    BEAT_P0 - BEAT_P0,
    BEAT_EP1_1 - BEAT_P0, BEAT_EP1_2 - BEAT_P0, BEAT_EP1_3 - BEAT_P0, BEAT_EP1_4 - BEAT_P0,
    BEAT_EP1_5 - BEAT_P0, BEAT_EP1_6 - BEAT_P0, BEAT_INT1 - BEAT_P0,
    BEAT_EP2_1 - BEAT_P0, BEAT_EP2_2 - BEAT_P0, BEAT_EP2_3 - BEAT_P0, BEAT_EP2_4 - BEAT_P0,
    BEAT_EP2_5 - BEAT_P0, BEAT_F1 - BEAT_P0, BEAT_EP2_6 - BEAT_P0, BEAT_D1 - BEAT_P0, BEAT_EP2_7 - BEAT_P0,
    BEAT_EP3_1 - BEAT_P0, BEAT_EP3_2 - BEAT_P0, BEAT_EP3_3 - BEAT_P0, BEAT_EP3_4 - BEAT_P0,
    BEAT_F3 - BEAT_P0, BEAT_EP3_5 - BEAT_P0, BEAT_EP3_6 - BEAT_P0, BEAT_EP3_7 - BEAT_P0,
    BEAT_F2 - BEAT_P0, BEAT_INT3 - BEAT_P0,
    BEAT_EP4_1 - BEAT_P0, BEAT_EP4_2 - BEAT_P0, BEAT_EP4_3 - BEAT_P0, BEAT_EP4_4 - BEAT_P0,
    BEAT_D2 - BEAT_P0, BEAT_EP4_5 - BEAT_P0, BEAT_F4 - BEAT_P0, BEAT_EP4_6 - BEAT_P0, BEAT_EP4_7 - BEAT_P0,
    BEAT_E1 - BEAT_P0, BEAT_E2 - BEAT_P0
};

int story_beat_episode(int beat)
{
    if (beat >= 0 && beat < BEAT_COUNT) return BEAT_P0 + legacy_ep[beat];
    if (beat >= BEAT_P0 && beat < BEAT_ALL_COUNT) return beat;
    return -1;
}

int story_episode_order(int beat)
{
    int i, e = story_beat_episode(beat) - BEAT_P0;
    for (i = 0; i < BEAT_NEW_COUNT; i++) if (story_order[i] == e) return i;
    return -1;
}

int story_episode_done(int beat)
{
    int e = story_beat_episode(beat) - BEAT_P0;
    if (e < 0) return 0;
    return (stry.episode_done[e >> 3] >> (e & 7)) & 1;
}

void story_episode_set_done(int beat, int on)
{
    int e = story_beat_episode(beat) - BEAT_P0;
    if (e < 0) return;
    if (on) stry.episode_done[e >> 3] = (uint8_t)(stry.episode_done[e >> 3] | (1u << (e & 7)));
    else stry.episode_done[e >> 3] = (uint8_t)(stry.episode_done[e >> 3] & ~(1u << (e & 7)));
}

/* ---- the 118 side-mission rows, 2 bits each (STRY.side2) ------------------------------ */
int story_side2_state(int sid)
{
    if (sid < 0 || sid >= STORY_SIDE2_ROWS) return 0;
    return (stry.side2[sid >> 2] >> ((sid & 3) * 2)) & 3;
}

void story_side2_set_state(int sid, int st)
{
    int sh;
    if (sid < 0 || sid >= STORY_SIDE2_ROWS) return;
    sh = (sid & 3) * 2;
    stry.side2[sid >> 2] = (uint8_t)((stry.side2[sid >> 2] & ~(3u << sh)) | ((unsigned)(st & 3) << sh));
}

/* ---- the chunk -------------------------------------------------------------------------- */
static void migrate(void)
{
    int quest = doomo_town.story.quest, i, cur, ph = HP_COSY;
    if (!(doomo_town.story.flags[0] & 1) || quest >= BEAT_COUNT) return;     /* SF_STARTED clear: new */
    cur = story_episode_order(quest);
    for (i = 0; i < cur && i < BEAT_NEW_COUNT; i++) {
        int e = story_order[i];
        stry.episode_done[e >> 3] = (uint8_t)(stry.episode_done[e >> 3] | (1u << (e & 7)));
    }
    if (quest == BEAT_DONE) story_episode_set_done(BEAT_EP4_7, 1);
    if (cur >= story_episode_order(BEAT_EP4_4)) ph = HP_OPEN;
    else if (cur >= story_episode_order(BEAT_INT3)) ph = HP_EMPTY;
    else if (cur >= story_episode_order(BEAT_EP3_2)) ph = HP_BARRICADED;
    else if (cur >= story_episode_order(BEAT_EP2_3)) ph = HP_HAUNTED;
    stry.home_phase = (uint8_t)ph;
}

static void stry_reset(void)
{
    memset(&stry, 0, sizeof(stry));
    stry.ver = STRY_CHUNK_VERSION;
    migrate();
}

static uint32_t stry_size(void) { return (uint32_t)sizeof(stry); }

static uint32_t stry_write(uint8_t *dst, uint32_t cap)
{
    if (cap < sizeof(stry)) return 0;
    stry.ver = STRY_CHUNK_VERSION;
    memcpy(dst, &stry, sizeof(stry));
    return (uint32_t)sizeof(stry);
}

static int stry_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    if (version != STRY_CHUNK_VERSION || len != sizeof(stry)) return 0;
    memcpy(&stry, src, sizeof(stry));
    if (stry.home_phase >= HP_COUNT) stry.home_phase = HP_COSY;
    return 1;
}

static const DoomoSaveExt stry_ext = {
    STRY_CHUNK_TAG, STRY_CHUNK_VERSION, stry_size, stry_write, stry_read, stry_reset, 96
};
const DoomoSaveExt *const doomo_save_ext_story2 = &stry_ext;
