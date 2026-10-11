/* fp_island.c - SCENE_ISLAND: the free play hub (owner: freeplay).
 *
 * The illustrated map of HaHaland with Hellvile Park in the middle. The
 * D-pad hops between locations (nearest one in that direction), A opens a
 * location's menu: its built-in services (shop, exorcisms, homes, ...)
 * plus every activity registered for it in activities.h (minigames,
 * concerts, fishing, cooking, collections, dreams ...), locked ones greyed
 * with their hint. Thought bubbles mark where citizens need help. The
 * island keeps living (sim_tick) the whole time.
 *
 *   A       visit             START  menu (save, options, unlocks ...)
 *   R       citizens          L      news
 *   SELECT  Doomogachi        B      menu
 */
#include "fp_island.h"

/* ====================================================================== */
/* locations                                                                */
typedef struct {
    const char *name;
    const char *blurb;
    uint8_t kind, mx, my;
    uint8_t actloc;          /* ACTLOC_* or 0xFF                      */
    uint8_t act;             /* ACT_* citizens hang out here, 0xFF none */
    const char *host;        /* roster name of the host, or NULL       */
    const char *greet;
} Loc;

#define NOLOC 0xFF
static const Loc locs[] = {
    { "Hillvile Towers", "500 cozy homes on the good side.", LOC_TOWERS, 30, 70, ACTLOC_APARTMENTS, ACT_HOME,
      "Keysie", "Five hundred doors, five hundred keys!" },
    { "Town Hall", "Make citizens, save, options.", LOC_TOWN_HALL, 66, 64, ACTLOC_TOWN_HALL, NOLOC,
      "Mayor Giggs", "Hee hee! Welcome to Town Hall, friend!" },
    { "Rivet's Workshop", "Weld cute or brutal weapons.", LOC_WORKSHOP, 100, 74, NOLOC, ACT_WORKSHOP,
      "Rivet", "Mind the sparks! Wanna weld something?" },
    { "The Wall & Gate", "The wall between the two sides.", LOC_WALL, 128, 58, NOLOC, NOLOC,
      "Halt", "HALT! ...Oh, it's you. Hi." },
    { "Black Market", "Edgy goods. No refunds.", LOC_BLACK_MARKET, 168, 70, NOLOC, NOLOC,
      "Swipey", "Psst. Fell off a hellwagon. Interested?" },
    { "Hellvile Pits", "500 spooky-cute homes.", LOC_PITS, 222, 70, ACTLOC_APARTMENTS, ACT_HOME,
      "Gutterworth", "Rent's due. Always. Forever. Hehe." },
    { "Shop", "Food, gifts, care and holy stuff.", LOC_SHOP, 26, 114, NOLOC, ACT_SHOP,
      "Bargain Bun", "Welcome, welcome! Today's deals are fresh!" },
    { "Brimstone Bites", "The cafe. Treats and cooking.", LOC_CAFE, 60, 110, ACTLOC_CAFE, ACT_CAFE,
      "Cardamom", "Table for one? The cupcakes are warm!" },
    { "St. Cuddles Church", "Exorcisms with Father Fuzz.", LOC_CHURCH, 96, 116, NOLOC, ACT_CHURCH,
      "Father Fuzz", "Bless your fluffy heart. Need help?" },
    { "Hellvile Square", "The bad side's hangout.", LOC_SQUARE, 172, 116, ACTLOC_HELLVILE, ACT_VAPE_SHOP,
      "DJ Echo", "Echo... echo... welcome to the Square." },
    { "Demon Arena", "Fight waves of demons.", LOC_ARENA, 216, 118, ACTLOC_ARENA, NOLOC,
      "Fangela", "Rrrr. Only the brave get past me." },
    { "Hospital", "Dr. Bloom treats the sick.", LOC_HOSPITAL, 30, 156, NOLOC, ACT_HOSPITAL,
      "Dr. Bloom", "Cold hands, warm heart. Who's sick?" },
    { "Fun Fair", "Minigames and prizes!", LOC_FUN_FAIR, 64, 158, ACTLOC_FUN_FAIR, NOLOC,
      "Ferris", "Step right up! Every ride is a winner!" },
    { "Library", "Collections, records, music.", LOC_LIBRARY, 98, 162, ACTLOC_LIBRARY, NOLOC,
      "Shushana", "Shhh... welcome. Books are friends." },
    { "Hellvile Park", "Where both sides could meet.", LOC_PARK, 128, 154, NOLOC, ACT_PARK,
      "Rusty", "Nice day for the park, eh?" },
    { "Lake of Fire", "Toasty fishing on the bad side.", LOC_LAKE, 212, 180, ACTLOC_LAKE, NOLOC,
      "Capt. Hooky", "Arr! The lava's bitin' today!" },
    { "Beach", "Sand, sun and the sea.", LOC_BEACH, 34, 214, ACTLOC_LAKE, ACT_BEACH,
      "Sunny Dunes", "Sunscreen on? Let's play!" },
    { "Concert Hall", "Songs, shows and the studio.", LOC_CONCERT, 80, 206, ACTLOC_CONCERT_HALL, ACT_CONCERT,
      "Maestro Mew", "Mrrrr-aestro! The stage awaits!" },
    { "Graveyard", "Rest, flowers and a ghost or two.", LOC_GRAVEYARD, 166, 208, NOLOC, ACT_HAUNTING,
      "Mr. Solemn", "Welcome. Everyone ends up here... visiting." },
    { "Doomogachi", "Your tamagotchi pet.", LOC_PET, 14, 236, NOLOC, NOLOC, NULL, NULL },
    { "Citizens", "Everyone on the island.", LOC_ROSTER, 242, 236, NOLOC, NOLOC, NULL, NULL },
};
#define NLOCS DOOMO_COUNTOF(locs)
DOOMO_STATIC_ASSERT(sizeof(locs) / sizeof(locs[0]) <= FP_LOC_MAX, locs_fit);

static const Loc menu_loc = {
    "Menu", "", LOC_MENU, 0, 0, NOLOC, NOLOC, NULL, NULL
};

