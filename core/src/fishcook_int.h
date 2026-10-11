/* fishcook_int.h - private glue of the Lake of Fire fishing (fish*.c) and
 * the Brimstone Bites kitchen (cook*.c), owner: fishcook. Not part of the
 * contract.
 *
 * FISHING (SCENE_FISHING, fish_scene.c): cast (hold A, release at the
 * right power), wait while the bobber bobs, hook the bite, then reel with
 * the tension meter: hold A to reel, let go when the fish runs. 30
 * species (fish_data.c) from Bubblegum Guppy to Lord Vapula's Lost Sock,
 * five rarity tiers + junk, day / night / twilight species, bait (existing
 * food items), rods, size records, a Fishdex (one unlock bit per species)
 * and a cooler: keep fish to cook them, sell them, or let them go.
 *
 * COOKING (SCENE_COOKING, cook_scene.c): Granny Hopscotch's kitchen. Pick a
 * recipe in the book (22, cook_data.c), then three timing games - CHOP,
 * STIR and a heat step (BAKE / FRY / SIMMER) - grade the dish (BURNT, OK,
 * GREAT, PERFECT -> portions). Ingredients are food items, the new pantry
 * items (items_ext.c) and fish from the cooler. At the counter you serve
 * today's customers (sim_feed + the dish's flavour, tips).
 *
 * Persistent data: the save chunk 'FISH' (fishcook.c). Unlocks: ids
 * 960..1023 (unlocks_fishcook.c; the roster team's unused range).
 * Budgets: .bss well under 3 KB, no .data, no malloc, no floats.
 */
#ifndef DOOMO_FISHCOOK_INT_H
#define DOOMO_FISHCOOK_INT_H

#include "doomo/doomo.h"

/* ---- scene args ---------------------------------------------------------- */
#define FISH_ARG_DEX       1        /* open on the Fishdex                     */
#define FISH_ARG_COOLER    2        /* open on the cooler                      */
#define FISH_ARG_MODE_MASK 3
#define FISH_ARG_AUTOPLAY  0x20     /* the bot fishes (tests, screenshots)     */
#define FISH_ARG_NIGHT     0x40     /* debug: set the clock to 23:00 first     */
#define FISH_ARG_DEMO      0x80     /* debug: fill the Fishdex + cooler first  */
#define FISH_ARG_FORCE(sp) (((sp) + 1) << 8)   /* debug: bites are species sp */

#define COOK_ARG_COUNTER   1        /* open at the counter (serve customers)   */
#define COOK_ARG_AUTOPLAY  0x20     /* the bot cooks (tests, screenshots)      */
#define COOK_ARG_STOCK     0x40     /* debug: stock the pantry + cooler first  */
#define COOK_ARG_DEMO      0x80     /* debug: premade citizens on an empty isle */
#define COOK_ARG_RECIPE(r) (((r) + 1) << 8)    /* start cooking recipe r     */

/* ---- species --------------------------------------------------------------- */
enum {
    /* cute (Hillvile-ish) */
    FISH_BUBBLEGUM_GUPPY, FISH_MARSHMALLOW_MINNOW, FISH_SPRINKLE_TROUT,
    FISH_HONEY_BASS, FISH_TEACUP_TETRA, FISH_PLUSH_KOI, FISH_COTTON_CATFISH,
    FISH_LULLABY_LOACH, FISH_MOCHI_PUFFER, FISH_STARLIGHT_SUNFISH,
    FISH_RAINBOW_RIBBON, FISH_ANGEL_ANGELFISH, FISH_PUDDINGFIN,
    /* hellish */
    FISH_BRIMSTONE_BREAM, FISH_SCREAMING_CARP, FISH_GOTH_GUPPY,
    FISH_SKULL_SARDINE, FISH_SULFUR_SALMON, FISH_LAVA_EEL, FISH_VAPE_PIKE,
    FISH_CINDER_SNAPPER, FISH_CURSED_COD, FISH_HELLMOUTH_BASS,
    FISH_BONE_BARRACUDA, FISH_DEMON_DISCUS, FISH_GATE_GROUPER,
    FISH_MAMA_MAGMAW,
    /* junk (and one legendary sock) */
    FISH_SOGGY_BOOT, FISH_GHOST_CAN, FISH_VAPULA_SOCK,
    FISH_COUNT
};
DOOMO_STATIC_ASSERT(FISH_COUNT <= 32, fish_fit_dex_range);

