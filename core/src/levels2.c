/* levels2.c - the level registry (owner: levels; LV0 of the levels
 * expansion plan, docs/design/levels/LEVELS_EXPANSION_PLAN.md sections 2.5,
 * 7.1, 8.5 and 9): the table of all 31 levels (the seven built ones, Hell
 * Breach 8-16 and the fifteen New Game+ "Arrears" Notices), the page each
 * one sits on in the level select, the platform manifest (the GBA has the
 * seven old levels and seven of the new ones), the unlock rule of each
 * level and the text of its locked reason.
 *
 * Tables and pure lookups only: nothing here reads the save or the story
 * (levels.c evaluates the rules). A new level is registered here first
 * with map LV_NO_MAP: it is "not built", shows locked in the select and can
 * never be started. Its package (LV12 and later) fills in the map, the
 * boss, the intro tagline, the preview spot and the final par / gold
 * times; the par / gold here are the plan's (sections 3.0 and 5.0), and
 * Notice 15 has no par in the plan (5:00 / 3:30 stand in until LV37).
 */
#include "levels_int.h"

#define NOT_BUILT(par, dev, theme, accent, icon) \
    "", LV_NO_MAP, LB_NONE, par, dev, SONG_HELL_BREACH, theme, accent, icon, 0, 0, 0, 255

