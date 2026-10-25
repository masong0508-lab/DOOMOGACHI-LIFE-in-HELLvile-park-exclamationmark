/* home_visit.c - the 1000 citizens' homes (owner: H4; HF sec.3, 5.3; SPEC H4).
 *
 * A visit is `scene_push(&SCENE_HOME, HOME_ARG_VISIT(id))`. The room is
 * generated, never saved (0 save bytes for 1000 homes): the same seed, the
 * same roster record and the same live state give the same room on every
 * platform. Inputs (HF sec.3.2):
 *
 *   district     Hillvile pastel set / Hellvile goth set; a Hellborn in
 *                Hillvile gets a mix (the story's point)
 *   like_color   the wall paint; the floor takes its partner from a table
 *   like_hobby   1-2 hobby pieces (a 32 x 2 table)
 *   like_food    a snack (48 foods -> 12 snack pieces)
 *   like_music   a band poster in the genre's colour
 *   fear         never a trigger piece; The Dark always gets a lamp and
 *                sometimes a night light (the protective piece)
 *   persona      the layout: tidy (along the walls), chaotic (scattered),
 *                shy (the bed in the far corner), show-off (the biggest
 *                piece in the middle)
 *   status       possessed / ghost / dead (the sealed room) / away
 *   relations    a framed candid per married / best-friend edge (2 max),
 *                the tomato board for a nemesis
 *
 * Then the gifts the player left (HOME.gifts: u16 resident 10 | x 3 | y 3,
 * u16 furn 9 | rot 2 | paint 5) override tiles. Wants are recomputed each
 * week from (day / 7) ^ id. The work is counted in HomeVisit.ops; the test
 * keeps it under 60k per room (HF: about 3.5 ms on the GBA).
 */
#include "home_int.h"
#include "doomo/audio.h"
#include "../gen/furn_ids.h"

#define VISIT_ROOM_MIN 8
#define VISIT_ROOM_MAX 14

/* ---- the tables ----------------------------------------------------------------- */
typedef struct VisitSet { uint16_t bed[4], seat[5], table[3], lamp[3], store[4], rug[5], plant[3], plush[5], clock[3], mirror; } VisitSet;
static const VisitSet visit_sets[2] = {
    {   /* Hillvile */
        { FURN_SINGLE_BED, FURN_CLOUD_BED, FURN_BUNK_BED, FURN_SINGLE_BED },
        { FURN_WOODEN_CHAIR, FURN_COMFY_ARMCHAIR, FURN_BUBBLE_CHAIR, FURN_BUNNY_BEANBAG, FURN_MARSHMALLOW_SOFA },
        { FURN_TEACUP_TABLE, FURN_PINE_TABLE, FURN_NIGHTSTAND },
        { FURN_CUPCAKE_LAMP, FURN_HONEY_LAMP, FURN_MUSHROOM_LAMP },
        { FURN_PASTEL_WARDROBE, FURN_RAINBOW_BOOKCASE, FURN_CREAM_DRESSER, FURN_CANDY_CANE_SHELF },
        { FURN_DAISY_RUG, FURN_CLOUD_RUG, FURN_CHECKER_RUG, FURN_CANDY_STRIPE_RUG, FURN_WELCOME_MAT },
        { FURN_POTTED_FERN, FURN_HEDGE_BUSH, FURN_TOMATO_POT },
        { FURN_CUPCAKE_PLUSH, FURN_GHOST_PLUSH, FURN_DINO_PLUSH, FURN_HUGGINS_SPARE, FURN_GRAPE_CLOUD_PLUSH },
        { FURN_CHERRY_CLOCK, FURN_CHERRY_CLOCK, FURN_GEAR_CLOCK },
        FURN_HEART_MIRROR,
    },
    {   /* Hellvile */
        { FURN_COFFIN_BED, FURN_IRON_BED, FURN_HELL_BED, FURN_TORTURE_RACK_BED },
        { FURN_GOTH_ARMCHAIR, FURN_IMP_STOOL, FURN_VELVET_CHAISE, FURN_HELL_COUCH, FURN_GOTH_ARMCHAIR },
        { FURN_TOMBSTONE_TABLE, FURN_LAVA_TABLE, FURN_TOMBSTONE_TABLE },
        { FURN_CANDELABRA, FURN_LAVA_LAMP, FURN_EMBER_BRAZIER },
        { FURN_GOTHIC_WARDROBE, FURN_BONE_BOOKCASE, FURN_HELLSTEEL_SHELVING, FURN_GOTHIC_WARDROBE },
        { FURN_BONE_RUG, FURN_LAVA_CRACK_RUG, FURN_HELL_WELCOME_MAT, FURN_BONE_RUG, FURN_CHECKER_RUG },
        { FURN_SKULL_PILE, FURN_POTTED_FERN, FURN_SKULL_PILE },
        { FURN_HORNED_POOP_PLUSH, FURN_SNOOZULA_PILLOW, FURN_GHOST_PLUSH, FURN_HORNED_POOP_PLUSH, FURN_DINO_PLUSH },
        { FURN_BLACK_CAT_CLOCK, FURN_BLACK_CAT_CLOCK, FURN_GEAR_CLOCK },
        FURN_MIRROR_OF_VANITY,
    },
};