enum { FR_JUNK, FR_COMMON, FR_UNCOMMON, FR_RARE, FR_EPIC, FR_LEGEND, FR_COUNT };
enum { FT_ANY, FT_DAY, FT_NIGHT, FT_TWILIGHT };   /* when it bites */
enum { FSH_ROUND, FSH_OVAL, FSH_LONG, FSH_FLAT, FSH_PIKE, FSH_BIG,
       FSH_BOOT, FSH_CAN, FSH_SOCK };
enum { FPAT_NONE, FPAT_SPOTS, FPAT_STRIPES, FPAT_SPRINKLES, FPAT_STARS,
       FPAT_PATCHES, FPAT_CRACKS, FPAT_BONES, FPAT_RAINBOW };
enum { FEYE_CUTE, FEYE_SLEEPY, FEYE_ANGRY, FEYE_HOLLOW, FEYE_BUTTON,
       FEYE_GOTH, FEYE_SAD, FEYE_NONE };
/* extras (bit flags) */
enum { FF_HALO = 1, FF_HORNS = 2, FF_WHISKERS = 4, FF_FLAMES = 8,
       FF_VAPE = 16, FF_BUBBLE = 32, FF_SCREAM = 64, FF_TEETH = 128,
       FF_STITCH = 256, FF_CROWN = 512, FF_GLOW = 1024, FF_WINGS = 2048,
       FF_BLUSH = 4096, FF_CHAIN = 8192 };
/* how it fights on the line */
enum { FSTY_STEADY, FSTY_DARTER, FSTY_JUMPER, FSTY_SLEEPY, FSTY_THRASHER };

typedef struct FishInfo {
    const char *name;
    const char *desc;         /* Fishdex line                            */
    uint8_t  rarity;          /* FR_*                                     */
    uint8_t  time;            /* FT_*                                     */
    uint8_t  hell;            /* 1 = hellish (bait, recipes)              */
    uint8_t  shape;           /* FSH_*                                    */
    uint16_t size_min, size_max;   /* cm                                  */
    uint16_t price;           /* H$ for an average one                    */
    uint8_t  strength;        /* 1..9 pull                                */
    uint8_t  style;           /* FSTY_*                                   */
    uint8_t  body, belly, fin;     /* palette indices (base shades)       */
    uint8_t  pattern, eye;    /* FPAT_*, FEYE_*                           */
    uint16_t extras;          /* FF_*                                     */
} FishInfo;
extern const FishInfo fish_info[FISH_COUNT];
extern const char *const fish_rarity_names[FR_COUNT];
extern const char *const fish_time_names[4];

/* Rods (unlockable) and bait (food items from the bag). */
enum { ROD_TWIG, ROD_CANDY, ROD_HELLSTEEL, ROD_GOLDEN, ROD_COUNT };
typedef struct FishRod {
    const char *name;
    const char *desc;
    uint16_t unlock;          /* unlock id or ACT_ALWAYS                  */
    uint8_t  tension;         /* breaking point bonus, percent            */
    uint8_t  reel;            /* reel speed (1/16 distance per tick)      */
    uint8_t  color, tip;      /* palette                                  */
} FishRod;
extern const FishRod fish_rods[ROD_COUNT];
int fish_rod_ok(int rod);

enum { BAIT_NONE, BAIT_CUPCAKE, BAIT_PEPPERS, BAIT_EYEBALLS, BAIT_MEAT,
       BAIT_SUGAR, BAIT_COUNT };
