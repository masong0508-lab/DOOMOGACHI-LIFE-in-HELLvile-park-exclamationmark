/* story_places.c - buildings you can enter, signs you can read and the
 * names of the overworld's regions (owner: story).
 *
 * Doors are wall cells tagged STORY_TAG_DOOR + PLACE_*. Entering a place
 * opens a little menu: talk to whoever runs it, buy things (every item a
 * side quest asks for can be bought somewhere), the extra modes other
 * modules registered for that location in activities.h (with a friendly
 * "Coming soon!" while none are), rest and save at home, the workshop and
 * the full shop / graveyard screens.
 */
#include "story_int.h"

static const char *const place_names[PLACE_COUNT] = {
    "", "Your Cottage", "Town Hall", "Brimstone Bites", "HaHa Mart",
    "Church of St. Cuddles", "HaHaland Hospital", "Rivet's Workshop",
    "Hillvile Towers", "Concert Hall", "Library", "Fun Fair Big Top",
    "Cloud Nine Vapor", "The Pier", "The Wall Gate", "Graveyard",
    "Hellvile Pits", "Anvil's Forge", "The Sealed Gate", "Hellvile Arena",
    "Mother Nox's Chapel", "Vape Factory Ruins", "Bone Pier", "Imp Market",
    "Unity Stage", "Festival Stalls"
};

const char *story_place_name(int p) { return (p > 0 && p < PLACE_COUNT) ? place_names[p] : ""; }

int place_of_tag(int tag)
{
    int p = tag - STORY_TAG_DOOR;
    return (p > 0 && p < PLACE_COUNT) ? p : 0;
}

/* Where the activities registry files each place. */
static int actloc_of(int p)
{
    switch (p) {
    case PLACE_FUN_FAIR: return ACTLOC_FUN_FAIR;
    case PLACE_CONCERT_HALL: case PLACE_STAGE: return ACTLOC_CONCERT_HALL;
    case PLACE_PIER: case PLACE_HELL_PIER: return ACTLOC_LAKE;
    case PLACE_CAFE: case PLACE_STALLS: return ACTLOC_CAFE;
    case PLACE_LIBRARY: return ACTLOC_LIBRARY;
    case PLACE_APARTMENTS: case PLACE_HELL_PITS: case PLACE_HOME: return ACTLOC_APARTMENTS;
    case PLACE_ARENA: return ACTLOC_ARENA;
    case PLACE_MARKET: case PLACE_CHAPEL: case PLACE_FACTORY: case PLACE_FORGE: return ACTLOC_HELLVILE;
    case PLACE_TOWN_HALL: return ACTLOC_TOWN_HALL;
    default: return -1;
    }
}

/* Who runs it (talk option). */
static int keeper_of(int p)
{
    switch (p) {
    case PLACE_TOWN_HALL: return ID_MAYOR;
    case PLACE_CAFE: return ID_NANA;
    case PLACE_CHURCH: return ID_FUZZ;
    case PLACE_HOSPITAL: return ID_BLOOM;
    case PLACE_WORKSHOP: return story_flag(SF_C4_RIVET_HOME) ? -1 : ID_RIVET;
    case PLACE_FORGE: return ID_ANVIL;
    case PLACE_CHAPEL: return ID_MOTHER_NOX;
    case PLACE_SHOP: return 56;            /* Mintworth the piggy banker */
    case PLACE_LIBRARY: return 123;        /* Quillby                    */
    case PLACE_CONCERT_HALL: return 3;     /* Maestro Mew                */
    case PLACE_FUN_FAIR: return 64;        /* Squeaker the clown         */
    case PLACE_APARTMENTS: return 21;      /* Keysie the key keeper      */
    case PLACE_HELL_PITS: return 505;      /* Gutterworth the landlord   */
    case PLACE_MARKET: return 545;         /* Bargainia                  */
    case PLACE_ARENA: return 519;          /* Fangela                    */
    case PLACE_PIER: return 48;            /* Capt. Whisk                */
    case PLACE_HELL_PIER: return 768;      /* Barnaby                    */
    case PLACE_STALLS: return 128;         /* Taco Yaki                  */
    default: return -1;
    }
}

