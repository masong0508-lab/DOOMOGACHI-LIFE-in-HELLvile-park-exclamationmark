/* creator_data.c - the creator's const tables (owner: creator): the nine
 * tabs and their rows (every byte of DoomoLook and every identity field of
 * DoomoResident), 72 species presets (3 per species), outfit and voice
 * presets, personality texts, tab icons and the creator's own
 * unlockables (ids 256..319). All of it stays in ROM.
 */
#include "creator_int.h"

#define OFS(m)  ((uint8_t)offsetof(DoomoResident, m))
#define LK(m)   OFS(look.m)
#define END     CR_NOFIELD

/* ====================================================================== */
/* rows                                                                     */
#define HDR(lbl)                    { RK_HEADER, CR_NOFIELD, 0, 0, CR_NOLIST, 0, lbl, NULL }
#define ENUM_L(lbl, m, n, nm, cl)   { RK_ENUM, LK(m), n, 0, cl, 0, lbl, nm }
#define ENUM_R(lbl, m, n, nm)       { RK_ENUM, OFS(m), n, 0, CR_NOLIST, 0, lbl, nm }
#define SLIDE_L(lbl, m, n, nm)      { RK_SLIDER, LK(m), n, 0, CR_NOLIST, 0, lbl, nm }
#define SLIDE_R(lbl, m, n, nm)      { RK_SLIDER, OFS(m), n, 0, CR_NOLIST, 0, lbl, nm }
#define PAINT_L(lbl, m)             { RK_PAINT, LK(m), PAL_PAINT_COUNT, 0, CR_NOLIST, 0, lbl, NULL }
#define PAINT_R(lbl, m)             { RK_PAINT, OFS(m), PAL_PAINT_COUNT, 0, CR_NOLIST, 0, lbl, NULL }
#define CHEEK(lbl, bit)             { RK_FLAG, LK(cheeks), 2, 0, CR_NOLIST, bit, lbl, NULL }
#define ACTION(lbl, a)              { RK_ACTION, CR_NOFIELD, 0, 0, CR_NOLIST, a, lbl, NULL }

static const char *const n_size[2]      = { "Small", "Big" };
static const char *const n_height[2]    = { "Short", "Tall" };
static const char *const n_build[2]     = { "Skinny", "Chonky" };
static const char *const n_eye_size[2]  = { "Tiny", "Huge" };
static const char *const n_spacing[2]   = { "Close", "Wide" };
static const char *const n_lowhigh[2]   = { "Low", "High" };
static const char *const n_tilt[2]      = { "Droopy", "Sharp" };
static const char *const n_horn[2]      = { "Nubbly", "Mighty" };
static const char *const n_eye_count[3] = { "One", "Two", "Three" };
static const char *const n_fangs[4]     = { "None", "Tiny", "Pointy", "Vampy" };
static const char *const n_pitch[2]     = { "Deep", "Squeaky" };
static const char *const n_speed[2]     = { "Slow", "Fast" };
static const char *const n_quality[2]   = { "Thin", "Round" };
static const char *const n_inton[2]     = { "Flat", "Sing-song" };
static const char *const n_accent[2]    = { "Plain", "Fancy" };
static const char *const n_move[2]      = { "Slow", "Quick" };
static const char *const n_speech[2]    = { "Polite", "Blunt" };
static const char *const n_expr[2]      = { "Flat", "Expressive" };
static const char *const n_attitude[2]  = { "Serious", "Easygoing" };
static const char *const n_quirk[2]     = { "Normal", "Unhinged" };
static const char *const n_born[2]      = { "Hillvile", "Hellvile" };
static const char *const n_yesno[2]     = { "No", "Yes" };

static const CrRow rows_body[] = {
    HDR("Species"),
    ENUM_L("Species", species, SPECIES_COUNT, doomo_species_names, CLIST_SPECIES),
    ACTION("Preset", CA_SPECIES_PRESET),
    HDR("Body"),
    ENUM_L("Body", body, BODY_COUNT, doomo_body_names, CLIST_BODY),
    SLIDE_L("Height", height, 16, n_height),
    SLIDE_L("Build", build, 16, n_build),
    { RK_SKIN, LK(skin), 8 + PAL_PAINT_COUNT, 0, CR_NOLIST, 0, "Skin", NULL },
};

static const CrRow rows_face[] = {
    HDR("Head"),
    ENUM_L("Shape", head_shape, HEAD_COUNT, doomo_head_names, CLIST_HEAD),
    SLIDE_L("Head size", head_size, 16, n_size),
    HDR("Eyes"),
    ENUM_L("Eyes", eye_style, EYE_COUNT, doomo_eye_names, CLIST_EYE),
    PAINT_L("Eye colour", eye_color),
    SLIDE_L("Eye size", eye_size, 16, n_eye_size),
    SLIDE_L("Spacing", eye_spacing, 16, n_spacing),
    SLIDE_L("Eye height", eye_height, 16, n_lowhigh),
    SLIDE_L("Tilt", eye_tilt, 16, n_tilt),
    { RK_ENUM, LK(eye_count), 3, 1, CR_NOLIST, 0, "How many", n_eye_count },
    HDR("Brows"),
    ENUM_L("Brows", brow_style, BROW_COUNT, doomo_brow_names, CLIST_BROW),
    SLIDE_L("Brow height", brow_height, 16, n_lowhigh),
    HDR("Nose & mouth"),
    ENUM_L("Nose", nose_style, NOSE_COUNT, doomo_nose_names, CLIST_NOSE),
    SLIDE_L("Nose size", nose_size, 16, n_size),
    ENUM_L("Mouth", mouth_style, MOUTH_COUNT, doomo_mouth_names, CLIST_MOUTH),
    SLIDE_L("Mouth size", mouth_size, 16, n_size),
    ENUM_L("Fangs", fangs, 4, n_fangs, CR_NOLIST),
    HDR("Ears & cheeks"),
    ENUM_L("Ears", ears, EARS_COUNT, doomo_ear_names, CLIST_EARS),
    CHEEK("Blush", CHEEK_BLUSH),
    CHEEK("Freckles", CHEEK_FRECKLES),
    CHEEK("Scar", CHEEK_SCAR),
    CHEEK("Bandage", CHEEK_BANDAGE),
    CHEEK("Beauty mark", CHEEK_BEAUTY_MARK),
    CHEEK("Tear tattoo", CHEEK_TEAR_TATTOO),
    CHEEK("Dimples", CHEEK_DIMPLES),
    CHEEK("Sparkles", CHEEK_SPARKLES),
};

static const CrRow rows_hair[] = {
    HDR("Hair"),
    ENUM_L("Style", hair_style, HAIR_COUNT, doomo_hair_names, CLIST_HAIR),
    PAINT_L("Colour", hair_color),
    { RK_PAINT_NONE, LK(hair_highlight), PAL_PAINT_COUNT + 1, 0, CR_NOLIST, 0, "Highlight", NULL },
    ACTION("Match fur", CA_MATCH_FUR),
    HDR("Face fluff"),
    ENUM_L("Facial hair", facial_hair, FHAIR_COUNT, doomo_fhair_names, CLIST_FHAIR),
};

static const CrRow rows_spooky[] = {
    HDR("Horns"),
    ENUM_L("Horns", horns, HORNS_COUNT, doomo_horn_names, CLIST_HORNS),
    PAINT_L("Horn colour", horn_color),
    SLIDE_L("Horn size", horn_size, 16, n_horn),
    HDR("Tail & wings"),
    ENUM_L("Tail", tail, TAIL_COUNT, doomo_tail_names, CLIST_TAIL),
    ENUM_L("Wings", wings, WINGS_COUNT, doomo_wing_names, CLIST_WINGS),
    ENUM_L("Halo", halo, HALO_COUNT, doomo_halo_names, CLIST_HALO),
    HDR("Markings"),
    ENUM_L("Marking", marking, MARK_COUNT, doomo_mark_names, CLIST_MARK),
    PAINT_L("Mark colour", marking_color),
    HDR("Aura"),
    ENUM_L("Aura", aura, AURA_COUNT, doomo_aura_names, CLIST_AURA),
};

