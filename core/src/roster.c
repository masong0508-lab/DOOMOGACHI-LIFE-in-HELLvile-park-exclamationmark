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

const DoomoResident *roster_premade(int id)
{
    return valid(id) ? &doomo_roster_premade[id] : NULL;
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
    return valid(id) ? doomo_roster_premade[id].name : "";
}

int roster_traits(int id)
{
    return valid(id) ? doomo_roster_traits[id] : 0;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }

int roster_find(const char *name)
{
    int i, j;
    if (!name || !name[0]) return -1;
    for (i = 0; i < ROSTER_COUNT; i++) {
        const char *n = doomo_roster_premade[i].name;
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
    if (!valid(id) || !(doomo_roster_premade[id].flags & RF_STORY_NPC)) return -1;
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