/* like_hobby -> two pieces (HF sec.3.2) */
static const uint16_t hobby_pieces[HOBBY_COUNT][2] = {
    { FURN_COTTAGE_STOVE, FURN_CAKE_STAND },           /* Baking          */
    { FURN_COMFY_ARMCHAIR, FURN_WOOL_RUG },            /* Knitting        */
    { FURN_POTTED_FERN, FURN_TOMATO_POT },             /* Gardening       */
    { FURN_MOON_LAMP, FURN_CRYSTAL_BALL },             /* Stargazing      */
    { FURN_FORKLIFT_PALLET, FURN_BAND_POSTER },        /* Skateboarding   */
    { FURN_TUBE_TV, FURN_GAME_CONSOLE },               /* Video Games     */
    { FURN_WRITING_DESK, FURN_FRAMED_CANDID },         /* Painting        */
    { FURN_RECORD_PLAYER, FURN_BAND_POSTER },          /* Singing         */
    { FURN_FAIRY_LIGHTS, FURN_RECORD_PLAYER },         /* Dancing         */
    { FURN_RAINBOW_BOOKCASE, FURN_WRITING_DESK },      /* Reading         */
    { FURN_FISH_SMOKER, FURN_CLOUD_SHELF },            /* Fishing         */
    { FURN_PLUSH_OTTOMAN, FURN_TUCK_IN_BLANKET },      /* Napping         */
    { FURN_PASTEL_WARDROBE, FURN_HEART_POSTER },       /* Cosplay         */
    { FURN_TUBE_TV, FURN_RECORD_PLAYER },              /* Karaoke         */
    { FURN_TOY_CHEST, FURN_JUMBO_DICE },               /* Board Games     */
    { FURN_HEDGE_BUSH, FURN_SPRINKLE_VASE },           /* Bug Collecting  */
    { FURN_RAVEN_PERCH, FURN_BIRD_BATH },              /* Birdwatching    */
    { FURN_WOOL_RUG, FURN_DAISY_RUG },                 /* Yoga            */
    { FURN_SPRINKLE_VASE, FURN_SKULL_PLANTER },        /* Pottery         */
    { FURN_WRITING_DESK, FURN_PAPER_LANTERN },         /* Origami         */
    { FURN_TOY_CHEST, FURN_CUPCAKE_PLUSH },            /* Plush Making    */
    { FURN_CLOUD_RUG, FURN_CLOUD_SHELF },              /* Cloud Watching  */
    { FURN_BUBBLE_CHAIR, FURN_SPRINKLE_VASE },         /* Bubble Blowing  */
    { FURN_WRITING_DESK, FURN_CANDELABRA },            /* Poetry          */
    { FURN_CRYSTAL_BALL, FURN_CANDELABRA },            /* Tarot Reading   */
    { FURN_CANDELABRA, FURN_CRYSTAL_BALL },            /* Seances         */
    { FURN_VAPOR_DETECTOR_MK2, FURN_GHOST_PLUSH },     /* Ghost Hunting   */
    { FURN_PIPE_ORGAN_MINI, FURN_BAND_POSTER },        /* Metal Drumming  */
    { FURN_JUMBO_DICE, FURN_HORNED_POOP_PLUSH },       /* Pranks          */
    { FURN_NEON_ITS_FINE, FURN_BAND_POSTER },          /* Graffiti        */
    { FURN_WELDING_RIG, FURN_TOOL_BENCH },             /* Welding         */
    { FURN_SKULL_PILE, FURN_SKULL_PLANTER },           /* Skull Collecting */
};

/* like_food -> one of 12 snack pieces (smalls go on the table) */
static const uint16_t snack_pieces[12] = {
    FURN_CAKE_STAND, FURN_MELON_BREAD_TRAY, FURN_SOCK_STEW_POT, FURN_JELLYBEAN_JAR,
    FURN_KETTLE, FURN_LAVA_CAKE_MOULD, FURN_BAGEL_MOUNTAIN, FURN_PICKLE_BARREL,
    FURN_GRAVE_CAKE_DISPLAY, FURN_TAKOYAKI_GRIDDLE, FURN_CIDER_BARREL, FURN_STRAWBERRY_FRIDGE,
};
static const uint8_t food_snack[FOOD_COUNT] = {
    0, 1, 2, 6, 3, 0, 5, 0, 11, 11, 11, 4,          /* Cupcakes .. Coffee          */
    1, 1, 2, 3, 2, 1, 1, 1, 2, 0, 0, 11, 4,          /* Pancakes .. Bubble Tea      */
    1, 1, 9, 2, 3, 0, 3, 1, 2, 3,                    /* Honey Toast .. Cereal       */
    3, 5, 3, 3, 3, 1, 10, 3, 8, 3, 7, 2, 2,          /* Gumdrops .. Bone Broth      */
};

/* like_music -> the band poster's colour */
static const uint8_t genre_paint[MUSIC_COUNT] = {
    PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_CYAN, PAINT_LIME, PAINT_SKY, PAINT_HONEY,
    PAINT_BONE, PAINT_FOREST, PAINT_MAGENTA, PAINT_NAVY, PAINT_WHITE, PAINT_VAPE_PURPLE,
    PAINT_GRAPE, PAINT_CHERRY, PAINT_BLOOD, PAINT_CHARCOAL,
};

/* like_color -> the floor's partner paint */
static const uint8_t floor_partner[32] = {
    PAINT_CARAMEL, PAINT_CHARCOAL, PAINT_SILVER, PAINT_CHOCOLATE, PAINT_CARAMEL, PAINT_CHARCOAL,
    PAINT_PEACH, PAINT_CARAMEL, PAINT_CHOCOLATE, PAINT_CHOCOLATE, PAINT_CHARCOAL, PAINT_CHOCOLATE,
    PAINT_CARAMEL, PAINT_FOREST, PAINT_FOREST, PAINT_TEAL, PAINT_CARAMEL, PAINT_CHOCOLATE,
    PAINT_NAVY, PAINT_WHITE, PAINT_NAVY, PAINT_SILVER, PAINT_HONEY, PAINT_CHARCOAL,
    PAINT_JET, PAINT_GRAPE, PAINT_CARAMEL, PAINT_CHOCOLATE, PAINT_CHARCOAL, PAINT_CHARCOAL,
    PAINT_JET, PAINT_GRAPE,
};