typedef struct FishBait {
    uint8_t item;             /* ITEM_* used up per cast (0 = none)       */
    uint8_t cute, hell;       /* weight multipliers (x/2)                 */
    uint8_t rare;             /* extra weight for rare+ (percent)         */
    uint8_t size;             /* size bias 0..100                         */
    uint8_t speed;            /* bites come faster (percent shorter wait) */
    const char *effect;       /* "Cute fish love it"                      */
} FishBait;
extern const FishBait fish_baits[BAIT_COUNT];
int fish_bait_ok(int bait);   /* none, or you have the item                */

/* Can species sp bite right now (time of day, story gates)? */
int  fish_time_ok(int sp, int hour);
int  fish_hour_kind(int hour);            /* FT_DAY / FT_NIGHT / FT_TWILIGHT */
/* Weighted roll of the species that bites (deterministic for rng state). */
int  fish_roll_species(DoomoRng *r, int hour, int bait);
int  fish_roll_size(DoomoRng *r, int sp, int bait);   /* cm               */
int  fish_value(int sp, int size);                     /* H$ to sell       */
/* Weight table (tests): relative bite weight of sp at hour with bait. */
int  fish_weight(int sp, int hour, int bait);

/* A catch: Fishdex + record + counters + unlocks. Returns flags. */
enum { CATCH_NEW = 1, CATCH_RECORD = 2 };
int  fish_register_catch(int sp, int size);
void fish_keep(int sp);                   /* into the cooler (max 99)       */
int  fish_sell(int sp, int size);         /* H$ earned                      */
void fish_release(int sp);
int  fish_sell_cooler(int sp, int n);     /* sell n kept fish, H$ earned    */
int  fish_cooler_total(void);
int  fish_dex_count(void);                /* species caught                 */
int  fish_dex_has(int sp);

/* ---- recipes ----------------------------------------------------------------- */
enum { HEAT_BAKE, HEAT_FRY, HEAT_SIMMER, HEAT_SCORCH /* bake it to a crisp */ };
enum { ING_NONE, ING_ITEM, ING_FISH, ING_CUTE_FISH, ING_HELL_FISH, ING_ANY_FISH };
enum { DISH_CUPCAKE, DISH_BOWL, DISH_STACK, DISH_PLATE, DISH_ROLL, DISH_BALLS,
       DISH_PIZZA, DISH_CAKE, DISH_JELLY, DISH_BAGEL, DISH_LUMP, DISH_STEAK,
       DISH_PUDDING, DISH_SALAD };
enum { GRADE_NONE, GRADE_BURNT, GRADE_OK, GRADE_GREAT, GRADE_PERFECT };

typedef struct CookIng { uint8_t kind, id, qty; } CookIng;
typedef struct Recipe {
    const char *name;
    const char *desc;
    uint8_t  item;            /* ITEM_* it makes                          */
    int8_t   food;            /* doomo_food_names flavour, -1 none        */
    uint8_t  heat;            /* HEAT_*                                   */
    uint8_t  diff;            /* 1..5                                     */
    uint16_t unlock;          /* unlock id or ACT_ALWAYS                  */
    uint8_t  yield;           /* portions at OK                           */
    uint8_t  art;             /* DISH_*                                   */
    uint8_t  c1, c2, c3;      /* dish colours (palette)                   */
    CookIng  ing[3];
} Recipe;
#define RECIPE_COUNT 22
extern const Recipe cook_recipes[RECIPE_COUNT];
extern const char *const cook_grade_names[5];
extern const char *const cook_heat_names[4];

int  cook_recipe_ok(int r);               /* unlocked                       */
/* Ingredients: how many of ingredient i you have (fish: in the cooler). */
int  cook_have(const CookIng *g);
int  cook_can_make(int r);                /* every ingredient in stock       */
/* H$ to buy what is missing from Granny's pantry; -1 if something can't
 * be bought (fish, dishes you haven't unlocked). */
