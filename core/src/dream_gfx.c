/* dream_gfx.c - the dreams' drawing (owner: concert).
 *
 * The bedroom, the bubble that carries you into the dream, a dream sky
 * that rots from candy pastel to bruise purple and blood red, a wobbly
 * cloud frame, 48 procedural snack icons (they grow faces, then teeth),
 * 24 procedural fear icons and the 16 vignettes. creep 0..16 drives every
 * cute -> creepy change; the nightmare adds red and shaking. Everything
 * is plat_rect spans / ui primitives plus the citizens' own portraits and
 * chibis and a few of the demon sprites.
 */
#include "concert_int.h"

static int SW(void) { return plat_screen_w(); }
static int SH(void) { return plat_screen_h(); }
static int BIG(void) { return plat_screen_h() >= 200; }
static uint32_t h2(uint32_t a, uint32_t b) { return doomo_hash32(a * 2246822519u ^ (b * 3266489917u + 7u)); }

static void cspan(int x0, int x1, int y, int h, uint8_t c)
{
    if ((x0 + y) & 1) x0++;
    if (x1 > x0) plat_checker(x0, y, x1 - x0, h, c);
}

/* a soft round light (50% checker ellipse) */
static void glow(int cx, int cy, int rx, int ry, uint8_t c)
{
    int y;
    for (y = -ry; y <= ry; y += 2) {
        int dx = (int)fx_isqrt((uint32_t)((ry * ry - y * y) * rx * rx / (ry * ry)));
        cspan(cx - dx, cx + dx, cy + y, 2, c);
    }
}