/* Mini shops: what each place sells. */
static const uint8_t sells_cafe[] = { ITEM_CUPCAKE, ITEM_COFFEE, ITEM_MILK, ITEM_PUDDING, ITEM_ONIGIRI, ITEM_BRIMSTONE_BAGEL, ITEM_ANGEL_CAKE, 0 };
static const uint8_t sells_shop[] = { ITEM_SOAP, ITEM_FLOWERS, ITEM_CANDY_EYEBALLS, ITEM_PLUSHIE, ITEM_BOARD_GAME, ITEM_ENERGY_DRINK, ITEM_LOVE_LETTER, ITEM_PERFUME, 0 };
static const uint8_t sells_church[] = { ITEM_HOLY_WATER, ITEM_SAGE_BUNDLE, ITEM_BLESSED_SALT, ITEM_ROSARY, ITEM_PLUSH_CROSS, ITEM_HYMN_BOOK, 0 };
static const uint8_t sells_hospital[] = { ITEM_MEDICINE, ITEM_CHILL_PILL, ITEM_ENERGY_DRINK, ITEM_SOAP, 0 };
static const uint8_t sells_workshop[] = { ITEM_SCRAP, ITEM_GEARS, ITEM_WELD_ROD, ITEM_GLITTER, ITEM_PLUSH_STUFFING, 0 };
static const uint8_t sells_market[] = { ITEM_HELL_PEPPERS, ITEM_SOUL_SOUP, ITEM_CANDY_EYEBALLS, ITEM_SKULL_TRINKET, ITEM_MEAT_SLAB, ITEM_DEMON_HORN, 0 };
static const uint8_t sells_stalls[] = { ITEM_CUPCAKE, ITEM_SOUL_SOUP, ITEM_HELL_PEPPERS, ITEM_ANGEL_CAKE, ITEM_FLOWERS, ITEM_COFFEE, 0 };

static const uint8_t *sells(int p)
{
    switch (p) {
    case PLACE_CAFE: return sells_cafe;
    case PLACE_SHOP: return sells_shop;
    case PLACE_CHURCH: case PLACE_CHAPEL: return sells_church;
    case PLACE_HOSPITAL: return sells_hospital;
    case PLACE_WORKSHOP: case PLACE_FORGE: return sells_workshop;
    case PLACE_MARKET: return sells_market;
    case PLACE_STALLS: return sells_stalls;
    default: return NULL;
    }
}

int story_item_price(int item)
{
    static const uint8_t fallback[ITEM_COUNT] = { 0 };
    int p;
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    p = doomo_items[item].price;
    if (p == 0) p = fallback[item] ? fallback[item] : 20;
    return p;
}

/* ---- the menu ----------------------------------------------------------------------- */
enum { OPT_TALK, OPT_BUY, OPT_ACT, OPT_REST, OPT_WORKSHOP, OPT_SHOPSCREEN, OPT_GRAVE,
       OPT_HEAL, OPT_LEAVE };
#define MENU_MAX 6
static const char *menu_txt[MENU_MAX];
static uint8_t menu_op[MENU_MAX];
static int menu_n, menu_place;
static char menu_q[64];
static char talk_label[32];

static const DoomoActivity *acts[5];
static const char *act_txt[6];
static int act_n;

static char buy_txt[6][28];
static const char *buy_ptr[6];
static uint8_t buy_item[6];
static int buy_n, buy_page;

static void open_menu(int p);

static void act_pick(int c)
{
    if (c < 0 || c >= act_n) return;
    story_pending.kind = PEND_OTHER;
    story_pending.ctx = CTX_FREE;
    ow_sync_to_save();
    activity_launch(acts[c]);
}

static void show_activities(void)
{
    int loc = actloc_of(menu_place), i;
    act_n = loc >= 0 ? activities_at(loc, acts, 5) : 0;
    if (act_n == 0) {
        story_say(-1, "Coming soon! The folks here are still setting up. Check back later!");
        return;
    }
    for (i = 0; i < act_n; i++) act_txt[i] = acts[i]->name;
    act_txt[act_n] = "Back";
    story_choice(-1, "What would you like to do?", act_txt, act_n + 1, act_pick);
}

static void buy_pick(int c);

static void show_buy(void)
{
    const uint8_t *list = sells(menu_place);
    int i, n = 0, total = 0, start;
    if (!list) return;
    while (list[total]) total++;
    start = buy_page * 4;
    if (start >= total) { buy_page = 0; start = 0; }
    for (i = start; i < total && n < 4; i++, n++) {
        buy_item[n] = list[i];
        doomo_snprintf(buy_txt[n], sizeof(buy_txt[n]), "%s %dH$", doomo_items[list[i]].name,
                       story_item_price(list[i]));
        buy_ptr[n] = buy_txt[n];
    }
    if (total > 4) { buy_item[n] = 0xFE; buy_ptr[n] = "More..."; n++; }
    buy_item[n] = 0; buy_ptr[n] = "Done";
    n++;
    buy_n = n;
    {
        char q[48];
        doomo_snprintf(q, sizeof(q), "You have %u H$. Buy something?", (unsigned)doomo_town.money);
        story_choice(keeper_of(menu_place), q, buy_ptr, buy_n, buy_pick);
    }
}

