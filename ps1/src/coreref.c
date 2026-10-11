/* coreref.c - access to optional core pieces.
 *
 * The core is assembled by several teams in parallel; during bring-up (and
 * in the engine test build) the platform must still link and run when a
 * module has not landed yet. Every core symbol the platform uses outside
 * the strict frame protocol is referenced WEAKLY here: a missing symbol
 * resolves to NULL and the platform falls back to its own engine-test
 * assets. Once the full core is linked (with foundation's weak stubs for
 * anything unfinished) all of these resolve normally.
 *
 * Weak references are called through `long_call` so a NULL target never
 * needs a 26-bit JAL relocation.
 */
#include "ps1.h"

#define WEAKFN __attribute__((weak, long_call))
#define WEAKDATA __attribute__((weak))

extern void doomo_init(void) WEAKFN;
extern void doomo_frame(uint32_t buttons) WEAKFN;
extern const uint8_t *texture_pixels(int tex_id, uint16_t time) WEAKFN;
extern const uint8_t *sky_pixels(int sky_id, uint16_t time) WEAKFN;
extern const DoomoImage *sprite_image(int spr_id, int frame) WEAKFN;
extern void textures_init(void) WEAKFN;
extern void sprites_init(void) WEAKFN;
extern void palette_init(void) WEAKFN;
extern const uint8_t doomo_font[FONT_LAST - FONT_FIRST + 1][5] WEAKDATA;
extern void map_load(int map_id) WEAKFN;
extern const DoomoMapDef *const doomo_maps[MAPID_COUNT] WEAKDATA;
extern DoomoMap doomo_map WEAKDATA;
extern DoomoResident *town_res(int id) WEAKFN;
extern uint8_t look_paint(uint8_t paint, int shade) WEAKFN;
extern uint8_t look_skin(const DoomoLook *l, int shade) WEAKFN;
extern void combat_start(int map_id) WEAKFN;
extern void combat_build_view(DoomoView3D *v) WEAKFN;
extern DoomoPlayerState doomo_player WEAKDATA;
extern const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate) WEAKFN;

/* Hide the comparison from the optimiser so the address test is real. */
#define PRESENT(sym) ({ const void *p_ = (const void *)&(sym); \
                        __asm__("" : "+r"(p_)); p_ != NULL; })

int core_has_game(void) { return PRESENT(doomo_init) && PRESENT(doomo_frame); }
void core_doomo_init(void) { if (PRESENT(doomo_init)) doomo_init(); }
void core_doomo_frame(uint32_t b) { if (PRESENT(doomo_frame)) doomo_frame(b); }

const uint8_t *core_texture_pixels(int tex_id, uint16_t time)
{
    const uint8_t *p = NULL;
    if (PRESENT(texture_pixels))
        p = texture_pixels(tex_id, time);
    return p ? p : etest_texture(tex_id, time);
}

const uint8_t *core_sky_pixels(int sky_id, uint16_t time)
{
    const uint8_t *p = NULL;
    if (PRESENT(sky_pixels))
        p = sky_pixels(sky_id, time);
    return p ? p : etest_sky(time);
}

const DoomoImage *core_sprite_image(int spr, int frame)
{
    const DoomoImage *p = NULL;
    if (PRESENT(sprite_image))
        p = sprite_image(spr, frame);
    if (p && p->pix && p->w && p->h)
        return p;
    return etest_sprite(spr, frame);
}

const uint8_t (*core_font(void))[5]
{
    if (PRESENT(doomo_font))
        return doomo_font;
    return etest_font;
}

int core_has_maps(void)
{
    return PRESENT(map_load) && PRESENT(doomo_maps) && PRESENT(doomo_map);
}

int core_map_load(int map_id)
{
    if (!core_has_maps() || map_id < 0 || map_id >= MAPID_COUNT || !doomo_maps[map_id])
        return 0;
    map_load(map_id);
    return doomo_map.w > 0 && doomo_map.h > 0;
}

DoomoMap *core_map(void) { return PRESENT(doomo_map) ? &doomo_map : NULL; }

const DoomoResident *core_resident(int id)
{
    return PRESENT(town_res) ? town_res(id) : NULL;
}

uint8_t core_look_paint(uint8_t paint, int shade)
{
    if (PRESENT(look_paint))
        return look_paint(paint, shade);
    return PAL_PAINT(paint & 31, shade);
}

uint8_t core_look_skin(const DoomoLook *l, int shade)
{
    if (PRESENT(look_skin))
        return look_skin(l, shade);
    if (l->skin < 8)
        return PAL_SKIN(l->skin, shade);
    return PAL_PAINT((l->skin - 8) & 31, shade);
}

int core_has_combat(void)
{
    return PRESENT(combat_start) && PRESENT(combat_build_view) && PRESENT(doomo_player);
}
void core_combat_start(int map_id) { if (PRESENT(combat_start)) combat_start(map_id); }
void core_combat_build_view(DoomoView3D *v) { if (PRESENT(combat_build_view)) combat_build_view(v); }
DoomoPlayerState *core_player(void) { return PRESENT(doomo_player) ? &doomo_player : NULL; }

void core_palette_init(void)
{
    if (PRESENT(palette_init))
        palette_init();
    if (PRESENT(textures_init))
        textures_init();
    if (PRESENT(sprites_init))
        sprites_init();
}

const int8_t *core_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    if (PRESENT(audio_sfx_pcm))
        return audio_sfx_pcm(sfx, len, rate);
    *len = 0;
    *rate = 0;
    return NULL;
}