/* fear -> the pieces that set it off (-1 ends a row; HF sec.3.2) */
static const int16_t fear_pieces[FEAR_COUNT][4] = {
    { FURN_COBWEB_CURTAINS, -1 },                                                  /* Spiders  */
    { -1 },                                                                        /* The Dark: the room */
    { FURN_WEATHER_VANE, -1 },                                                     /* Thunder  */
    { FURN_HONKERS_NOSE, FURN_MASCOT_HEAD, -1 },                                   /* Clowns   */
    { FURN_GHOST_PLUSH, FURN_TRANSLUCENT_BALLOON, FURN_FOLLOWING_PORTRAIT, -1 },   /* Ghosts   */
    { FURN_BUNK_BED, FURN_CANDY_CANE_SHELF, -1 },                                  /* Heights  */
    { FURN_PAPER_BOAT_POND, FURN_PADDLING_POOL, FURN_WEEPING_FISH_TANK, -1 },      /* Deep Water */
    { FURN_VAPOR_DETECTOR_MK2, -1 },                                               /* Vacuum Cleaners */
    { FURN_BEE_BOX, FURN_PIPS_BEE_DRAWING, -1 },                                   /* Bees     */
    { FURN_TOOL_BENCH, FURN_WELDING_RIG, -1 },                                     /* Needles  */
    { FURN_HEART_MIRROR, FURN_MIRROR_OF_VANITY, -1 },                              /* Mirrors  */
    { -1 },                                                                        /* Being Alone */
    { FURN_MIC_STAND, -1 },                                                        /* Public Speaking */
    { FURN_BATH_TUB_PINK, FURN_STEAMYS_TUB, FURN_RUBBER_DUCK_GUEST, -1 },          /* Bath Time */
    { FURN_JELLYBEAN_JAR, -1 },                                                    /* Dentists */
    { FURN_HEART_BALLOON, FURN_BALLOON_YOU, FURN_TRANSLUCENT_BALLOON, FURN_BALLOON_LETTERS }, /* Balloons */
    { -1 },                                                                        /* Silence  */
    { FURN_BIRD_BATH, FURN_CHURCH_CANDLE, -1 },                                    /* Holy Water */
    { FURN_DAYTIME_WINDOW, FURN_SUNFLOWER, -1 },                                   /* Sunlight */
    { FURN_CHOIR_BELL, FURN_SMALL_BELL, -1 },                                      /* Church Bells */
    { FURN_CHURCH_CANDLE, FURN_HUGGINS_MARKER, -1 },                               /* Exorcisms */
    { FURN_GIANT_TEDDY, -1 },                                                      /* Hugs     */
    { FURN_CUPCAKE_PLUSH, FURN_BUNNY_BEANBAG, FURN_HEART_POSTER, -1 },             /* Cute Things */
    { FURN_FRAMED_INVOICE, FURN_CONTRACT_DESK, -1 },                               /* Tax Season */
};
/* the protective piece a fearful citizen sometimes owns */
static const int16_t fear_protect[FEAR_COUNT] = {
    -1, FURN_MOON_LAMP, -1, -1, FURN_CHURCH_CANDLE, -1, -1, -1, -1, -1, -1, FURN_GIANT_TEDDY,
    -1, -1, -1, -1, FURN_RECORD_PLAYER, -1, FURN_COBWEB_CURTAINS, -1, -1, -1, -1, -1,
};

/* ---- the generator ---------------------------------------------------------------- */
typedef struct Gen {
    HomeRoom *r;
    int room, fear, layout, hell, ghost;
    DoomoRng rng;
    uint32_t ops;
} Gen;

int home_visit_fear_hit(int fear, int furn)
{
    int i;
    if (fear < 0 || fear >= FEAR_COUNT) return 0;
    for (i = 0; i < 4 && fear_pieces[fear][i] >= 0; i++)
        if (fear_pieces[fear][i] == furn) return 1;
    return 0;
}

static int has_lit_lamp(const HomeRoom *room)
{
    int i;
    for (i = 0; i < room->nobj; i++) {
        const DoomoFurn *f = home_furn(HO_FURN(&room->obj[i]));
        if (f && (f->flags & FF_LIGHT) && !(room->obj[i].pa & HOS_OFF)) return 1;
    }
    return 0;
}

int home_visit_fear_room(const DoomoResident *r, const HomeRoom *room)
{
    return r && r->fear == 1 && !has_lit_lamp(room);
}

static int put(Gen *g, int furn, int x, int y, int rot, int sub, int pa, int pb)
{
    HomeFit fit;
    HomeObj *o;
    const DoomoFurn *f = home_furn(furn);
    g->ops += (uint32_t)g->r->nobj + 4;
    if (!f || g->r->nobj >= VISIT_ROOM_MAX + 4 || g->r->nobj >= HOME_OBJ_MAX) return -1;
    if (home_visit_fear_hit(g->fear, furn)) return -1;     /* never a trigger piece */
    if (home_fit_sub(g->r, g->room, furn, x, y, rot, sub, -1, &fit) != HPR_OK) return -1;
    o = &g->r->obj[g->r->nobj];
    o->w0 = HO_W0S(furn, rot, fit.on_surface, fit.on_surface ? sub : 0);
    o->pos = HO_POS(x, y);
    o->z = fit.z;
    o->pa = (uint8_t)(pa < 0 ? FURN_PAINT_DEFAULT : (f->paint_a == 0xFF ? FURN_PAINT_DEFAULT : pa));
    o->pb = (uint8_t)(pb < 0 ? FURN_PAINT_DEFAULT : (f->paint_b == 0xFF ? FURN_PAINT_DEFAULT : pb));
    if (g->ghost) {                                        /* dust sheets */
        if (f->paint_a != 0xFF) o->pa = PAINT_SILVER;
        if (f->paint_b != 0xFF) o->pb = PAINT_WHITE;
    }
    return g->r->nobj++;
}