/* ====================================================================== */
/* menu entries                                                             */
enum { E_APTS, E_MOVE_IN, E_SHOP, E_CAFE_SHOP, E_BLACK_MARKET, E_EXORCISE, E_PRACTICE,
       E_FUZZ_TALK, E_TREAT, E_WORKSHOP, E_ARENA, E_GRAVES, E_STROLL, E_PICNIC,
       E_VISIT_HELL, E_PEEK_WALL, E_CREATE_HILL, E_CREATE_HELL, E_RENT, E_AVATAR,
       E_NAME_ISLAND, E_SAVELOAD, E_OPTIONS, E_UNLOCKS, E_RECORDS, E_TITLE,
       E_NEWS, E_ROSTER, E_PET, E_HERE, E_ACTIVITY, E_SOON, E_GENRE, E_CLOSE };

#define MENU_MAX 24
typedef struct {
    uint8_t code, icon, enabled;
    int16_t arg;
    const DoomoActivity *act;
    char label[30];
    const char *desc;
} Entry;

/* the hub's big state lives in the shared scene scratch (scene.h) */
typedef struct {
    FpIslandGeo geo;
    Entry ent[MENU_MAX];
    FpBCache bc;                 /* (integration, perf) building sprites */
} IslScratch;
DOOMO_STATIC_ASSERT(sizeof(IslScratch) <= DOOMO_SCENE_SCRATCH, island_scratch_fits);
static IslScratch *isp;

static struct {
    int sel;
    uint8_t menu_open;
    const Loc *mloc;
    int menu_t;
    int nent, esel, etop;
    uint8_t menu_dirty;
    uint8_t prob[FP_LOC_MAX];
    uint16_t here[ACT_COUNT], here_acc[ACT_COUNT];
    int scan;
    uint8_t wait;
    int16_t wait_arg;
    int song;
    int idle;
    char namebuf[FP_ISLAND_NAME_LEN];
    char petname[8];
} I;

static void build_menu(const Loc *L);
static void isl_ready(void);

/* (Re)claim the scratch; after another scene took it the geometry is
 * rebuilt by the gfx module and an open menu is rebuilt here. */
static IslScratch *isl(void)
{
    if (!isp || !scene_scratch_mine(FP_ISLAND_TAG)) {
        isp = (IslScratch *)scene_scratch(FP_ISLAND_TAG, sizeof(IslScratch));
        memset(isp, 0, sizeof(*isp));
        I.nent = 0;
        I.menu_dirty = 1;
    }
    return isp;
}

FpIslandGeo *fp_island_geo(void) { return &isl()->geo; }
FpBCache *fp_island_bcache(void) { return &isl()->bc; }

static int loc_index_of(int kind)
{
    int i;
    for (i = 0; i < NLOCS; i++) if (locs[i].kind == kind) return i;
    return 0;
}

static int arena_locked(void) { return !unlock_mode_ok(MODE_ARENA); }

static Entry *add(int code, int icon, const char *label, const char *desc)
{
    Entry *e;
    if (I.nent >= MENU_MAX) return NULL;
    e = &isl()->ent[I.nent++];
    memset(e, 0, sizeof(*e));
    e->code = (uint8_t)code;
    e->icon = (uint8_t)icon;
    e->enabled = 1;
    fp_strcpy(e->label, label, sizeof(e->label));
    e->desc = desc ? desc : "";
    return e;
}

static void add_here(int act)
{
    Entry *e;
    char buf[30];
    if (act == NOLOC) return;
    doomo_snprintf(buf, sizeof(buf), "Who's here? (%d)", I.here[act % ACT_COUNT]);
    e = add(E_HERE, ICON_SMILE, buf, "See who's hanging out here right now.");
    if (e) { e->arg = (int16_t)act; e->enabled = I.here[act % ACT_COUNT] > 0; }
}

/* the activities registered at this location (activities.h): the ones
 * open right now first, then the locked / night-only ones, greyed with
 * their hint */
static void add_activities(int actloc)
{
    const DoomoActivity *open[16];
    int i, n = activity_count(), shown = 0, nopen;
    if (actloc == NOLOC) return;
    nopen = activities_at(actloc, open, 16);
    for (i = 0; i < nopen; i++) {
        Entry *e = add(E_ACTIVITY, open[i]->icon, open[i]->name, open[i]->desc);
        if (!e) break;
        e->act = open[i];
        shown++;
    }
    for (i = 0; i < n; i++) {
        const DoomoActivity *a = activity_at(i);
        Entry *e;
        if (!a || a->location != actloc || !fp_activity_visible(a) || activity_available(a)) continue;
        e = add(E_ACTIVITY, a->icon, a->name, a->desc);
        if (!e) break;
        e->act = a;
        e->enabled = 0;
        shown++;
    }
    if (!shown) add(E_SOON, ICON_SPARK, "Coming soon!", "Something fun is being built here. Check back later!");
}