static const CrRow rows_outfit[] = {
    ACTION("Preset", CA_OUTFIT_PRESET),
    HDR("Top"),
    ENUM_L("Top", top, TOP_COUNT, doomo_top_names, CLIST_TOP),
    PAINT_L("Top colour", top_color),
    ENUM_L("Print", pattern, PATTERN_COUNT, doomo_pattern_names, CLIST_PATTERN),
    PAINT_L("Print colour", pattern_color),
    HDR("Bottom & shoes"),
    ENUM_L("Bottom", bottom, BOTTOM_COUNT, doomo_bottom_names, CLIST_BOTTOM),
    PAINT_L("Bottom col.", bottom_color),
    ENUM_L("Shoes", shoes, SHOES_COUNT, doomo_shoes_names, CLIST_SHOES),
    PAINT_L("Shoe colour", shoes_color),
    HDR("Hat & extras"),
    ENUM_L("Hat", hat, HAT_COUNT, doomo_hat_names, CLIST_HAT),
    PAINT_L("Hat colour", hat_color),
    ENUM_L("Accessory", accessory, ACC_COUNT, doomo_acc_names, CLIST_ACC),
    PAINT_L("Acc. colour", accessory_color),
};

static const CrRow rows_voice[] = {
    ACTION("Preset", CA_VOICE_PRESET),
    HDR("Voice"),
    SLIDE_R("Pitch", voice[VOICE_PITCH], 32, n_pitch),
    SLIDE_R("Speed", voice[VOICE_SPEED], 32, n_speed),
    SLIDE_R("Quality", voice[VOICE_QUALITY], 32, n_quality),
    SLIDE_R("Intonation", voice[VOICE_INTONATION], 32, n_inton),
    SLIDE_R("Accent", voice[VOICE_ACCENT], 32, n_accent),
    ACTION("Test it!", CA_TEST_VOICE),
};

static const CrRow rows_persona[] = {
    SLIDE_R("Movement", persona[PERS_MOVEMENT], 16, n_move),
    SLIDE_R("Speech", persona[PERS_SPEECH], 16, n_speech),
    SLIDE_R("Expression", persona[PERS_EXPRESSION], 16, n_expr),
    SLIDE_R("Attitude", persona[PERS_ATTITUDE], 16, n_attitude),
    SLIDE_R("Quirk", persona[PERS_QUIRK], 16, n_quirk),
};

static const CrRow rows_profile[] = {
    { RK_NAME, OFS(name), 0, 0, CR_NOLIST, 0, "Name", NULL },
    { RK_NICK, CR_NOFIELD, 0, 0, CR_NOLIST, 0, "Nickname", NULL },
    ENUM_R("Pronouns", pronoun, PRONOUN_COUNT, doomo_pronoun_names),
    ENUM_R("Crushes on", romance, ROMANCE_COUNT, doomo_romance_names),
    HDR("Birthday"),
    ENUM_R("Month", bday_month, 12, doomo_month_names),
    { RK_ENUM, OFS(bday_day), 28, 1, CR_NOLIST, 0, "Day", NULL },
    HDR("Likes & fears"),
    ENUM_R("Fav food", like_food, FOOD_COUNT, doomo_food_names),
    ENUM_R("Hated food", hate_food, FOOD_COUNT, doomo_food_names),
    ENUM_R("Hobby", like_hobby, HOBBY_COUNT, doomo_hobby_names),
    ENUM_R("Music", like_music, MUSIC_COUNT, doomo_music_names),
    PAINT_R("Fav colour", like_color),
    ENUM_R("Fear", fear, FEAR_COUNT, doomo_fear_names),
    ENUM_R("Catchphrase", catchphrase, CATCHPHRASE_COUNT, doomo_catchphrases),
};

static const CrRow rows_home[] = {
    { RK_DISTRICT, CR_NOFIELD, 2, 0, CR_NOLIST, 0, "Moves to", NULL },
    { RK_RFLAG, OFS(flags), 2, 0, CR_NOLIST, RF_HELLBORN, "Born in", n_born },
    { RK_RFLAG, OFS(flags), 2, 0, CR_NOLIST, RF_FAVORITE, "Favourite", n_yesno },
};

#define TAB(name, shortname, rows, paint) \
    { name, shortname, rows, (uint8_t)DOOMO_COUNTOF(rows), paint }

const CrTab cr_tabs[CR_TAB_COUNT] = {
    TAB("Species & Body", "Body", rows_body, PAINT_CARAMEL),
    TAB("Head & Face", "Face", rows_face, PAINT_PEACH),
    TAB("Hair", "Hair", rows_hair, PAINT_LAVENDER),
    TAB("Supernatural", "Spooky", rows_spooky, PAINT_GRAPE),
    TAB("Outfit", "Outfit", rows_outfit, PAINT_SKY),
    TAB("Voice", "Voice", rows_voice, PAINT_MINT),
    TAB("Personality", "Heart", rows_persona, PAINT_ROSE),
    TAB("Profile", "Profile", rows_profile, PAINT_LEMON),
    TAB("Home", "Home", rows_home, PAINT_TANGERINE),
};

/* ====================================================================== */
/* species presets                                                          */
#define K(m, v)  LK(m), (uint8_t)(v)
#define SK(p)    (8 + (p))
#define SP(s)    K(species, s)

const CrFieldDef cr_species_base[] = {
    { LK(skin), SKIN_PEACH }, { LK(head_shape), HEAD_ROUND },
    { LK(ears), EARS_NORMAL }, { LK(nose_style), NOSE_DOT },
    { LK(mouth_style), MOUTH_SMILE }, { LK(fangs), 0 },
    { LK(horns), HORNS_NONE }, { LK(horn_color), PAINT_BONE },
    { LK(tail), TAIL_NONE }, { LK(wings), WINGS_NONE }, { LK(halo), HALO_NONE },
    { LK(marking), MARK_NONE }, { LK(marking_color), PAINT_WHITE },
    { LK(eye_count), 2 },
    { CR_NOFIELD, 0 }
};

