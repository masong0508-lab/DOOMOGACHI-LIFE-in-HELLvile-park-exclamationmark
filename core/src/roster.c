/* roster.c - lookups into the 1000 premade citizens (owner: roster).
 *
 * The data itself is const ROM data generated from core/data/roster/ by
 * tools/roster_build.py into core/gen/roster_data.c. Nothing here keeps
 * any RAM state, and every function is a bounded table lookup (the only
 * loop over the roster is roster_find / roster_count_traits, which are
 * for menus and setup, never per tick).
 */
#include "doomo/roster.h"

static int valid(int id) { return id >= 0 && id < ROSTER_COUNT; }

int roster_premade_record(int id, DoomoResident *out)
{
    const DoomoPremadeRec *p;
    const char *n;
    unsigned long long bits = 0;
    int i;
    memset(out, 0, sizeof(*out));
    if (!valid(id)) return 0;
    p = &doomo_roster_pack[id];
    out->species = p->species;
    for (i = 6; i >= 0; i--) bits = (bits << 8) | p->stat7[i];       /* 8 stats x 7 bits */
    for (i = 0; i < STAT_COUNT; i++) out->stat[i] = (uint8_t)((bits >> (7 * i)) & 127u);
    out->status = RES_ALIVE;
    out->flags = p->flags;
    out->mood = (uint8_t)(p->mood_act >> 5);
    out->activity = (uint8_t)(p->mood_act & 31u);
    n = doomo_roster_name_text + doomo_roster_name_ofs[id];
    for (i = 0; i < DOOMO_NAME_LEN - 1 && n[i]; i++) out->name[i] = n[i];
    memcpy(out->gene, p->gene, sizeof(out->gene));
    return 1;
}

const DoomoResident *roster_premade(int id)
{
    static DoomoResident ring[8];
    static unsigned next;
    DoomoResident *r;
    if (!valid(id)) return NULL;
    r = &ring[next++ & 7u];
    roster_premade_record(id, r);
    return r;
}

const char *roster_bio(int id)
{
    return valid(id) ? doomo_roster_bio_text + doomo_roster_bio_ofs[id] : "";
}

const char *roster_job(int id)
{
    return valid(id) ? doomo_roster_job_text + doomo_roster_job_ofs[id] : "";
}

const char *roster_name(int id)
{
    return valid(id) ? doomo_roster_name_text + doomo_roster_name_ofs[id] : "";
}

int roster_traits(int id)
{
    return valid(id) ? doomo_roster_traits[id] : 0;
}

int roster_stage(int id)
{
    int t = roster_traits(id);
    if (t & ROSTER_T_KID) return LIFE_CHILD;
    if (t & ROSTER_T_TEEN) return LIFE_TEEN;
    if (t & ROSTER_T_ELDER) return LIFE_ELDER;
    return LIFE_ADULT;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }

int roster_find(const char *name)
{
    int i, j;
    if (!name || !name[0]) return -1;
    for (i = 0; i < ROSTER_COUNT; i++) {
        const char *n = doomo_roster_name_text + doomo_roster_name_ofs[i];
        for (j = 0; j < DOOMO_NAME_LEN; j++) {
            int a = lower((unsigned char)n[j]), b = lower((unsigned char)name[j]);
            if (a != b) break;
            if (a == 0) return i;
        }
    }
    return -1;
}

int roster_story_npc(int npc)
{
    return (npc >= 0 && npc < NPC_COUNT) ? (int)doomo_story_npc[npc] : -1;
}

int roster_npc_of(int id)
{
    int k;
    if (!valid(id) || !(doomo_roster_pack[id].flags & RF_STORY_NPC)) return -1;
    for (k = 0; k < NPC_COUNT; k++)
        if (doomo_story_npc[k] == id) return k;
    return -1;
}

int roster_count_traits(int traits, int district)
{
    int i, n = 0, lo = 0, hi = ROSTER_COUNT;
    if (district == DISTRICT_HILLVILE) hi = DOOMO_DISTRICT_SIZE;
    else if (district == DISTRICT_HELLVILE) lo = DOOMO_DISTRICT_SIZE;
    for (i = lo; i < hi; i++)
        if ((doomo_roster_traits[i] & traits) == traits) n++;
    return n;
}