static void buy_pick(int c)
{
    int it, price;
    if (c < 0 || c >= buy_n) return;
    it = buy_item[c];
    if (it == 0xFE) { buy_page++; show_buy(); return; }
    if (it == 0) return;
    price = story_item_price(it);
    if (doomo_town.money < (uint32_t)price) {
        audio_sfx(SFX_ERROR);
        story_say(keeper_of(menu_place), "Oh dear, you're a little short on H$. Help folks out around town and come back!");
        return;
    }
    if (inv_count(it) >= 99) { audio_sfx(SFX_ERROR); ui_toast("Your bag is full of those!"); return; }
    doomo_town.money -= (uint32_t)price;
    inv_add(it, 1);
    audio_sfx(SFX_PICKUP);
    {
        char t[40];
        doomo_snprintf(t, sizeof(t), STR_ICON_COIN " Bought %s!", doomo_items[it].name);
        ui_toast(t);
    }
    show_buy();
}

static void rest(void)
{
    int m = doomo_town.minute, until = (24 * 60 - m + 7 * 60) % (24 * 60);
    if (until < 60) until += 24 * 60;
    sim_advance_minutes(until);
    {
        DoomoResident *p = town_res(DOOMO_PLAYER_ID);
        if (p) { p->stat[STAT_ENERGY] = 100; p->stat[STAT_HEALTH] = 100; }
    }
    doomo_town.story.hp = 100;
    story_say(-1, pet_exists() ? "You sleep like a baby. @E snores tiny snores. Good morning!" :
                                 "You sleep like a baby. Good morning, HaHaland!");
    if (save_game(0)) ui_toast(STR_ICON_HEART " Game saved!");
    else ui_toast("Couldn't save...");
}

static void heal(void)
{
    DoomoResident *p = town_res(DOOMO_PLAYER_ID);
    if (p) { p->stat[STAT_HEALTH] = 100; p->stat[STAT_CORRUPTION] = 0; p->stat[STAT_SANITY] = 100; }
    doomo_town.story.hp = 100;
    if (pet_exists() && pet_is_sick()) pet_medicine();
    audio_sfx(SFX_HOLY);
    story_say(ID_BLOOM, "There we go. Fully patched, purple-free and fluffy. Cold hands, warm heart!");
}

static void menu_pick(int c)
{
    int k;
    if (c < 0 || c >= menu_n) return;
    switch (menu_op[c]) {
    case OPT_TALK:
        k = keeper_of(menu_place);
        if (k >= 0) story_interact_npc(k);
        break;
    case OPT_BUY: buy_page = 0; show_buy(); break;
    case OPT_ACT: show_activities(); break;
    case OPT_REST: rest(); break;
    case OPT_HEAL: heal(); break;
    case OPT_WORKSHOP:
        story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
        break;
    case OPT_SHOPSCREEN:
        story_push_scene(&SCENE_SHOP, 0, PEND_OTHER, CTX_FREE, 0);
        break;
    case OPT_GRAVE:
        story_push_scene(&SCENE_GRAVEYARD, 0, PEND_OTHER, CTX_FREE, 0);
        break;
    default: break;
    }
}

static void add_opt(const char *t, int op)
{
    if (menu_n < MENU_MAX) { menu_txt[menu_n] = t; menu_op[menu_n] = (uint8_t)op; menu_n++; }
}

static void open_menu(int p)
{
    int k = keeper_of(p), loc = actloc_of(p);
    const DoomoResident *r = town_res(k);
    menu_place = p;
    menu_n = 0;
    if (k >= 0 && r && r->status == RES_ALIVE) {
        doomo_snprintf(talk_label, sizeof(talk_label), "Talk to %s", r->name);
        add_opt(talk_label, OPT_TALK);
    }
    if (sells(p)) add_opt("Buy", OPT_BUY);
    if (p == PLACE_WORKSHOP || p == PLACE_FORGE) {
        if (story_beat() >= BEAT_C2_CELLAR) add_opt("Weld & armory", OPT_WORKSHOP);
    }
    if (p == PLACE_HOME || p == PLACE_APARTMENTS || p == PLACE_HELL_PITS) add_opt("Rest & save", OPT_REST);
    if (p == PLACE_HOSPITAL) add_opt("Get patched up", OPT_HEAL);
    if (p == PLACE_SHOP) add_opt("Browse every shelf", OPT_SHOPSCREEN);
    if (p == PLACE_GRAVEYARD) add_opt("Visit the graves", OPT_GRAVE);
    if (loc >= 0) add_opt("Activities", OPT_ACT);
    add_opt("Leave", OPT_LEAVE);
    doomo_snprintf(menu_q, sizeof(menu_q), "%s", place_names[p]);
    if (menu_n == 1) {
        story_say(-1, p == PLACE_SHOP ? "The shop is closed." : "Nobody's home right now.");
        return;
    }
    audio_sfx(SFX_DOOR);
    story_choice(-1, menu_q, menu_txt, menu_n, menu_pick);
}

