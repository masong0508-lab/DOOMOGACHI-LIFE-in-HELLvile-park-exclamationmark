/* talk_act.c - TM2e: the pure rules of the sim actions a Talk reply can carry.
 * Owner: TALK. docs/TALK.md 6p; design docs/design/talk/TALK_MULTILEVEL.md section 6.
 *
 * Nothing here writes to the sim. The drama manager (talk_dm.c) asks these before it lets a line say it is
 * doing something, and talk_outcome.c asks them again before it applies the event (defence in depth: the
 * minor rules are code in two places and a table in the tests). Every function is a table or a few ifs with
 * no hidden state, so the exhaustive stage x venue x item tests drive them directly.
 *
 *   invitation   a minor in the room (speaker, player or listener): only a public kid-safe venue (park, cafe,
 *                church, the beach in daytime). Never the vape shop, a concert hall at night or a date. A date
 *                (TA_ASK_OUT) is a romance act with its own consent path and is never an invitation action.
 *   gift         a minor on either side: food or a toy, never an item flagged adult.
 *   call over    a partner is never called into a room with a minor; the dead, ghosts and the pet are never named.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/items.h"
#include "doomo/catalog.h"
#include "doomo/resident_acc.h"

int talk_act_invite_ok(int act, int venue, int minor_room, int hour)
{
    if (act != TA_INVITE) return 0;                      /* a date, a favour, anything else: not an invitation action */
    switch (venue) {
    case ACT_PARK: case ACT_CAFE: case ACT_CHURCH: case ACT_BEACH:
        return !minor_room || (hour >= 8 && hour < 18);   /* with a child: daytime only */
    case ACT_SHOP: case ACT_WORKSHOP: case ACT_CONCERT: case ACT_VAPE_SHOP:
        return !minor_room;
    default:                                             /* home, bed, the hospital, the streets, the night */
        return 0;
    }
}

int talk_act_item_adult(int item)
{
    switch (item) {
    case ITEM_LOVE_LETTER: case ITEM_PERFUME: case ITEM_RING: case ITEM_VAPE_PEN:
    case ITEM_CHILL_PILL: case ITEM_ENERGY_DRINK: case ITEM_COFFEE:
        return 1;
    default:
        return 0;
    }
}

int talk_act_gift_ok(int item, int minor_giver, int minor_taker)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT || doomo_items[item].category == ICAT_KEY) return 0;
    if (minor_giver || minor_taker) {
        int toy = item == ITEM_PLUSHIE || item == ITEM_BOARD_GAME || item == ITEM_VIDEO_GAME;
        if (talk_act_item_adult(item)) return 0;
        return doomo_items[item].category == ICAT_FOOD || toy;
    }
    return 1;
}

int talk_act_call_ok(int room_minor, int partner, int alive)
{
    if (!alive) return 0;
    return !(room_minor && partner);
}

int talk_act_venue_of(uint16_t topic)
{
    static const struct { const char *w; uint8_t act; } v[] = {
        { "park", ACT_PARK }, { "Brimstone Bites", ACT_CAFE }, { "shop", ACT_SHOP }, { "St. Cuddles", ACT_CHURCH },
        { "workshop", ACT_WORKSHOP }, { "beach", ACT_BEACH }, { "concert hall", ACT_CONCERT },
        { "Cloud Nine Vapor", ACT_VAPE_SHOP }
    };
    char f[40];
    int i;
    if (!topic || talk_db_form(topic, TF_LEMMA, f, (int)sizeof f) < 0) return -1;
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++)
        if (!strcmp(f, v[i].w)) return v[i].act;
    return -1;
}

int talk_act_food_item(int id, int minor)
{
    const DoomoResident *r = id >= 0 && id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    int fi, item;
    if (!r || r->status == RES_EMPTY) return 0;
    fi = RES_LIKE_FOOD(r) % FOOD_COUNT;
    item = ITEM_CUPCAKE + fi;
    if (item <= ITEM_NONE || item >= ITEM_COUNT || doomo_items[item].category != ICAT_FOOD) return 0;   /* the food list outruns the items */
    if (fi == RES_HATE_FOOD(r) % FOOD_COUNT) return 0;
    if (minor && talk_act_item_adult(item)) return 0;
    return item;
}