const CrPreset cr_species_presets[SPECIES_COUNT * CR_PRESETS_PER_SPECIES] = {
    /* ---- human ---- */
    { "Sunny Kid", { SP(SPECIES_HUMAN), K(skin, SKIN_PEACH), K(head_shape, HEAD_ROUND),
        K(ears, EARS_NORMAL), K(nose_style, NOSE_DOT), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_SKY), K(brow_style, BROW_THIN),
        K(hair_style, HAIR_BOB), K(hair_color, PAINT_CHOCOLATE),
        K(cheeks, CHEEK_BLUSH | CHEEK_FRECKLES), END } },
    { "Bookish", { SP(SPECIES_HUMAN), K(skin, SKIN_TAN), K(head_shape, HEAD_OVAL),
        K(nose_style, NOSE_BUTTON), K(eye_style, EYE_ROUND), K(eye_color, PAINT_CHOCOLATE),
        K(brow_style, BROW_ARCHED), K(hair_style, HAIR_BUN), K(hair_color, PAINT_JET),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Rockstar", { SP(SPECIES_HUMAN), K(skin, SKIN_BROWN), K(nose_style, NOSE_BUTTON),
        K(mouth_style, MOUTH_GRIN), K(eye_style, EYE_LASHES), K(eye_color, PAINT_HONEY),
        K(brow_style, BROW_RAISED), K(hair_style, HAIR_AFRO), K(hair_color, PAINT_GRAPE),
        K(hair_highlight, PAINT_BUBBLEGUM), K(cheeks, CHEEK_DIMPLES), END } },
    /* ---- imp ---- */
    { "Lil' Imp", { SP(SPECIES_IMP), K(skin, SK(PAINT_CHERRY)), K(ears, EARS_POINTY),
        K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_GRIN), K(fangs, 1),
        K(horns, HORNS_NUBS), K(horn_color, PAINT_BONE), K(tail, TAIL_DEVIL),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_LEMON), K(hair_style, HAIR_SPIKY),
        K(hair_color, PAINT_JET), K(cheeks, CHEEK_BLUSH), END } },
    { "Pocket Devil", { SP(SPECIES_IMP), K(skin, SK(PAINT_ROSE)), K(horns, HORNS_DEVIL),
        K(horn_color, PAINT_JET), K(wings, WINGS_TINY_BAT), K(eye_style, EYE_CAT),
        K(eye_color, PAINT_HONEY), K(hair_style, HAIR_MESSY), K(hair_color, PAINT_JET),
        K(mouth_style, MOUTH_SMIRK), K(fangs, 2), END } },
    { "Grape Gremlin", { SP(SPECIES_IMP), K(skin, SK(PAINT_GRAPE)), K(horns, HORNS_RAM),
        K(horn_color, PAINT_LAVENDER), K(eye_style, EYE_ANIME), K(eye_color, PAINT_LEMON),
        K(hair_style, HAIR_WILD), K(hair_color, PAINT_BUBBLEGUM),
        K(mouth_style, MOUTH_TONGUE), K(cheeks, CHEEK_BLUSH), END } },
    /* ---- cherub ---- */
    { "Cherub", { SP(SPECIES_CHERUB), K(skin, SKIN_PEACH), K(halo, HALO_HALO),
        K(wings, WINGS_ANGEL), K(nose_style, NOSE_DOT), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_SKY), K(hair_style, HAIR_CURLY),
        K(hair_color, PAINT_LEMON), K(cheeks, CHEEK_BLUSH), END } },
    { "Star Angel", { SP(SPECIES_CHERUB), K(skin, SKIN_TAN), K(halo, HALO_STARS),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_LAVENDER), K(mouth_style, MOUTH_O),
        K(hair_style, HAIR_WAVY), K(hair_color, PAINT_WHITE),
        K(hair_highlight, PAINT_LAVENDER), K(cheeks, CHEEK_BLUSH | CHEEK_SPARKLES), END } },
    { "Choir Cherub", { SP(SPECIES_CHERUB), K(skin, SKIN_BROWN), K(eye_style, EYE_HAPPY_ARC),
        K(eye_color, PAINT_HONEY), K(mouth_style, MOUTH_OPEN), K(hair_style, HAIR_AFRO),
        K(hair_color, PAINT_HONEY), K(cheeks, CHEEK_BLUSH | CHEEK_DIMPLES), END } },
    /* ---- ghoul ---- */
    { "Mint Ghoul", { SP(SPECIES_GHOUL), K(skin, SK(PAINT_MINT)), K(head_shape, HEAD_OVAL),
        K(ears, EARS_ELF), K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_FLAT),
        K(eye_style, EYE_SLEEPY), K(eye_color, PAINT_CHERRY), K(brow_style, BROW_NONE),
        K(hair_style, HAIR_EMO_FRINGE), K(hair_color, PAINT_JET), K(cheeks, CHEEK_BLUSH), END } },
    { "Graveyard Goth", { SP(SPECIES_GHOUL), K(skin, SK(PAINT_LAVENDER)), K(ears, EARS_POINTY),
        K(eye_style, EYE_LASHES), K(eye_color, PAINT_GRAPE), K(mouth_style, MOUTH_SMIRK),
        K(hair_style, HAIR_LONG), K(hair_color, PAINT_JET), K(hair_highlight, PAINT_GRAPE),
        K(marking, MARK_EYELINER), K(marking_color, PAINT_JET), END } },
    { "Sleepy Zombie", { SP(SPECIES_GHOUL), K(skin, SK(PAINT_LIME)), K(ears, EARS_NORMAL),
        K(eye_style, EYE_DROOPY), K(eye_color, PAINT_LEMON), K(mouth_style, MOUTH_WAVY),
        K(hair_style, HAIR_MESSY), K(hair_color, PAINT_FOREST), K(marking, MARK_STITCHES),
        K(marking_color, PAINT_CHARCOAL), K(cheeks, CHEEK_BANDAGE), END } },
    /* ---- skeleton ---- */
    { "Skelly", { SP(SPECIES_SKELETON), K(skin, SK(PAINT_BONE)), K(ears, EARS_NONE),
        K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_TEETH), K(eye_style, EYE_DOT),
        K(eye_color, PAINT_SKY), K(hair_style, HAIR_BALD), END } },
    { "Dapper Bones", { SP(SPECIES_SKELETON), K(skin, SK(PAINT_WHITE)), K(head_shape, HEAD_SKULL),
        K(nose_style, NOSE_SKULL), K(mouth_style, MOUTH_GRIN), K(eye_style, EYE_ROUND),
        K(eye_color, PAINT_CHERRY), K(facial_hair, FHAIR_MUSTACHE), K(hair_style, HAIR_BALD), END } },
    { "Spooky Scholar", { SP(SPECIES_SKELETON), K(skin, SK(PAINT_SILVER)), K(head_shape, HEAD_OVAL),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_MINT), K(mouth_style, MOUTH_SMILE),
        K(hair_style, HAIR_WILD), K(hair_color, PAINT_WHITE), END } },
    /* ---- slime ---- */
    { "Jelly Slime", { SP(SPECIES_SLIME), K(skin, SK(PAINT_MINT)), K(head_shape, HEAD_BLOB),
        K(ears, EARS_NONE), K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_JET), K(hair_style, HAIR_BALD),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Bubblegum Blob", { SP(SPECIES_SLIME), K(skin, SK(PAINT_BUBBLEGUM)), K(head_shape, HEAD_MOCHI),
        K(mouth_style, MOUTH_O), K(eye_style, EYE_ANIME), K(eye_color, PAINT_GRAPE),
        K(hair_style, HAIR_BALD), K(cheeks, CHEEK_BLUSH | CHEEK_SPARKLES), END } },
    { "Classic Goo", { SP(SPECIES_SLIME), K(skin, SK(PAINT_SLIME)), K(mouth_style, MOUTH_TONGUE),
        K(eye_style, EYE_HAPPY_ARC), K(eye_color, PAINT_FOREST), K(hair_style, HAIR_SLIME_DRIP),
        K(hair_color, PAINT_SLIME), K(cheeks, CHEEK_BLUSH), END } },
    /* ---- hellhound ---- */
    { "Hellpup", { SP(SPECIES_HELLHOUND), K(skin, SK(PAINT_CHARCOAL)), K(ears, EARS_DOG),
        K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_GRIN), K(fangs, 1),
        K(tail, TAIL_PUP_WAGGY), K(eye_style, EYE_ROUND), K(eye_color, PAINT_CHERRY),
        K(hair_style, HAIR_MESSY), K(hair_color, PAINT_CHARCOAL),
        K(marking, MARK_FUR_MUZZLE), K(marking_color, PAINT_SILVER), END } },
    { "Ember Hound", { SP(SPECIES_HELLHOUND), K(skin, SK(PAINT_RUST)), K(horns, HORNS_BULL),
        K(horn_color, PAINT_BONE), K(tail, TAIL_DEVIL), K(eye_style, EYE_ANGRY),
        K(eye_color, PAINT_TANGERINE), K(mouth_style, MOUTH_TEETH), K(fangs, 2),
        K(hair_style, HAIR_SPIKY), K(hair_color, PAINT_CHERRY), END } },
    { "Midnight Mutt", { SP(SPECIES_HELLHOUND), K(skin, SK(PAINT_JET)), K(horns, HORNS_DEVIL),
        K(horn_color, PAINT_CHERRY), K(eye_style, EYE_CAT), K(eye_color, PAINT_LEMON),
        K(mouth_style, MOUTH_SMIRK), K(marking_color, PAINT_CHARCOAL), END } },
    /* ---- cacoblob ---- */
    { "Cacoblob", { SP(SPECIES_CACOBLOB), K(skin, SK(PAINT_CHERRY)), K(head_shape, HEAD_BLOB),
        K(eye_count, 1), K(eye_style, EYE_ROUND), K(eye_color, PAINT_LEMON),
        K(ears, EARS_NONE), K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_TEETH),
        K(fangs, 2), K(horns, HORNS_DEVIL), K(horn_color, PAINT_BONE),
        K(hair_style, HAIR_BALD), END } },
    { "Grape Caco", { SP(SPECIES_CACOBLOB), K(skin, SK(PAINT_GRAPE)), K(eye_style, EYE_ANIME),
        K(eye_color, PAINT_MINT), K(mouth_style, MOUTH_BIG_GRIN), K(fangs, 0),
        K(horns, HORNS_NUBS), K(horn_color, PAINT_LAVENDER), END } },
    { "Rosy Caco", { SP(SPECIES_CACOBLOB), K(skin, SK(PAINT_ROSE)), K(eye_style, EYE_HAPPY_ARC),
        K(eye_color, PAINT_SKY), K(mouth_style, MOUTH_SMILE), K(fangs, 1),
        K(horns, HORNS_NUBS), K(cheeks, CHEEK_BLUSH), END } },
    /* ---- batling ---- */
    { "Batling", { SP(SPECIES_BATLING), K(skin, SK(PAINT_GRAPE)), K(ears, EARS_POINTY),
        K(wings, WINGS_TINY_BAT), K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_SMILE),
        K(fangs, 1), K(eye_style, EYE_ROUND), K(eye_color, PAINT_LEMON),
        K(hair_style, HAIR_MESSY), K(hair_color, PAINT_JET), K(cheeks, CHEEK_BLUSH), END } },
    { "Night Flutter", { SP(SPECIES_BATLING), K(skin, SK(PAINT_LAVENDER)), K(wings, WINGS_BAT),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_GRAPE), K(hair_style, HAIR_EMO_FRINGE),
        K(hair_color, PAINT_GRAPE), K(mouth_style, MOUTH_SMIRK), END } },
    { "Vampy Bat", { SP(SPECIES_BATLING), K(skin, SK(PAINT_CHARCOAL)), K(ears, EARS_BAT),
        K(wings, WINGS_BAT), K(eye_style, EYE_CAT), K(eye_color, PAINT_CHERRY),
        K(hair_style, HAIR_SLICKED), K(hair_color, PAINT_JET), K(mouth_style, MOUTH_SMIRK),
        K(fangs, 3), END } },
    /* ---- robot ---- */
    { "Beep Bot", { SP(SPECIES_ROBOT), K(skin, SK(PAINT_SILVER)), K(head_shape, HEAD_SQUARE),
        K(horns, HORNS_ANTENNAE), K(horn_color, PAINT_CHERRY), K(ears, EARS_NONE),
        K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_FLAT), K(eye_style, EYE_DOT),
        K(eye_color, PAINT_CYAN), K(hair_style, HAIR_BALD), END } },
    { "Sky Bot", { SP(SPECIES_ROBOT), K(skin, SK(PAINT_SKY)), K(horn_color, PAINT_LEMON),
        K(eye_style, EYE_WIDE), K(eye_color, PAINT_LEMON), K(mouth_style, MOUTH_SMILE),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Diamond Droid", { SP(SPECIES_ROBOT), K(skin, SK(PAINT_COBALT)), K(head_shape, HEAD_DIAMOND),
        K(horn_color, PAINT_BUBBLEGUM), K(eye_style, EYE_GLOW), K(eye_color, PAINT_MINT),
        K(mouth_style, MOUTH_O), END } },
    /* ---- plushie ---- */
    { "Patchwork Plush", { SP(SPECIES_PLUSHIE), K(skin, SK(PAINT_BUBBLEGUM)), K(ears, EARS_BEAR),
        K(tail, TAIL_BEAR_NUB), K(eye_style, EYE_BUTTON), K(eye_color, PAINT_JET),
        K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_STITCHED), K(marking, MARK_PATCHES),
        K(marking_color, PAINT_MINT), K(hair_style, HAIR_BALD), K(cheeks, CHEEK_BLUSH), END } },
    { "Bunny Plush", { SP(SPECIES_PLUSHIE), K(skin, SK(PAINT_LAVENDER)), K(ears, EARS_BUNNY),
        K(tail, TAIL_BUNNY), K(eye_style, EYE_BUTTON), K(eye_color, PAINT_CHARCOAL),
        K(nose_style, NOSE_DOT), K(mouth_style, MOUTH_SMILE), K(marking, MARK_STITCHES),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_BALD), K(cheeks, CHEEK_BLUSH), END } },
    { "Teddy Plush", { SP(SPECIES_PLUSHIE), K(skin, SK(PAINT_CARAMEL)), K(eye_style, EYE_BUTTON),
        K(eye_color, PAINT_CHOCOLATE), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_BONE), K(hair_style, HAIR_BALD), END } },
    /* ---- ghost ---- */
    { "Wisp", { SP(SPECIES_GHOST), K(skin, SK(PAINT_WHITE)), K(ears, EARS_NONE),
        K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_O), K(eye_style, EYE_ROUND),
        K(eye_color, PAINT_SKY), K(hair_style, HAIR_BALD), K(cheeks, CHEEK_BLUSH), END } },
    { "Minty Boo", { SP(SPECIES_GHOST), K(skin, SK(PAINT_MINT)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_DOT), K(eye_color, PAINT_TEAL), K(cheeks, CHEEK_BLUSH), END } },
    { "Sheet Ghost", { SP(SPECIES_GHOST), K(skin, SK(PAINT_LAVENDER)), K(mouth_style, MOUTH_OPEN),
        K(eye_style, EYE_HAPPY_ARC), K(eye_color, PAINT_GRAPE),
        K(cheeks, CHEEK_BLUSH | CHEEK_SPARKLES), END } },
    /* ---- mummy ---- */
    { "Mummy", { SP(SPECIES_MUMMY), K(skin, SK(PAINT_BONE)), K(nose_style, NOSE_DOT),
        K(mouth_style, MOUTH_FLAT), K(eye_style, EYE_ROUND), K(eye_color, PAINT_HONEY),
        K(hair_style, HAIR_BALD), K(accessory, ACC_BANDAGES), K(accessory_color, PAINT_BONE), END } },
    { "Wrapped Cutie", { SP(SPECIES_MUMMY), K(skin, SK(PAINT_WHITE)), K(eye_style, EYE_ANIME),
        K(eye_color, PAINT_MINT), K(mouth_style, MOUTH_SMILE), K(cheeks, CHEEK_BLUSH),
        K(accessory, ACC_BANDAGES), K(accessory_color, PAINT_WHITE), END } },
    { "Pharaoh", { SP(SPECIES_MUMMY), K(skin, SK(PAINT_CARAMEL)), K(eye_style, EYE_LASHES),
        K(eye_color, PAINT_COBALT), K(mouth_style, MOUTH_SMIRK), K(marking, MARK_EYELINER),
        K(marking_color, PAINT_JET), K(accessory, ACC_BANDAGES),
        K(accessory_color, PAINT_BONE), END } },
    /* ---- mothkin ---- */
    { "Mothkin", { SP(SPECIES_MOTHKIN), K(skin, SK(PAINT_LAVENDER)), K(wings, WINGS_BUTTERFLY),
        K(horns, HORNS_ANTENNAE), K(horn_color, PAINT_CHARCOAL), K(ears, EARS_NONE),
        K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_SMILE), K(eye_style, EYE_ROUND),
        K(eye_color, PAINT_JET), K(hair_style, HAIR_CURLY), K(hair_color, PAINT_WHITE),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Luna Moth", { SP(SPECIES_MOTHKIN), K(skin, SK(PAINT_MINT)), K(horn_color, PAINT_LEMON),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_LEMON), K(hair_style, HAIR_WAVY),
        K(hair_color, PAINT_WHITE), END } },
    { "Fuzzy Moth", { SP(SPECIES_MOTHKIN), K(skin, SK(PAINT_BONE)), K(horn_color, PAINT_CHOCOLATE),
        K(eye_style, EYE_DOT), K(eye_color, PAINT_JET), K(hair_style, HAIR_AFRO),
        K(hair_color, PAINT_WHITE), K(cheeks, CHEEK_BLUSH), END } },
    /* ---- frogling ---- */
    { "Froggy", { SP(SPECIES_FROGLING), K(skin, SK(PAINT_LIME)), K(head_shape, HEAD_MOCHI),
        K(ears, EARS_NONE), K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_WAVY),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_JET), K(hair_style, HAIR_BALD),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Lily Pad", { SP(SPECIES_FROGLING), K(skin, SK(PAINT_MINT)), K(head_shape, HEAD_ROUND),
        K(mouth_style, MOUTH_SMILE), K(eye_style, EYE_HAPPY_ARC), K(eye_color, PAINT_FOREST),
        K(marking, MARK_SPOTS), K(marking_color, PAINT_FOREST), END } },
    { "Rain Frog", { SP(SPECIES_FROGLING), K(skin, SK(PAINT_SKY)), K(mouth_style, MOUTH_O),
        K(eye_style, EYE_WIDE), K(eye_color, PAINT_JET), K(marking, MARK_SPOTS),
        K(marking_color, PAINT_COBALT), END } },
    /* ---- bunnykin ---- */
    { "Bun Bun", { SP(SPECIES_BUNNYKIN), K(skin, SK(PAINT_WHITE)), K(ears, EARS_BUNNY),
        K(tail, TAIL_BUNNY), K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_CAT),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_ROSE), K(marking, MARK_FUR_BELLY),
        K(marking_color, PAINT_BUBBLEGUM), K(hair_style, HAIR_SHORT),
        K(hair_color, PAINT_WHITE), K(cheeks, CHEEK_BLUSH), END } },
    { "Cocoa Bunny", { SP(SPECIES_BUNNYKIN), K(skin, SK(PAINT_CHOCOLATE)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_HONEY), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_BOB), K(hair_color, PAINT_CHOCOLATE),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Lavender Lop", { SP(SPECIES_BUNNYKIN), K(skin, SK(PAINT_LAVENDER)), K(eye_style, EYE_LASHES),
        K(eye_color, PAINT_GRAPE), K(marking, MARK_FUR_TIPPED_EARS), K(marking_color, PAINT_GRAPE),
        K(hair_style, HAIR_PIGTAILS), K(hair_color, PAINT_LAVENDER), END } },
    /* ---- catfolk ---- */
    { "Caramel Kitty", { SP(SPECIES_CATFOLK), K(skin, SK(PAINT_CARAMEL)), K(ears, EARS_CAT),
        K(tail, TAIL_CAT), K(nose_style, NOSE_CAT), K(mouth_style, MOUTH_CAT),
        K(eye_style, EYE_CAT), K(eye_color, PAINT_MINT), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_MESSY), K(hair_color, PAINT_CARAMEL),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Calico Cutie", { SP(SPECIES_CATFOLK), K(skin, SK(PAINT_WHITE)), K(eye_style, EYE_ROUND),
        K(eye_color, PAINT_HONEY), K(marking, MARK_FUR_CALICO), K(marking_color, PAINT_TANGERINE),
        K(hair_style, HAIR_SHORT), K(hair_color, PAINT_TANGERINE), K(cheeks, CHEEK_BLUSH), END } },
    { "Midnight Tabby", { SP(SPECIES_CATFOLK), K(skin, SK(PAINT_CHARCOAL)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_CAT), K(eye_color, PAINT_LEMON), K(marking, MARK_FUR_TABBY),
        K(marking_color, PAINT_SILVER), K(hair_style, HAIR_EMO_FRINGE),
        K(hair_color, PAINT_JET), END } },
    /* ---- foxkin ---- */
    { "Tangerine Fox", { SP(SPECIES_FOXKIN), K(skin, SK(PAINT_TANGERINE)), K(ears, EARS_FOX),
        K(tail, TAIL_FOX), K(nose_style, NOSE_CAT), K(mouth_style, MOUTH_CAT),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_HONEY), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_MESSY),
        K(hair_color, PAINT_TANGERINE), K(cheeks, CHEEK_BLUSH), END } },
    { "Snow Fox", { SP(SPECIES_FOXKIN), K(skin, SK(PAINT_WHITE)), K(eye_style, EYE_ROUND),
        K(eye_color, PAINT_SKY), K(marking, MARK_FUR_TIPPED_EARS), K(marking_color, PAINT_SILVER),
        K(hair_style, HAIR_LONG), K(hair_color, PAINT_WHITE), END } },
    { "Rosy Fox", { SP(SPECIES_FOXKIN), K(skin, SK(PAINT_BUBBLEGUM)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_LASHES), K(eye_color, PAINT_GRAPE), K(marking, MARK_FUR_BELLY),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_PONYTAIL), K(hair_color, PAINT_ROSE),
        K(cheeks, CHEEK_BLUSH), END } },
    /* ---- bearkin ---- */
    { "Cocoa Bear", { SP(SPECIES_BEARKIN), K(skin, SK(PAINT_CHOCOLATE)), K(ears, EARS_BEAR),
        K(tail, TAIL_BEAR_NUB), K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_DOT), K(eye_color, PAINT_JET), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_CARAMEL), K(hair_style, HAIR_SHORT),
        K(hair_color, PAINT_CHOCOLATE), K(cheeks, CHEEK_BLUSH), END } },
    { "Honey Bear", { SP(SPECIES_BEARKIN), K(skin, SK(PAINT_HONEY)), K(mouth_style, MOUTH_OPEN),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_CHOCOLATE), K(marking, MARK_FUR_BELLY),
        K(marking_color, PAINT_BONE), K(hair_style, HAIR_CURLY), K(hair_color, PAINT_HONEY), END } },
    { "Polar Pal", { SP(SPECIES_BEARKIN), K(skin, SK(PAINT_WHITE)), K(eye_style, EYE_HAPPY_ARC),
        K(eye_color, PAINT_SKY), K(marking_color, PAINT_SILVER), K(hair_style, HAIR_BUZZ),
        K(hair_color, PAINT_WHITE), K(cheeks, CHEEK_BLUSH), END } },
    /* ---- pupkin ---- */
    { "Golden Pup", { SP(SPECIES_PUPKIN), K(skin, SK(PAINT_HONEY)), K(ears, EARS_FLOPPY_PUP),
        K(tail, TAIL_PUP_WAGGY), K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_TONGUE),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_CHOCOLATE), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_SHORT), K(hair_color, PAINT_HONEY),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Spotty Pup", { SP(SPECIES_PUPKIN), K(skin, SK(PAINT_WHITE)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_SKY), K(marking, MARK_SPOTS),
        K(marking_color, PAINT_JET), K(hair_style, HAIR_BALD), END } },
    { "Husky Pup", { SP(SPECIES_PUPKIN), K(skin, SK(PAINT_SILVER)), K(ears, EARS_DOG),
        K(mouth_style, MOUTH_OPEN), K(eye_style, EYE_ROUND), K(eye_color, PAINT_SKY),
        K(marking, MARK_FUR_MASK), K(marking_color, PAINT_WHITE), K(hair_style, HAIR_MESSY),
        K(hair_color, PAINT_SILVER), END } },
    /* ---- mousekin ---- */
    { "Squeaky Mouse", { SP(SPECIES_MOUSEKIN), K(skin, SK(PAINT_SILVER)), K(ears, EARS_MOUSE),
        K(tail, TAIL_MOUSE), K(nose_style, NOSE_CAT), K(mouth_style, MOUTH_CAT),
        K(eye_style, EYE_DOT), K(eye_color, PAINT_JET), K(marking, MARK_FUR_BELLY),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_SHORT), K(hair_color, PAINT_SILVER),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Strawberry Mouse", { SP(SPECIES_MOUSEKIN), K(skin, SK(PAINT_WHITE)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_ROSE), K(marking, MARK_FUR_TIPPED_EARS),
        K(marking_color, PAINT_BUBBLEGUM), K(hair_style, HAIR_BOB),
        K(hair_color, PAINT_BUBBLEGUM), K(cheeks, CHEEK_BLUSH), END } },
    { "Cheddar Mouse", { SP(SPECIES_MOUSEKIN), K(skin, SK(PAINT_CARAMEL)), K(mouth_style, MOUTH_GRIN),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_HONEY), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_WHITE), K(hair_style, HAIR_BUN), K(hair_color, PAINT_LEMON), END } },
    /* ---- sheepkin ---- */
    { "Cloud Lamb", { SP(SPECIES_SHEEPKIN), K(skin, SK(PAINT_WHITE)), K(ears, EARS_SHEEP),
        K(tail, TAIL_SHEEP_PUFF), K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_SLEEPY), K(eye_color, PAINT_CHOCOLATE), K(marking, MARK_FUR_MUZZLE),
        K(marking_color, PAINT_BONE), K(hair_style, HAIR_CURLY), K(hair_color, PAINT_WHITE),
        K(cheeks, CHEEK_BLUSH), END } },
    { "Cotton Candy Lamb", { SP(SPECIES_SHEEPKIN), K(skin, SK(PAINT_BUBBLEGUM)),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_SKY), K(hair_style, HAIR_AFRO),
        K(hair_color, PAINT_BUBBLEGUM), K(hair_highlight, PAINT_WHITE),
        K(marking_color, PAINT_WHITE), K(cheeks, CHEEK_BLUSH), END } },
    { "Cocoa Ram", { SP(SPECIES_SHEEPKIN), K(skin, SK(PAINT_BONE)), K(mouth_style, MOUTH_FLAT),
        K(horns, HORNS_RAM), K(horn_color, PAINT_CARAMEL), K(eye_style, EYE_DOT),
        K(eye_color, PAINT_JET), K(hair_style, HAIR_CURLY), K(hair_color, PAINT_CHOCOLATE), END } },
    /* ---- raccoonkin ---- */
    { "Trash Panda", { SP(SPECIES_RACCOONKIN), K(skin, SK(PAINT_SILVER)), K(ears, EARS_RACCOON),
        K(tail, TAIL_RACCOON_RINGED), K(nose_style, NOSE_BUTTON), K(mouth_style, MOUTH_GRIN),
        K(eye_style, EYE_ROUND), K(eye_color, PAINT_JET), K(marking, MARK_FUR_MASK),
        K(marking_color, PAINT_CHARCOAL), K(hair_style, HAIR_MESSY),
        K(hair_color, PAINT_SILVER), END } },
    { "Lavender Bandit", { SP(SPECIES_RACCOONKIN), K(skin, SK(PAINT_LAVENDER)), K(mouth_style, MOUTH_SMILE),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_LEMON), K(marking_color, PAINT_GRAPE),
        K(hair_style, HAIR_SPIKY), K(hair_color, PAINT_GRAPE), K(cheeks, CHEEK_BLUSH), END } },
    { "Cinnamon Coon", { SP(SPECIES_RACCOONKIN), K(skin, SK(PAINT_CARAMEL)), K(mouth_style, MOUTH_SMIRK),
        K(eye_style, EYE_CAT), K(eye_color, PAINT_MINT), K(marking_color, PAINT_CHOCOLATE),
        K(hair_style, HAIR_WAVY), K(hair_color, PAINT_CHOCOLATE), END } },
    /* ---- owlkin ---- */
    { "Hoot Owl", { SP(SPECIES_OWLKIN), K(skin, SK(PAINT_CARAMEL)), K(ears, EARS_OWL_TUFTS),
        K(tail, TAIL_FEATHER_FAN), K(nose_style, NOSE_NONE), K(mouth_style, MOUTH_BEAK),
        K(eye_style, EYE_WIDE), K(eye_color, PAINT_HONEY), K(marking, MARK_FUR_BELLY),
        K(marking_color, PAINT_BONE), K(hair_style, HAIR_BALD), END } },
    { "Snowy Owl", { SP(SPECIES_OWLKIN), K(skin, SK(PAINT_WHITE)), K(eye_style, EYE_ROUND),
        K(eye_color, PAINT_LEMON), K(marking, MARK_SPOTS), K(marking_color, PAINT_SILVER), END } },
    { "Starlight Owl", { SP(SPECIES_OWLKIN), K(skin, SK(PAINT_LAVENDER)), K(wings, WINGS_ANGEL),
        K(eye_style, EYE_ANIME), K(eye_color, PAINT_SKY), K(marking_color, PAINT_SKY),
        K(cheeks, CHEEK_SPARKLES), END } },
};