/* A floor piece by the layout template: tidy goes along the walls facing
 * in, chaotic scatters, shy and show-off are handled by their callers. */
static int place_floor(Gen *g, int furn, int pa, int pb)
{
    int w = HR_W(g->r), d = HR_D(g->r), tries, i;
    if (g->layout != 1) {
        /* the perimeter, starting at a seeded point: wall A (faces S), wall
         * B (faces E), then the two open sides */
        int n = 2 * (w + d) - 4, start = rng_range(&g->rng, n);
        for (i = 0; i < n; i++) {
            int k = (start + i) % n, x, y, rot;
            if (k < w) { x = k; y = 0; rot = 0; }
            else if (k < w + d - 1) { x = 0; y = k - w + 1; rot = 1; }
            else if (k < 2 * w + d - 2) { x = k - (w + d - 1) + 1; y = d - 1; rot = 2; }
            else { x = w - 1; y = k - (2 * w + d - 2) + 1; rot = 3; }
            if (put(g, furn, x, y, rot, 0, pa, pb) >= 0) return g->r->nobj - 1;
            if (put(g, furn, x, y, (rot + 1) & 3, 0, pa, pb) >= 0) return g->r->nobj - 1;
        }
    }
    for (tries = 0; tries < 16; tries++) {
        int x = rng_range(&g->rng, w), y = rng_range(&g->rng, d), rot = rng_range(&g->rng, 4);
        if (put(g, furn, x, y, rot, 0, pa, pb) >= 0) return g->r->nobj - 1;
    }
    return -1;
}

static int place_wall(Gen *g, int furn, int pa, int pb)
{
    int w = HR_W(g->r), d = HR_D(g->r), i, n = w + d, start = rng_range(&g->rng, n);
    for (i = 0; i < n; i++) {
        int k = (start + i) % n;
        if (k < w ? put(g, furn, k, 0, 0, 0, pa, pb) >= 0 : put(g, furn, 0, k - w, 1, 0, pa, pb) >= 0)
            return g->r->nobj - 1;
    }
    return -1;
}

/* a small on any surface (the table first) */
static int place_small(Gen *g, int furn, int on, int pa, int pb)
{
    int i, sub;
    for (i = on >= 0 ? on : 0; i < g->r->nobj; i++) {
        const HomeObj *o = &g->r->obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        if (!f || !(f->flags & FF_SURFACE) || (f->flags & FF_WALL) || f->role == FR_HEARTH) continue;
        for (sub = 0; sub < 4; sub++)
            if (put(g, furn, HO_X(o), HO_Y(o), HO_ROT(o), sub, pa, pb) >= 0) return g->r->nobj - 1;
        if (on >= 0) break;
    }
    return on >= 0 ? place_small(g, furn, -1, pa, pb) : -1;
}

static int place_any(Gen *g, int furn, int pa, int pb)
{
    const DoomoFurn *f = home_furn(furn);
    if (!f) return -1;
    if (f->flags & FF_WALL) return place_wall(g, furn, pa, pb);
    if (f->flags & FF_SMALL) return place_small(g, furn, -1, pa, pb);
    return place_floor(g, furn, pa, pb);
}

/* turn every piece to face the wall (the possessed room) */
static void face_walls(HomeRoom *r, int room)
{
    int i, w = HR_W(r), d = HR_D(r);
    for (i = 0; i < r->nobj; i++) {
        HomeObj *o = &r->obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        int x = HO_X(o), y = HO_Y(o), rot, dw[4], k, best = 0;
        if (!f || (f->flags & (FF_WALL | FF_RUG)) || (f->flags2 & FF_PORTAL) || HO_ONSURF(o)) continue;
        dw[0] = d - 1 - y; dw[1] = w - 1 - x; dw[2] = y; dw[3] = x;   /* S E N W */
        for (k = 1; k < 4; k++) if (dw[k] < dw[best]) best = k;
        rot = best;
        if (home_fit(r, room, HO_FURN(o), x, y, rot, i, NULL) == HPR_OK)
            o->w0 = (uint16_t)((o->w0 & ~(3u << 9)) | ((unsigned)rot << 9));
    }
}

static void gifts_into(Gen *g, int id, HomeVisit *info)
{
    HomeState *s = home_state();
    int k;
    if (!s) return;
    for (k = 0; k < HOME_GIFTS; k++) {
        const uint8_t *e = s->gifts[k];
        int w0 = e[0] | (e[1] << 8), w1 = e[2] | (e[3] << 8), furn, x, y, rot, pa, i;
        if (!w1 || (w0 & 0x3FF) != id) continue;
        furn = w1 & 0x1FF; rot = (w1 >> 9) & 3; pa = (w1 >> 11) & 31;
        x = (w0 >> 10) & 7; y = (w0 >> 13) & 7;
        g->ops += 8;
        /* a gift overrides the tile: whatever generated piece is there moves out */
        for (i = 0; i < 3; i++) {
            int j = home_obj_at(g->r, x, y);
            HomeFit fit;
            if (home_fit(g->r, g->room, furn, x, y, rot, -1, &fit) == HPR_OK) break;
            if (j < 0 || home_obj_is_fixed(g->r, j) || home_stackers_on(g->r, j) > 0) break;
            memmove(&g->r->obj[j], &g->r->obj[j + 1], (size_t)(g->r->nobj - j - 1) * sizeof(HomeObj));
            g->r->nobj--;
        }
        {
            int fear = g->fear;
            g->fear = -1;                                  /* the player's gift may scare them */
            if (put(g, furn, x, y, rot, 0, pa == FURN_PAINT_DEFAULT ? -1 : pa, -1) >= 0) info->gifts++;
            g->fear = fear;
        }
    }
}