int  cook_missing_cost(int r);
int  cook_buy_missing(int r);             /* 1 if bought (and paid)         */
void cook_take(int r);                    /* use the ingredients up         */
/* Grade from the three step scores (0..100 each). */
int  cook_grade(int r, int chop, int stir, int heat);
/* Portions a grade gives. */
int  cook_yield(int r, int grade);
/* Records the result: inventory, best grade, counters, unlocks. Returns
 * the number of portions added (of cook_result_item()). */
int  cook_finish(int r, int grade);
int  cook_result_item(int r, int grade);
/* The flavour (doomo_food_names index) of an item: -1 if none. Dishes
 * carry their recipe's flavour (a Fish Ramen tastes like Ramen). */
int  cook_item_food(int item);

/* ---- customers (the counter) --------------------------------------------- */
#define FC_CUSTOMERS 4
/* Pick today's customers if not done yet; returns how many there are. */
int  cook_customers(void);
int  cook_customer(int i);                /* resident id or -1              */
int  cook_customer_served(int i);
/* Serve item to customer i: sim_feed + flavour bonus + tip. Writes the
 * reaction into buf. Returns SERVE_* (or SERVE_FAIL). */
enum { SERVE_FAIL, SERVE_OK, SERVE_FAV, SERVE_HATE };
int  cook_serve(int i, int item, char *buf, int cap, int *tip);

/* ---- persistent data (save chunk 'FISH', fishcook.c) ---------------------- */
typedef struct FcData {
    uint16_t record[FISH_COUNT];   /* best size (cm), 0 = never caught    */
    uint8_t  caught[FISH_COUNT];   /* times caught (saturates at 255)     */
    uint8_t  cooler[FISH_COUNT];   /* kept fish, 0..99                    */
    uint8_t  grade[RECIPE_COUNT];  /* best GRADE_* per recipe             */
    uint8_t  rod, bait;
    uint16_t cooked, perfects, burnt, released, sold, served;
    uint32_t cust_day;             /* day + 1 the customers were picked   */
    int16_t  cust[FC_CUSTOMERS];   /* resident ids, -1 = empty seat       */
    uint8_t  cust_served;          /* bit per customer                    */
    uint8_t  feats;                /* FEAT_* bits (code-granted trophies)  */
} FcData;
extern FcData fc;
enum { FEAT_FAV_SERVED = 1, FEAT_FULL_HOUSE = 2 };

/* ---- unlock ids (unlocks_fishcook.c) ---------------------------------------- */
#define FC_UL_FIRST       960
#define FC_UL_DEX(sp)     (960 + (sp))       /* 960..989 Fishdex entries    */
#define FC_UL_ROD_CANDY   990
#define FC_UL_ROD_HELL    991
#define FC_UL_ROD_GOLD    992
#define FC_UL_RECIPE0     993                /* 993..1008 locked recipes    */
#define FC_UL_FIRST_BITE  1009
#define FC_UL_MASTER      1010
#define FC_UL_WHOPPER     1011
#define FC_UL_RELEASE     1012
#define FC_UL_MONGER      1013
#define FC_UL_NIGHT_OWL   1014
#define FC_UL_LEGEND      1015
#define FC_UL_GOLD_STAR   1016
#define FC_UL_HEAD_CHEF   1017
#define FC_UL_DISASTER    1018
#define FC_UL_REGULAR     1019
#define FC_UL_FULL_HOUSE  1020
#define FC_UL_LAST        1020
/* Grant every unlock whose condition holds now (code-granted ones). */
void fc_check_unlocks(void);

/* ---- shared juice (fishcook.c): one particle pool for both scenes ---------- */
enum { FCX_SPARK, FCX_DROP, FCX_STEAM, FCX_HEART, FCX_STAR, FCX_COIN,
       FCX_CRUMB, FCX_BUBBLE, FCX_EMBER, FCX_SMOKE, FCX_CONFETTI, FCX_RING };