/* ====================================================================== */
/* outfit sets                                                              */
#define OUTFIT(tp_, tc_, pt_, pc_, bt_, bc_, sh_, sc_, ht_, hc_, ac_, acc_) \
    K(top, tp_), K(top_color, tc_), K(pattern, pt_), K(pattern_color, pc_), \
    K(bottom, bt_), K(bottom_color, bc_), K(shoes, sh_), K(shoes_color, sc_), \
    K(hat, ht_), K(hat_color, hc_), K(accessory, ac_), K(accessory_color, acc_), END

const CrPreset cr_outfit_presets[] = {
    { "Comfy Hoodie", { OUTFIT(TOP_HOODIE, PAINT_BUBBLEGUM, PATTERN_SOLID, PAINT_WHITE,
        BOTTOM_SHORTS, PAINT_SKY, SHOES_SNEAKERS, PAINT_WHITE, HAT_NONE, PAINT_LEMON,
        ACC_NONE, PAINT_MINT) } },
    { "Picnic Day", { OUTFIT(TOP_OVERALLS, PAINT_SKY, PATTERN_SOLID, PAINT_WHITE,
        BOTTOM_NONE, PAINT_SKY, SHOES_BOOTS, PAINT_CARAMEL, HAT_CAP, PAINT_LEMON,
        ACC_BACKPACK, PAINT_PEACH) } },
    { "Sweater Weather", { OUTFIT(TOP_SWEATER, PAINT_PEACH, PATTERN_STRIPES, PAINT_WHITE,
        BOTTOM_PANTS, PAINT_CHOCOLATE, SHOES_BOOTS, PAINT_CARAMEL, HAT_BEANIE, PAINT_ROSE,
        ACC_SCARF, PAINT_MINT) } },
    { "Sunday Best", { OUTFIT(TOP_DRESS, PAINT_LAVENDER, PATTERN_POLKA, PAINT_WHITE,
        BOTTOM_NONE, PAINT_LAVENDER, SHOES_SANDALS, PAINT_ROSE, HAT_BERET, PAINT_LAVENDER,
        ACC_EARRINGS, PAINT_LEMON) } },
    { "Dapper", { OUTFIT(TOP_SUIT, PAINT_NAVY, PATTERN_SOLID, PAINT_WHITE,
        BOTTOM_PANTS, PAINT_NAVY, SHOES_BOOTS, PAINT_JET, HAT_TOP_HAT, PAINT_JET,
        ACC_GLASSES, PAINT_SILVER) } },
    { "Skater", { OUTFIT(TOP_TSHIRT, PAINT_CHERRY, PATTERN_STRIPES, PAINT_WHITE,
        BOTTOM_RIPPED_JEANS, PAINT_SKY, SHOES_SNEAKERS, PAINT_CHERRY, HAT_CAP, PAINT_CHERRY,
        ACC_SUNGLASSES, PAINT_JET) } },
    { "Sporty", { OUTFIT(TOP_TANK, PAINT_SKY, PATTERN_STRIPES, PAINT_WHITE,
        BOTTOM_SWEATS, PAINT_NAVY, SHOES_SNEAKERS, PAINT_WHITE, HAT_HEADPHONES, PAINT_MINT,
        ACC_NONE, PAINT_WHITE) } },
    { "Dance Class", { OUTFIT(TOP_CROP_TOP, PAINT_ROSE, PATTERN_SOLID, PAINT_WHITE,
        BOTTOM_LEGGINGS, PAINT_BUBBLEGUM, SHOES_BARE, PAINT_ROSE, HAT_NONE, PAINT_ROSE,
        ACC_SCARF, PAINT_ROSE) } },
    { "Bookworm", { OUTFIT(TOP_SWEATER_VEST, PAINT_HONEY, PATTERN_PLAID, PAINT_CHOCOLATE,
        BOTTOM_PANTS, PAINT_CHOCOLATE, SHOES_CLOGS, PAINT_CHOCOLATE, HAT_NONE, PAINT_HONEY,
        ACC_GLASSES, PAINT_CHOCOLATE) } },
    { "Cozy Cloak", { OUTFIT(TOP_CLOAK, PAINT_FOREST, PATTERN_SOLID, PAINT_HONEY,
        BOTTOM_PANTS, PAINT_CHOCOLATE, SHOES_BOOTS, PAINT_CHOCOLATE, HAT_NONE, PAINT_FOREST,
        ACC_SCARF, PAINT_HONEY) } },
    { "Beach Day", { OUTFIT(TOP_TANK, PAINT_LEMON, PATTERN_POLKA, PAINT_WHITE,
        BOTTOM_SHORTS, PAINT_TEAL, SHOES_SANDALS, PAINT_PEACH, HAT_NONE, PAINT_LEMON,
        ACC_SUNGLASSES, PAINT_BUBBLEGUM) } },
    { "Goth Night", { OUTFIT(TOP_LEATHER_JACKET, PAINT_JET, PATTERN_SOLID, PAINT_CHERRY,
        BOTTOM_RIPPED_JEANS, PAINT_CHARCOAL, SHOES_COMBAT, PAINT_JET, HAT_NONE, PAINT_JET,
        ACC_EARRINGS, PAINT_SILVER) } },
    { "Band Kid", { OUTFIT(TOP_BAND_TEE, PAINT_JET, PATTERN_SOLID, PAINT_CHERRY,
        BOTTOM_CARGO, PAINT_CHARCOAL, SHOES_COMBAT, PAINT_CHERRY, HAT_BEANIE, PAINT_CHERRY,
        ACC_SUNGLASSES, PAINT_JET) } },
    { "Hellvile Edge", { OUTFIT(TOP_HOODIE, PAINT_JET, PATTERN_STRIPES, PAINT_CHERRY,
        BOTTOM_CARGO, PAINT_JET, SHOES_COMBAT, PAINT_JET, HAT_BANDANA, PAINT_CHERRY,
        ACC_EARRINGS, PAINT_SILVER) } },
    { "Just Fluff", { OUTFIT(TOP_NONE, PAINT_WHITE, PATTERN_SOLID, PAINT_WHITE,
        BOTTOM_NONE, PAINT_WHITE, SHOES_BARE, PAINT_WHITE, HAT_NONE, PAINT_WHITE,
        ACC_NONE, PAINT_WHITE) } },
    { "Nurse Shift", { OUTFIT(TOP_NURSE, PAINT_WHITE, PATTERN_SOLID, PAINT_ROSE,
        BOTTOM_PANTS, PAINT_WHITE, SHOES_SNEAKERS, PAINT_WHITE, HAT_NONE, PAINT_WHITE,
        ACC_NONE, PAINT_ROSE) } },
    { "Little Chef", { OUTFIT(TOP_TSHIRT, PAINT_WHITE, PATTERN_PLAID, PAINT_CHERRY,
        BOTTOM_PANTS, PAINT_CHARCOAL, SHOES_CLOGS, PAINT_JET, HAT_CHEF, PAINT_WHITE,
        ACC_SCARF, PAINT_CHERRY) } },
    { "Mad Scientist", { OUTFIT(TOP_LAB_COAT, PAINT_WHITE, PATTERN_SOLID, PAINT_WHITE,
        BOTTOM_PANTS, PAINT_CHARCOAL, SHOES_BOOTS, PAINT_JET, HAT_NONE, PAINT_WHITE,
        ACC_GLASSES, PAINT_SILVER) } },
    { "Witchy", { OUTFIT(TOP_ROBE, PAINT_GRAPE, PATTERN_POLKA, PAINT_LEMON,
        BOTTOM_NONE, PAINT_GRAPE, SHOES_BOOTS, PAINT_JET, HAT_WITCH, PAINT_GRAPE,
        ACC_NONE, PAINT_LEMON) } },
    { "Sleepover", { OUTFIT(TOP_PAJAMAS, PAINT_SKY, PATTERN_POLKA, PAINT_WHITE,
        BOTTOM_NONE, PAINT_SKY, SHOES_SLIPPERS, PAINT_BUBBLEGUM, HAT_NONE, PAINT_SKY,
        ACC_TEDDY, PAINT_CARAMEL) } },
};
const int cr_outfit_preset_count = DOOMO_COUNTOF(cr_outfit_presets);

