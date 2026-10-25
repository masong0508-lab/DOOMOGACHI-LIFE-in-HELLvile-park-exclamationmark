/* show_registry.c - every player module, in call order (owner: ENGINE lane 1;
 * show_api.h).
 *
 * Each name is a WEAK reference: a module whose .c file is not linked yet
 * resolves to NULL and is skipped, so packages plug in by defining
 * `const ShowModule show_mod_<name>` in their own file and never edit this
 * one. The order is the order of reset/prep/ext/tick/hash and, inside a
 * draw layer, of drawing. A package that needs a name not reserved here adds
 * ONE line (keep the hunk tiny; the spare slots exist so nobody must).
 */
#include <stddef.h>
#include "show/show_api.h"

#define SHOW_MOD(n) extern const ShowModule show_mod_##n __attribute__((weak));
#define SHOW_MODULES(X) \
    X(core)       /* lane 1: show ops 0x000-0x03F, fades (show_ext.c)        */ \
    X(cast)       /* lane 2: doomo_town.res[] from SHWR_LOOK, options        */ \
    X(cam)        /* lane 2: the stage camera (installs the plane transform) */ \
    X(stage)      /* lane 1: BANDS, IMAGE, GRAIN, FILL planes                */ \
    X(tilemap)    /* lane 2: TILEMAP planes                                  */ \
    X(strokes)    /* lane 2: STROKES planes                                  */ \
    X(amb)        /* ENGINE: the sets' ambient layers (show_amb.c)           */ \
    X(overworld)  /* lane 3: OVERWORLD                                       */ \
    X(room)       /* lane 3: ROOM                                            */ \
    X(view3d)     /* lane 3: VIEW3D + rails                                  */ \
    X(actor)      /* lane 2: busts, figures, chibis, tracks                  */ \
    X(crowd)      /* lane 2: crowds                                          */ \
    X(prop)       /* lane 2: props, flipbooks                                */ \
    X(fx)         /* lane 2: particles, wipes, Split, dissolve               */ \
    X(text)       /* lane 2: SUPERs, signs, cards                            */ \
    X(hud)        /* lane 3: first-person HUD                                */ \
    X(slug)       /* lane 1: slug cards                                      */ \
    X(l2_spare1) X(l2_spare2) X(l3_spare1) X(l3_spare2) X(l1_spare1)

SHOW_MODULES(SHOW_MOD)

#define SHOW_MOD_PTR(n) &show_mod_##n,
static const ShowModule *const k_all[] = { SHOW_MODULES(SHOW_MOD_PTR) };
#define K_ALL_N ((int)(sizeof k_all / sizeof k_all[0]))

static const ShowModule *s_live[K_ALL_N];
static int s_n = -1;

static void scan(void)
{
    int i;
    s_n = 0;
    for (i = 0; i < K_ALL_N; i++) {
        const ShowModule *m = k_all[i];
        /* the asm barrier keeps gcc from folding "&weak != NULL" to true */
        __asm__("" : "+r"(m));
        if (m && m->api == SHOW_API_VERSION) s_live[s_n++] = m;
    }
}

int show_module_count(void)
{
    if (s_n < 0) scan();
    return s_n;
}

const ShowModule *show_module(int i)
{
    if (s_n < 0) scan();
    return (i >= 0 && i < s_n) ? s_live[i] : NULL;
}