void fc_fx(int kind, int x, int y, int vx16, int vy16, uint8_t color, int life);
void fc_burst(int kind, int x, int y, int n, int speed16, uint8_t color);
void fc_popup(int x, int y, const char *text, uint8_t color);
void fc_fx_update(void);
void fc_fx_draw(int ox, int oy);
void fc_fx_clear(void);
int  fc_fx_live(void);
/* Local RNG for visuals / rolls. */
extern DoomoRng fc_rng;
int  fc_rand(int n);

/* ---- shared drawing (fishcook.c) ------------------------------------------- */
int  fc_w(void);
int  fc_h(void);
int  fc_big(void);                         /* 320x240-class screen         */
void fc_text(int x, int y, const char *s, uint8_t c);      /* outlined     */
void fc_textc(int cx, int y, const char *s, uint8_t c);
void fc_money_pill(int right_x, int y);
void fc_stars(int x, int y, int n, int max, int t);       /* rarity       */
void fc_pill(int x, int y, int w, int h, uint8_t fill);
/* A speech bubble with a tail pointing to (tx, ty); wraps text. */
void fc_bubble(int x, int y, int w, const char *s, int tx, int ty, int chars);
void fc_shadow(int cx, int cy, int rx);
/* Grade stamp (PERFECT! ...) centred at cx, cy; t = ticks since shown. */
void fc_grade_stamp(int cx, int cy, int grade, int t);

/* ---- fish drawing (fish_gfx.c) -------------------------------------------- */
enum { FDRAW_FLIP = 1, FDRAW_FIGHT = 2, FDRAW_SILHOUETTE = 4, FDRAW_HAPPY = 8 };
/* Species sp centred at cx,cy facing right; r = half length in px. */
void fish_draw(int sp, int cx, int cy, int r, int t, int flags, uint8_t sil);
/* Half length (px) to draw a size at in a card of max half length rmax. */
int  fish_draw_radius(int sp, int size, int rmax);
/* Scenery. */
typedef struct LakeView {
    int w, h, horizon, pier_y, pier_x1;  /* layout                        */
    int night, twilight;
    uint32_t t;
} LakeView;
void lake_layout(LakeView *v);
void lake_draw_sky(const LakeView *v);
void lake_draw_water(const LakeView *v);
void lake_draw_pier(const LakeView *v);
void lake_draw_angler(const LakeView *v, int x, int y, int pose, int bend,
                      int rod, int *tipx, int *tipy);
void lake_draw_bobber(int x, int y, int t, int under);
void lake_draw_ripple(int x, int y, int r, uint8_t c);
void lake_draw_shadow(int x, int y, int r);

/* ---- kitchen drawing (cook_gfx.c) ------------------------------------------ */
void kitchen_draw_back(uint32_t t, int busy);   /* busy: leave room for gauges */
void kitchen_draw_counter(int y, uint32_t t);
void kitchen_steam(int cx, int cy, int t, int n);   /* wisps rising */
/* Granny Hopscotch, the plush bunny granny chef. mood: 0 calm, 1 talk,
 * 2 cheer, 3 dismay. s = size unit (px per 16). */
enum { GRAN_CALM, GRAN_TALK, GRAN_CHEER, GRAN_DISMAY };
void granny_draw(int cx, int cy, int s, int t, int mood);
/* A dish (recipe r) centred at cx,cy, half width rw; doneness 0..255
 * browns it (255 = as cooked, beyond = burnt), t animates steam. */
void dish_draw(int r, int cx, int cy, int rw, int t, int burnt);
void dish_draw_item(int item, int cx, int cy, int rw, int t);
/* An ingredient icon (item or fish) at cx,cy. */
void ingredient_draw(const CookIng *g, int cx, int cy, int rw, int t);

/* ---- test hooks (fish_scene.c, cook_scene.c) ------------------------------ */
int fish_scene_state(void);       /* FS_* (fish_scene.c)                    */
int fish_scene_species(void);
int fish_scene_catches(void);
void fish_scene_reel_info(int *dist, int *tension, int *stamina);
int cook_scene_state(void);       /* CS_* (cook_scene.c)                    */
int cook_scene_grade(void);
int cook_scene_score(int step);
int cook_scene_recipe(void);

#endif