void story_place_menu(int place)
{
    if (place <= PLACE_NONE || place >= PLACE_COUNT) return;
    if (place == PLACE_GATE) {
        story_say(-1, "The old hell gate. Sealed tight, and now covered in flowers.");
        return;
    }
    open_menu(place);
}

/* ---- flower beds ------------------------------------------------------------------------ */
int story_pick_flowers(int map_id, int cx, int cy)
{
    /* one bunch per bed per day: hash the bed into 8 rotating slots */
    static uint32_t picked_day[8];
    static uint16_t picked_key[8];
    uint16_t key = (uint16_t)((map_id << 12) ^ (cx << 6) ^ cy);
    int i, slot = key & 7;
    for (i = 0; i < 8; i++)
        if (picked_key[i] == key && picked_day[i] == doomo_town.day + 1) {
            story_say(-1, "You already picked flowers here today. Let them grow back!");
            return 1;
        }
    picked_key[slot] = key;
    picked_day[slot] = doomo_town.day + 1;
    inv_add(ITEM_FLOWERS, 1);
    audio_sfx(SFX_SPARKLE);
    ui_toast(STR_ICON_HEART " Picked some Flowers!");
    return 1;
}

/* ---- signs ------------------------------------------------------------------------------- */
typedef struct { uint8_t map, x, y; const char *text; } SignText;
static const SignText signs[] = {
    { MAPID_HAHALAND, 36, 27, "BUS STOP - Welcome to HaHaland, the happiest place on Earth! Pop. 500 (and counting!)" },
    { MAPID_HAHALAND, 27, 16, "BRIMSTONE BITES - Today: cinnamon swirls! (Not actually made of brimstone.)" },
    { MAPID_HAHALAND, 44, 16, "HAHA MART - Everything you need, nothing you don't, and a few things you might." },
    { MAPID_HAHALAND, 28, 8, "TOWN HALL - Mayor Gigglesworth is IN. (He is always in. He lives upstairs.)" },
    { MAPID_HAHALAND, 13, 23, "HOSPITAL - Dr. Bloom: gentle hands, cold hands, kind heart." },
    { MAPID_HAHALAND, 24, 35, "CONCERT HALL - Tonight: The Fluffones! Tomorrow: probably also The Fluffones." },
    { MAPID_HAHALAND, 42, 35, "LIBRARY - Shhh. Quiet giggling only." },
    { MAPID_HAHALAND, 31, 45, "DUCK POND - Please do not feed the ducks glitter. They know what they did." },
    { MAPID_HAHALAND, 54, 11, "RIVET'S WORKSHOP - Welding, fixing and hugging. Careful with the hugging." },
    { MAPID_HAHALAND, 51, 37, "CLOUD NINE VAPOR - Grand opening soon! Free samples! No refunds! No questions!" },
    { MAPID_HAHALAND, 37, 56, "THE PIER - Fish responsibly. The fish are watching." },
    { MAPID_HAHALAND, 4, 32, "Here lies Mr. Wiggles. A good worm. The best worm." },
    { MAPID_HAHALAND, 6, 32, "Here lies Grandpa Puff's moustache. It had a good run." },
    { MAPID_HAHALAND, 9, 32, "Here lies the Great Cake of '89. Gone too soon. Delicious." },
    { MAPID_HAHALAND, 4, 35, "Here lies Fluffy. Fluffy was a cloud. We miss Fluffy." },
    { MAPID_HAHALAND, 6, 35, "Rest in pieces, Mr. Huggins the 1st. You were exorcised with love." },
    { MAPID_HAHALAND, 9, 35, "Reserved for Mayor Giggs. \"Not yet!\" - the Mayor" },
    { MAPID_HELLVILE, 13, 26, "HELLVILE - Pop. 500. Abandon hope (optional). Wipe your hooves." },
    { MAPID_HELLVILE, 31, 11, "ANVIL'S FORGE - We fix what the Duke broke. Hot soup on Sundays." },
    { MAPID_HELLVILE, 4, 46, "Here lies Count Snoozula. Finally getting some sleep." },
    { MAPID_HELLVILE, 8, 40, "Crypt for rent. Ask Graveley. Families with kids: half price. Shh." },
    { MAPID_PARK_UNITED, 16, 11, "UNITY STAGE - Built from the bricks of the old Wall. Dance on it!" },
    { MAPID_PARK_UNITED, 47, 11, "<- HILLVILE SIDE        HELLVILE SIDE ->   (it's all one park now!)" },
};