static int gen_room(int id, int force, HomeRoom *out, HomeVisit *info)
{
    DoomoResident *r = town_res(id);
    int status, flags;
    HomeVisit tmp;
    Gen g;
    const VisitSet *set;
    int hill_hell, n, i, bed, table, lamp, extra;
    uint16_t rel[8];
    if (!info) info = &tmp;
    memset(info, 0, sizeof(*info));
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS || !r || !out) return 0;
    status = r->status;
    flags = r->flags;
    if (force && status != RES_EMPTY) {                /* HOME_ARG_VISIT_AS: a variant on show */
        status = force == HVK_POSSESSED ? RES_POSSESSED : force == HVK_GHOST ? RES_GHOST :
                 force == HVK_SEALED ? RES_DEAD : RES_ALIVE;
        if (force == HVK_AWAY) flags |= RF_TRAVELER; else flags &= ~RF_TRAVELER;
    }
    info->id = (uint16_t)id;
    info->hobby_obj = 0xFF;
    memset(&g, 0, sizeof(g));
    g.r = out;
    g.hell = RES_DISTRICT(id);
    g.room = g.hell ? HOME_ROOM_PITS_FLAT : HOME_ROOM_TOWERS_7B;
    info->room = (uint8_t)g.room;
    home_room_template(g.room, out);
    rng_seed(&g.rng, doomo_town.seed ^ ((uint32_t)id * 0x9E3779B1u) ^ HOME_CTX_VISIT);
    info->music = (uint8_t)(g.hell ? SONG_HELLVILE : SONG_MINGLE);
    if (status == RES_EMPTY) {
        /* a vacant flat: the realtor's sign and nothing else */
        info->kind = HVK_VACANT;
        g.fear = -1;
        put(&g, FURN_REALTORS_SIGN, 2, 3, 0, 0, -1, -1);
        info->music = SONG_NONE;
        info->ops = g.ops;
        return 1;
    }
    g.fear = r->fear < FEAR_COUNT ? r->fear : -1;
    g.ghost = status == RES_GHOST;
    /* the layout from the personality (HF sec.3.2) */
    if (r->persona[PERS_QUIRK] >= 10) g.layout = 1;                       /* chaotic */
    else if (r->persona[PERS_EXPRESSION] < 6 && r->persona[PERS_SPEECH] < 8) g.layout = 2;   /* shy */
    else if (r->persona[PERS_EXPRESSION] >= 10 && r->persona[PERS_SPEECH] >= 8) g.layout = 3; /* show-off */
    else g.layout = 0;                                                   /* tidy */
    info->layout = (uint8_t)g.layout;
    /* the set: Hellborn in Hillvile get a mix */
    hill_hell = g.hell;
    if ((flags & RF_HELLBORN) && !g.hell) hill_hell = rng_range(&g.rng, 2);
    set = &visit_sets[hill_hell];
    out->wall_paint = (uint8_t)(r->like_color < 32 ? r->like_color : PAINT_BUBBLEGUM);
    out->floor_paint = floor_partner[out->wall_paint & 31];
    {
        static const uint8_t wp[2][4] = { { 0, 1, 4, 7 }, { 3, 5, 6, 3 } }, fl[2][4] = { { 1, 0, 2, 7 }, { 3, 5, 6, 3 } };
        out->wallpaper = wp[hill_hell][rng_range(&g.rng, 4)];
        out->floor = fl[hill_hell][rng_range(&g.rng, 4)];
    }
    if (status == RES_DEAD) {
        /* the sealed room (HF sec.3.2, 5.5): candles, the flowers, their
         * favourite hobby piece on the bed, no music, no gag */
        info->kind = HVK_SEALED;
        info->music = SONG_NONE;
        info->daylight = 6;
        g.fear = -1;
        bed = put(&g, set->bed[0], 4, 2, 0, 0, -1, -1);
        if (bed < 0) bed = place_floor(&g, set->bed[0], -1, -1);
        table = place_floor(&g, set->table[0], -1, -1);
        place_floor(&g, FURN_CANDELABRA, -1, -1);
        if (table >= 0) {
            place_small(&g, FURN_CHURCH_CANDLE, table, -1, -1);
            place_small(&g, FURN_SPRINKLE_VASE, table, PAINT_WHITE, -1);
            place_small(&g, FURN_BLOSSOM_BRANCH, table, -1, -1);
        }
        n = r->like_hobby < HOBBY_COUNT ? hobby_pieces[r->like_hobby][1] : FURN_CUPCAKE_PLUSH;
        if (bed >= 0 && home_furn(n) && (home_furn(n)->flags & FF_SMALL)) place_small(&g, n, bed, -1, -1);
        else place_small(&g, set->plush[0], bed, -1, -1);
        place_wall(&g, FURN_FRAMED_CANDID, -1, -1);
        gifts_into(&g, id, info);
        info->ops = g.ops;
        return 1;
    }
    if (status == RES_POSSESSED || status == RES_SOUL_LOST) info->kind = HVK_POSSESSED;
    else if (status == RES_GHOST) info->kind = HVK_GHOST;
    else if (flags & RF_TRAVELER) info->kind = HVK_AWAY;
    if (info->kind == HVK_POSSESSED) {
        out->wallpaper = 4;                                /* bleeding hearts */
        out->wall_paint = PAINT_BLOOD;
        info->tint = TINT_RED;
        info->music = SONG_OMINOUS;
    } else if (info->kind == HVK_GHOST) {
        info->tint = TINT_NIGHT;
        info->daylight = 8;
        info->music = SONG_SAD;
    }
    /* the bed first: shy citizens push it into the far corner, show-offs
     * put the biggest piece in the middle */
    i = rng_range(&g.rng, 4);
    if (g.layout == 2) {
        bed = put(&g, set->bed[i], 0, HR_D(out) - 2, 1, 0, -1, -1);
        if (bed < 0) bed = put(&g, set->bed[i], HR_W(out) - 1, HR_D(out) - 2, 0, 0, -1, -1);
    } else if (g.layout == 3) {
        bed = put(&g, set->bed[i], HR_W(out) / 2 - 1, HR_D(out) / 2 - 1, 0, 0, -1, -1);
    } else {
        bed = -1;
    }
    if (bed < 0) bed = place_floor(&g, set->bed[i], -1, -1);
    if (bed < 0) bed = place_floor(&g, set->bed[3], -1, -1);
    /* a rug, a table, a seat, a lamp (The Dark: always; others mostly) */
    place_floor(&g, set->rug[rng_range(&g.rng, 5)], -1, -1);
    table = place_floor(&g, set->table[rng_range(&g.rng, 3)], -1, -1);
    place_floor(&g, set->seat[rng_range(&g.rng, 5)], -1, -1);
    lamp = -1;
    if (g.fear == 1 || rng_chance(&g.rng, 80) || info->kind == HVK_POSSESSED)
        lamp = place_floor(&g, set->lamp[rng_range(&g.rng, 3)], -1, -1);
    if (g.fear == 1 && lamp < 0) lamp = place_small(&g, FURN_MOON_LAMP, table, -1, -1);
    /* the likes */
    if (r->like_hobby < HOBBY_COUNT) {
        int h0 = hobby_pieces[r->like_hobby][0], h1 = hobby_pieces[r->like_hobby][1];
        if (hill_hell && h0 == FURN_RAINBOW_BOOKCASE) h0 = FURN_BONE_BOOKCASE;
        n = place_any(&g, h0, -1, -1);
        if (n >= 0) info->hobby_obj = (uint8_t)n;
        if (rng_chance(&g.rng, 60)) {
            n = place_any(&g, h1, h1 == FURN_BAND_POSTER ? genre_paint[r->like_music % MUSIC_COUNT] : -1, -1);
            if (info->hobby_obj == 0xFF && n >= 0) info->hobby_obj = (uint8_t)n;
        }
    }
    if (r->like_food < FOOD_COUNT) {
        int s = snack_pieces[food_snack[r->like_food]];
        if (home_furn(s) && (home_furn(s)->flags & FF_SMALL)) place_small(&g, s, table, -1, -1);
        else place_floor(&g, s, -1, -1);
    }
    place_wall(&g, FURN_BAND_POSTER, genre_paint[r->like_music % MUSIC_COUNT], -1);
    /* storage, a plant, a clock, a plush on the bed */
    place_floor(&g, set->store[rng_range(&g.rng, 4)], -1, -1);
    if (rng_chance(&g.rng, 70)) place_floor(&g, set->plant[rng_range(&g.rng, 3)], -1, -1);
    place_wall(&g, set->clock[rng_range(&g.rng, 3)], -1, -1);
    if (bed >= 0 && rng_chance(&g.rng, 75)) place_small(&g, set->plush[rng_range(&g.rng, 5)], bed, -1, -1);
    if (rng_chance(&g.rng, 40)) place_wall(&g, set->mirror, -1, -1);
    /* the protective piece, sometimes */
    if (g.fear >= 0 && fear_protect[g.fear] >= 0 && rng_chance(&g.rng, 50) &&
        place_any(&g, fear_protect[g.fear], -1, -1) >= 0)
        info->protect = 1;
    /* relationships: a framed candid per married / best-friend edge (2 max),
     * the tomato board for a nemesis */
    n = rel_list(id, rel, 8);
    g.ops += (uint32_t)n * 4 + 64;
    for (i = 0, extra = 0; i < n && extra < 2; i++) {
        int k = rel_kind(id, rel[i]);
        if (k == REL_MARRIED || k == REL_BEST_FRIEND) {
            if (place_wall(&g, FURN_FRAMED_CANDID, -1, -1) >= 0) extra++;
        }
    }
    for (i = 0; i < n; i++)
        if (rel_kind(id, rel[i]) == REL_NEMESIS) { place_wall(&g, FURN_TOMATO_REVIEW_BOARD, -1, -1); break; }
    /* top up to VISIT_ROOM_MIN with the district's basics */
    for (i = 0; out->nobj < VISIT_ROOM_MIN + 3 && i < 6; i++)
        place_floor(&g, i & 1 ? set->seat[i % 5] : set->plant[i % 3], -1, -1);
    if (info->kind == HVK_POSSESSED) face_walls(out, g.room);
    if (info->kind == HVK_AWAY && lamp >= 0) out->obj[lamp].pa |= HOS_OFF;   /* nobody home */
    gifts_into(&g, id, info);
    home_visit_wants(id, info->wants);
    info->ops = g.ops;
    return 1;
}