/* ---- the levels ------------------------------------------------------------- */
LV_HOSTVAR LevelInfo lv_info[LV_TABLE_N] = {
    { "The Vape Cellar", "HELL BREACH 1",
      "Under Cloud Nine Vapor: crates,\na crawlspace and a vape wraith.",
      "Smells like Brimstone Blast down here",
      MAPID_VAPE_CELLAR, LB_CELLAR_WRAITH, 150, 95, SONG_HELL_BREACH, WIN_CUTE,
      PAL_PAINT(PAINT_VAPE_PURPLE, 1), ICON_CLOUD, 6, 5, 3, 255 },
    { "Cloud Nine Spire", "HELL BREACH 2",
      "Vex's tower in the vape storm.\nLord Vapula waits at the top.",
      "Three phases of vapour and spite.",
      MAPID_CLOUD_NINE_SPIRE, LB_VAPULA, 300, 200, SONG_BOSS, WIN_HELL,
      PAL_UI_PURPLE, ICON_BOLT, 28, 50, 6, 255 },
    { "Sugar Rush Sewers", "HELL BREACH 3",
      "Strawberry soda canals... that\nturn into something fleshy.",
      "The soda down here tastes... meaty.",
      MAPID_SUGAR_RUSH_SEWERS, LB_GUTTERKING, 210, 140, SONG_HELL_BREACH, WIN_MINT,
      PAL_PAINT(PAINT_BUBBLEGUM, 1), ICON_DROP, 6, 9, 5, 200 },
    { "Plushie Factory", "HELL BREACH 4",
      "Conveyor belts, stuffing vats\nand plushies with demon eyes.",
      "Now hiring: souls. No experience!",
      MAPID_PLUSHIE_FACTORY, LB_FLUFFNUTS, 220, 150, SONG_INVASION, WIN_CREAM,
      PAL_UI_GOLD, ICON_WRENCH, 24, 9, 6, 200 },
    { "Graveyard Shift", "HELL BREACH 5",
      "A night at the cemetery. Crypts,\nghosts, a weeping in the garden.",
      "Somebody is crying in the garden...",
      MAPID_GRAVEYARD_SHIFT, LB_WIDOW, 260, 175, SONG_HELLVILE, WIN_DARK,
      PAL_PAINT(PAINT_ECTOPLASM, 1), ICON_GHOST, 17, 37, 5, 230 },
    { "Bone Cathedral", "HELL BREACH 6",
      "A gothic hell church that goes\nUP. Bell tower. Pipe organ.",
      "Let us pray. Loudly. With rockets.",
      MAPID_BONE_CATHEDRAL, LB_BONEBOTTOM, 280, 190, SONG_HELL_BREACH, WIN_HELL,
      PAL_RAMP(RAMP_BONE, 6), ICON_CROSS, 22, 33, 10, 200 },
    { "Hellvile Underbelly", "HELL BREACH 7",
      "Below the bad city: a market,\npipes, pistons, the hell gate.",
      "Mind the gap. He's undressing you.",
      MAPID_HELLVILE_UNDERBELLY, LB_GRUMBLEGUTS, 300, 200, SONG_INVASION, WIN_HELL,
      PAL_UI_RED, ICON_PENTAGRAM, 40, 7, 6, 230 },
#if LV_HAS_NEW
    /* ---- Hell Breach 8-16 (main game; plan section 3) ---- */
    { "Steam Room Panic", "HELL BREACH 8",
      "The Bath House after dark.\nFog, valves and angry towels.",
      NOT_BUILT(200, 130, WIN_MINT, PAL_PAINT(PAINT_CYAN, 1), ICON_DROP) },
    { "The Undercroft", "HELL BREACH 9",
      "Under St. Cuddles: one lantern,\nlittle oil. Light the candles.",
      NOT_BUILT(240, 165, WIN_DARK, PAL_PAINT(PAINT_HONEY, 1), ICON_CROSS) },
    { "Fright Night at the Fun Fair", "HELL BREACH 10",
      "The rides run themselves now.\nKeep your arms inside.",
      NOT_BUILT(270, 180, WIN_HELL, PAL_PAINT(PAINT_TANGERINE, 1), ICON_STAR) },
    { "The Glum Pipes", "HELL BREACH 11",
      "The sludge is rising.\nFind the relief valves first.",
      NOT_BUILT(230, 160, WIN_DARK, PAL_PAINT(PAINT_TEAL, 1), ICON_WRENCH) },
    { "The Wall", "HELL BREACH 12",
      "The Wall has two faces. Neither\none is happy to see you.",
      NOT_BUILT(250, 170, WIN_DARK, PAL_PAINT(PAINT_SILVER, 1), ICON_HOUSE) },
    { "Boneyard Buffet & Bandstand", "HELL BREACH 13",
      "Dinner and a show! Lights on:\nthe band holds very still.",
      NOT_BUILT(220, 150, WIN_CREAM, PAL_PAINT(PAINT_CHERRY, 1), ICON_NOTE) },
    { "Hellvile Pits Highrise", "HELL BREACH 14",
      "Tall towers, one landlord and\nlaundry lines you can ride.",
      NOT_BUILT(260, 175, WIN_HELL, PAL_PAINT(PAINT_RUST, 1), ICON_HOUSE) },
    { "The Department of Contracts", "HELL BREACH 15",
      "Take a number. Get it stamped.\nThen stamp back.",
      NOT_BUILT(210, 140, WIN_CREAM, PAL_PAINT(PAINT_CARAMEL, 1), ICON_CHECK) },
    { "The Ninth Cloud", "HELL BREACH 16",
      "A dream above the storm.\nGravity is on its lunch break.",
      NOT_BUILT(240, 160, WIN_CUTE, PAL_PAINT(PAINT_SKY, 1), ICON_CLOUD) },
    /* ---- Arrears, Notices 1-15 (New Game+; plan section 5) ---- */
    { "Day One", "NOTICE 1",
      "Both hands full of bricks.\nBridge the gaps, seal the holes.",
      NOT_BUILT(180, 130, WIN_DARK, PAL_PAINT(PAINT_CHARCOAL, 1), ICON_HOUSE) },
    { "The Dead Letter Office", "NOTICE 2",
      "Every letter that never came.\nRide the tubes, mind the levers.",
      NOT_BUILT(180, 120, WIN_CREAM, PAL_PAINT(PAINT_PEACH, 1), ICON_BOLT) },
    { "Repo Night", "NOTICE 3",
      "The tow hooks want your gun.\nThe impound cages have it.",
      NOT_BUILT(220, 150, WIN_HELL, PAL_PAINT(PAINT_NAVY, 1), ICON_GUN) },
    { "The Interest Engine", "NOTICE 4",
      "Leave a debt alone and it\ndoubles. Do not leave it alone.",
      NOT_BUILT(230, 160, WIN_HELL, PAL_PAINT(PAINT_FOREST, 1), ICON_COIN) },
    { "Lake of Fire Regatta", "NOTICE 5",
      "One raft, one lake of lava.\nSteer with the current.",
      NOT_BUILT(260, 180, WIN_HELL, PAL_PAINT(PAINT_HELLFIRE, 1), ICON_FIRE) },
    { "Everything Must Go", "NOTICE 6",
      "The big sale. You can buy some\nof it back. Not all of it.",
      NOT_BUILT(220, 150, WIN_CREAM, PAL_PAINT(PAINT_LEMON, 1), ICON_COIN) },
    { "The Audit", "NOTICE 7",
      "Nobody gets hurt in an audit.\nYour shots are objections.",
      NOT_BUILT(180, 135, WIN_CREAM, PAL_PAINT(PAINT_COBALT, 1), ICON_CHECK) },
    { "The Debtors' Prison", "NOTICE 8",
      "The wardens are asleep.\nKeep it that way.",
      NOT_BUILT(240, 165, WIN_DARK, PAL_PAINT(PAINT_JET, 1), ICON_ZZZ) },
    { "The Artist's Studio", "NOTICE 9",
      "You left your body at the door.\nBorrow a stunned demon's.",
      NOT_BUILT(250, 170, WIN_DARK, PAL_PAINT(PAINT_LAVENDER, 1), ICON_GHOST) },
    { "The Ward of Forgotten Names", "NOTICE 10",
      "A room without a name fades.\nBring name tags.",
      NOT_BUILT(230, 160, WIN_DARK, PAL_PAINT(PAINT_ECTOPLASM, 1), ICON_CROSS) },
    { "The Hollow Mingle", "NOTICE 11",
      "Walk a friend across the\nfestival that nobody came to.",
      NOT_BUILT(270, 190, WIN_DARK, PAL_PAINT(PAINT_GRAPE, 1), ICON_HEART) },
    { "The Interest", "NOTICE 12",
      "It cannot be stopped and the\nfloor is leaving. Run.",
      NOT_BUILT(160, 110, WIN_HELL, PAL_PAINT(PAINT_BLOOD, 1), ICON_BOLT) },
    { "The Vault of Unfelt Things", "NOTICE 13",
      "The doors open when you stand\nstill. Company is coming.",
      NOT_BUILT(270, 190, WIN_DARK, PAL_PAINT(PAINT_SILVER, 1), ICON_HEART) },
    { "The Ledger", "FINAL NOTICE",
      "Seven pages. Every rule again.\nThen guard the jar.",
      NOT_BUILT(540, 390, WIN_HELL, PAL_PAINT(PAINT_MAGENTA, 1), ICON_PENTAGRAM) },
    { "Both Are Better", "PAID IN FULL",
      "No demons tonight.\nLight the sky lanterns.",
      NOT_BUILT(300, 210, WIN_CUTE, PAL_PAINT(PAINT_ROSE, 1), ICON_SPARK) },
#endif
};