/* ====================================================================== */
/* voice types                                                              */
#define VOICE(p, s, q, i, a) \
    OFS(voice[VOICE_PITCH]), p, OFS(voice[VOICE_SPEED]), s, OFS(voice[VOICE_QUALITY]), q, \
    OFS(voice[VOICE_INTONATION]), i, OFS(voice[VOICE_ACCENT]), a, END

const CrPreset cr_voice_presets[] = {
    { "Everyday", { VOICE(16, 16, 16, 16, 16) } },
    { "Squeaky", { VOICE(28, 22, 6, 20, 10) } },
    { "Deep Rumble", { VOICE(3, 8, 26, 6, 12) } },
    { "Sing-Song", { VOICE(20, 14, 22, 30, 24) } },
    { "Robot", { VOICE(14, 16, 0, 0, 0) } },
    { "Sleepyhead", { VOICE(10, 3, 28, 8, 6) } },
    { "Chatterbox", { VOICE(22, 31, 12, 18, 16) } },
    { "Grumbly Imp", { VOICE(7, 20, 4, 12, 28) } },
    { "Ghostly", { VOICE(24, 6, 31, 26, 4) } },
    { "Royal", { VOICE(12, 10, 20, 14, 30) } },
    { "Baby Talk", { VOICE(31, 12, 24, 24, 8) } },
    { "Gravelly", { VOICE(0, 12, 2, 4, 20) } },
};
const int cr_voice_preset_count = DOOMO_COUNTOF(cr_voice_presets);