static void build_menu(const Loc *L)
{
    Entry *e;
    int free_hill = town_alloc(0) >= 0, free_hell = town_alloc(1) >= 0;
    I.nent = 0;
    switch (L->kind) {
    case LOC_TOWERS: case LOC_PITS: {
        int d = L->kind == LOC_PITS;
        add(E_APTS, ICON_HOUSE, "Browse homes", d ? "Peek into the windows of the Pits." :
            "Peek into the windows of the Towers.")->arg = (int16_t)d;
        e = add(E_MOVE_IN, ICON_EGG, "Move someone in", "Create a brand new citizen for a free home.");
        if (e) { e->arg = (int16_t)d; e->enabled = (uint8_t)(d ? free_hell : free_hill); }
        add_activities(ACTLOC_APARTMENTS);
        break; }
    case LOC_TOWN_HALL:
        e = add(E_CREATE_HILL, ICON_HEART, "Create (Hillvile)", "Make a new good-side citizen in the creator.");
        if (e) e->enabled = (uint8_t)free_hill;
        e = add(E_CREATE_HELL, ICON_PENTAGRAM, "Create (Hellvile)", "Make a new bad-side citizen. Spooky-cute!");
        if (e) e->enabled = (uint8_t)free_hell;
        add(E_RENT, ICON_COIN, "Collect rent", "Once a day: everyone chips in a little.");
        add(E_AVATAR, ICON_SMILE, "Your avatar", "Your own look (it fights in the Arena).");
        add(E_NAME_ISLAND, ICON_STAR, "Name the island", "Give your island a name of its own.");
        add(E_SAVELOAD, ICON_HEART, "Save / Load", "Keep your island safe in a save slot.");
        add(E_OPTIONS, ICON_WRENCH, "Options", "Game, shooter, sim, audio, display...");
        add(E_RECORDS, ICON_CHECK, "Town records", "Population, couples, exorcisms...");
        add_activities(ACTLOC_TOWN_HALL);
        break;
    case LOC_SHOP:
        add(E_SHOP, ICON_COIN, "Go shopping", "Today's stock: food, gifts, care, holy and welding.");
        add_here(L->act);
        break;
    case LOC_BLACK_MARKET:
        add(E_BLACK_MARKET, ICON_SKULL, "Browse the stall", "Edgier stuff. Cheaper scrap. No holy water.");
        break;
    case LOC_CAFE:
        add(E_CAFE_SHOP, ICON_FOOD, "Order treats", "Fresh cakes and snacks for your citizens.");
        add_here(L->act);
        add_activities(ACTLOC_CAFE);
        break;
    case LOC_CHURCH: {
        int n = 0, i;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) if (fp_pick_match(PICK_POSSESSED, i)) n++;
        e = add(E_EXORCISE, ICON_CROSS, n ? "Exorcise someone!" : "Exorcise someone",
                "Father Fuzz's ritual: prayer, holy water, true name.");
        if (e) e->enabled = (uint8_t)(n > 0);
        add(E_PRACTICE, ICON_HEART, "Practice ritual", "Rehearse on a plush dummy. No stakes!");
        add(E_FUZZ_TALK, ICON_SMILE, "Talk to Father Fuzz", "Ask how the island's souls are doing.");
        add(E_SHOP, ICON_DROP, "Holy supplies", "Holy water, sage, salt and friends.")->arg = 3;
        add_here(L->act);
        break; }
    case LOC_HOSPITAL: {
        int n = 0, i;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) if (fp_pick_match(PICK_SICK, i)) n++;
        e = add(E_TREAT, ICON_CROSS, "Treat someone (H$40)", "Dr. Bloom patches up a sick citizen.");
        if (e) e->enabled = (uint8_t)(n > 0);
        add_here(L->act);
        break; }
    case LOC_WORKSHOP:
        add(E_WORKSHOP, ICON_WRENCH, "Weld & armory", "Rivet's welding bench and your weapons.");
        add_here(L->act);
        break;
    case LOC_ARENA:
        e = add(E_ARENA, ICON_GUN, "Fight demon waves", "Survive wave after wave. Brutal or cute.");
        if (e) e->enabled = (uint8_t)!arena_locked();
        add_activities(ACTLOC_ARENA);
        break;
    case LOC_GRAVEYARD:
        add(E_GRAVES, ICON_SKULL, "Visit the graves", "Flowers, memories, and the ghost rite.");
        add_here(L->act);
        break;
    case LOC_PARK:
        add(E_STROLL, ICON_ARROW_R, unlock_mode_ok(MODE_UNITED_PARK) ? "Stroll the park" : "Stroll HaHaland",
            "Walk around the island yourself.");
        add(E_PICNIC, ICON_FOOD, "Throw a picnic (H$120)", "Everyone at the park gets snacks and smiles.");
        add_here(L->act);
        break;
    case LOC_WALL:
        e = add(E_VISIT_HELL, ICON_PENTAGRAM, "Visit Hellvile", "Cross the gate and walk the bad side.");
        if (e) e->enabled = (uint8_t)unlock_mode_ok(MODE_HELLVILE_VISITS);
        add(E_PEEK_WALL, ICON_CLOUD, "Peek over the Wall", "What's going on over there?");
        break;
    case LOC_SQUARE:
        add_here(L->act);
        add_activities(ACTLOC_HELLVILE);
        break;
    case LOC_LAKE: case LOC_BEACH:
        add_here(L->act);
        add_activities(ACTLOC_LAKE);
        break;
    case LOC_FUN_FAIR:
        add_activities(ACTLOC_FUN_FAIR);
        break;
    case LOC_LIBRARY:
        add_activities(ACTLOC_LIBRARY);
        add(E_UNLOCKS, ICON_STAR, "Unlocks gallery", "Everything you've earned, and how to get the rest.");
        add(E_NEWS, ICON_CLOUD, "News archive", "Everything that's happened lately.");
        add(E_ROSTER, ICON_SMILE, "Citizen records", "Look up any citizen.");
        break;
    case LOC_CONCERT:
        add_activities(ACTLOC_CONCERT_HALL);
        add_here(L->act);
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Tonight: %s", doomo_music_names[sim_concert_genre() % MUSIC_COUNT]);
        add(E_GENRE, ICON_NOTE, fp_tmp, "Fans of tonight's genre flock to the hall.");
        break;
    case LOC_MENU:
        add(E_ROSTER, ICON_SMILE, "Citizens", "Browse every citizen on the island.");
        add(E_NEWS, ICON_CLOUD, "News", "The island's news feed.");
        add(E_PET, ICON_EGG, "Doomogachi", "Your tamagotchi pet.");
        add(E_SAVELOAD, ICON_HEART, "Save / Load", "Keep your island safe.");
        add(E_OPTIONS, ICON_WRENCH, "Options", "Settings for everything.");
        add(E_UNLOCKS, ICON_STAR, "Unlocks", "Everything you've earned.");
        add(E_RECORDS, ICON_CHECK, "Town records", "Island statistics.");
        add(E_TITLE, ICON_ARROW_L, "Back to title", "Leave the island (save first!).");
        break;
    default: break;
    }
    add(E_CLOSE, ICON_X, "Leave", "");
}

/* ====================================================================== */
/* callbacks                                                                */
static void cb_title(int yes) { if (yes) scene_set(&SCENE_TITLE, 0); }

static void cb_name_island(int ok)
{
    if (!ok) return;
    fp_set_island_name(I.namebuf);
    if (I.namebuf[0]) fp_achieve(UL_FP_NAMER);
    audio_sfx(SFX_SPARKLE);
    doomo_snprintf(fp_tmp, sizeof(fp_tmp), STR_ICON_STAR " Welcome to %s!", I.namebuf[0] ? I.namebuf : "Hellvile Park");
    ui_toast(fp_tmp);
}

static void cb_pet_name(int ok)
{
    if (!ok) return;
    pet_new(I.petname[0] ? I.petname : "Mochi");
    audio_sfx(SFX_EGG_CRACK);
    scene_push(&SCENE_PET, 0);
}