const char *place_sign_text(int map, int cx, int cy)
{
    int i;
    if (map == MAPID_HAHALAND_INVADED) {
        for (i = 0; i < DOOMO_COUNTOF(signs); i++)
            if (signs[i].map == MAPID_HAHALAND && signs[i].x == cx && signs[i].y == cy)
                return "The sign is scorched and dripping. Somebody scrawled on it: \"VAPULA WAS HERE\".";
        return "";
    }
    for (i = 0; i < DOOMO_COUNTOF(signs); i++)
        if (signs[i].map == map && signs[i].x == cx && signs[i].y == cy) {
            if (map == MAPID_HAHALAND && cx == 51 && cy == 37 && story_beat() >= BEAT_C2_VAPOR)
                return story_beat() >= BEAT_C4_GATE ? "CLOUD NINE VAPOR - CLOSED FOREVER. (Thank goodness.)" :
                       "CLOUD NINE VAPOR - Now open! Today's flavour: purple. Just purple.";
            return signs[i].text;
        }
    return "";
}

/* ---- regions ------------------------------------------------------------------------------ */
typedef struct { uint8_t map, x0, y0, x1, y1; const char *name; } Region;
static const Region regions[] = {
    { MAPID_HAHALAND, 2, 2, 20, 17, "Hillvile Lane" },
    { MAPID_HAHALAND, 21, 2, 35, 10, "Town Hall Hill" },
    { MAPID_HAHALAND, 36, 2, 47, 10, "Hillvile Towers" },
    { MAPID_HAHALAND, 21, 11, 47, 27, "Town Plaza" },
    { MAPID_HAHALAND, 2, 18, 20, 37, "St. Cuddles Square" },
    { MAPID_HAHALAND, 2, 38, 19, 51, "The Fun Fair" },
    { MAPID_HAHALAND, 20, 28, 47, 51, "Hellvile Park - Hillvile Side" },
    { MAPID_HAHALAND, 48, 2, 55, 51, "The East Bank" },
    { MAPID_HAHALAND, 0, 52, 63, 63, "HaHa Beach" },
    { MAPID_HELLVILE, 2, 2, 12, 19, "Chapel Hill" },
    { MAPID_HELLVILE, 2, 20, 12, 35, "Gate Plaza" },
    { MAPID_HELLVILE, 13, 2, 26, 11, "The Pits" },
    { MAPID_HELLVILE, 27, 2, 38, 11, "Forge Row" },
    { MAPID_HELLVILE, 13, 12, 33, 38, "Imp Market Quarter" },
    { MAPID_HELLVILE, 39, 2, 61, 19, "Vape Factory Ruins" },
    { MAPID_HELLVILE, 34, 20, 48, 38, "The Sealed Gate" },
    { MAPID_HELLVILE, 49, 20, 61, 38, "Hellvile Arena" },
    { MAPID_HELLVILE, 2, 36, 12, 53, "Crypt Row" },
    { MAPID_HELLVILE, 13, 39, 61, 53, "The Lake of Fire" },
    { MAPID_PARK_UNITED, 2, 2, 61, 11, "The Unity Stage" },
    { MAPID_PARK_UNITED, 2, 12, 29, 45, "Hillvile Meadow" },
    { MAPID_PARK_UNITED, 30, 12, 33, 45, "The River" },
    { MAPID_PARK_UNITED, 34, 12, 61, 45, "Hellvile Garden" },
    { MAPID_PARK_UNITED, 2, 46, 61, 53, "Parade Road" },
};

const char *story_region_name(int map, int cx, int cy)
{
    int i, m = map == MAPID_HAHALAND_INVADED ? MAPID_HAHALAND : map;
    for (i = 0; i < DOOMO_COUNTOF(regions); i++) {
        const Region *r = &regions[i];
        if (r->map == m && cx >= r->x0 && cx <= r->x1 && cy >= r->y0 && cy <= r->y1) return r->name;
    }
    return "";
}