/* ====================================================================== */
/* texts                                                                    */
const char *const cr_persona_desc[PERSONA_TYPE_COUNT] = {
    "Calm and careful. Loves a quiet corner and a good book.",
    "Takes life slow. Naps anywhere, anytime, very happily.",
    "Grumbles a lot but secretly cares. Do not wake them.",
    "Unbothered by anything. Says odd things very calmly.",
    "Soft-hearted and sincere. Cries at every sunset.",
    "Warm, kind and always smiling. Everyone's comfort pal.",
    "Feels everything VERY deeply, and very out loud.",
    "Teasing and playful. Impossible to stay mad at.",
    "Tidy, punctual and first to lend a helping paw.",
    "Always on the move, never in a hurry. Goes with the flow.",
    "Gives orders, follows rules, secretly adores cake.",
    "Totally unpredictable. Nobody knows what comes next!",
    "Bursting with plans and ready to do them all today.",
    "Pure sunshine on legs. Befriends anyone in seconds.",
    "Loud, bold and passionate. Born to lead the parade.",
    "Mischief in plush form. Has definitely licked a battery.",
};

const uint8_t cr_persona_expr[PERSONA_TYPE_COUNT] = {
    EXPR_NEUTRAL, EXPR_SLEEPY, EXPR_ANGRY, EXPR_NEUTRAL,
    EXPR_LOVE, EXPR_HAPPY, EXPR_SAD, EXPR_HAPPY,
    EXPR_HAPPY, EXPR_HAPPY, EXPR_ANGRY, EXPR_SHOCK,
    EXPR_HAPPY, EXPR_LOVE, EXPR_ANGRY, EXPR_SHOCK,
};