int home_visit_gen(int id, HomeRoom *out, HomeVisit *info) { return gen_room(id, 0, out, info); }
int home_visit_gen_as(int id, int kind, HomeRoom *out, HomeVisit *info)
{
    return gen_room(id, kind > HVK_HOME && kind < HVK_VACANT ? kind : 0, out, info);
}

/* ---- wants ------------------------------------------------------------------------ */
static const char *const want_text[HWANT_COUNT] = {
    "somewhere to sit", "something pink", "a plant", "more light", "a plushie",
    "something spooky", "something cute", "a good book", "a snack", "some music",
    "my hobby", "my colour", "a soft rug", "something for the wall", "a table",
    "a game",
};
const char *home_visit_want_text(int want)
{
    return want >= 0 && want < HWANT_COUNT ? want_text[want] : "";
}

void home_visit_wants(int id, uint8_t out[3])
{
    DoomoRng rng;
    DoomoResident *r = town_res(id);
    int n = 0, guard = 0;
    rng_seed(&rng, ((uint32_t)(doomo_town.day / 7) ^ (uint32_t)id) * 0x2545F491u ^ doomo_town.seed ^ HOME_CTX_WANTS);
    while (n < 3 && guard++ < 32) {
        int w = rng_range(&rng, HWANT_COUNT), k, dup = 0;
        if (r && r->fear == 1 && n == 0) w = HWANT_LAMP;                /* The Dark wants light */
        if (r && r->fear == 22 && w == HWANT_CUTE) continue;            /* Cute Things */
        if (r && r->fear == 21 && w == HWANT_PLUSH) continue;           /* Hugs */
        for (k = 0; k < n; k++) dup |= out[k] == w;
        if (!dup) out[n++] = (uint8_t)w;
    }
    while (n < 3) { out[n] = (uint8_t)n; n++; }
}