static void cb_exorcise(int id)
{
    if (id < 0) return;
    I.wait = FP_WAIT_EXORCISM;
    scene_push(&SCENE_EXORCISM, id);
}

static void cb_open(int id) { if (id >= 0) fp_open_resident(id); }

static void cb_treat(int id)
{
    const char *line;
    if (id < 0) return;
    if (doomo_town.money < 40) { ui_say(-1, "Dr. Bloom: \"That's H$40, dear. Come back with coins.\""); return; }
    doomo_town.money -= 40;
    inv_add(ITEM_MEDICINE, 1);
    line = sim_heal(id, ITEM_MEDICINE);
    audio_sfx(SFX_HOLY);
    ui_say(id, line);
    fp_achieve(UL_FP_HEALER);
}

static void open_pet(void)
{
    if (pet_exists()) { scene_push(&SCENE_PET, 0); return; }
    fp_strcpy(I.petname, "Mochi", sizeof(I.petname));
    ui_say(-1, "A mysterious egg is wobbling on your doorstep... It's a Doomogachi! Give it a name!");
    ui_say_done(NULL);
    ui_text_entry("Name your Doomogachi", I.petname, sizeof(I.petname), cb_pet_name);
}

static void town_records(void)
{
    int i, hill = 0, hell = 0, poss = 0, lost = 0, ghosts = 0, couples = 0, cross = 0;
    char buf[160];
    const DoomoSimStats *s = sim_stats();
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        int st = doomo_town.res[i].status;
        if (st == RES_EMPTY || st == RES_DEAD) continue;
        if (RES_DISTRICT(i)) hell++; else hill++;
        if (st == RES_POSSESSED) poss++;
        if (st == RES_SOUL_LOST) lost++;
        if (st == RES_GHOST) ghosts++;
    }
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int k = e->kind & REL_KIND_MASK;
        if (k == REL_DATING || k == REL_MARRIED) couples++;
        if (e->a < DOOMO_MAX_RESIDENTS && e->b < DOOMO_MAX_RESIDENTS &&
            RES_DISTRICT(e->a) != RES_DISTRICT(e->b) && (k == REL_FRIEND || k == REL_BEST_FRIEND))
            cross++;
    }
    doomo_snprintf(buf, sizeof(buf), "Population: %d in Hillvile, %d in Hellvile. %d ghosts float about.",
                   hill, hell, ghosts);
    ui_say(-1, buf);
    doomo_snprintf(buf, sizeof(buf), "Couples: %d. Cross-border friendships: %d. Graves: %d.",
                   couples, cross, doomo_town.grave_count);
    ui_say(-1, buf);
    doomo_snprintf(buf, sizeof(buf), "Possessed right now: %d. Lost souls: %d. Exorcisms won: %u.",
                   poss, lost, (unsigned)progress_get(PROG_EXORCISMS_WON));
    ui_say(-1, buf);
    doomo_snprintf(buf, sizeof(buf), "Since you arrived: %u weddings, %u babies, %u breakups, %u news stories.",
                   (unsigned)s->marriages, (unsigned)s->babies, (unsigned)s->breakups, (unsigned)s->news);
    ui_say(-1, buf);
    if (doomo_town.monster_meter)
        ui_say(-1, "...and people whisper about a MONSTER. Everyone remembers.");
}

static void fuzz_talk(void)
{
    int i, poss = 0, lost = 0, dark = 0, fuzz = loc_index_of(LOC_CHURCH);
    int speaker = fp_host_in_town(roster_find("Father Fuzz")) ? roster_find("Father Fuzz") : -1;
    char buf[160];
    (void)fuzz;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        if (r->status == RES_POSSESSED) poss++;
        else if (r->status == RES_SOUL_LOST) lost++;
        else if (r->status == RES_ALIVE && r->stat[STAT_CORRUPTION] >= 70) dark++;
    }
    if (!poss && !lost && !dark) {
        ui_say(speaker, "All the island's souls are bright and fluffy today. Praise be! Keep them happy and clean.");
        return;
    }
    doomo_snprintf(buf, sizeof(buf), "%d possessed, %d teetering on the edge... and %d lost souls.",
                   poss, dark, lost);
    ui_say(speaker, buf);
    ui_say(speaker, "Holy water, sage and a well-rested host help the ritual. Three failures and a soul is lost.");
    if (lost) ui_say(speaker, "A lost soul can only be laid to rest. It's... a kindness. I'll be with you.");
}

static void peek_wall(void)
{
    int i, hell = 0, poss = 0, cross = 0;
    char buf[160];
    for (i = DOOMO_DISTRICT_SIZE; i < DOOMO_MAX_RESIDENTS; i++) {
        int st = doomo_town.res[i].status;
        if (fp_living_status(st)) hell++;
        if (st == RES_POSSESSED) poss++;
    }
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        if (e->a < DOOMO_MAX_RESIDENTS && e->b < DOOMO_MAX_RESIDENTS &&
            RES_DISTRICT(e->a) != RES_DISTRICT(e->b) && e->value > 15) cross++;
    }
    if (!hell) {
        ui_say(-1, "You climb up and peek over... Hellvile is empty. Just a few tumbleweeds and a sad bat.");
        return;
    }
    doomo_snprintf(buf, sizeof(buf), "Over the Wall: %d Hellvile citizens. Purple lights, loud music, smells like toast.",
                   hell);
    ui_say(-1, buf);
    doomo_snprintf(buf, sizeof(buf), "%d friendships cross the Wall already. %s", cross,
                   poss ? "Something is possessing folks over there..." : "Maybe one day it'll come down.");
    ui_say(-1, buf);
}

static void picnic(void)
{
    int i, n = 0;
    char buf[80];
    if (doomo_town.money < 120) { audio_sfx(SFX_ERROR); ui_toast("A picnic costs H$120."); return; }
    doomo_town.money -= 120;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = &doomo_town.res[i];
        if (r->status != RES_ALIVE && r->status != RES_GHOST) continue;
        if (r->activity != ACT_PARK && r->activity != ACT_WANDERING) continue;
        r->stat[STAT_HUNGER] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HUNGER] + 30);
        r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + 15);
        r->stat[STAT_SOCIAL] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SOCIAL] + 15);
        n++;
    }
    progress_add(PROG_MEALS_SERVED, n);
    doomo_snprintf(buf, sizeof(buf), "Picnic in Hellvile Park! %d citizens munched sandwiches.", n);
    sim_news_push(buf);
    audio_sfx(SFX_HAPPY);
    fp_fx_burst(fp_mx(128), fp_my(140), FX_CONFETTI, 24);
    ui_toast(STR_ICON_HEART " What a lovely picnic!");
    fp_achieve(UL_FP_PICNIC);
}

