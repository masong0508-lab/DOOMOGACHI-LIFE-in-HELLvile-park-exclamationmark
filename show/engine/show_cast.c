/* show_cast.c - the shot's cast in the game's own resident table (WP6;
 * SHOW_ENGINE_SPEC.md sec.4.1, 8.4, 13.5).
 *
 * The portrait engine resolves busts, chibis and fighters through
 * town_res(rid) (portrait.c res_of()). town.c is linked anyway (beat.c and
 * hcmd.c reference doomo_town), so the show keeps no roster of its own: at
 * every shot's prep the LOOK chunk's 48-byte looks are written into
 * doomo_town.res[0..n-1] as living residents, and the slots the previous shot
 * used beyond n are emptied, so no look can leak from one shot into the next
 * (purity, sec.8.2). Figures take the same look by pointer.
 *
 * The options the core reads while drawing are pinned to one show profile
 * (camfx's shake scale and flash caps, tween's bounce amount, the gore
 * filter), so a frame never depends on a game save's settings.
 */
#include "doomo/doomo.h"
#include "doomo/town.h"
#include "doomo/resident.h"
#include "doomo/portrait.h"
#include "show/show_actor.h"

static int s_used;               /* slots the current shot wrote */

void show_cast_reset(int gore)
{
    int i;
    for (i = 0; i < s_used && i < SHOW_MAX_LOOKS; i++)
        memset(&doomo_town.res[i], 0, sizeof(doomo_town.res[i]));   /* RES_EMPTY */
    s_used = 0;
    /* the show profile (DECISIONS.md "WP6"): the game's defaults, then the
     * options that change pixels, pinned explicitly */
    opt_defaults();
    opt_set(OPT_SCREEN_SHAKE, 2);        /* 100 % (camfx shake_pct)            */
    opt_set(OPT_REDUCE_FLASH, 0);        /* the TIME-FLASH lint caps flashes;
                                            camfx keeps its own < 3 Hz gap     */
    opt_set(OPT_UI_BOUNCE, 2);           /* boingy: tween amplitudes at 100 %  */
    opt_set(OPT_CAMERA_FILTER, 0);       /* the shot's look= does the register */
    opt_set(OPT_GORE, gore ? GORE_BRUTAL : GORE_CUTE);
    opt_set(OPT_BLOOD_COLOR, 0);
    opt_set(OPT_CHEAT_BIG_HEAD, 0);
    opt_set(OPT_CHEAT_PAPER_MODE, 0);
    opt_set(OPT_CHEAT_CUTE_DEMONS, 0);
    opt_set(OPT_CHEAT_ALL_GHOSTS, 0);
}

int show_cast_load(const DoomoLook *looks, int n)
{
    int i;
    if (!looks || n < 0) n = 0;
    if (n > SHOW_MAX_LOOKS) n = SHOW_MAX_LOOKS;
    for (i = 0; i < n; i++) {
        DoomoResident *r = &doomo_town.res[i];
        memset(r, 0, sizeof(*r));
        memcpy(&r->look, &looks[i], sizeof(r->look));
        r->status = RES_ALIVE;
        r->flags = RF_STORY_NPC;
        r->name[0] = (char)('A' + i % 26);
    }
    for (; i < s_used; i++) memset(&doomo_town.res[i], 0, sizeof(doomo_town.res[i]));
    s_used = n;
    return n;
}

void show_cast_set_status(int slot, int status)
{
    if (slot < 0 || slot >= s_used) return;
    if (status < RES_ALIVE || status > RES_GHOST) status = RES_ALIVE;
    doomo_town.res[slot].status = (uint8_t)status;
}

int show_cast_status(int slot)
{
    if (slot < 0 || slot >= s_used) return RES_EMPTY;
    return doomo_town.res[slot].status;
}

int show_cast_count(void) { return s_used; }

/* ---- the whole package, for the player's shot loop --------------------------- */
void show_wp6_reset(int gore)
{
    portrait_cache_reset();              /* portrait.c: drop every cached image */
    show_cast_reset(gore);
    show_actor_reset();
    show_crowd_reset();
}

void show_wp6_tick(int t)
{
    show_actor_tick(t);
    show_crowd_tick(t);
}