/* how each personality type says hello (shown in the Personality tab) */
const char *const cr_persona_greet[PERSONA_TYPE_COUNT] = {
    "Oh. Hello. Do you like books?",
    "Mmh... hi... nap with me?",
    "What do YOU want? ...Hi.",
    "Hello. The moon is a big egg.",
    "Hi! Your smile made my day!",
    "Hiii! I baked you a hug!",
    "Behold! I have arrived! Hi!",
    "Heyyy cutie, miss me?",
    "Good day! Need a paw?",
    "Yo! Wanna go somewhere?",
    "Report! Name, rank, cake!",
    "Bonjour! Or is it Tuesday?",
    "Hi! Let's do ALL the things!",
    "HIII! Best friends now?!",
    "Follow me! To glory! Hi!",
    "Heehee! Licked your door!",
};

const char *const cr_quirk_names[4] = {
    "Very normal.", "A little odd.", "Pretty weird!", "Completely unhinged!"
};

const char *const cr_district_names[2] = { "Hillvile", "Hellvile" };
const char *const cr_home_names[2] = { "Hillvile Towers", "Hellvile Pits" };

const char *const cr_move_in_lines[8] = {
    "%s moved in! Yay!",
    "Welcome home, %s!",
    "%s is all unpacked!",
    "%s joined the park!",
    "Say hi to %s!",
    "%s moved in. Ooh!",
    "%s is home sweet home!",
    "Hello, %s!",
};