static void collect_rent(void)
{
    int i, n = 0, amount;
    char buf[64];
    if (fp_rent_day() == doomo_town.day + 1) {
        ui_say(-1, "You've already collected today's rent. Come back tomorrow!");
        return;
    }
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status == RES_ALIVE || doomo_town.res[i].status == RES_GHOST) n++;
    amount = 50 + n * 2;
    doomo_town.money += (uint32_t)amount;
    progress_add(PROG_MONEY_EARNED, amount);
    fp_set_rent_day(doomo_town.day + 1);
    doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " Rent collected: +%d H$", amount);
    ui_toast(buf);
    audio_sfx(SFX_PICKUP);
    fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_COIN, 16);
    fp_achieve(UL_FP_LANDLORD);
}

static void run_entry(const Entry *e)
{
    if (!e->enabled) {
        audio_sfx(SFX_ERROR);
        if (e->code == E_ACTIVITY) ui_toast(fp_activity_reason(e->act));
        else if (e->code == E_ARENA) {
            const DoomoUnlock *u = unlock_find(UL_CAT_MODE, 0, MODE_ARENA);
            ui_toast(u && u->how ? u->how : "Locked!");
        } else if (e->code == E_VISIT_HELL) {
            const DoomoUnlock *u = unlock_find(UL_CAT_MODE, 0, MODE_HELLVILE_VISITS);
            ui_toast(u && u->how ? u->how : "The gate is locked tight.");
        } else if (e->code == E_HERE) ui_toast("Nobody's here right now.");
        else if (e->code == E_EXORCISE) ui_toast("Nobody is possessed. Praise be!");
        else if (e->code == E_TREAT) ui_toast("Everyone is healthy!");
        else if (e->code == E_MOVE_IN || e->code == E_CREATE_HILL || e->code == E_CREATE_HELL)
            ui_toast("No vacancies on that side!");
        return;
    }
    if (e->code != E_CLOSE) audio_sfx(SFX_SELECT);
    switch (e->code) {
    case E_CLOSE: I.menu_open = 0; audio_sfx(SFX_CANCEL); break;
    case E_APTS: scene_push(&SCENE_APARTMENTS, e->arg); break;
    case E_MOVE_IN: case E_CREATE_HILL: case E_CREATE_HELL: {
        int d = e->code == E_CREATE_HELL || (e->code == E_MOVE_IN && e->arg);
        I.wait = FP_WAIT_CREATOR;
        scene_push(&SCENE_CREATOR, d ? -2 : -1);
        break; }
    case E_AVATAR: I.wait = FP_WAIT_OTHER; scene_push(&SCENE_CREATOR, -3); break;
    case E_SHOP: scene_push(&SCENE_SHOP, FP_SHOP_ARG_SHOP | (e->arg << 4)); break;
    case E_CAFE_SHOP: scene_push(&SCENE_SHOP, FP_SHOP_ARG_CAFE); break;
    case E_BLACK_MARKET: scene_push(&SCENE_SHOP, FP_SHOP_ARG_BLACK_MARKET); break;
    case E_EXORCISE: fp_pick("Who needs saving?", PICK_POSSESSED, -1, cb_exorcise); break;
    case E_PRACTICE: I.wait = FP_WAIT_EXORCISM; scene_push(&SCENE_EXORCISM, -1); break;
    case E_FUZZ_TALK: fuzz_talk(); break;
    case E_TREAT: fp_pick("Who's feeling sick?", PICK_SICK, -1, cb_treat); break;
    case E_WORKSHOP: scene_push(&SCENE_WORKSHOP, 0); break;
    case E_ARENA: scene_push(&SCENE_ARENA, 0); break;
    case E_GRAVES: scene_push(&SCENE_GRAVEYARD, 0); break;
    case E_STROLL:
        scene_push(&SCENE_STORY, unlock_mode_ok(MODE_UNITED_PARK) ? STORY_ARG_VISIT_PARK : STORY_ARG_VISIT_HAHALAND);
        break;
    case E_PICNIC: picnic(); break;
    case E_VISIT_HELL: scene_push(&SCENE_STORY, STORY_ARG_VISIT_HELLVILE); break;
    case E_PEEK_WALL: peek_wall(); break;
    case E_RENT: collect_rent(); break;
    case E_NAME_ISLAND:
        fp_strcpy(I.namebuf, fp_island_name(), sizeof(I.namebuf));
        ui_text_entry("Name your island", I.namebuf, FP_ISLAND_NAME_LEN, cb_name_island);
        break;
    case E_SAVELOAD: scene_push(&SCENE_SAVELOAD, FP_SL_ARG_MANAGE); break;
    case E_OPTIONS: scene_push(&SCENE_OPTIONS, 0); break;
    case E_UNLOCKS: scene_push(&SCENE_UNLOCKS, 0); break;
    case E_RECORDS: town_records(); break;
    case E_TITLE: ui_confirm("Back to the title screen? Unsaved changes will be lost!", cb_title); break;
    case E_NEWS: scene_push(&SCENE_NEWS, 0); break;
    case E_ROSTER: scene_push(&SCENE_ROSTER, FP_ROSTER_ARG_BROWSE); break;
    case E_PET: open_pet(); break;
    case E_HERE:
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "%s", fp_activity_name(e->arg));
        fp_pick_act(fp_tmp, e->arg, cb_open);
        break;
    case E_ACTIVITY: activity_launch(e->act); break;
    case E_SOON: fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_STAR, 8); audio_sfx(SFX_SQUEAK); break;
    case E_GENRE:
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Tonight is %s night! Fans are lining up.",
                       doomo_music_names[sim_concert_genre() % MUSIC_COUNT]);
        ui_say(-1, fp_tmp);
        break;
    default: break;
    }
}