#if LV_HAS_NEW          /* (the PS1 has the level table above and nothing of this, for now: LV_TABLE_N) */
/* ---- the registry: page, manifest, unlock rule -------------------------------
 * The old seven: the cellar is always open; the spire and the sewers after
 * the cellar; then one after the other (as before LV0). Hell Breach 8-16
 * open when their story episode is done (LVK_BEAT, plan 3.0; a later chapter
 * counts too, so an old save that is further on is not locked out), the
 * Notices in New Game+ by chapter (Notices 1-6) and then one after the other
 * (plan 5.0). Where the plan names an episode the arg is that BEAT_EP*; the
 * level packages (LV12-LV21) and the arc (LV22) re-point a row when the
 * story lands a better beat or flag. A Notice after one that is not on
 * this platform follows that one's own rule (lv_rule_eval). */
const LevelReg lv_reg[LV_TABLE_N] = {
    { LVP_BREACH, LVG_GBA, LVK_ALWAYS, 0, LVT_NONE },
    { LVP_BREACH, LVG_GBA, LVK_AFTER_LEVEL, LV_VAPE_CELLAR, LVT_CLEAR_FIRST },
    { LVP_BREACH, LVG_GBA, LVK_AFTER_LEVEL, LV_VAPE_CELLAR, LVT_CLEAR_FIRST },
    { LVP_BREACH, LVG_GBA, LVK_AFTER_LEVEL, LV_SEWERS, LVT_CLEAR_FIRST },
    { LVP_BREACH, LVG_GBA, LVK_AFTER_LEVEL, LV_FACTORY, LVT_CLEAR_FIRST },
    { LVP_BREACH, LVG_GBA, LVK_AFTER_LEVEL, LV_GRAVEYARD, LVT_CLEAR_FIRST },
    { LVP_BREACH, LVG_GBA, LVK_AFTER_LEVEL, LV_CATHEDRAL, LVT_CLEAR_FIRST },
    { LVP_HELL, LVG_GBA, LVK_BEAT, BEAT_EP2_6, LVT_CH2 },                     /* Steam Room Panic */
    { LVP_HELL, LVG_GBA | LVG_DARK, LVK_BEAT, BEAT_EP3_4, LVT_CH3 },          /* The Undercroft */
    { LVP_HELL, 0, LVK_CHAPTER, STORY_CH3, LVT_CH3 },                         /* Fright Night at the Fun Fair */
    { LVP_HELL, LVG_DARK, LVK_BEAT, BEAT_EP3_4, LVT_CH3 },                    /* The Glum Pipes */
    { LVP_HELL, LVG_DARK, LVK_BEAT, BEAT_EP4_1, LVT_CH4 },                    /* The Wall */
    { LVP_HELL, LVG_GBA, LVK_BEAT, BEAT_EP4_3, LVT_CH4 },                     /* Boneyard Buffet & Bandstand */
    { LVP_HELL, 0, LVK_BEAT, BEAT_EP4_4, LVT_CH4 },                           /* Hellvile Pits Highrise */
    { LVP_HELL, LVG_GBA, LVK_BEAT, BEAT_EP4_5, LVT_CH4 },                     /* The Department of Contracts */
    { LVP_HELL, 0, LVK_CHAPTER, STORY_CH_DONE, LVT_POSTGAME },                /* The Ninth Cloud */
    { LVP_ARREARS, LVG_GBA | LVG_DARK, LVK_NGPLUS, 0, LVT_NGPLUS },           /* Day One */
    { LVP_ARREARS, 0, LVK_CHAPTER, STORY_CH1, LVT_NG_CH1 },                   /* The Dead Letter Office */
    { LVP_ARREARS, 0, LVK_CHAPTER, STORY_CH2, LVT_NG_CH2 },                   /* Repo Night */
    { LVP_ARREARS, 0, LVK_CHAPTER, STORY_CH3, LVT_NG_CH3 },                   /* The Interest Engine */
    { LVP_ARREARS, 0, LVK_CHAPTER, STORY_CH4, LVT_NG_CH4 },                   /* Lake of Fire Regatta */
    { LVP_ARREARS, 0, LVK_CHAPTER, STORY_CH4, LVT_NG_CH4 },                   /* Everything Must Go */
    { LVP_ARREARS, LVG_GBA, LVK_AFTER_LEVEL, LV_N6, LVT_NOTICE_BEFORE },      /* The Audit */
    { LVP_ARREARS, LVG_DARK, LVK_AFTER_LEVEL, LV_N7, LVT_NOTICE_BEFORE },     /* The Debtors' Prison */
    { LVP_ARREARS, LVG_DARK, LVK_AFTER_LEVEL, LV_N8, LVT_NOTICE_BEFORE },     /* The Artist's Studio */
    { LVP_ARREARS, LVG_DARK, LVK_AFTER_LEVEL, LV_N9, LVT_NOTICE_BEFORE },     /* The Ward of Forgotten Names */
    { LVP_ARREARS, LVG_DARK, LVK_AFTER_LEVEL, LV_N10, LVT_NOTICE_BEFORE },    /* The Hollow Mingle */
    { LVP_ARREARS, 0, LVK_AFTER_LEVEL, LV_N11, LVT_NOTICE_BEFORE },           /* The Interest */
    { LVP_ARREARS, LVG_DARK, LVK_AFTER_LEVEL, LV_N12, LVT_NOTICE_BEFORE },    /* The Vault of Unfelt Things */
    { LVP_ARREARS, LVG_GBA, LVK_AFTER_LEVEL, LV_N13, LVT_NOTICE_BEFORE },     /* The Ledger */
    { LVP_ARREARS, 0, LVK_AFTER_LEVEL, LV_N14, LVT_NOTICE_BEFORE },           /* Both Are Better */
};