/* eyes + mouth on a snack / fear; creep 0..16 */
static void face(int cx, int cy, int s, int creep, uint32_t t)
{
    int ex = s / 2 > 1 ? s / 2 : 1, blink = ((t + (uint32_t)cx) % 90) < 4 && creep < 10;
    uint8_t eye = creep >= 10 ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_RAMP(RAMP_STONE, 0);
    if (blink) {
        plat_rect(cx - ex - 1, cy, 3, 1, eye);
        plat_rect(cx + ex - 1, cy, 3, 1, eye);
    } else {
        plat_rect(cx - ex - 1, cy - 1, 2, 2 + (s > 6), eye);
        plat_rect(cx + ex, cy - 1, 2, 2 + (s > 6), eye);
        if (creep < 10) {
            plat_rect(cx - ex - 1, cy - 1, 1, 1, PAL_WHITE);
            plat_rect(cx + ex, cy - 1, 1, 1, PAL_WHITE);
        } else {
            plat_rect(cx - ex - 1, cy - 1, 1, 1, PAL_RAMP(RAMP_LAVA, 7));
            plat_rect(cx + ex, cy - 1, 1, 1, PAL_RAMP(RAMP_LAVA, 7));
        }
    }
    if (creep < 8) {
        /* :3 */
        plat_rect(cx - 1, cy + 2, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(cx + 1, cy + 2, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(cx, cy + 3, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        if (s > 5) {
            plat_rect(cx - ex - 2, cy + 2, 2, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            plat_rect(cx + ex + 1, cy + 2, 2, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        }
    } else {
        /* a toothy grin that widens with the creep */
        int mw = 2 + (creep - 8) * s / 12, k;
        plat_rect(cx - mw, cy + 2, 2 * mw + 1, 3, PAL_RAMP(RAMP_BLOOD, 0));
        for (k = -mw; k <= mw; k += 2) {
            plat_rect(cx + k, cy + 2, 1, 2, PAL_WHITE);
            plat_rect(cx + k + 1, cy + 4, 1, 1, PAL_WHITE);
        }
    }
}

/* ====================================================================== */
/* snacks                                                                   */
enum { FS_CUPCAKE, FS_ONIGIRI, FS_BOWL, FS_DONUT, FS_EYEBALL, FS_PUDDING, FS_PEPPER, FS_SLICE,
       FS_MEAT, FS_CUP, FS_STACK, FS_BLOB, FS_TACO, FS_SUSHI, FS_MACARON, FS_CONE, FS_TOAST,
       FS_BUN, FS_STICK, FS_CLOUD, FS_CANDY, FS_BAT, FS_WORM, FS_FRUIT, FS_PICKLE };
/* shape, main paint, accent paint per food (catalog order) */
static const uint8_t food_look[FOOD_COUNT][3] = {
    { FS_CUPCAKE, PAINT_BUBBLEGUM, PAINT_CARAMEL },   /* Cupcakes          */
    { FS_ONIGIRI, PAINT_WHITE, PAINT_JET },           /* Onigiri           */
    { FS_BOWL, PAINT_LAVENDER, PAINT_WHITE },         /* Soul Soup         */
    { FS_DONUT, PAINT_CARAMEL, PAINT_HELLFIRE },      /* Brimstone Bagels  */
    { FS_EYEBALL, PAINT_WHITE, PAINT_COBALT },        /* Candy Eyeballs    */
    { FS_PUDDING, PAINT_HONEY, PAINT_CARAMEL },       /* Pudding           */
    { FS_PEPPER, PAINT_CHERRY, PAINT_LIME },          /* Hell Peppers      */
    { FS_SLICE, PAINT_WHITE, PAINT_LEMON },           /* Angel Cake        */
    { FS_MEAT, PAINT_ROSE, PAINT_BONE },              /* Meat Slabs        */
    { FS_BOWL, PAINT_LIME, PAINT_TANGERINE },         /* Salad             */
    { FS_CUP, PAINT_WHITE, PAINT_SKY },               /* Milk              */
    { FS_CUP, PAINT_CHOCOLATE, PAINT_WHITE },         /* Coffee            */
    { FS_STACK, PAINT_CARAMEL, PAINT_HONEY },         /* Pancakes          */
    { FS_STACK, PAINT_HONEY, PAINT_CARAMEL },         /* Waffles           */
    { FS_BOWL, PAINT_HONEY, PAINT_TANGERINE },        /* Ramen             */
    { FS_BLOB, PAINT_WHITE, PAINT_BUBBLEGUM },        /* Mochi             */
    { FS_BLOB, PAINT_PEACH, PAINT_WHITE },            /* Dumplings         */
    { FS_SLICE, PAINT_HONEY, PAINT_CHERRY },          /* Pizza             */
    { FS_TACO, PAINT_HONEY, PAINT_LIME },             /* Tacos             */
    { FS_SUSHI, PAINT_WHITE, PAINT_TANGERINE },       /* Sushi             */
    { FS_BOWL, PAINT_HONEY, PAINT_CARAMEL },          /* Curry             */
    { FS_DONUT, PAINT_BUBBLEGUM, PAINT_CARAMEL },     /* Donuts            */
    { FS_MACARON, PAINT_LAVENDER, PAINT_MINT },       /* Macarons          */
    { FS_CONE, PAINT_BUBBLEGUM, PAINT_CARAMEL },      /* Ice Cream         */
    { FS_CUP, PAINT_CARAMEL, PAINT_JET },             /* Bubble Tea        */
    { FS_TOAST, PAINT_HONEY, PAINT_CARAMEL },         /* Honey Toast       */
    { FS_BUN, PAINT_LIME, PAINT_HONEY },              /* Melon Bread       */
    { FS_STICK, PAINT_CARAMEL, PAINT_ROSE },          /* Takoyaki          */
    { FS_BLOB, PAINT_LEMON, PAINT_CHERRY },           /* Omurice           */
    { FS_CLOUD, PAINT_BUBBLEGUM, PAINT_SKY },         /* Cotton Candy      */
    { FS_SLICE, PAINT_TANGERINE, PAINT_WHITE },       /* Carrot Cake       */
    { FS_CUP, PAINT_WHITE, PAINT_CHERRY },            /* Popcorn           */
    { FS_MEAT, PAINT_CARAMEL, PAINT_BONE },           /* Fried Chicken     */
    { FS_BOWL, PAINT_LEMON, PAINT_CHERRY },           /* Spaghetti         */
    { FS_BOWL, PAINT_HONEY, PAINT_WHITE },            /* Cereal            */
    { FS_CANDY, PAINT_LIME, PAINT_BUBBLEGUM },        /* Gumdrops          */
    { FS_CUPCAKE, PAINT_CHOCOLATE, PAINT_HELLFIRE },  /* Lava Cake         */
    { FS_BAT, PAINT_JET, PAINT_VAPE_PURPLE },         /* Bat Wing Chips    */
    { FS_WORM, PAINT_LIME, PAINT_BUBBLEGUM },         /* Worm Gummies      */
    { FS_STICK, PAINT_JET, PAINT_CHARCOAL },          /* Black Licorice    */
    { FS_TOAST, PAINT_CHARCOAL, PAINT_JET },          /* Burnt Toast       */
    { FS_CUP, PAINT_LEMON, PAINT_SLIME },             /* Sulfur Soda       */
    { FS_EYEBALL, PAINT_SLIME, PAINT_BLOOD },         /* Eyeball Jelly     */
    { FS_SLICE, PAINT_CHOCOLATE, PAINT_LIME },        /* Grave Dirt Cake   */
    { FS_FRUIT, PAINT_BLOOD, PAINT_FOREST },          /* Blood Oranges     */
    { FS_PICKLE, PAINT_LIME, PAINT_FOREST },          /* Pickles           */
    { FS_FRUIT, PAINT_FOREST, PAINT_LIME },           /* Brussels Sprouts  */
    { FS_BOWL, PAINT_BONE, PAINT_WHITE },             /* Bone Broth        */
};

void dream_food(int food, int cx, int cy, int s, int teeth, uint32_t t)
{
    const uint8_t *fl = food_look[(unsigned)food % FOOD_COUNT];
    uint8_t m = PAL_PAINT(fl[1], 1), md = PAL_PAINT(fl[1], 0), ml = PAL_PAINT(fl[1], 2);
    uint8_t a = PAL_PAINT(fl[2], 1), ad = PAL_PAINT(fl[2], 0), al = PAL_PAINT(fl[2], 2);
    int fy = cy, k;
    if (s < 3) s = 3;
    switch (fl[0]) {
    case FS_CUPCAKE:
        plat_rect(cx - s + 1, cy, 2 * s - 1, s, ad);
        for (k = -s + 2; k < s - 1; k += 3) plat_rect(cx + k, cy + 1, 1, s - 1, a);
        ui_fill_ellipse(cx, cy - 1, s, s * 2 / 3, md);
        ui_fill_ellipse(cx, cy - 2, s - 1, s / 2, m);
        ui_fill_circle(cx, cy - s / 2 - 2, s / 2, ml);
        ui_fill_circle(cx + 1, cy - s - 1, 2, PAL_PAINT(PAINT_CHERRY, 1));
        fy = cy - 1;
        break;
    case FS_ONIGIRI:
        for (k = 0; k < 2 * s; k++) {
            int hw = 1 + k * s / (2 * s) + k / 3;
            plat_rect(cx - hw, cy - s + k, 2 * hw + 1, 1, k == 0 ? md : m);
        }
        plat_rect(cx - s / 2, cy + s / 3, s + 1, s * 2 / 3, PAL_PAINT(PAINT_JET, 1));
        fy = cy - 1;
        break;
    case FS_BOWL:
        ui_fill_ellipse(cx, cy - 1, s + 1, s / 3 + 1, a);
        for (k = -s + 2; k < s; k += 3) plat_rect(cx + k, cy - 2 - ((k + (int)t / 8) & 1), 2, 2, al);
        for (k = 0; k < s; k++) {
            int hw = s - k * k / s;
            plat_rect(cx - hw, cy + k, 2 * hw + 1, 1, k < 2 ? ml : m);
        }
        plat_rect(cx - s / 2, cy + s - 1, s + 1, 2, md);
        fy = cy + s / 2;
        break;
    case FS_DONUT:
        ui_fill_circle(cx, cy, s, md);
        ui_fill_circle(cx, cy - 1, s - 1, m);
        ui_fill_ellipse(cx, cy - 2, s - 1, s / 2 + 1, a);
        for (k = 0; k < 4; k++) plat_rect(cx - s / 2 + k * s / 3, cy - s / 2 + (k & 1), 1, 2, PAL_WHITE);
        ui_fill_circle(cx, cy, s / 3, 0);
        if (s / 3 > 0) ui_fill_circle(cx, cy, s / 3, PAL_RAMP(RAMP_STONE, 0));
        fy = cy + s / 2;
        break;
    case FS_EYEBALL:
        ui_fill_circle(cx, cy, s, PAL_RAMP(RAMP_STONE, 5));
        ui_fill_circle(cx, cy - 1, s - 1, m);
        ui_fill_circle(cx + 1, cy, s / 2, a);
        ui_fill_circle(cx + 1, cy, s / 4, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(cx, cy - 1, 1, 1, PAL_WHITE);
        if (teeth > 8) for (k = 0; k < 3; k++) plat_line(cx - s + 1, cy - 2 + k * 2, cx - s / 2, cy - 1 + k, PAL_RAMP(RAMP_BLOOD, 5));
        return;                                    /* it IS the face */
    case FS_PUDDING:
        for (k = 0; k < s + 2; k++) {
            int hw = s / 2 + k * s / (2 * (s + 2)) + 1;
            plat_rect(cx - hw, cy - s / 2 + k, 2 * hw + 1, 1, k < s / 3 ? a : m);
        }
        plat_rect(cx - s, cy + s / 2 + 2, 2 * s + 1, 2, PAL_PAINT(PAINT_WHITE, 2));
        fy = cy + 1;
        break;
    case FS_PEPPER:
        ui_fill_ellipse(cx, cy, s / 2 + 1, s, md);
        ui_fill_ellipse(cx - 1, cy - 1, s / 2, s - 1, m);
        plat_rect(cx - 2, cy - s - 2, 5, 3, a);
        plat_rect(cx, cy - s - 4, 1, 2, ad);
        plat_rect(cx - 2, cy - s / 2, 1, s / 2, ml);
        break;
    case FS_SLICE:
        for (k = 0; k < 2 * s; k++) {
            int hw = k / 2 + 1;
            plat_rect(cx - hw, cy - s + k, 2 * hw + 1, 1, k < 3 ? a : m);
        }
        plat_rect(cx - s - 1, cy + s - 2, 2 * s + 3, 3, k > 0 ? md : m);
        if (fl[2] == PAINT_CHERRY) for (k = 0; k < 3; k++) ui_fill_circle(cx - 2 + k * 2, cy - 2 + (k & 1) * 4, 1, a);
        fy = cy + 1;
        break;
    case FS_MEAT:
        ui_fill_ellipse(cx - 1, cy, s, s * 2 / 3 + 1, md);
        ui_fill_ellipse(cx - 2, cy - 1, s - 1, s * 2 / 3, m);
        plat_rect(cx + s - 2, cy - 1, s / 2 + 2, 3, a);
        ui_fill_circle(cx + s + s / 2, cy - 1, 2, al);
        ui_fill_circle(cx + s + s / 2, cy + 2, 2, al);
        fy = cy;
        cx -= 2;
        break;
    case FS_CUP:
        plat_rect(cx - s / 2 - 1, cy - s, s + 3, 2 * s, ad);
        plat_rect(cx - s / 2, cy - s + 1, s + 1, 2 * s - 2, a);
        plat_rect(cx - s / 2, cy - s + 1, s + 1, 3, m);
        if (fl[2] == PAINT_JET) for (k = 0; k < 3; k++) ui_fill_circle(cx - s / 3 + k * s / 3, cy + s - 3, 1, PAL_PAINT(PAINT_JET, 1));
        if (fl[2] == PAINT_CHERRY) for (k = -s / 2; k <= s / 2; k += 3) plat_rect(cx + k, cy - s + 3, 1, 2 * s - 4, PAL_PAINT(PAINT_CHERRY, 1));
        plat_line(cx + 1, cy - s - 4, cx + 3, cy - s + 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        plat_rect(cx + s / 2 + 2, cy - s / 3, 2, s / 2 + 1, ad);
        fy = cy + 1;
        break;
    case FS_STACK:
        for (k = 2; k >= 0; k--) {
            ui_fill_ellipse(cx, cy + k * (s / 2) - s / 3, s + 1, s / 3 + 1, md);
            ui_fill_ellipse(cx, cy + k * (s / 2) - s / 3 - 1, s, s / 3, m);
        }
        plat_rect(cx - s / 2, cy - s / 2 - 2, s, 2, a);
        plat_rect(cx - 2, cy - s / 2, 4, s / 2 + 2, PAL_PAINT(PAINT_HONEY, 2));
        fy = cy + s / 3;
        break;
    case FS_BLOB:
        ui_fill_ellipse(cx, cy + 1, s + 1, s * 3 / 4 + 1, md);
        ui_fill_ellipse(cx, cy, s, s * 3 / 4, m);
        ui_fill_ellipse(cx - s / 3, cy - s / 3, s / 3, s / 4, ml);
        if (fl[2] == PAINT_CHERRY) plat_line(cx - s / 2, cy - s / 3, cx + s / 2, cy - s / 3 + 1, a);
        else ui_fill_circle(cx + s / 2, cy - s / 2, 1, a);
        break;
    case FS_TACO:
        ui_fill_ellipse(cx, cy, s + 1, s * 2 / 3, md);
        ui_fill_ellipse(cx, cy - 1, s, s * 2 / 3 - 1, m);
        for (k = -s + 2; k < s - 1; k += 3) plat_rect(cx + k, cy - s / 2 - 1, 2, 2, (k & 1) ? a : PAL_PAINT(PAINT_CHERRY, 1));
        break;
    case FS_SUSHI:
        plat_rect(cx - s, cy - 1, 2 * s + 1, s / 2 + 2, PAL_PAINT(PAINT_WHITE, 1));
        plat_rect(cx - s, cy - s / 2 - 1, 2 * s + 1, s / 2 + 1, a);
        for (k = -s + 2; k < s; k += 3) plat_rect(cx + k, cy - s / 2, 1, s / 2, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx - 1, cy - s / 2 - 1, 3, s / 2 + s / 2 + 2, PAL_PAINT(PAINT_JET, 1));
        fy = cy + 1;
        break;
    case FS_MACARON:
        ui_fill_ellipse(cx, cy - s / 3, s, s / 3 + 1, m);
        plat_rect(cx - s + 1, cy - 1, 2 * s - 1, 3, a);
        ui_fill_ellipse(cx, cy + s / 3 + 1, s, s / 3 + 1, md);
        fy = cy - 1;
        break;
    case FS_CONE:
        for (k = 0; k < s + 2; k++) {
            int hw = (s + 2 - k) * s / (2 * (s + 2)) + 1;
            plat_rect(cx - hw, cy + k, 2 * hw, 1, (k & 2) ? ad : a);
        }
        ui_fill_circle(cx, cy - s / 3, s * 2 / 3 + 1, md);
        ui_fill_circle(cx, cy - s / 3 - 1, s * 2 / 3, m);
        fy = cy - s / 3;
        break;
    case FS_TOAST:
        ui_round_rect(cx - s, cy - s, 2 * s + 1, 2 * s, s / 2 + 1, ad);
        ui_round_rect(cx - s + 1, cy - s + 1, 2 * s - 1, 2 * s - 2, s / 2, m);
        plat_rect(cx - s / 2, cy - s / 2, s / 2, s / 3, PAL_PAINT(PAINT_HONEY, 2));
        break;
    case FS_BUN:
        ui_fill_ellipse(cx, cy, s + 1, s * 3 / 4 + 1, md);
        ui_fill_ellipse(cx, cy - 1, s, s * 3 / 4, m);
        for (k = -s / 2; k <= s / 2; k += s / 3 + 1) plat_line(cx + k - 2, cy - s / 2, cx + k + 2, cy + s / 3, ml);
        break;
    case FS_STICK:
        plat_rect(cx, cy - s, 1, 2 * s + 2, PAL_PAINT(PAINT_CARAMEL, 2));
        for (k = 0; k < 3; k++) {
            ui_fill_circle(cx, cy - s + 2 + k * (s * 2 / 3), s / 3 + 1, md);
            ui_fill_circle(cx, cy - s + 1 + k * (s * 2 / 3), s / 3, m);
        }
        fy = cy;
        break;
    case FS_CLOUD:
        plat_rect(cx, cy, 1, s + 2, PAL_PAINT(PAINT_WHITE, 2));
        ui_fill_circle(cx - s / 2, cy - s / 3, s * 2 / 3, m);
        ui_fill_circle(cx + s / 2, cy - s / 3, s * 2 / 3, a);
        ui_fill_circle(cx, cy - s * 2 / 3, s * 2 / 3, ml);
        fy = cy - s / 2;
        break;
    case FS_CANDY:
        for (k = 0; k < 2 * s; k++) {
            int hw = DOOMO_MIN(s, 1 + k * 2 / 3);
            plat_rect(cx - hw, cy - s + k, 2 * hw + 1, 1, k < 2 ? ml : m);
        }
        for (k = 0; k < 4; k++) plat_rect(cx - s / 2 + k * s / 3, cy - s / 3 + (k & 1) * 3, 1, 1, PAL_WHITE);
        break;
    case FS_BAT:
        ui_fill_ellipse(cx, cy, s / 2 + 1, s / 2, m);
        for (k = 1; k <= s; k++) {
            int hh = s / 2 - k / 3 + ((k & 3) == 0 ? 1 : 0);
            plat_rect(cx - s / 2 - k, cy - hh / 2 - ((t / 6) & 1), 1, hh > 0 ? hh : 1, md);
            plat_rect(cx + s / 2 + k, cy - hh / 2 - ((t / 6) & 1), 1, hh > 0 ? hh : 1, md);
        }
        plat_rect(cx - 2, cy - s / 2 - 1, 1, 2, a);
        plat_rect(cx + 2, cy - s / 2 - 1, 1, 2, a);
        break;
    case FS_WORM:
        for (k = 0; k < 2 * s; k++) {
            int yy = cy + ui_wave(k * 6 + (int)t * 2, 40, s / 3 + 1);
            plat_rect(cx - s + k, yy - 1, 1, 3, (k / 3) & 1 ? m : a);
        }
        fy = cy + ui_wave(s * 6 + (int)t * 2, 40, s / 3 + 1);
        cx += s - 2;
        s = s * 2 / 3;
        break;
    case FS_FRUIT:
        ui_fill_circle(cx, cy, s, md);
        ui_fill_circle(cx - 1, cy - 1, s - 1, m);
        ui_fill_circle(cx - s / 3, cy - s / 3, s / 4 + 1, ml);
        plat_rect(cx - 1, cy - s - 2, 4, 2, a);
        break;
    case FS_PICKLE:
        ui_fill_ellipse(cx, cy, s / 2 + 1, s + 1, md);
        ui_fill_ellipse(cx, cy, s / 2, s, m);
        for (k = -s + 2; k < s; k += 3) plat_rect(cx - s / 4 + (k & 2), cy + k, 1, 1, a);
        break;
    default: break;
    }
    if (s >= 5) face(cx, fy, s * 2 / 3, teeth, t);
}

/* ====================================================================== */
/* fears                                                                    */
void dream_fear(int fear, int cx, int cy, int s, int creep, uint32_t t)
{
    uint8_t dk = creep >= 12 ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_STONE, 1);
    int k, wig = ui_wave((int)t + cx, 16, 1);
    switch ((unsigned)fear % FEAR_COUNT) {
    case 0:     /* spiders: round and fuzzy, eight wiggly legs */
        for (k = 0; k < 4; k++) {
            int ly = cy - s / 2 + k * s / 3;
            plat_line(cx - s / 2, ly, cx - s - 3, ly + (k < 2 ? -3 : 3) + ((k + (int)t / 4) & 1), dk);
            plat_line(cx + s / 2, ly, cx + s + 3, ly + (k < 2 ? -3 : 3) + ((k + (int)t / 4 + 1) & 1), dk);
        }
        ui_fill_circle(cx, cy, s, dk);
        ui_fill_circle(cx - s / 3, cy - s / 3, s / 4, PAL_RAMP(RAMP_STONE, 3));
        face(cx, cy, s / 2 + 1, creep, t);
        if (creep >= 12) for (k = 0; k < 3; k++) plat_rect(cx - s / 2 + k * s / 2, cy - s / 2 - 2, 1, 1, PAL_RAMP(RAMP_BLOOD, 5));
        break;
    case 1:     /* the dark: a shadow blob that peeks */
        ui_fill_ellipse(cx, cy, s + 2, s, PAL_RAMP(RAMP_VAPE, 0));
        ui_fill_ellipse(cx, cy + s / 2, s + 4, s / 2, PAL_RAMP(RAMP_VAPE, 0));
        face(cx, cy - 1, s / 2 + 1, DOOMO_MAX(creep, 10), t);
        break;
    case 2:     /* thunder: a grumpy cloud */
        ui_fill_circle(cx - s / 2, cy, s * 2 / 3, PAL_RAMP(RAMP_STONE, 3));
        ui_fill_circle(cx + s / 2, cy, s * 2 / 3, PAL_RAMP(RAMP_STONE, 3));
        ui_fill_circle(cx, cy - s / 3, s * 3 / 4, PAL_RAMP(RAMP_STONE, 4));
        if (((t + (uint32_t)cx) % 24) < 6) {
            plat_line(cx, cy + s / 2, cx - 3, cy + s + 3, PAL_PAINT(PAINT_LEMON, 2));
            plat_line(cx - 3, cy + s + 3, cx + 2, cy + s + 3, PAL_PAINT(PAINT_LEMON, 2));
            plat_line(cx + 2, cy + s + 3, cx - 1, cy + 2 * s + 3, PAL_PAINT(PAINT_LEMON, 2));
        }
        face(cx, cy - 1, s / 2, creep, t);
        break;
    case 3:     /* clowns */
        ui_fill_circle(cx - s, cy - s / 3, s / 2, PAL_PAINT(PAINT_TANGERINE, 1));
        ui_fill_circle(cx + s, cy - s / 3, s / 2, PAL_PAINT(PAINT_TANGERINE, 1));
        ui_fill_circle(cx, cy, s, PAL_PAINT(PAINT_WHITE, 2));
        face(cx, cy - 1, s / 2 + 1, creep, t);
        ui_fill_circle(cx, cy + 1, 2, PAL_PAINT(PAINT_CHERRY, 1));
        break;
    case 4:     /* ghosts */
        ui_fill_circle(cx, cy - s / 3, s, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx - s, cy - s / 3, 2 * s + 1, s, PAL_PAINT(PAINT_WHITE, 2));
        for (k = -s; k <= s; k += 4) ui_fill_circle(cx + k + 1, cy + s * 2 / 3 + wig, 2, PAL_PAINT(PAINT_WHITE, 2));
        face(cx, cy - s / 3, s / 2 + 1, creep, t);
        break;
    case 5:     /* heights: a wobbly tall ladder */
        plat_rect(cx - s / 2, cy - 2 * s, 1, 3 * s, PAL_RAMP(RAMP_WOOD, 4));
        plat_rect(cx + s / 2, cy - 2 * s, 1, 3 * s, PAL_RAMP(RAMP_WOOD, 4));
        for (k = -2 * s + 2; k < s; k += 4) plat_rect(cx - s / 2, cy + k + wig, s + 1, 1, PAL_RAMP(RAMP_WOOD, 5));
        face(cx, cy - 2 * s - 3, 3, creep, t);
        break;
    case 6:     /* deep water: a wave with a fin */
        for (k = -s - 3; k <= s + 3; k++) {
            int top = cy + ui_wave(k * 8 + (int)t * 3, 60, 2);
            plat_rect(cx + k, top, 1, s, PAL_PAINT(PAINT_COBALT, 1));
            plat_rect(cx + k, top, 1, 1, PAL_PAINT(PAINT_SKY, 2));
        }
        for (k = 0; k < s; k++) plat_rect(cx - k / 2 + (int)(t / 3 % 6) - 3, cy - s + k, k / 2 + 1, 1, PAL_RAMP(RAMP_STEEL, 3));
        break;
    case 7:     /* vacuum cleaners */
        ui_round_rect(cx - s, cy - s / 2, s + s / 2, s + 2, 3, PAL_PAINT(PAINT_CHERRY, 1));
        ui_fill_circle(cx - s / 2, cy + s / 2 + 1, 2, PAL_RAMP(RAMP_STONE, 0));
        ui_fill_circle(cx + s / 3, cy + s / 2 + 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_line(cx + s / 2, cy - s / 2, cx + s + 2, cy - s, PAL_RAMP(RAMP_STONE, 1));
        plat_line(cx + s + 2, cy - s, cx + s + 4, cy + s, PAL_RAMP(RAMP_STONE, 1));
        face(cx - s / 4, cy, s / 3 + 1, creep, t);
        break;
    case 8:     /* bees */
        ui_fill_ellipse(cx - s / 3, cy - s / 2 + wig, s / 2, s / 3, PAL_PAINT(PAINT_WHITE, 2));
        ui_fill_ellipse(cx + s / 3, cy - s / 2 - wig, s / 2, s / 3, PAL_PAINT(PAINT_WHITE, 2));
        ui_fill_ellipse(cx, cy, s, s * 2 / 3, PAL_PAINT(PAINT_LEMON, 1));
        for (k = -s / 2; k <= s / 2; k += 4) plat_rect(cx + k, cy - s / 2, 2, s, PAL_PAINT(PAINT_JET, 1));
        plat_rect(cx + s, cy, 2, 1, PAL_PAINT(PAINT_JET, 1));
        face(cx - s / 3, cy, s / 3 + 1, creep, t);
        break;
    case 9:     /* needles */
        plat_rect(cx - s, cy - s / 3, 2 * s, s * 2 / 3 + 1, PAL_PAINT(PAINT_SKY, 2));
        plat_rect(cx - s, cy - s / 3, s, s * 2 / 3 + 1, PAL_PAINT(PAINT_CHERRY, 1));
        plat_rect(cx + s, cy, s, 1, PAL_RAMP(RAMP_STEEL, 6));
        plat_rect(cx - s - 3, cy - 1, 3, 3, PAL_RAMP(RAMP_STEEL, 4));
        face(cx, cy - 1, s / 3 + 1, creep, t);
        break;
    case 10:    /* mirrors */
        ui_fill_ellipse(cx, cy, s * 2 / 3 + 2, s + 2, PAL_PAINT(PAINT_HONEY, 1));
        ui_fill_ellipse(cx, cy, s * 2 / 3, s, PAL_PAINT(PAINT_SKY, 2));
        plat_line(cx - s / 3, cy - s / 2, cx, cy - s / 2 + 4, PAL_WHITE);
        face(cx, cy, s / 3 + 1, creep, t);
        break;
    case 11:    /* being alone: one sad little balloon */
        plat_line(cx, cy, cx + wig, cy + s + 4, PAL_RAMP(RAMP_STONE, 4));
        ui_fill_ellipse(cx, cy - s / 2, s * 2 / 3, s, PAL_PAINT(PAINT_SKY, 1));
        face(cx, cy - s / 2, s / 3 + 1, creep > 6 ? creep : 0, t);
        break;
    case 12:    /* public speaking: a judging microphone */
        plat_rect(cx - 1, cy, 3, s + 4, PAL_RAMP(RAMP_STEEL, 3));
        ui_fill_circle(cx, cy - s / 3, s * 2 / 3, PAL_RAMP(RAMP_STEEL, 5));
        for (k = -s / 2; k <= s / 2; k += 2) plat_rect(cx - s / 2, cy - s / 3 + k, s, 1, PAL_RAMP(RAMP_STEEL, 3));
        face(cx, cy - s / 3, s / 3 + 1, creep, t);
        break;
    case 13:    /* bath time */
        plat_rect(cx - s - 2, cy, 2 * s + 5, s, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx - s - 2, cy, 2 * s + 5, 1, PAL_PAINT(PAINT_SKY, 1));
        for (k = 0; k < 5; k++) ui_fill_circle(cx - s + k * s / 2, cy - 1 - (k & 1) * 2, 3, PAL_PAINT(PAINT_SKY, 2));
        face(cx, cy + s / 2, s / 3 + 1, creep, t);
        break;
    case 14:    /* dentists: a drill-wielding tooth */
        ui_round_rect(cx - s, cy - s, 2 * s + 1, s * 3 / 2, s / 2, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx - s, cy + s / 2 - 1, s / 2 + 1, s / 2 + 2, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx + s / 2, cy + s / 2 - 1, s / 2 + 1, s / 2 + 2, PAL_PAINT(PAINT_WHITE, 2));
        plat_line(cx + s + 1, cy - s, cx + s + 6, cy - s - 5 + wig, PAL_RAMP(RAMP_STEEL, 5));
        face(cx, cy - s / 3, s / 2, creep, t);
        break;
    case 15:    /* popping balloons */
        plat_line(cx, cy + s, cx, cy + 2 * s, PAL_RAMP(RAMP_STONE, 4));
        if (((t + (uint32_t)cx) % 40) < 34) {
            ui_fill_circle(cx, cy, s, PAL_PAINT(PAINT_CHERRY, 1));
            ui_fill_circle(cx - s / 3, cy - s / 3, s / 4 + 1, PAL_PAINT(PAINT_CHERRY, 2));
            face(cx, cy, s / 2, creep, t);
        } else {
            for (k = 0; k < 8; k++)
                plat_rect(cx + (int)((fx_cos(k * 512) * s) >> 16), cy + (int)((fx_sin(k * 512) * s) >> 16), 2, 2,
                          PAL_PAINT(PAINT_CHERRY, 1));
            ui_text(cx - 9, cy - 3, "POP", PAL_WHITE);
        }
        break;
    case 16:    /* silence: a zipped mouth */
        ui_fill_ellipse(cx, cy, s, s / 2, PAL_PAINT(PAINT_ROSE, 1));
        plat_rect(cx - s + 2, cy, 2 * s - 3, 1, PAL_RAMP(RAMP_STEEL, 5));
        for (k = -s + 3; k < s - 2; k += 2) plat_rect(cx + k, cy - 1, 1, 3, PAL_RAMP(RAMP_STEEL, 3));
        ui_text(cx + s + 2, cy - s, "...", PAL_WHITE);
        break;
    case 17:    /* holy water */
        ui_fill_circle(cx, cy + s / 3, s * 2 / 3 + 1, PAL_PAINT(PAINT_SKY, 1));
        plat_rect(cx - 2, cy - s, 5, s, PAL_PAINT(PAINT_SKY, 2));
        plat_rect(cx - 3, cy - s - 2, 7, 2, PAL_PAINT(PAINT_HONEY, 1));
        plat_rect(cx, cy, 1, s / 2 + 2, PAL_WHITE);
        plat_rect(cx - 2, cy + 2, 5, 1, PAL_WHITE);
        break;
    case 18:    /* sunlight */
        for (k = 0; k < 8; k++) {
            int a = k * 512 + (int)t * 8;
            plat_line(cx + (int)((fx_cos(a) * (s + 2)) >> 16), cy + (int)((fx_sin(a) * (s + 2)) >> 16),
                      cx + (int)((fx_cos(a) * (s + 6)) >> 16), cy + (int)((fx_sin(a) * (s + 6)) >> 16),
                      PAL_PAINT(PAINT_LEMON, 2));
        }
        ui_fill_circle(cx, cy, s, PAL_PAINT(PAINT_LEMON, 2));
        plat_rect(cx - s / 2 - 2, cy - 2, s + 5, 3, PAL_PAINT(PAINT_JET, 1));   /* shades */
        face(cx, cy + 1, s / 3 + 1, creep, t);
        break;
    case 19:    /* church bells */
        for (k = 0; k < s + 2; k++) {
            int hw = s / 2 + k * s / (2 * (s + 2)) + (k > s ? 2 : 0);
            plat_rect(cx - hw + wig, cy - s + k, 2 * hw + 1, 1, PAL_PAINT(PAINT_HONEY, k < 2 ? 2 : 1));
        }
        ui_fill_circle(cx + wig * 2, cy + 3, 2, PAL_PAINT(PAINT_HONEY, 0));
        face(cx + wig, cy - s / 3, s / 3 + 1, creep, t);
        break;
    case 20:    /* exorcisms: the holy book and a plush cross */
        plat_rect(cx - s, cy - s / 2, 2 * s + 1, s + 2, PAL_PAINT(PAINT_CHERRY, 0));
        plat_rect(cx - s + 1, cy - s / 2 + 1, s - 1, s, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx + 1, cy - s / 2 + 1, s - 1, s, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx - 1, cy - s - 4, 3, s, PAL_PAINT(PAINT_HONEY, 2));
        plat_rect(cx - 3, cy - s - 1, 7, 2, PAL_PAINT(PAINT_HONEY, 2));
        break;
    case 21:    /* hugs: arms wide open... too wide */
        plat_line(cx - s, cy, cx - 2 * s, cy - s - wig, PAL_PAINT(PAINT_PEACH, 1));
        plat_line(cx + s, cy, cx + 2 * s, cy - s + wig, PAL_PAINT(PAINT_PEACH, 1));
        ui_heart(cx, cy, s, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        face(cx, cy - 1, s / 3 + 1, creep, t);
        break;
    case 22:    /* cute things: a kitten. Too cute. */
        ui_fill_circle(cx, cy, s, PAL_PAINT(PAINT_PEACH, 2));
        for (k = 0; k < s / 2 + 1; k++) {
            plat_rect(cx - s + 1, cy - s - k + s / 2, 1 + k, 1, PAL_PAINT(PAINT_PEACH, 2));
            plat_rect(cx + s - k, cy - s - k + s / 2, 1 + k, 1, PAL_PAINT(PAINT_PEACH, 2));
        }
        face(cx, cy, s / 2, creep, t);
        ui_sparkle(cx + s + 2, cy - s, (int)(t / 2) & 15, PAL_PAINT(PAINT_LEMON, 2));
        break;
    default:    /* tax season: a form with teeth */
        plat_rect(cx - s, cy - s - 2, 2 * s, 2 * s + 4, PAL_PAINT(PAINT_WHITE, 2));
        for (k = 0; k < 4; k++) plat_rect(cx - s + 2, cy - s + 2 + k * 4, s + s / 2, 1, PAL_RAMP(RAMP_STONE, 4));
        ui_text(cx + s / 3, cy + s / 3 - 3, "$", PAL_PAINT(PAINT_FOREST, 1));
        face(cx - s / 3, cy, s / 3 + 1, creep, t);
        break;
    }
}

/* ====================================================================== */
/* sky + frame                                                              */
/* sky colour of band i (0 top .. 5) for a creep tier */
static uint8_t sky_col(int i, int tier, int nightmare)
{
    static const uint8_t cute[6] = { RAMP_PASTEL_PINK, RAMP_PASTEL_PINK, RAMP_VAPE, RAMP_VAPE, RAMP_SKY, RAMP_SKY };
    if (tier == 0) return PAL_RAMP(cute[i], 7 - (i & 1));
    if (tier == 1) return PAL_RAMP(RAMP_VAPE, 6 - i / 2 - (i & 1));
    return nightmare ? PAL_RAMP(RAMP_BLOOD, 3 - i / 2) : PAL_RAMP(RAMP_VAPE, 3 - i / 2);
}

void dream_draw_sky(int creep, int nightmare, uint32_t t)
{
    int sw = SW(), sh = SH(), i, x, band = sh / 6 + 1;
    int tier = creep < 6 ? 0 : creep < 12 ? 1 : 2;
    /* bands with soft wavy borders, like a lava lamp */
    plat_rect(0, 0, sw, band + 8, sky_col(0, tier, nightmare));
    for (i = 1; i < 6; i++) {
        uint8_t c = sky_col(i, tier, nightmare);
        for (x = 0; x < sw; x += 8) {
            int y0 = i * band + ui_wave((int)t + x * 2 + i * 40, 160, 3 + tier);
            plat_rect(x, y0, 8, (i == 5 ? sh : (i + 1) * band + 6) - y0, c);
            cspan(x, x + 8, y0 - 2, 2, c);
        }
    }
    /* drifting puffs */
    for (i = 0; i < 5; i++) {
        int px = (int)((h2((uint32_t)i, 17u) % (uint32_t)(sw + 60) + t / (2 + (uint32_t)i % 3)) % (uint32_t)(sw + 60)) - 30;
        int py = 18 + (int)(h2((uint32_t)i, 19u) % (uint32_t)(sh / 2));
        uint8_t pc = tier == 0 ? PAL_PAINT(PAINT_WHITE, 2) : tier == 1 ? PAL_RAMP(RAMP_VAPE, 6) :
                     nightmare ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 1);
        ui_fill_ellipse(px, py, 14 + (i & 1) * 6, 4, pc);
        ui_fill_ellipse(px - 6, py - 3, 7, 4, pc);
        ui_fill_ellipse(px + 5, py - 4, 8, 5, pc);
    }
    /* stars, or eyes in the sky */
    for (i = 0; i < 14; i++) {
        uint32_t r = h2((uint32_t)i, 41u);
        int sx = (int)(r % (uint32_t)sw), sy = (int)((r >> 9) % (uint32_t)(sh * 2 / 3));
        if (creep < 10) ui_sparkle(sx, sy, (int)(t / 3 + r) & 15, tier ? PAL_PAINT(PAINT_LEMON, 2) : PAL_WHITE);
        else if ((i & 1) == 0) {
            int open = ((t / 2 + r) % 70) > 5;
            ui_fill_ellipse(sx, sy, 4, open ? 2 : 0, PAL_WHITE);
            if (open) plat_rect(sx - 1 + (int)((t / 20) % 3) - 1, sy - 1, 2, 2, nightmare ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_BLACK);
        }
    }
}

/* the dream floor: a surreal checkerboard in perspective, from y0 down */
static void dream_floor(int y0, int creep, int nightmare, uint32_t t, uint8_t c0, uint8_t c1)
{
    int sw = SW(), sh = SH(), y, row = 0, scroll = (int)(t / 2);
    if (creep >= 12) { c0 = nightmare ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 1); c1 = PAL_RAMP(RAMP_STONE, 0); }
    else if (creep >= 6) { c0 = PAL_RAMP(RAMP_VAPE, 4); c1 = PAL_RAMP(RAMP_VAPE, 2); }
    for (y = y0; y < sh; row++) {
        int h = 2 + row, tw = 10 + row * 6, x, k = 0;
        int off = (scroll * (row + 1) / 3) % (2 * tw);
        for (x = sw / 2 - off - tw * ((sw / 2) / tw + 2); x < sw; x += tw, k++)
            plat_rect(x, y, tw, h, ((k + row) & 1) ? c0 : c1);
        y += h;
    }
    plat_rect(0, y0, sw, 1, ui_lighter(c0));
}

void dream_draw_frame(int creep, int nightmare, uint32_t t)
{
    int sw = SW(), sh = SH(), x, r = BIG() ? 12 : 8, step = r * 12 / 5, band = r / 2;
    uint8_t c = creep < 8 ? PAL_PAINT(PAINT_WHITE, 2) : creep < 13 ? PAL_RAMP(RAMP_VAPE, 6) :
                nightmare ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_VAPE, 2);
    uint8_t ol = creep < 8 ? PAL_RAMP(RAMP_VAPE, 5) : creep < 13 ? PAL_RAMP(RAMP_VAPE, 3) :
                 nightmare ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 0);
    /* a puffy thought-bubble rim: straight bands + a few bumps, outline
     * pass then fill pass (~70 circles: cheap enough for the GBA) */
    int pass;
    for (pass = 0; pass < 2; pass++) {
        uint8_t col = pass ? c : ol;
        int g = pass ? 0 : 1;
        plat_rect(0, 0, sw, band + g, col);
        plat_rect(0, sh - band - g, sw, band + g, col);
        plat_rect(0, 0, band + g, sh, col);
        plat_rect(sw - band - g, 0, band + g, sh, col);
        for (x = step / 2 - r; x < sw + r; x += step) {
            int w1 = ui_wave((int)t + x, 50, 2);
            ui_fill_circle(x, band - 2 + w1, r / 2 + 2 + g, col);
            ui_fill_circle(x + step / 2, sh - band + 1 - w1, r / 2 + 2 + g, col);
        }
        for (x = step; x < sh - step / 2; x += step) {
            int w1 = ui_wave((int)t + x * 2, 60, 2);
            ui_fill_circle(band - 2 + w1, x, r / 2 + 2 + g, col);
            ui_fill_circle(sw - band + 1 - w1, x + step / 2, r / 2 + 2 + g, col);
        }
    }
    if (creep >= 13) {
        /* thorns creep in from the sides */
        for (x = 0; x < 6; x++) {
            plat_line(4, sh / 3 + x * 8, 10 + x * 2, sh / 3 + x * 8 + 3, PAL_RAMP(RAMP_STONE, 0));
            plat_line(sw - 5, sh / 3 + x * 8, sw - 11 - x * 2, sh / 3 + x * 8 + 3, PAL_RAMP(RAMP_STONE, 0));
        }
    }
}

/* ====================================================================== */
/* the bedroom                                                              */
void dream_draw_bedroom(int id, const DreamCast *c, int expr, uint32_t t, int shake)
{
    int sw = SW(), sh = SH(), big = BIG(), fy = sh * 70 / 100, x, y, k;
    int edgy = c->edgy;
    uint8_t wall = edgy ? PAL_RAMP(RAMP_VAPE, 1) : PAL_RAMP(RAMP_SKY, 1);
    uint8_t wall2 = edgy ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_SKY, 2);
    uint8_t quilt = PAL_PAINT(c->color, 1), quilt_d = PAL_PAINT(c->color, 0), quilt_l = PAL_PAINT(c->color, 2);
    int bx = sw * 58 / 100, bw = big ? 150 : 112, by = fy - (big ? 30 : 22);
    int sx = shake && opt_get(OPT_SCREEN_SHAKE) ? (int)(t & 1) * 2 - 1 : 0;
    /* walls: wallpaper stripes with little stars / skulls */
    plat_rect(0, 0, sw, fy, wall);
    for (x = 8; x < sw; x += 16) plat_rect(x, 0, 4, fy, wall2);
    for (y = 10; y < fy - 10; y += 22)
        for (x = (y / 22 & 1) * 8 + 4; x < sw; x += 16)
            plat_rect(x, y, 2, 2, edgy ? PAL_RAMP(RAMP_BONE, 3) : PAL_RAMP(RAMP_SKY, 4));
    /* floor */
    plat_rect(0, fy, sw, sh - fy, PAL_RAMP(RAMP_WOOD, 1));
    for (y = fy + 6; y < sh; y += 7) plat_rect(0, y, sw, 1, PAL_RAMP(RAMP_WOOD, 0));
    plat_rect(0, fy, sw, 2, PAL_RAMP(RAMP_WOOD, 2));
    /* the window: night sky, moon, twinkles, curtains */
    {
        int wx = big ? 22 : 12, wy = big ? 26 : 16, ww = big ? 76 : 56, wh = big ? 70 : 52;
        plat_rect(wx - 3, wy - 3, ww + 6, wh + 6, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(wx, wy, ww, wh, PAL_RAMP(RAMP_SKY, 0));
        ui_fill_circle(wx + ww * 2 / 3, wy + wh / 3, big ? 9 : 7, PAL_PAINT(PAINT_LEMON, 2));
        ui_fill_circle(wx + ww * 2 / 3 + 4, wy + wh / 3 - 3, big ? 8 : 6, PAL_RAMP(RAMP_SKY, 0));
        for (k = 0; k < 5; k++)
            ui_sparkle(wx + 6 + (int)(h2((uint32_t)k, 5u) % (uint32_t)(ww - 12)), wy + 5 + (int)(h2((uint32_t)k, 9u) % (uint32_t)(wh - 10)),
                       (int)(t / 4 + (uint32_t)k * 3) & 15, PAL_WHITE);
        plat_rect(wx + ww / 2 - 1, wy, 2, wh, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(wx, wy + wh / 2 - 1, ww, 2, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(wx - 8, wy - 6, 12, wh + 18, quilt_d);
        plat_rect(wx + ww - 4, wy - 6, 12, wh + 18, quilt_d);
        for (k = 0; k < 2; k++) plat_rect(wx - 6 + k * (ww + 4), wy - 6, 2, wh + 18, quilt);
        plat_rect(wx - 10, wy - 8, ww + 20, 3, PAL_RAMP(RAMP_WOOD, 4));
    }
    /* night stand with a little lamp */
    {
        int nx = bx - bw / 2 - (big ? 34 : 26), ny = fy - (big ? 26 : 20);
        plat_rect(nx, ny, big ? 26 : 20, fy - ny + 2, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(nx, ny, big ? 26 : 20, 2, PAL_RAMP(RAMP_WOOD, 5));
        ui_fill_ellipse(nx + (big ? 13 : 10), ny - 10, big ? 9 : 7, big ? 6 : 5, PAL_PAINT(PAINT_LEMON, 2));
        plat_rect(nx + (big ? 12 : 9), ny - 5, 2, 5, PAL_RAMP(RAMP_STEEL, 4));
        glow(nx + (big ? 13 : 10), ny - 10, big ? 30 : 24, big ? 20 : 16, PAL_PAINT(PAINT_LEMON, 2));
    }
    /* the bed */
    plat_rect(bx - bw / 2 + sx, by - (big ? 36 : 28), big ? 10 : 8, fy - by + (big ? 38 : 30), PAL_RAMP(RAMP_WOOD, 3));
    ui_fill_circle(bx - bw / 2 + (big ? 5 : 4) + sx, by - (big ? 36 : 28), big ? 6 : 5, PAL_RAMP(RAMP_WOOD, 4));
    if (edgy) {
        plat_line(bx - bw / 2 + sx, by - (big ? 40 : 32), bx - bw / 2 - 5 + sx, by - (big ? 48 : 38), PAL_RAMP(RAMP_BLOOD, 3));
        plat_line(bx - bw / 2 + 8 + sx, by - (big ? 40 : 32), bx - bw / 2 + 13 + sx, by - (big ? 48 : 38), PAL_RAMP(RAMP_BLOOD, 3));
    }
    plat_rect(bx + bw / 2 - (big ? 8 : 6) + sx, by - 6, big ? 8 : 6, fy - by + 8, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(bx - bw / 2 + sx, by + 4, bw, big ? 14 : 10, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(bx - bw / 2 + 4 + sx, by, bw - 8, 6, PAL_PAINT(PAINT_WHITE, 1));
    /* pillow + the sleeper's head */
    ui_fill_ellipse(bx - bw / 2 + (big ? 30 : 24) + sx, by - 4, big ? 20 : 16, big ? 8 : 6, PAL_PAINT(PAINT_WHITE, 2));
    if (id >= 0) {
        const DoomoImage *img = portrait_image(id, expr);
        if (img) plat_image(bx - bw / 2 + (big ? 30 : 24) - img->w / 2 + sx,
                            by - img->h + (big ? 20 : 18) + ui_wave((int)t, 90, 1), img, 1, 0, 0);
    }
    /* the quilt, tucked up to the chin, rising and falling */
    {
        int qy = by + (big ? 4 : 6) + ui_wave((int)t, 90, 1), qx = bx - bw / 2 + (big ? 8 : 6) + sx, qw = bw - (big ? 14 : 12);
        ui_round_rect(qx, qy, qw, fy - qy + 2, 4, quilt_d);
        ui_round_rect(qx + 1, qy + 1, qw - 2, fy - qy - 2, 3, quilt);
        for (x = qx + 6; x < qx + qw - 4; x += 10)
            for (y = qy + 4; y < fy - 4; y += 9)
                plat_rect(x, y, 3, 3, ((x + y) / 9 & 1) ? quilt_l : PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(qx + 2, qy + 1, qw - 4, 1, quilt_l);
        /* a teddy keeps watch */
        {
            int tx = qx + qw - (big ? 22 : 16), ty = qy - (big ? 6 : 4);
            uint8_t tc = edgy ? PAL_PAINT(PAINT_GRAPE, 1) : PAL_PAINT(PAINT_CARAMEL, 1);
            ui_fill_circle(tx - 4, ty - 6, 2, tc);
            ui_fill_circle(tx + 4, ty - 6, 2, tc);
            ui_fill_circle(tx, ty - 2, big ? 6 : 5, tc);
            ui_fill_circle(tx, ty + 6, big ? 6 : 5, tc);
            plat_rect(tx - 2, ty - 3, 1, 1, PAL_BLACK);
            plat_rect(tx + 2, ty - 3, 1, 1, PAL_BLACK);
            plat_rect(tx - 1, ty, 3, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        }
    }
    /* Zzz drifting up */
    if (expr == EXPR_SLEEPY) {
        for (k = 0; k < 3; k++) {
            int ph = (int)((t + (uint32_t)k * 30) % 90);
            int zx = bx - bw / 2 + (big ? 44 : 36) + ph / 4 + ui_wave(ph, 30, 2);
            int zy = by - (big ? 40 : 32) - ph / 2;
            ui_text_outline(zx, zy, k == 1 ? "Z" : "z", PAL_WHITE, PAL_RAMP(RAMP_SKY, 0));
        }
    }
    /* night-time hush: a soft vignette */
    cspan(0, sw, 0, 4, PAL_RAMP(RAMP_SKY, 0));
}

void dream_draw_bubble_in(int id, int t, int total, uint32_t clock)
{
    int sw = SW(), sh = SH(), big = BIG(), k;
    int hx = sw * 58 / 100 - (big ? 75 : 56) + (big ? 30 : 24), hy = sh * 70 / 100 - (big ? 30 : 22) - 30;
    (void)id;
    /* three little bubbles pop up, then the big one swells */
    for (k = 0; k < 3; k++) {
        int appear = k * 6;
        if (t > appear)
            ui_fill_circle(hx + 8 + k * 9, hy - k * 10, 2 + k * 2, PAL_PAINT(PAINT_WHITE, 2));
    }
    if (t > 18) {
        int p = (t - 18) * 256 / (total - 18), r = 8 + (sw + sh) * p / 256 * p / 256;
        int cx = hx + 40 + (sw / 2 - hx - 40) * p / 256, cy = hy - 34 + (sh / 2 - hy + 34) * p / 256;
        ui_fill_circle(cx, cy, r + 2, PAL_RAMP(RAMP_VAPE, 4));
        ui_fill_circle(cx, cy, r, PAL_RAMP(RAMP_PASTEL_PINK, 7));
        /* a glimpse of the dream inside */
        if (r > 16) {
            ui_fill_ellipse(cx - r / 3, cy + r / 4, r / 3, r / 8 + 1, PAL_PAINT(PAINT_WHITE, 2));
            ui_fill_ellipse(cx + r / 3, cy - r / 4, r / 4, r / 10 + 1, PAL_PAINT(PAINT_WHITE, 2));
            ui_fill_circle(cx - r / 3, cy - r / 3, r / 10 + 1, PAL_RAMP(RAMP_VAPE, 6));
        }
        for (k = 0; k < 6; k++)
            ui_sparkle(cx + (int)((fx_cos(k * 682 + t * 40) * r) >> 17), cy + (int)((fx_sin(k * 682 + t * 40) * r) >> 17),
                       (int)(clock / 2 + (uint32_t)k * 3) & 15, PAL_WHITE);
    }
}

/* ====================================================================== */
/* the vignettes                                                            */
static int expr_for(int creep) { return creep >= 10 ? EXPR_SHOCK : EXPR_HAPPY; }

static void portrait_at(int id, int cx, int cy, int expr, int flags, uint8_t col)
{
    const DoomoImage *img = id >= 0 ? portrait_image(id, expr) : NULL;
    if (img) plat_image(cx - img->w / 2, cy - img->h / 2, img, 1, flags, col);
}

static void chibi_at(int id, int cx, int feet, int scale, int dir, int frame, int flags, uint8_t col)
{
    const DoomoImage *img = id >= 0 ? bodyspr_image(id, dir, frame) : NULL;
    if (img) plat_image(cx - img->w * scale / 2, feet - img->h * scale, img, scale, flags, col);
}

static void sprite_at(int spr, int frame, int cx, int feet, int scale, int flags, uint8_t col)
{
    const DoomoImage *img = sprite_image(spr, frame);
    if (img) plat_image(cx - img->w * scale / 2, feet - img->h * scale, img, scale, flags, col);
}

static void cloud(int cx, int cy, int r, uint8_t c, uint8_t c2)
{
    ui_fill_circle(cx - r, cy + r / 3, r * 2 / 3, c2);
    ui_fill_circle(cx + r, cy + r / 3, r * 2 / 3, c2);
    ui_fill_circle(cx, cy, r, c);
    ui_fill_circle(cx - r, cy + r / 4, r * 2 / 3 - 1, c);
    ui_fill_circle(cx + r, cy + r / 4, r * 2 / 3 - 1, c);
}

static void party_hat(int cx, int top, int h, uint8_t c)
{
    int k;
    for (k = 0; k < h; k++) {
        int hw = k / 2;
        plat_rect(cx - hw, top + k, 2 * hw + 1, 1, (k / 2) & 1 ? c : PAL_PAINT(PAINT_WHITE, 2));
    }
    ui_fill_circle(cx, top - 1, 1, PAL_PAINT(PAINT_LEMON, 2));
}

void dream_draw_vignette(int v, const DreamCast *c, int t, int creep, int nightmare)
{
    int sw = SW(), sh = SH(), big = BIG(), cx = sw / 2, k;
    int ground = sh - (big ? 46 : 36);                 /* above the caption */
    int sc = big ? 2 : 1;
    uint32_t tt = (uint32_t)t;
    int shake = nightmare && creep >= 12 && opt_get(OPT_SCREEN_SHAKE) ? (int)(tt & 2) - 1 : 0;
    /* most dreams stand on a surreal checkerboard floor */
    if ((0xB38Bu >> v) & 1u)    /* vignettes 0,1,3,7,8,9,12,13,15 */
        dream_floor(ground - (big ? 16 : 12), creep, nightmare, tt, PAL_PAINT(c->color, 2), PAL_PAINT(PAINT_WHITE, 2));
    cx += shake;
    switch (v) {
    case 0: {   /* snack rain */
        for (k = 0; k < (big ? 10 : 7); k++) {
            uint32_t r = h2((uint32_t)k, 3u);
            int x = 16 + k * (sw - 32) / (big ? 9 : 6) + (int)(r % 9u) - 4;
            int sp = 1 + (int)((r >> 8) % 2) + creep / 6;
            int y = (int)(((r >> 12) % (uint32_t)sh + tt * (uint32_t)sp) % (uint32_t)(ground + 30)) - 15;
            dream_food(c->food, x + ui_wave(t + k * 9, 40, 2), y, (big ? 11 : 8) + creep / 4, creep, tt);
        }
        portrait_at(c->dreamer, cx, ground - (big ? 22 : 16), expr_for(creep), 0, 0);
        break; }
    case 1: {   /* the giant plush */
        int who = c->crush >= 0 ? c->crush : c->friend_id >= 0 ? c->friend_id : c->dreamer;
        const DoomoResident *r = town_res(who);
        const DoomoImage *img = r ? figure_image(&r->look, RES_ALIVE, creep >= 10 ? EXPR_SHOCK : EXPR_LOVE) : NULL;
        int gx = cx + ui_wave(t, 80, 2), gy = ground + (big ? 30 : 34);
        for (k = 0; k < 6; k++)
            ui_heart(gx + ui_wave(t + k * 30, 120, sw / 3), ground - (int)((tt * 2 + (uint32_t)k * 31) % (uint32_t)ground),
                     3 + (k & 1), creep >= 12 ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_BUBBLEGUM, 1));
        if (img) {
            int s2 = 2, x0 = gx - img->w * s2 / 2, y0 = gy - img->h * s2;
            plat_image(x0, y0, img, s2, IMG_SHADOW, 0);
            /* stitches; with the creep, stuffing pops out of the seams */
            for (k = 0; k < 6; k++) {
                int sx = x0 + img->w * s2 / 4 + (k % 3) * img->w * s2 / 4, sy = y0 + img->h * s2 / 3 + (k / 3) * img->h * s2 / 4;
                plat_line(sx - 2, sy - 2, sx + 2, sy + 2, PAL_RAMP(RAMP_STONE, 1));
                plat_line(sx + 2, sy - 2, sx - 2, sy + 2, PAL_RAMP(RAMP_STONE, 1));
                if (creep > 4 + k * 2) ui_fill_circle(sx + 3, sy, 2 + creep / 6, PAL_PAINT(PAINT_WHITE, 2));
            }
        }
        chibi_at(c->dreamer, cx + (big ? 70 : 58), ground + 6, sc, 1, 0, 0, 0);
        break; }
    case 2: {   /* falling up */
        int py = ground - (t * (ground - 30)) / 180;
        for (k = 0; k < 10; k++) {
            int x = (int)(h2((uint32_t)k, 8u) % (uint32_t)sw), y = (int)((h2((uint32_t)k, 9u) + tt * 6) % (uint32_t)sh);
            plat_rect(x, y, 1, 8, PAL_PAINT(PAINT_WHITE, 2));
        }
        for (k = 0; k < 6; k++)
            plat_rect(k * sw / 6 + 4, ground + 10 + t / 4, sw / 8, 20, PAL_RAMP(RAMP_STONE, 2));
        for (k = 0; k < 7; k++)
            ui_fill_circle(cx - sw / 3 + k * sw / 9, (big ? 34 : 24) + ui_wave(t + k * 12, 50, 3),
                           (big ? 22 : 16) + (k & 1) * 4, creep >= 8 ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 5));
        if (creep >= 8) {
            face(cx, big ? 34 : 24, big ? 14 : 10, creep, tt);
            face(cx - sw / 4, big ? 30 : 22, 4, creep, tt);
        }
        portrait_at(c->dreamer, cx + ui_wave(t, 60, 6), py, expr_for(creep), 0, 0);
        break; }
    case 3: {   /* fear parade */
        int n = big ? 6 : 5, sz = (big ? 9 : 7) + creep / 3;
        for (k = 0; k < n; k++) {
            int x = (int)((tt * (1 + (uint32_t)creep / 8) + (uint32_t)k * (sw + 40) / n) % (uint32_t)(sw + 40)) - 20;
            int y = ground - sz - 6 - (((t / 4 + k) & 1) ? 3 : 0);
            dream_fear(c->fear, x, y, sz, creep, tt + (uint32_t)k * 13);
            party_hat(x, y - sz - 8, 8, PAL_PAINT(k & 1 ? PAINT_SKY : PAINT_BUBBLEGUM, 1));
        }
        concert_confetti(8, 8, sw - 16, ground - 20, tt, 24, 1);
        chibi_at(c->dreamer, cx, ground + 2 - (big ? 50 : 36), sc, 0, 0, 0, 0);
        ui_text_center(cx, big ? 20 : 14, creep >= 10 ? "PARADE...?" : "PARADE!", creep >= 10 ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_PAINT(PAINT_LEMON, 2));
        break; }
    case 4: {   /* picnic */
        int bw = big ? 160 : 120, by = ground - 6;
        ui_fill_ellipse(cx, ground + 30, sw, 40, PAL_RAMP(RAMP_GRASS, creep >= 12 ? 1 : 4));
        for (k = 0; k < 8; k++) {
            int x = cx - bw / 2 + k * bw / 8;
            plat_rect(x, by, bw / 8, 6, (k & 1) ? PAL_PAINT(c->color, 1) : PAL_PAINT(PAINT_WHITE, 2));
            plat_rect(x, by + 6, bw / 8, 5, (k & 1) ? PAL_PAINT(PAINT_WHITE, 2) : PAL_PAINT(c->color, 1));
        }
        chibi_at(c->dreamer, cx - bw / 3, by + 4, sc, 2, 0, 0, 0);
        if (c->friend_id >= 0) chibi_at(c->friend_id, cx + bw / 3, by + 4, sc, 1, 0, 0, 0);
        dream_food(c->food, cx, by - (big ? 8 : 6), big ? 9 : 7, creep / 2, tt);
        /* the ants are imps */
        if (creep > 2) {
            for (k = 0; k < 4; k++) {
                int x = sw + 10 - (creep * (sw / 2 + 20)) / 16 + k * (big ? 20 : 14);
                sprite_at(SPR_IMPLING, (t / 6 + k) & 1, x, by + 12, 1, 0, 0);
            }
        }
        break; }
    case 5: {   /* starry hobby on the moon */
        int my = ground - (big ? 40 : 30);
        ui_fill_circle(cx, my, big ? 30 : 22, PAL_PAINT(PAINT_LEMON, 2));
        ui_fill_circle(cx + (big ? 12 : 9), my - (big ? 12 : 9), big ? 28 : 21, creep >= 12 ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 5));
        portrait_at(c->dreamer, cx - (big ? 8 : 6), my - (big ? 14 : 12) + ui_wave(t, 70, 2), expr_for(creep), 0, 0);
        for (k = 0; k < 8; k++) {
            int a = k * 512 + t * 6, r = big ? 70 : 52;
            int x = cx + (int)((fx_cos(a) * r) >> 16), y = my - 10 + (int)((fx_sin(a) * r * 2 / 3) >> 16);
            if (creep < 10) ui_star(x, y, 3 + (k & 1), PAL_PAINT(PAINT_LEMON, 2));
            else {
                ui_fill_ellipse(x, y, 5, 3, PAL_WHITE);
                plat_rect(x + (cx > x ? 1 : -2), y - 1, 2, 2, nightmare ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_BLACK);
            }
        }
        concert_note_glyph(cx + (big ? 30 : 22), my - 30 - (t / 3) % 20, PAL_WHITE);
        break; }
    case 6: {   /* cloud concert for sheep */
        cloud(cx, ground - (big ? 30 : 22), big ? 26 : 20, PAL_PAINT(PAINT_WHITE, 2), PAL_RAMP(RAMP_VAPE, 6));
        chibi_at(c->dreamer, cx, ground - (big ? 34 : 26), sc, 0, 0, 0, 0);
        for (k = 0; k < 3; k++) concert_note_glyph(cx - 20 + k * 18, ground - (big ? 80 : 60) - (int)((tt + (uint32_t)k * 9) % 24),
                                                    PAL_PAINT(PAINT_LEMON, 2));
        for (k = 0; k < (big ? 7 : 5); k++) {
            int x = (k + 1) * sw / ((big ? 7 : 5) + 1), y = ground + 4 - (((t / 5) + k) & 1) * 2;
            uint8_t wool = creep >= 12 ? PAL_RAMP(RAMP_STONE, 0) : PAL_PAINT(PAINT_WHITE, 2);
            ui_fill_circle(x - 4, y, 5, wool);
            ui_fill_circle(x + 4, y, 5, wool);
            ui_fill_circle(x, y - 3, 6, wool);
            ui_fill_ellipse(x, y + 1, 3, 4, PAL_RAMP(RAMP_STONE, 1));
            if (creep >= 10) {
                int e;
                for (e = 0; e < 1 + creep / 5; e++) plat_rect(x - 3 + e * 2, y - 2 + (e & 1) * 2, 1, 1, PAL_RAMP(RAMP_BLOOD, 5));
            } else {
                plat_rect(x - 2, y, 1, 1, PAL_WHITE);
                plat_rect(x + 1, y, 1, 1, PAL_WHITE);
            }
        }
        break; }
    case 7: {   /* wobbly teeth */
        int mw = (big ? 70 : 52) + creep, mh = (big ? 30 : 22) + creep / 2, my = ground - mh - 10;
        ui_fill_ellipse(cx, my, mw + 4, mh + 4, PAL_PAINT(PAINT_ROSE, 1));
        ui_fill_ellipse(cx, my, mw, mh, PAL_RAMP(RAMP_BLOOD, 0));
        ui_fill_ellipse(cx, my + mh / 2, mw / 2, mh / 3, PAL_PAINT(PAINT_ROSE, 2));
        for (k = -mw + 8; k < mw - 8; k += 10) {
            int wob = ui_wave(t + k, 12, 1), rows = 1 + creep / 6, r;
            for (r = 0; r < rows; r++) {
                int th = 6 + (creep >= 8 ? 3 : 0);
                plat_rect(cx + k + wob, my - mh + 2 + r * 8, 7, th, PAL_WHITE);
                if (creep >= 8) plat_rect(cx + k + 3 + wob, my - mh + 2 + r * 8 + th, 1, 2, PAL_WHITE);
            }
        }
        for (k = 0; k < 4; k++) {
            int fy2 = my + (int)((tt * 2 + (uint32_t)k * 25) % (uint32_t)(ground - my + 10));
            if ((k & 1) == 0) plat_rect(cx - 30 + k * 20, fy2, 6, 7, PAL_WHITE);
            else dream_food(c->food, cx - 30 + k * 20, fy2, 5, creep, tt);
        }
        break; }
    case 8: {   /* hell university */
        int bw = big ? 170 : 130, bh = big ? 60 : 44, bx = cx - bw / 2, by = big ? 22 : 14;
        int cxk = sw - (big ? 46 : 34), cyk = big ? 56 : 42, a;
        plat_rect(bx - 3, by - 3, bw + 6, bh + 6, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(bx, by, bw, bh, creep >= 10 ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_GRASS, 0));
        ui_text(bx + 6, by + 5, creep >= 10 ? "WHAT IS YOUR" : "POP QUIZ!", creep >= 10 ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_WHITE);
        ui_text(bx + 6, by + 16, creep >= 10 ? "TRUE NAME?" : "1. 2+2 = fish", creep >= 10 ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_RAMP(RAMP_STONE, 6));
        if (creep < 10) ui_text(bx + 6, by + 27, "2. spell \"BOO\"", PAL_RAMP(RAMP_STONE, 6));
        /* the clock spins (and melts) */
        ui_fill_ellipse(cxk, cyk + creep / 2, 12, 12 + creep / 2, PAL_PAINT(PAINT_WHITE, 2));
        a = t * (16 + creep * 8);
        plat_line(cxk, cyk, cxk + (int)((fx_cos(a) * 9) >> 16), cyk + (int)((fx_sin(a) * 9) >> 16), PAL_BLACK);
        plat_line(cxk, cyk, cxk + (int)((fx_cos(a / 12) * 6) >> 16), cyk + (int)((fx_sin(a / 12) * 6) >> 16), PAL_BLACK);
        /* desk + paper */
        plat_rect(cx - 40, ground - 8, 80, 6, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(cx - 36, ground - 2, 3, 14, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx + 33, ground - 2, 3, 14, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx - 12, ground - 12, 24, 5, PAL_PAINT(PAINT_WHITE, 2));
        if (creep >= 12) for (k = 0; k < 4; k++) ui_fill_circle(cx - 9 + k * 6, ground - 14 - (int)((tt + (uint32_t)k * 3) % 6), 3 - (k & 1), PAL_RAMP(RAMP_LAVA, 4 + (k & 1)));
        chibi_at(c->dreamer, cx + 50, ground + 8, sc, 1, 0, 0, 0);
        break; }
    case 9: {   /* the big spoon */
        int run = (t / 4) & 3, dx = cx + (big ? 40 : 30), gap = (big ? 90 : 70) - creep * 3;
        for (k = 0; k < sw; k += 12) plat_rect((k - t * 4) % (sw + 12) + sw, ground + 8, 6, 2, PAL_RAMP(RAMP_GRASS, 2));
        for (k = 0; k < sw / 12 + 2; k++) plat_rect(((k * 12 - t * 4) % (sw + 12) + sw + 12) % (sw + 12) - 6, ground + 8, 6, 2,
                                                    PAL_RAMP(RAMP_GRASS, 3));
        chibi_at(c->dreamer, dx, ground + 8 - (run & 1), sc, 2, run, 0, 0);
        if (c->rival >= 0) {
            chibi_at(c->rival, dx - gap, ground + 8 - ((run + 1) & 1), sc, 2, run, creep >= 12 ? IMG_SILHOUETTE : 0,
                     PAL_RAMP(RAMP_BLOOD, 1));
            if (creep >= 12) { plat_rect(dx - gap - 3, ground - 14 * sc, 2, 2, PAL_RAMP(RAMP_LAVA, 7)); plat_rect(dx - gap + 2, ground - 14 * sc, 2, 2, PAL_RAMP(RAMP_LAVA, 7)); }
        } else {
            dream_food(c->hate_food, dx - gap, ground - 6, big ? 14 : 11, creep, tt);
        }
        /* the spoon */
        plat_line(dx - gap + 8, ground - 10 * sc, dx - gap + 22 + creep, ground - 26 * sc + ui_wave(t, 10, 2), PAL_RAMP(RAMP_STEEL, 5));
        ui_fill_ellipse(dx - gap + 26 + creep, ground - 28 * sc + ui_wave(t, 10, 2), 6 + creep / 4, 4 + creep / 6, PAL_RAMP(RAMP_STEEL, 6));
        break; }
    case 10: {  /* fluffy wings over HaHaland */
        int py = ground - (big ? 60 : 46) + ui_wave(t, 40, 4), flap = ui_wave(t, 12, 6);
        uint8_t wc = creep >= 10 ? PAL_RAMP(RAMP_VAPE, 0) : PAL_PAINT(PAINT_WHITE, 2);
        for (k = 0; k < 8; k++) {
            int hx = ((k * (sw / 6)) - t * 2) % (sw + 40);
            int hh = 8 + (k % 3) * 4;
            if (hx < -20) hx += sw + 40;
            plat_rect(hx, ground + 10 - hh, 14, hh, creep >= 12 ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT((k & 1) ? PAINT_PEACH : PAINT_MINT, 2));
            plat_rect(hx - 2, ground + 8 - hh - 2, 18, 3, creep >= 12 ? PAL_RAMP(RAMP_LAVA, 4 + (k & 1)) : PAL_PAINT(PAINT_CHERRY, 1));
            plat_rect(hx + 5, ground + 12 - hh, 3, 3, PAL_PAINT(PAINT_LEMON, 2));
        }
        if (creep < 10) {
            int side;
            for (side = -1; side <= 1; side += 2) {
                ui_fill_ellipse(cx + side * 30, py - flap / 2, 19, 9 + flap / 2, PAL_RAMP(RAMP_VAPE, 4));
                ui_fill_ellipse(cx + side * 30, py - flap / 2 - 1, 18, 8 + flap / 2, wc);
                ui_fill_ellipse(cx + side * 36, py - flap / 2 + 3, 10, 4, PAL_RAMP(RAMP_SKY, 6));
            }
        } else {
            for (k = 0; k < 18; k++) {
                int hgt = 12 - k / 2 + (k % 6 == 0 ? 3 : 0) + flap / 3;
                plat_rect(cx - 16 - k * 2, py - hgt, 2, hgt + 2, wc);
                plat_rect(cx + 15 + k * 2, py - hgt, 2, hgt + 2, wc);
            }
        }
        portrait_at(c->dreamer, cx, py, expr_for(creep), 0, 0);
        break; }
    case 11: {  /* tea under the sea */
        for (k = 0; k < 4; k++) cspan(0, sw, k * sh / 4, sh / 4, creep >= 12 ? PAL_RAMP(RAMP_SKY, 0) : PAL_PAINT(PAINT_COBALT, 1 + (k & 1)));
        for (k = 0; k < 10; k++) {
            int x = (int)(h2((uint32_t)k, 2u) % (uint32_t)sw), y = sh - (int)((h2((uint32_t)k, 3u) + tt * 2) % (uint32_t)sh);
            ui_fill_circle(x + ui_wave(t + k * 10, 30, 2), y, 2 + (k & 1), PAL_PAINT(PAINT_SKY, 2));
        }
        plat_rect(cx - 50, ground - 4, 100, 5, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(cx - 45, ground + 1, 3, 12, PAL_PAINT(PAINT_WHITE, 1));
        plat_rect(cx + 42, ground + 1, 3, 12, PAL_PAINT(PAINT_WHITE, 1));
        for (k = 0; k < 3; k++) {
            plat_rect(cx - 30 + k * 26, ground - 10, 10, 6, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            plat_rect(cx - 21 + k * 26, ground - 9, 2, 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        }
        portrait_at(c->dreamer, cx, ground - 38, expr_for(creep), 0, 0);
        for (k = 0; k < 4; k++) {
            int dir = (k & 1) ? 1 : -1, x = (int)((tt * (2 + (uint32_t)k) + (uint32_t)k * 70) % (uint32_t)(sw + 40)) - 20;
            int y = 24 + k * (ground - 40) / 4;
            if (dir < 0) x = sw - x;
            ui_fill_ellipse(x, y, 9, 5, PAL_PAINT(k & 2 ? PAINT_TANGERINE : PAINT_LEMON, 1));
            plat_line(x - dir * 9, y, x - dir * 15, y - 4, PAL_PAINT(PAINT_TANGERINE, 0));
            plat_line(x - dir * 9, y, x - dir * 15, y + 4, PAL_PAINT(PAINT_TANGERINE, 0));
            plat_rect(x + dir * 4, y - 2, 2, 2, PAL_BLACK);
            if (creep >= 10) { int e; for (e = 0; e < 3; e++) plat_rect(x + dir * (6 - e * 2), y + 2, 1, 2, PAL_WHITE); }
        }
        break; }
    case 12: {  /* the mirror */
        int mx = cx + (big ? 40 : 30), my = ground - (big ? 44 : 34), lead = creep >= 6 ? 12 : 0;
        ui_fill_ellipse(mx, my, big ? 38 : 30, big ? 46 : 36, PAL_PAINT(PAINT_HONEY, 1));
        ui_fill_ellipse(mx, my, big ? 34 : 26, big ? 42 : 32, PAL_PAINT(PAINT_SKY, creep >= 10 ? 0 : 2));
        portrait_at(c->dreamer, cx - (big ? 54 : 44), my + ui_wave(t, 40, 3), expr_for(creep), 0, 0);
        /* the reflection moves first */
        {
            const DoomoImage *img = portrait_image(c->dreamer, expr_for(creep));
            if (img) {
                int ry = my + ui_wave(t + lead, 40, 3);
                plat_image(mx - img->w / 2, ry - img->h / 2, img, 1, IMG_FLIPX | (creep >= 12 ? IMG_SILHOUETTE : 0),
                           PAL_RAMP(RAMP_VAPE, 0));
                if (creep >= 12) {
                    plat_rect(mx - 8, ry - 4, 3, 2, PAL_RAMP(RAMP_BLOOD, 5));
                    plat_rect(mx + 5, ry - 4, 3, 2, PAL_RAMP(RAMP_BLOOD, 5));
                    plat_rect(mx - 7, ry + 6, 15, 1, PAL_WHITE);
                }
            }
        }
        if (creep >= 6 && creep < 12 && ((t / 6) & 1)) ui_text(mx + 20, my - 40, "*wave*", PAL_WHITE);
        break; }
    case 13: {  /* giant Doomogachi */
        int s4 = big ? 5 : 4, frame = (t / 10) & 3;
        int bounce = ui_wave(t, 30, 3) > 0 ? -ui_wave(t, 30, 3) : 0;
        sprite_at(SPR_PET, frame, cx, ground + 4 + bounce, s4, creep >= 12 ? IMG_SILHOUETTE : 0, PAL_RAMP(RAMP_VAPE, 0));
        if (creep >= 12) {
            ui_fill_circle(cx - 12, ground - 50 + bounce, 3, PAL_RAMP(RAMP_BLOOD, 5));
            ui_fill_circle(cx + 12, ground - 50 + bounce, 3, PAL_RAMP(RAMP_BLOOD, 5));
            plat_rect(cx - 2, ground - 30 + bounce, 2, 6 + (int)(tt % 8), PAL_PAINT(PAINT_SKY, 2));
        }
        chibi_at(c->dreamer, cx + (big ? 80 : 62), ground + 8, sc, 1, 0, 0, 0);
        if (creep < 10) ui_heart(cx + (big ? 60 : 46), ground - (big ? 90 : 70) - (int)(tt % 20), 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        break; }
    case 14: {  /* endless doors */
        int vy = ground - (big ? 50 : 40), d, y;
        uint8_t wallc = creep >= 12 ? (nightmare ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 1)) :
                        creep >= 6 ? PAL_RAMP(RAMP_VAPE, 4) : PAL_RAMP(RAMP_PASTEL_PINK, 5);
        plat_rect(0, 0, sw, sh, wallc);
        for (y = 0; y < sh; y += 2) {
            int half = (y > vy ? y - vy : vy - y) * sw * 6 / (10 * (sh - vy)) + 6;
            uint8_t fc = y > vy ? (((y - vy) / 6) & 1 ? PAL_PAINT(c->color, 1) : PAL_PAINT(c->color, 0))
                                : ui_lighter(wallc);
            plat_rect(cx - half, y, 2 * half, 2, fc);
        }
        plat_line(0, sh, cx - 6, vy, PAL_RAMP(RAMP_STONE, 3));
        plat_line(sw, sh, cx + 6, vy, PAL_RAMP(RAMP_STONE, 3));
        plat_line(0, 0, cx - 6, vy, PAL_RAMP(RAMP_STONE, 4));
        plat_line(sw, 0, cx + 6, vy, PAL_RAMP(RAMP_STONE, 4));
        for (d = 5; d >= 0; d--) {
            int z = (d * 64 + 64 - (t * 2) % 64);               /* 64 .. 384 */
            int dw = 3000 / z, dh = 5000 / z, off = 9000 / z;
            int shake2 = creep >= 10 ? (int)(h2(tt / 2, (uint32_t)d) % 3) - 1 : 0;
            for (k = -1; k <= 1; k += 2) {
                int x = cx + k * off - dw / 2 + shake2, y = vy + 600 / z - dh;
                plat_rect(x, y, dw, dh, PAL_RAMP(RAMP_WOOD, d & 1 ? 2 : 3));
                plat_rect(x + dw - dw / 4, y + dh / 2, 1, 1, PAL_PAINT(PAINT_HONEY, 2));
                if (creep >= 8) plat_rect(x, y + dh - 1, dw, 1, PAL_RAMP(RAMP_BLOOD, 4));
            }
        }
        if (creep >= 12) {
            ui_text(cx - 24, vy + 6, "knock", PAL_RAMP(RAMP_BLOOD, 5));
            ui_text(cx + 4, vy + 16, "knock", PAL_RAMP(RAMP_BLOOD, 5));
        }
        dream_food(c->hate_food, cx, vy + 4, 4, creep, tt);
        chibi_at(c->dreamer, cx, ground + 2, sc, 3, 0, 0, 0);
        break; }
    default: {  /* the demon conga */
        static const uint8_t dancers[4] = { SPR_IMPLING, SPR_PUFF_DEMON, SPR_HELLHOUND_PUP, SPR_BONE_JESTER };
        int wy = big ? 70 : 54, n = 4 + creep / 4, i;
        for (k = 0; k < sw; k += 16) {
            int r;
            for (r = 0; r < 4; r++) plat_rect(k + (r & 1) * 8, wy - 30 + r * 8, 15, 7, PAL_RAMP(RAMP_STONE, 2 + (r & 1)));
        }
        ui_fill_ellipse(cx, wy - 10, 22, 26, PAL_RAMP(RAMP_VAPE, 0));
        ui_fill_ellipse(cx, wy - 10, 18, 22, PAL_RAMP(RAMP_VAPE, 3 + (int)((tt / 6) & 1)));
        for (i = n - 1; i >= 0; i--) {
            int span = n * 30 + sw;
            int x = (int)((tt * (1 + (uint32_t)creep / 6)) % (uint32_t)span) - n * 30 + (n - 1 - i) * 30 + 15;
            int y = ground + 6 + ui_wave(x * 4, 120, 6) - (((t / 6) + i) & 1) * 2;
            if (i == 0) chibi_at(c->dreamer, x, y, sc, 2, (t / 6) & 3, 0, 0);
            else sprite_at(dancers[i % 4], ((t / 8) + i) & 1, x, y, 1, 0, 0);
        }
        break; }
    }
    (void)nightmare;
}