int home_visit_want_met(int want, const DoomoResident *r, int furn, int pa)
{
    const DoomoFurn *f = home_furn(furn);
    int paint;
    if (!f) return 0;
    paint = pa >= 0 && pa < 32 ? pa : f->paint_a;
    switch (want) {
    case HWANT_SEAT: return f->role == FR_SEAT || f->interact == FI_SIT;
    case HWANT_PINK: return paint == PAINT_BUBBLEGUM || paint == PAINT_ROSE || paint == PAINT_CHERRY || paint == PAINT_MAGENTA;
    case HWANT_PLANT: return f->role == FR_PLANT;
    case HWANT_LAMP: return (f->flags & FF_LIGHT) != 0;
    case HWANT_PLUSH: return f->role == FR_PLUSH;
    case HWANT_SPOOKY: return f->spooky >= 4;
    case HWANT_CUTE: return f->cute >= 6;
    case HWANT_BOOK: return f->interact == FI_READ;
    case HWANT_SNACK: return f->interact == FI_EAT;
    case HWANT_MUSIC: return f->interact == FI_MUSIC;
    case HWANT_HOBBY: return r && r->like_hobby < HOBBY_COUNT &&
                             (hobby_pieces[r->like_hobby][0] == furn || hobby_pieces[r->like_hobby][1] == furn);
    case HWANT_COLOR: return r && paint == r->like_color;
    case HWANT_RUG: return (f->flags & FF_RUG) != 0;
    case HWANT_WALL: return (f->flags & FF_WALL) != 0;
    case HWANT_TABLE: return f->role == FR_TABLE;
    case HWANT_GAME: return f->interact == FI_PLAY;
    default: return 0;
    }
}

/* ---- reactions (HF sec.3.4) -------------------------------------------------------- */
static const char *const lines_polite[HVR_COUNT] = {
    "", "Oh! Oh. Oh!", "...oh! For me?", "oh. a piece.", "I love it!", "My favourite thing!",
    "Get it away from me!", "Is that... food?", "Don't mind if I do.", "Acceptable.",
};
static const char *const lines_blunt[HVR_COUNT] = {
    "", "WHAT. Okay. Yes.", "Huh. Yeah.", "a thing.", "Love. It.", "Mine now.",
    "NOPE.", "No.", "Sitting.", "Fine.",
};

int home_visit_react(const DoomoResident *r, int furn, int pa, const char **line)
{
    const DoomoFurn *f = home_furn(furn);
    int react, paint;
    const char *const *lines;
    if (line) *line = "";
    if (!r || !f) return HVR_NONE;
    lines = r->persona[PERS_SPEECH] >= 8 ? lines_blunt : lines_polite;
    paint = pa >= 0 && pa < 32 ? pa : f->paint_a;
    if (home_visit_fear_hit(r->fear, furn)) react = HVR_FLEE;
    else if (r->like_food < FOOD_COUNT && r->hate_food < FOOD_COUNT &&
             snack_pieces[food_snack[r->hate_food]] == furn &&
             snack_pieces[food_snack[r->like_food]] != furn) react = HVR_EW;
    else if (r->like_hobby < HOBBY_COUNT &&
             (hobby_pieces[r->like_hobby][0] == furn || hobby_pieces[r->like_hobby][1] == furn)) react = HVR_BANG;
    else if (paint == r->like_color) react = HVR_HEARTS;
    else if (r->persona[PERS_ATTITUDE] >= 10 && (f->interact == FI_SIT || (f->flags & FF_LIGHT))) react = HVR_SIT;
    else if (r->persona[PERS_ATTITUDE] < 5) react = HVR_NOD;
    else if (r->persona[PERS_EXPRESSION] >= 10) react = HVR_DOUBLE_TAKE;
    else if (r->persona[PERS_EXPRESSION] >= 5) react = HVR_DAWNING;
    else react = HVR_DEADPAN;
    if (line) *line = lines[react];
    return react;
}