/* "%s" is the title of the level to clear first (LVT_CLEAR_FIRST only). */
const char *const lv_reason_text[] = {
    "",
    "Clear %s first",
    "Opens in Chapter 2",
    "Opens in Chapter 3",
    "Opens in Chapter 4",
    "Opens after the credits",
    "Opens in New Game+",
    "New Game+, Chapter 1",
    "New Game+, Chapter 2",
    "New Game+, Chapter 3",
    "New Game+, Chapter 4",
    "Clear the Notice before it",
    "Not built yet",
};

const char *const lv_page_names[LVP_COUNT] = { "Breach 1-7", "Hell Breach 8-16", "Arrears" };

/* ---- the platform manifest ---------------------------------------------------- */
#if !defined(DOOMO_LEVELS_GBA_SET) && !defined(DOOMO_PLATFORM_PS1)
uint8_t lv_list_req;                          /* host: 0 = every level; tests set LVG_GBA */
#endif

int lv_listed(int level)
{
    if (level < 0 || level >= LV_TABLE_N) return 0;
    return (lv_reg[level].flags & LV_LIST_REQ) == LV_LIST_REQ;
}

int lv_listed_count(void)
{
    int i, n = 0;
    for (i = 0; i < LV_TABLE_N; i++) n += lv_listed(i);
    return n;
}

/* The record of new level `level` in 'BRCH': the listed new levels in id
 * order (all 24, or the GBA's seven). */
int lv_rec_slot(int level)
{
    int i, n = 0;
    if (level < LV_OLD_COUNT || !lv_listed(level)) return -1;
    for (i = LV_OLD_COUNT; i < level; i++) n += lv_listed(i);
    return n < LV_REC_N ? n : -1;
}

int lv_built(int level) { return lv_listed(level) && lv_info[level].map != LV_NO_MAP; }

int lv_page_of(int level) { return level >= 0 && level < LV_TABLE_N ? lv_reg[level].page : LVP_BREACH; }
#endif /* LV_HAS_NEW */