/* ====================================================================== */
/* problems / crowds                                                        */
static void count_problems(void)
{
    int i, n = sim_problem_count();
    memset(I.prob, 0, sizeof(I.prob));
    for (i = 0; i < n; i++) {
        const DoomoProblem *p = sim_problem(i);
        int k;
        if (!p) continue;
        switch (p->kind) {
        case PROB_POSSESSED: case PROB_CORRUPTION: case PROB_NIGHTMARE: k = LOC_CHURCH; break;
        case PROB_SICK: k = LOC_HOSPITAL; break;
        case PROB_HUNGRY: k = LOC_CAFE; break;
        default: k = RES_DISTRICT(p->resident) ? LOC_PITS : LOC_TOWERS; break;
        }
        k = loc_index_of(k);
        if (I.prob[k] < 255) I.prob[k]++;
    }
}

static void scan_crowds(void)
{
    int k;
    for (k = 0; k < 125; k++) {
        const DoomoResident *r = &doomo_town.res[I.scan];
        if (fp_living_status(r->status) && r->activity < ACT_COUNT) I.here_acc[r->activity]++;
        if (++I.scan >= DOOMO_MAX_RESIDENTS) {
            I.scan = 0;
            memcpy(I.here, I.here_acc, sizeof(I.here));
            memset(I.here_acc, 0, sizeof(I.here_acc));
        }
    }
}

static void full_scan(void)
{
    int i;
    memset(I.here_acc, 0, sizeof(I.here_acc));
    I.scan = 0;
    for (i = 0; i < 8; i++) scan_crowds();
    count_problems();
}

/* ====================================================================== */
/* navigation                                                               */
static void loc_screen(int i, int *x, int *y)
{
    *x = fp_mx(locs[i].mx);
    *y = fp_my(locs[i].my);
}

static int move_dir(int from, int dx, int dy)
{
    int i, best = -1, best_s = 1 << 30, fx, fy;
    loc_screen(from, &fx, &fy);
    for (i = 0; i < NLOCS; i++) {
        int x, y, ax, ay, along, side, s;
        if (i == from) continue;
        loc_screen(i, &x, &y);
        ax = x - fx;
        ay = y - fy;
        along = dx ? ax * dx : ay * dy;
        side = dx ? (ay < 0 ? -ay : ay) : (ax < 0 ? -ax : ax);
        if (along <= 0 || side > along * 2 + 8) continue;
        s = along + side * 2;
        if (s < best_s) { best_s = s; best = i; }
    }
    return best < 0 ? from : best;
}

static void open_menu(const Loc *L)
{
    I.mloc = L;
    I.menu_open = 1;
    I.menu_t = 0;
    I.esel = 0;
    I.etop = 0;
    full_scan();
    build_menu(L);
    audio_sfx(SFX_DOOR);
}

/* ====================================================================== */
/* scene                                                                    */
static void island_enter(int arg)
{
    int keep = I.sel;
    memset(&I, 0, sizeof(I));
    isp = NULL;
    isl_ready();
    I.sel = (keep > 0 && keep < NLOCS) ? keep : loc_index_of(LOC_TOWN_HALL);
    fp_island_geometry();
    full_scan();
    I.song = fp_island_song();
    audio_music(I.song);
    if (arg == FP_ISLAND_ARG_WELCOME) {
        char nm[24];
        fp_island_title(nm, sizeof(nm));
        if (town_count(-1) == 0) {
            doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Welcome to %s! It's very quiet... nobody lives here yet.", nm);
            ui_say(-1, fp_tmp);
            ui_say(-1, "Let's create your very first citizen! You can make up to 500 on each side.");
            I.wait = FP_WAIT_CREATOR;
            I.wait_arg = 1;
        } else {
            doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Welcome to %s! %d citizens live here. Visit them, feed them, love them!",
                           nm, town_count(-1));
            ui_say(-1, fp_tmp);
            ui_say(-1, "Thought bubbles show who needs help. A visits a place, START opens the menu.");
        }
        fp_fx_burst(fp_sw() / 2, fp_sh() / 3, FX_CONFETTI, 30);
    }
}

static void after_wait(void)
{
    int w = I.wait, first = I.wait_arg;
    I.wait = FP_WAIT_NONE;
    I.wait_arg = 0;
    full_scan();
    if (I.menu_open) build_menu(I.mloc);
    if (w == FP_WAIT_CREATOR) {
        int id = first ? -1 : creator_last_result();
        if (first) {
            /* the welcome dialogue is over: off to the creator */
            I.wait = FP_WAIT_CREATOR;
            scene_push(&SCENE_CREATOR, -1);
            return;
        }
        if (id >= 0 && id < DOOMO_MAX_RESIDENTS) {
            doomo_snprintf(fp_tmp, sizeof(fp_tmp), STR_ICON_HEART " %s moved in!", doomo_town.res[id].name);
            ui_toast(fp_tmp);
            fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_CONFETTI, 24);
            audio_sfx(SFX_HAPPY);
            if (fp_is_freeplay()) fp_achieve(UL_FP_FIRST_CITIZEN);
            fp_check_island_achievements();
            if (town_count(-1) == 1)
                ui_say(-1, "Your first citizen! Visit them in their home, or make some friends for them.");
        }
    }
}