/* ---- gifts, the log, thank-yous ----------------------------------------------------- */
static void take_one(int furn)
{
    HomeState *s = home_state();
    int n;
    if (!s || furn < 0 || furn >= FURN_MAX) return;
    n = (s->owned[furn >> 1] >> ((furn & 1) * 4)) & 15;
    if (n > 0) n--;
    s->owned[furn >> 1] = (uint8_t)((s->owned[furn >> 1] & (0xF0 >> ((furn & 1) * 4))) | (n << ((furn & 1) * 4)));
}

int home_visit_gift(int id, int furn, int x, int y, int rot, int pa)
{
    HomeState *s = home_state();
    int k, w0, w1;
    if (!s || id < 0 || id >= DOOMO_MAX_RESIDENTS || furn <= 0 || furn >= FURN_MAX) return -1;
    if (home_owned(furn) - home_placed(furn, NULL, -1) <= 0) return -1;
    for (k = 0; k < HOME_GIFTS; k++)
        if (!(s->gifts[k][2] | s->gifts[k][3])) break;
    if (k == HOME_GIFTS) return -1;
    w0 = (id & 0x3FF) | ((x & 7) << 10) | ((y & 7) << 13);
    w1 = (furn & 0x1FF) | ((rot & 3) << 9) | (((pa < 0 ? FURN_PAINT_DEFAULT : pa) & 31) << 11);
    s->gifts[k][0] = (uint8_t)w0; s->gifts[k][1] = (uint8_t)(w0 >> 8);
    s->gifts[k][2] = (uint8_t)w1; s->gifts[k][3] = (uint8_t)(w1 >> 8);
    take_one(furn);
    s->ver_flags |= HVF_TOUCHED;
    return k;
}

int home_visit_gift_count(int id)
{
    HomeState *s = home_state();
    int k, n = 0;
    if (!s) return 0;
    for (k = 0; k < HOME_GIFTS; k++)
        if ((s->gifts[k][2] | s->gifts[k][3]) && ((s->gifts[k][0] | (s->gifts[k][1] << 8)) & 0x3FF) == id) n++;
    return n;
}

int home_visited(int id)
{
    HomeState *s = home_state();
    if (!s || id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    return (s->visited[id >> 3] >> (id & 7)) & 1;
}

void home_visit_log(int id)
{
    HomeState *s = home_state();
    int i, n = 0;
    if (!s || id < 0 || id >= DOOMO_MAX_RESIDENTS) return;
    s->visited[id >> 3] |= (uint8_t)(1 << (id & 7));
    /* the visitor log: 4 x u16, most recent first, no duplicates */
    {
        uint16_t log[4];
        int k = 0;
        log[k++] = (uint16_t)(id + 1);
        for (i = 0; i < 4 && k < 4; i++) {
            int v = s->vlog[i * 2] | (s->vlog[i * 2 + 1] << 8);
            if (v && v != id + 1) log[k++] = (uint16_t)v;
        }
        while (k < 4) log[k++] = 0;
        for (i = 0; i < 4; i++) { s->vlog[i * 2] = (uint8_t)log[i]; s->vlog[i * 2 + 1] = (uint8_t)(log[i] >> 8); }
    }
    for (i = 0; i < HOME_SZ_VISITED; i++) {
        uint8_t b = s->visited[i];
        while (b) { n += b & 1; b >>= 1; }
    }
    if (n >= 50) unlock_grant(HUL_VISIT_50);
    if (n >= 500) unlock_grant(HUL_VISIT_500);
    if (n >= 1000) unlock_grant(HUL_VISIT_1000);
    s->ver_flags |= HVF_TOUCHED;
}

int home_visit_recent(int i)
{
    HomeState *s = home_state();
    int v;
    if (!s || i < 0 || i >= 4) return -1;
    v = s->vlog[i * 2] | (s->vlog[i * 2 + 1] << 8);
    return v ? v - 1 : -1;
}

/* HOME.wants: u16 the last citizen thanked (+1), u16 the week it happened */
int home_visit_thank(int id, int furn, int pa, int *thanks_furn)
{
    HomeState *s = home_state();
    DoomoResident *r = town_res(id);
    uint8_t w[3];
    int k, met = 0, all = 1, week = doomo_town.day / 7;
    if (thanks_furn) *thanks_furn = -1;
    if (!r) return 0;
    home_visit_wants(id, w);
    for (k = 0; k < 3; k++)
        if (home_visit_want_met(w[k], r, furn, pa)) met |= 1 << k;
    if (met) r->level = (uint8_t)DOOMO_MIN(99, r->level + 8);
    if (!s) return met;
    /* all three: a gift in their home meets each want */
    for (k = 0; k < 3; k++) {
        int j, ok = 0;
        for (j = 0; j < HOME_GIFTS && !ok; j++) {
            const uint8_t *e = s->gifts[j];
            int w1 = e[2] | (e[3] << 8);
            if (w1 && ((e[0] | (e[1] << 8)) & 0x3FF) == id)
                ok = home_visit_want_met(w[k], r, w1 & 0x1FF, (w1 >> 11) & 31);
        }
        all &= ok;
    }
    if (all && !((s->wants[0] | (s->wants[1] << 8)) == id + 1 && (s->wants[2] | (s->wants[3] << 8)) == (week & 0xFFFF))) {
        int give = r->like_hobby < HOBBY_COUNT ? hobby_pieces[r->like_hobby][0] : FURN_CUPCAKE_PLUSH;
        s->wants[0] = (uint8_t)(id + 1); s->wants[1] = (uint8_t)((id + 1) >> 8);
        s->wants[2] = (uint8_t)week; s->wants[3] = (uint8_t)(week >> 8);
        if (home_grant(give) && thanks_furn) *thanks_furn = give;
    }
    return met;
}