/* ====================================================================== */
/* tab icons (11x11)                                                        */
const char *const cr_icons[CR_TAB_COUNT][CR_ICON_W] = {
    {   /* species & body: a little plush critter */
        "..o.....o..",
        "..oooooooo.",
        ".olllbbbbo.",
        ".olobbbobo.",
        ".obpbobpso.",
        "..ossssso..",
        ".ooobbbooo.",
        "oboblllbobo",
        ".ooobbbsoo.",
        "...obo.oso.",
        "...ooo.ooo.",
    },
    {   /* head & face: a round smiling face */
        "...ooooo...",
        "..olllbbo..",
        ".ollbbbbbo.",
        "olbbbbbbbso",
        "olowbbbowso",
        "oboobbbooso",
        "obpbbbbbpso",
        "obbbobobbso",
        ".obbbobbso.",
        "..ossssso..",
        "...ooooo...",
    },
    {   /* hair: a comb */
        "...........",
        ".ooooooooo.",
        "olllllllllo",
        "obbbbbbbbso",
        "ossssssssso",
        "obobobobobo",
        "obobobobobo",
        "obosobosobo",
        "o.o.o.o.o.o",
        "...........",
        "...........",
    },
    {   /* supernatural: horned little devil */
        "o.........o",
        "oo.......oo",
        "opo.....opo",
        "oppoooooppo",
        "olbbbbbbbso",
        "olbobbbobso",
        "obbobbbobso",
        "obpbbbbbpso",
        ".obbowobso.",
        "..ossssso..",
        "...ooooo...",
    },
    {   /* outfit: t-shirt with a heart */
        "...........",
        ".ooo...ooo.",
        "ollbooobbbo",
        "olbbbbbbbso",
        "oobbpbpbsoo",
        ".obbpppbso.",
        ".obbbpbbso.",
        ".obbbbbbso.",
        ".ossssssso.",
        ".ooooooooo.",
        "...........",
    },
    {   /* voice: music note */
        ".....oooo..",
        ".....ollbo.",
        ".....obbbso",
        ".....obooso",
        ".....obo.oo",
        ".....obo...",
        "..ooooso...",
        ".olllbso...",
        "olbbbbso...",
        ".ossssoo...",
        "..oooo.....",
    },
    {   /* personality: heart */
        "...........",
        ".ooo...ooo.",
        "ollbo.olbbo",
        "olwbbobbbso",
        "olbbbbbbbso",
        "obbbbbbbbso",
        ".obbbbbbso.",
        "..obbbbso..",
        "...obbso...",
        "....oso....",
        ".....o.....",
    },
    {   /* profile: name card */
        "...........",
        "...........",
        "ooooooooooo",
        "owwwwwwwwwo",
        "owbbbwooowo",
        "owobowwwwwo",
        "owbbbwooowo",
        "owwwwwwwwwo",
        "ooooooooooo",
        "...........",
        "...........",
    },
    {   /* home: cosy house */
        ".....o.....",
        "....olo....",
        "...ollbo...",
        "..ollbbbo..",
        ".ollbbbbso.",
        "ooooooooooo",
        ".olllllllo.",
        ".olwwloolo.",
        ".olwwloolo.",
        ".olllloolo.",
        ".ooooooooo.",
    },
};

/* ====================================================================== */
/* creator unlockables (ids 256..319)                                       */
#define CP(id, list, idx, name, how, prog, need) \
    { id, UL_CAT_PART, list, idx, name, how, prog, need }

const DoomoUnlock doomo_unlocks_creator[] = {
    /* rewards for filling the park with citizens */
    CP(256, CLIST_EYE, EYE_SPARKLE, "Sparkle Eyes", "Create your first citizen", PROG_CITIZENS_CREATED, 1),
    CP(257, CLIST_HAT, HAT_WITCH, "Witch Hat", "Create 3 citizens", PROG_CITIZENS_CREATED, 3),
    CP(258, CLIST_MARK, MARK_STAR_FRECKLES, "Star Freckles", "Create 5 citizens", PROG_CITIZENS_CREATED, 5),
    CP(259, CLIST_ACC, ACC_MONOCLE, "Monocle", "Create 8 citizens", PROG_CITIZENS_CREATED, 8),
    CP(260, CLIST_HORNS, HORNS_UNICORN, "Unicorn Horn", "Create 12 citizens", PROG_CITIZENS_CREATED, 12),
    CP(261, CLIST_SHOES, SHOES_PLATFORM, "Platform Shoes", "Create 15 citizens", PROG_CITIZENS_CREATED, 15),
    CP(262, CLIST_HAT, HAT_FEZ, "Fez", "Create 18 citizens", PROG_CITIZENS_CREATED, 18),
    CP(263, CLIST_EYE, EYE_GOAT, "Goat Eyes", "Create 22 citizens", PROG_CITIZENS_CREATED, 22),
    CP(264, CLIST_HAT, HAT_SKULL_CAP, "Skull Cap", "Create 30 citizens", PROG_CITIZENS_CREATED, 30),
    CP(265, CLIST_TOP, TOP_LAB_COAT, "Lab Coat", "Create 40 citizens", PROG_CITIZENS_CREATED, 40),
    CP(266, CLIST_HEAD, HEAD_HEART, "Heart Head", "Create 50 citizens", PROG_CITIZENS_CREATED, 50),
    CP(267, CLIST_WINGS, WINGS_FLY, "Fly Wings", "Create 75 citizens", PROG_CITIZENS_CREATED, 75),
    CP(268, CLIST_HAIR, HAIR_POMPADOUR, "Pompadour", "Create 100 citizens", PROG_CITIZENS_CREATED, 100),
    /* creator secrets (granted by creator.c) */
    CP(CRU_MOVE_TO_HELLVILE, CLIST_HORNS, HORNS_SPIRAL, "Spiral Horns",
       "Move a new citizen into Hellvile", 0, 0),
    CP(CRU_MAKEOVER, CLIST_FHAIR, FHAIR_CHOPS, "Mutton Chops",
       "Give a citizen a makeover", 0, 0),
    CP(CRU_SELF_PORTRAIT, CLIST_HAT, HAT_COWBOY, "Cowboy Hat",
       "Create yourself for the story", 0, 0),
    CP(CRU_UNDO_LOTS, CLIST_BROW, BROW_BUSHY, "Bushy Brows",
       "Undo 30 times in one sitting", 0, 0),
    CP(CRU_VOICE_TESTS, CLIST_ACC, ACC_NOSE_RING, "Nose Ring",
       "Test 10 voices in one sitting", 0, 0),
    CP(CRU_RANDOMISE_LOTS, CLIST_MARK, MARK_SCALES, "Scales",
       "Randomise 25 times in one sitting", 0, 0),
    CP(CRU_COPY_PREMADE, CLIST_HAIR, HAIR_BOWL, "Bowl Cut",
       "Copy the look of a premade citizen", 0, 0),
    CP(CRU_POSSESSED_PEEK, CLIST_TAIL, TAIL_LIZARD, "Lizard Tail",
       "Peek at a possessed preview", 0, 0),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