static void menu_update(void)
{
    int vis = fp_big() ? 11 : 7;
    I.menu_t++;
    if ((I.menu_t & 31) == 0) { int s = I.esel; build_menu(I.mloc); I.esel = DOOMO_MIN(s, I.nent - 1); }
    if (btn_repeat(BTN_UP)) { I.esel = (I.esel + I.nent - 1) % I.nent; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { I.esel = (I.esel + 1) % I.nent; audio_sfx(SFX_CURSOR); }
    if (I.esel < I.etop) I.etop = I.esel;
    if (I.esel >= I.etop + vis) I.etop = I.esel - vis + 1;
    if (btn_pressed(BTN_A)) { run_entry(&isl()->ent[I.esel]); return; }
    if (btn_pressed(BTN_B)) { I.menu_open = 0; audio_sfx(SFX_CANCEL); }
}

/* Make sure the scratch is ours and an open menu has its entries. */
static void isl_ready(void)
{
    isl();
    if (I.menu_dirty) {
        I.menu_dirty = 0;
        if (I.menu_open && I.mloc) {
            int s = I.esel;
            build_menu(I.mloc);
            I.esel = DOOMO_MIN(s, I.nent - 1);
        }
    }
}

static void island_update(void)
{
    int old = I.sel;
    isl_ready();
    fp_live();
    if (I.wait) { after_wait(); if (I.wait) return; }
    scan_crowds();
    if ((fp_t() & 15) == 0) count_problems();
    if ((fp_t() % 300) == 7) {
        fp_check_island_achievements();
        if (fp_is_freeplay() && town_hour() < 4) fp_achieve(UL_FP_NIGHT_OWL);
    }
    if (fp_island_song() != I.song) { I.song = fp_island_song(); audio_music(I.song); }
    if (I.menu_open) { menu_update(); return; }
    I.idle++;
    if (btn_repeat(BTN_LEFT)) I.sel = move_dir(I.sel, -1, 0);
    if (btn_repeat(BTN_RIGHT)) I.sel = move_dir(I.sel, 1, 0);
    if (btn_repeat(BTN_UP)) I.sel = move_dir(I.sel, 0, -1);
    if (btn_repeat(BTN_DOWN)) I.sel = move_dir(I.sel, 0, 1);
    if (I.sel != old) { audio_sfx(SFX_CURSOR); I.idle = 0; }
    if (btn_pressed(BTN_A)) {
        const Loc *L = &locs[I.sel];
        I.idle = 0;
        if (L->kind == LOC_PET) { audio_sfx(SFX_SELECT); open_pet(); return; }
        if (L->kind == LOC_ROSTER) { audio_sfx(SFX_SELECT); scene_push(&SCENE_ROSTER, 0); return; }
        open_menu(L);
        return;
    }
    if (btn_pressed(BTN_START | BTN_B)) { open_menu(&menu_loc); return; }
    if (btn_pressed(BTN_R)) { audio_sfx(SFX_SELECT); scene_push(&SCENE_ROSTER, 0); return; }
    if (btn_pressed(BTN_L)) { audio_sfx(SFX_SELECT); scene_push(&SCENE_NEWS, 0); return; }
    if (btn_pressed(BTN_SELECT)) { audio_sfx(SFX_SELECT); open_pet(); return; }
}

/* ====================================================================== */
/* drawing                                                                  */
static void draw_round_button(int cx, int cy, int icon, const char *label, int sel, int alert)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int t = fp_t(), b = sel ? (ui_wave(t, 20, 2) > 0) * 2 : 0;
    char g[2] = { (char)icon, 0 };
    if (alert) b += (t / 4) % 4 == 0;
    ui_fill_circle(cx + 1, cy + 2, 10, PAL_RAMP(RAMP_SKY, 3));
    ui_fill_circle(cx, cy - b, 10, sel ? st->accent : st->outline);
    ui_fill_circle(cx, cy - b, 9, sel ? ui_lighter(st->accent) : st->fill_hi);
    ui_fill_circle(cx - 3, cy - 4 - b, 3, PAL_WHITE);
    ui_text_big(cx - 6, cy - 8 - b, g, 2, sel ? PAL_WHITE : st->accent, st->outline);
    if (fp_big() || sel) {
        int w = ui_text_width(label);
        ui_text_outline(cx - w / 2, cy + 10, label, PAL_WHITE, st->outline);
    }
    if (alert) {
        ui_fill_circle(cx + 8, cy - 8 - b, 4, PAL_UI_RED_D);
        ui_fill_circle(cx + 8, cy - 8 - b, 3, PAL_UI_RED);
        plat_text(cx + 6, cy - 12 - b, "!", PAL_WHITE);
    }
}

static void draw_plate(int i)
{
    const Loc *L = &locs[i];
    const UiStyle *st = ui_style(WIN_CUTE);
    int x, y, w, px, py, t = fp_t(), sw = fp_sw();
    char buf[48];
    loc_screen(i, &x, &y);
    if (L->kind == LOC_ARENA && arena_locked())
        doomo_snprintf(buf, sizeof(buf), "%s " STR_ICON_X, L->name);
    else if (L->kind == LOC_TOWN_HALL && town_count(-1) == 0)
        doomo_snprintf(buf, sizeof(buf), "%s: make citizens!", L->name);
    else if (L->kind == LOC_PET)
        doomo_snprintf(buf, sizeof(buf), "%s", pet_exists() ? doomo_town.pet.name : "Adopt a pet!");
    else
        doomo_snprintf(buf, sizeof(buf), "%s", L->name);
    w = ui_text_width(buf) + 14;
    px = DOOMO_CLAMP(x - w / 2, 2, sw - w - 2);
    py = y - (L->kind >= LOC_COUNT ? 30 : 38);
    if (py < 18) py = y + 4;
    fp_shadow(px, py, w, 12);
    fp_pill(px, py, w, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
    ui_text_shadow(px + 7, py + 2, buf, st->title, st->outline);
    ui_cursor(px - 9, py + 2);
    ui_sparkle(px + w - 2, py, (t / 3) & 15, PAL_WHITE);
}

static void draw_bubbles(const FpLocDraw *ld)
{
    int i, t = fp_t();
    for (i = 0; i < NLOCS; i++) {
        int n = I.prob[i], x = ld[i].x, y = ld[i].y;
        char num[6];
        if (!n || ld[i].kind >= LOC_COUNT) continue;
        fp_bubble(x + 6, y - 20, n > 1 ? ICON_CLOUD : ICON_HEART,
                  locs[i].kind == LOC_CHURCH ? PAL_UI_RED : PP(PAINT_ROSE, 1), t + i * 9);
        doomo_snprintf(num, sizeof(num), "%d", n);
        if (n > 1) ui_text_outline(x + 17, y - 30 + ui_wave(t + i * 9, 40, 1), num, PAL_WHITE, PAL_UI_RED_D);
    }
}

static void draw_menu(void)
{
    const Loc *L = I.mloc;
    int sw = fp_sw(), sh = fp_sh(), big = fp_big();
    int w = DOOMO_MIN(sw - 12, 300), h = big ? 190 : 128, x = (sw - w) / 2, y = big ? 26 : 18;
    int has_host = L->host != NULL, lx, lw, vis = big ? 11 : 7, i, t = fp_t();
    int style = (L->kind == LOC_PITS || L->kind == LOC_BLACK_MARKET || L->kind == LOC_SQUARE ||
                 L->kind == LOC_ARENA || L->kind == LOC_LAKE || L->kind == LOC_GRAVEYARD) ? WIN_DARK : WIN_CUTE;
    const UiStyle *st = ui_style(style);
    plat_shade_rect(0, 0, sw, sh, 11);
    if (!ui_window_pop(x, y, w, h, style, I.menu_t)) return;
    /* title */
    ui_title(x + w / 2, y - 7, L->name, style);
    lx = x + 8;
    lw = w - 16;
    if (has_host) {
        int rid = roster_find(L->host), fx = x + 6, fy = y + h - FIGURE_H - 6;
        if (rid >= 0) {
            const DoomoLook *look = fp_host_look(rid);
            ui_fill_ellipse(fx + FIGURE_W / 2, y + h - 9, 20, 3, st->fill_lo);
            fp_figure(fx, fy + (ui_wave(t, 60, 1) > 0), look, RES_ALIVE,
                      I.menu_t < 40 ? EXPR_TALK : EXPR_HAPPY);
        }
        lx = x + FIGURE_W + 12;
        lw = w - FIGURE_W - 18;
        /* the host's greeting */
        {
            char g[80];
            doomo_snprintf(g, sizeof(g), "%s: %s", L->host, L->greet ? L->greet : "Hi!");
            fp_pill(x + 6, y + 8, w - 12, 12, st->fill_hi, st->outline, PAL_WHITE);
            fp_marquee(x + 11, y + 10, w - 22, g, st->text_dim, I.menu_t);
        }
    }
    {
        int ly = y + (has_host ? 25 : 12);
        for (i = 0; i < vis && I.etop + i < I.nent; i++) {
            const Entry *e = &isl()->ent[I.etop + i];
            int ry = ly + i * 11, sel = I.etop + i == I.esel;
            uint8_t tc = e->enabled ? st->text : st->text_dim;
            if (sel) {
                fp_pill(lx + 8, ry - 1, lw - 8, 11, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
                ui_cursor(lx - 1, ry + 1);
            }
            fp_icon(lx + 11, ry + 1, e->icon, e->enabled ? st->accent : st->text_dim);
            fp_marquee(lx + 20, ry + 1, lw - 32, e->label, tc, sel ? t : 0);
            if (!e->enabled) {
                int kx = lx + lw - 9;
                plat_rect(kx + 1, ry + 1, 3, 1, st->text_dim);
                plat_rect(kx, ry + 2, 1, 2, st->text_dim);
                plat_rect(kx + 4, ry + 2, 1, 2, st->text_dim);
                plat_rect(kx - 1, ry + 4, 7, 4, st->text_dim);
            }
        }
        if (I.nent > vis)
            ui_scroll_arrows(lx + lw - 3, ly - 5, ly + vis * 11 - 2, I.etop > 0, I.etop + vis < I.nent);
    }
    /* the selected entry's description (or why it's locked) */
    {
        const Entry *e = &isl()->ent[I.esel];
        const char *d = e->desc;
        uint8_t c = st->text_dim;
        if (!e->enabled && e->code == E_ACTIVITY) { d = fp_activity_reason(e->act); c = PAL_UI_RED; }
        else if (!e->enabled && (e->code == E_ARENA || e->code == E_VISIT_HELL)) {
            const DoomoUnlock *u = unlock_find(UL_CAT_MODE, 0, e->code == E_ARENA ? MODE_ARENA :
                                               MODE_HELLVILE_VISITS);
            if (u && u->how) d = u->how;
            c = PAL_UI_RED;
        }
        fp_pill(lx, y + h - 22, lw, 18, st->fill_lo, st->outline, st->fill);
        ui_text_wrap(lx + 4, y + h - 21 + (ui_text_wrap(0, 0, lw - 8, d, 0, 0) > 1 ? 0 : 4), lw - 8, d, c, 1);
    }
}

static void island_draw(void)
{
    FpLocDraw ld[FP_LOC_MAX];
    int i, sw = fp_sw(), sh = fp_sh(), style = WIN_CUTE;
    isl_ready();
    for (i = 0; i < NLOCS; i++) {
        loc_screen(i, &ld[i].x, &ld[i].y);
        ld[i].kind = locs[i].kind;
        ld[i].locked = (uint8_t)(locs[i].kind == LOC_ARENA && arena_locked());
    }
    plat_clear(PAL_RAMP(RAMP_SKY, 5));
    fp_island_draw_map(ld, NLOCS, I.menu_open ? -1 : I.sel);
    draw_bubbles(ld);
    /* the corner buttons */
    for (i = 0; i < NLOCS; i++) {
        if (locs[i].kind == LOC_PET)
            draw_round_button(ld[i].x, ld[i].y - 10, ICON_EGG, "Pet", i == I.sel && !I.menu_open,
                              pet_exists() && pet_calling());
        else if (locs[i].kind == LOC_ROSTER)
            draw_round_button(ld[i].x, ld[i].y - 10, ICON_SMILE, "Folks", i == I.sel && !I.menu_open, 0);
    }
    /* an arrow that nags toward Town Hall on an empty island */
    if (town_count(-1) == 0 && !I.menu_open && locs[I.sel].kind != LOC_TOWN_HALL) {
        int k = loc_index_of(LOC_TOWN_HALL), b = ui_wave(fp_t(), 24, 2);
        plat_text(ld[k].x - 3, ld[k].y - 42 + b, STR_ICON_ARROW_D, PAL_UI_RED);
    }
    if (!I.menu_open) draw_plate(I.sel);
    fp_hud(style);
    {
        char nm[24];
        if (fp_island_name()[0] && fp_big()) {
            fp_island_title(nm, sizeof(nm));
            ui_text_outline(sw / 2 - ui_text_width(nm) / 2, 18, nm, PAL_WHITE, PAL_RAMP(RAMP_SKY, 2));
        }
    }
    if (!I.menu_open) fp_ticker(sh - 13, style);
    if (I.menu_open) draw_menu();
    fp_fx_draw();
}

const DoomoScene SCENE_ISLAND = { "ISLAND", island_enter, NULL, island_update, island_draw, 0 };

/* ---- test hooks ------------------------------------------------------------ */
int fp_island_select_kind(int kind) { I.sel = loc_index_of(kind); return I.sel; }
int fp_island_menu_open(void) { return I.menu_open; }
int fp_island_menu_count(void) { return I.nent; }
const char *fp_island_menu_label(int i) { return (i >= 0 && i < I.nent) ? isl()->ent[i].label : ""; }
int fp_island_menu_select(const char *label)
{
    int i;
    for (i = 0; i < I.nent; i++) if (strcmp(isl()->ent[i].label, label) == 0) { I.esel = i; return 1; }
    return 0;
}
