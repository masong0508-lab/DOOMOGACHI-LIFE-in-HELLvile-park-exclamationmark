/* test_creator.c - SCENE_CREATOR, driven headlessly (owner: creator).
 *
 * Creates citizens through the real scene with scripted button presses and
 * checks what lands in the town: a valid, sanitised resident with the
 * chosen values; edits of a premade citizen keep everything the creator
 * doesn't own (stats, relationships, slot); cancelling leaves the town
 * byte-for-byte unchanged; locked options can be looked at but not
 * chosen; undo/redo; the randomiser respects locks; nicknames persist.
 */
#include "test.h"
#include "../src/creator_int.h"
#include "doomo/playas.h"
#include "doomo/part_meta.h"

static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}

static void press(uint32_t b)
{
    frames(1, b);
    frames(1, 0);
}

static void press_n(uint32_t b, int n)
{
    while (n-- > 0) press(b);
}

static void type_text(const char *s)
{
    while (*s) ui_text_entry_feed(*s++);
}

/* An empty scene to sit under the creator, so leaving it pops back. */
static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

static void open_creator(int arg)
{
    scene_set(&base_scene, 0);
    scene_push(&SCENE_CREATOR, arg);
    frames(3, 0);
}

static int row_index(int tab, int field, int kind)
{
    int i;
    for (i = 0; i < cr_tabs[tab].nrows; i++)
        if (cr_tabs[tab].rows[i].field == field && cr_tabs[tab].rows[i].kind == kind) return i;
    return -1;
}

/* Select row `i` of tab `tab` by pressing L/R and UP/DOWN like a player. */
static void go_row(int tab, int i)
{
    int guard = 0;
    while (cr.tab != tab && guard++ < 20) press(BTN_R);
    guard = 0;
    while (cr.row[cr.tab] != i && guard++ < 64) press(BTN_DOWN);
}

static int count_living(void)
{
    return town_count(-1);
}

/* ---------------------------------------------------------------------- */
/* tables                                                                   */
TEST(creator_rows_cover_every_field)
{
    /* rows address fields by id (resident_acc.h): 0..GF_COUNT-1 are gene fields,
     * RES_F_SPECIES / ROMANCE / FLAGS / NAME are the record-only ones */
    uint8_t covered[256];
    int t, i;
    memset(covered, 0, sizeof(covered));
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *r = &cr_tabs[t].rows[i];
            if (r->field == CR_NOFIELD) continue;
            CHECK(r->field < GF_COUNT || (r->field >= RES_F_SPECIES && r->field <= RES_F_NAME));
            covered[r->field] = 1;
        }
    /* every look field (species is the hot byte, the rest are GF_BODY..GF_PATTERN_COLOR) ... */
    CHECK_MSG(covered[RES_F_SPECIES], "species has no creator row");
    for (i = GF_BODY; i <= GF_PATTERN_COLOR; i++)
        CHECK_MSG(covered[i], "look field %d has no creator row", i);
    /* ... and every identity field of the resident */
    for (i = 0; i < VOICE_COUNT; i++) CHECK(covered[GF_VOICE0 + i]);
    for (i = 0; i < PERS_COUNT; i++) CHECK(covered[GF_PERSONA0 + i]);
    CHECK(covered[RES_F_NAME]);
    CHECK(covered[GF_PRONOUN]);
    CHECK(covered[RES_F_ROMANCE]);
    CHECK(covered[GF_BDAY_MONTH]);
    CHECK(covered[GF_BDAY_DAY]);
    CHECK(covered[GF_LIKE_FOOD]);
    CHECK(covered[GF_LIKE_HOBBY]);
    CHECK(covered[GF_LIKE_MUSIC]);
    CHECK(covered[GF_LIKE_COLOR]);
    CHECK(covered[GF_HATE_FOOD]);
    CHECK(covered[GF_FEAR]);
    CHECK(covered[GF_CATCHPHRASE]);
    CHECK(covered[RES_F_FLAGS]);
}

TEST(creator_enum_rows_match_the_catalog)
{
    static const struct { int clist, count; } lists[] = {
        { CLIST_SPECIES, SPECIES_COUNT }, { CLIST_BODY, BODY_COUNT }, { CLIST_HEAD, HEAD_COUNT },
        { CLIST_EYE, EYE_COUNT }, { CLIST_BROW, BROW_COUNT }, { CLIST_NOSE, NOSE_COUNT },
        { CLIST_MOUTH, MOUTH_COUNT }, { CLIST_EARS, EARS_COUNT }, { CLIST_HAIR, HAIR_COUNT },
        { CLIST_FHAIR, FHAIR_COUNT }, { CLIST_HORNS, HORNS_COUNT }, { CLIST_TAIL, TAIL_COUNT },
        { CLIST_WINGS, WINGS_COUNT }, { CLIST_HALO, HALO_COUNT }, { CLIST_MARK, MARK_COUNT },
        { CLIST_TOP, TOP_COUNT }, { CLIST_BOTTOM, BOTTOM_COUNT }, { CLIST_SHOES, SHOES_COUNT },
        { CLIST_HAT, HAT_COUNT }, { CLIST_ACC, ACC_COUNT }, { CLIST_AURA, AURA_COUNT },
        { CLIST_PATTERN, PATTERN_COUNT }
    };
    int t, i, k, seen[CLIST_COUNT];
    char buf[48];
    memset(seen, 0, sizeof(seen));
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *r = &cr_tabs[t].rows[i];
            if (r->kind != RK_ENUM) continue;
            CHECK(r->count > 0);
            if (r->clist != CR_NOLIST) {
                CHECK(r->clist < CLIST_COUNT);
                seen[r->clist] = 1;
                for (k = 0; k < DOOMO_COUNTOF(lists); k++)
                    if (lists[k].clist == r->clist) CHECK_EQ(r->count, lists[k].count);
            }
            /* every choice has a name */
            for (k = 0; k < r->count; k++) {
                const char *n = cr_choice_name(r, k, buf);
                CHECK_MSG(n && n[0], "%s choice %d has no name", r->label, k);
            }
        }
    for (k = 0; k < DOOMO_COUNTOF(lists); k++)
        CHECK_MSG(seen[lists[k].clist], "catalog list %d has no row", lists[k].clist);
}

static int preset_ok(const CrPreset *p)
{
    int i;
    for (i = 0; i + 1 < CR_PRESET_KV; i += 2) {
        if (p->kv[i] == CR_NOFIELD) return 1;
        if (p->kv[i] > RES_F_FLAGS) return 0;
    }
    return 0;   /* not terminated */
}

TEST(creator_presets_are_valid)
{
    int i, sp;
    DoomoResident r;
    for (i = 0; i < SPECIES_COUNT * CR_PRESETS_PER_SPECIES; i++) {
        const CrPreset *p = &cr_species_presets[i];
        CHECK_MSG(preset_ok(p), "species preset %d (%s) is malformed", i, p->name);
        CHECK(p->name && p->name[0]);
        /* grouped by species, species first */
        CHECK_EQ(p->kv[0], RES_F_SPECIES);
        CHECK_EQ(p->kv[1], i / CR_PRESETS_PER_SPECIES);
        /* applying it gives a valid resident */
        resident_default(&r);
        {
            int k;
            for (k = 0; k + 1 < CR_PRESET_KV && p->kv[k] != CR_NOFIELD; k += 2)
                res_field_set(&r, p->kv[k], p->kv[k + 1]);
        }
        CHECK_MSG(resident_sanitize(&r) == 0, "preset %s produces invalid values", p->name);
    }
    for (i = 0; i < cr_outfit_preset_count; i++) CHECK(preset_ok(&cr_outfit_presets[i]));
    for (i = 0; i < cr_voice_preset_count; i++) CHECK(preset_ok(&cr_voice_presets[i]));
    /* switching species on a fresh island never brings in a locked part
     * (a base part another team locks falls back to a neutral one) */
    open_creator(-1);
    go_row(CR_TAB_BODY, row_index(CR_TAB_BODY, RES_F_SPECIES, RK_ENUM));
    for (sp = 0; sp < SPECIES_COUNT; sp++) {
        int t, k;
        press(BTN_RIGHT);
        for (t = 0; t < CR_TAB_COUNT; t++)
            for (k = 0; k < cr_tabs[t].nrows; k++) {
                const CrRow *row = &cr_tabs[t].rows[k];
                if (row->kind == RK_ENUM || row->kind == RK_SKIN || row->kind == RK_PAINT)
                    CHECK_MSG(!cr_value_locked(row, cr_row_value(&cr.draft, row)),
                              "species %d brought a locked %s", RES_SPECIES(&cr.draft), row->label);
            }
    }
    /* the show canon's species (C1) can be made from day one: their first
     * presets (Mad Hatter, Tribute Bot) use no locked option */
    CHECK(!cr_preset_locked(&cr_species_presets[SPECIES_TURKEY * CR_PRESETS_PER_SPECIES], NULL));
    CHECK(!cr_preset_locked(&cr_species_presets[SPECIES_ANIMATRONIC * CR_PRESETS_PER_SPECIES], NULL));
}

TEST(creator_unlock_table_is_in_range)
{
    int i, n = 0;
    for (i = 0; doomo_unlocks_creator[i].name; i++, n++) {
        const DoomoUnlock *u = &doomo_unlocks_creator[i];
        CHECK(u->id >= 256 && u->id <= 319);
        CHECK(u->how && u->how[0]);
        CHECK(unlock_by_id(u->id) == u);
    }
    CHECK(n >= 16);
}

/* ---------------------------------------------------------------------- */
/* making a citizen                                                         */
TEST(creator_makes_a_new_hillvile_citizen)
{
    int species_row = row_index(CR_TAB_BODY, RES_F_SPECIES, RK_ENUM);
    int start_species, id;
    const DoomoResident *r;
    CHECK(species_row >= 0);
    CHECK_EQ(count_living(), 0);
    open_creator(-1);
    CHECK(scene_top() == &SCENE_CREATOR);
    CHECK_EQ(cr.mode, CR_NEW);
    CHECK_EQ(cr.district, DISTRICT_HILLVILE);
    start_species = RES_SPECIES(&cr.draft);
    /* species: two to the right */
    go_row(CR_TAB_BODY, species_row);
    press_n(BTN_RIGHT, 2);
    CHECK_EQ(RES_SPECIES(&cr.draft), (start_species + 2) % SPECIES_COUNT);
    /* height slider up */
    go_row(CR_TAB_BODY, row_index(CR_TAB_BODY, GF_HEIGHT, RK_SLIDER));
    press_n(BTN_RIGHT, 3);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 10);
    /* top colour through the swatch grid: two right, one down */
    go_row(CR_TAB_OUTFIT, row_index(CR_TAB_OUTFIT, GF_TOP_COLOR, RK_PAINT));
    {
        int before = RES_LOOK(&cr.draft, GF_TOP_COLOR);
        press(BTN_A);
        CHECK_EQ(cr.phase, CRP_COLOR);
        press_n(BTN_RIGHT, 2);
        press(BTN_DOWN);
        press(BTN_A);
        CHECK_EQ(cr.phase, CRP_MAIN);
        CHECK_EQ(RES_LOOK(&cr.draft, GF_TOP_COLOR), (before + 2 + 8) % PAL_PAINT_COUNT);
    }
    /* blush off */
    go_row(CR_TAB_FACE, row_index(CR_TAB_FACE, GF_CHEEKS, RK_FLAG));
    CHECK(RES_LOOK(&cr.draft, GF_CHEEKS) & CHEEK_BLUSH);
    press(BTN_A);
    CHECK(!(RES_LOOK(&cr.draft, GF_CHEEKS) & CHEEK_BLUSH));
    /* personality: quirk to the max */
    go_row(CR_TAB_PERSONA, 4);
    frames(80, BTN_RIGHT);
    frames(1, 0);
    CHECK_EQ(RES_PERSONA(&cr.draft, PERS_QUIRK), 15);
    /* move in: there is no name yet, so the keyboard opens first */
    press(BTN_START);
    CHECK_EQ(cr.phase, CRP_MENU);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    CHECK_EQ(count_living(), 0);
    type_text("Mochi\n");
    CHECK(scene_top() == &SCENE_CREATOR);
    CHECK_EQ(cr.phase, CRP_MOVEIN);
    /* the town has a valid, sanitised, custom citizen with our choices */
    id = creator_last_result();
    CHECK_EQ(id, 0);
    CHECK_EQ(count_living(), 1);
    r = town_res(id);
    CHECK_STR(r->name, "Mochi");
    CHECK_EQ(r->status, RES_ALIVE);
    CHECK(r->flags & RF_CUSTOM);
    CHECK(!(r->flags & RF_HELLBORN));
    CHECK_EQ(RES_SPECIES(r), (start_species + 2) % SPECIES_COUNT);
    CHECK_EQ(RES_LOOK(r, GF_HEIGHT), 10);
    CHECK(!(RES_LOOK(r, GF_CHEEKS) & CHEEK_BLUSH));
    CHECK_EQ(RES_PERSONA(r, PERS_QUIRK), 15);
    {
        DoomoResident copy = *r;
        CHECK_EQ(resident_sanitize(&copy), 0);
    }
    CHECK_EQ(progress_get(PROG_CITIZENS_CREATED), 1);
    /* the finale ends and pops back */
    frames(30, 0);
    press(BTN_A);
    CHECK(scene_top() == &base_scene);
}

TEST(creator_new_hellvile_citizen_moves_to_hellvile)
{
    int id;
    open_creator(-2);
    CHECK_EQ(cr.district, DISTRICT_HELLVILE);
    CHECK(cr.draft.flags & RF_HELLBORN);
    press(BTN_START);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    type_text("Grimsby\n");
    id = creator_last_result();
    CHECK(id >= DOOMO_DISTRICT_SIZE && id < DOOMO_MAX_RESIDENTS);
    CHECK_STR(town_res(id)->name, "Grimsby");
    CHECK(town_res(id)->flags & RF_HELLBORN);
    CHECK(unlock_has(CRU_MOVE_TO_HELLVILE));
}

TEST(creator_district_can_be_switched_for_new_citizens)
{
    int id;
    open_creator(-1);
    go_row(CR_TAB_HOME, 0);
    press(BTN_RIGHT);
    CHECK_EQ(cr.district, DISTRICT_HELLVILE);
    CHECK(cr.draft.flags & RF_HELLBORN);
    CHECK_EQ(cr.home_id, DOOMO_DISTRICT_SIZE);
    /* a typed name skips the prompt */
    go_row(CR_TAB_PROFILE, 0);
    press(BTN_A);
    type_text("Nyx\n");
    CHECK(cr.name_ok);
    press(BTN_START);
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_MOVEIN);
    id = creator_last_result();
    CHECK_EQ(id, DOOMO_DISTRICT_SIZE);
}

TEST(creator_full_district_refuses_politely)
{
    int i;
    for (i = 0; i < DOOMO_DISTRICT_SIZE; i++) {
        resident_generate(&doomo_town.res[i], (uint32_t)i);
    }
    open_creator(-1);
    CHECK_EQ(cr.home_id, -1);
    go_row(CR_TAB_PROFILE, 0);
    press(BTN_A);
    type_text("Extra\n");
    press(BTN_START);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_CREATOR);
    CHECK(cr.phase != CRP_MOVEIN);
    CHECK_EQ(town_count_district(DISTRICT_HILLVILE, -1), DOOMO_DISTRICT_SIZE);
    CHECK(ui_toast_pending() > 0);
}

/* ---------------------------------------------------------------------- */
/* editing                                                                  */
TEST(creator_edits_a_premade_citizen_in_place)
{
    const int id = ROSTER_ID_PIP;
    DoomoResident before;
    int edges_before, top_row, rels_before;
    uint16_t ids[64];
    town_new(TOWN_FREEPLAY, 77, TOWN_POP_PREMADE);
    before = *town_res(id);
    edges_before = doomo_town.edge_count;
    rels_before = rel_list(id, ids, 64);
    open_creator(id);
    CHECK_EQ(cr.mode, CR_EDIT);
    CHECK(cr.name_ok);
    CHECK_STR(cr.draft.name, before.name);
    /* taller, a different build, blush toggled, louder voice */
    top_row = row_index(CR_TAB_BODY, GF_HEIGHT, RK_SLIDER);
    go_row(CR_TAB_BODY, top_row);
    press(RES_LOOK(&before, GF_HEIGHT) < 15 ? BTN_RIGHT : BTN_LEFT);
    go_row(CR_TAB_FACE, row_index(CR_TAB_FACE, GF_CHEEKS, RK_FLAG));
    press(BTN_A);
    press(BTN_START);
    press(BTN_A);                       /* named already: straight to the finale */
    CHECK_EQ(cr.phase, CRP_MOVEIN);
    CHECK_EQ(creator_last_result(), id);
    {
        const DoomoResident *r = town_res(id);
        DoomoResident copy = *r;
        CHECK_EQ(RES_LOOK(r, GF_HEIGHT), RES_LOOK(&before, GF_HEIGHT) < 15 ? RES_LOOK(&before, GF_HEIGHT) + 1
                                                          : RES_LOOK(&before, GF_HEIGHT) - 1);
        CHECK_EQ(RES_LOOK(r, GF_CHEEKS) & CHEEK_BLUSH, (RES_LOOK(&before, GF_CHEEKS) & CHEEK_BLUSH) ^ CHEEK_BLUSH);
        CHECK_STR(r->name, before.name);
        CHECK(memcmp(r->stat, before.stat, sizeof(before.stat)) == 0);
        CHECK_EQ(r->status, before.status);
        CHECK_EQ(r->level, before.level);
        CHECK(r->flags & RF_CUSTOM);
        CHECK_EQ(resident_sanitize(&copy), 0);
    }
    CHECK_EQ(doomo_town.edge_count, edges_before);
    CHECK_EQ(rel_list(id, ids, 64), rels_before);
    CHECK_EQ(town_count(-1), DOOMO_MAX_RESIDENTS);
    /* edits are makeovers, not new citizens */
    CHECK_EQ(progress_get(PROG_CITIZENS_CREATED), 0);
    CHECK(unlock_has(CRU_MAKEOVER));
}

TEST(creator_cancel_leaves_the_town_unchanged)
{
    static DoomoTown snap;
    const int id = 600;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_PREMADE);
    memcpy(&snap, &doomo_town, sizeof(snap));
    open_creator(id);
    press_n(BTN_RIGHT, 3);              /* new species */
    press(BTN_R);
    press(BTN_DOWN);
    press_n(BTN_RIGHT, 2);
    CHECK(cr_dirty());
    press(BTN_B);                       /* asks first */
    CHECK(scene_top() == &SCENE_CONFIRM);
    press(BTN_B);                       /* "no": keep editing */
    CHECK(scene_top() == &SCENE_CREATOR);
    press(BTN_B);
    CHECK(scene_top() == &SCENE_CONFIRM);
    press(BTN_LEFT);                    /* "yes" */
    press(BTN_A);
    CHECK(scene_top() == &base_scene);
    CHECK_EQ(creator_last_result(), -1);
    CHECK(memcmp(&snap, &doomo_town, sizeof(snap)) == 0);
    /* a new citizen that is cancelled leaves no trace either */
    open_creator(-1);
    press_n(BTN_RIGHT, 2);
    press(BTN_B);
    press(BTN_LEFT);
    press(BTN_A);
    CHECK(scene_top() == &base_scene);
    CHECK(memcmp(&snap, &doomo_town, sizeof(snap)) == 0);
    /* nothing changed: B leaves at once */
    open_creator(id);
    press(BTN_B);
    CHECK(scene_top() == &base_scene);
}

TEST(creator_story_protagonist)
{
    open_creator(-3);
    CHECK_EQ(cr.mode, CR_PLAYER);
    CHECK(!cr.name_ok);                 /* "You" is only a placeholder */
    CHECK_EQ(cr.phase, CRP_HERO);       /* PA2: the three cards come first */
    press_n(BTN_DOWN, 2);               /* "make your own" */
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_MAIN);
    press(BTN_START);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    type_text("Pal\n");
    CHECK_EQ(creator_last_result(), DOOMO_PLAYER_ID);
    CHECK_STR(doomo_town.player.name, "Pal");
    CHECK(doomo_town.player.flags & RF_PLAYER);
    CHECK(doomo_town.player.flags & RF_CUSTOM);
    CHECK_EQ(count_living(), 0);        /* the player is not a resident slot */
}

TEST(creator_hero_cards_sunny_and_marvin)
{
    int k;
    for (k = 0; k < 2; k++) {
        town_new(TOWN_STORY, 90 + (uint32_t)k, TOWN_POP_PREMADE);
        open_creator(-3);
        CHECK_EQ(cr.phase, CRP_HERO);
        CHECK_EQ(cr.hero_sel, 0);
        if (k) press(BTN_DOWN);
        CHECK_STR(cr.draft.name, k ? "Marvin" : "Sunny");
        press(BTN_A);
        CHECK_EQ(cr.phase, CRP_MOVEIN);
        CHECK_EQ(playas_hero_kind() == (k ? HERO_MARVIN : HERO_SUNNY), 1);
        CHECK_STR(doomo_town.player.name, k ? "Marvin" : "Sunny");
        CHECK_EQ(RES_PRONOUN(&doomo_town.player), k ? PRONOUN_HE : PRONOUN_SHE);
        CHECK_EQ(RES_SPECIES(&doomo_town.player), k ? SPECIES_FOXKIN : SPECIES_BUNNYKIN);
        CHECK(doomo_town.player.flags & RF_PLAYER);
        CHECK_EQ(creator_last_result(), DOOMO_PLAYER_ID);
    }
    playas_set_hero_kind(HERO_CUSTOM);
}

TEST(creator_hero_card_texts_fit_the_small_screen)
{
    int i;
    /* the card text starts 16 px into the window; the GBA screen is 240 wide */
    for (i = 0; i < 3; i++) {
        CHECK_MSG(ui_text_width(cr_hero_name(i)) + 16 + 12 <= 240, "card name %d too wide", i);
        CHECK_MSG(ui_text_width(cr_hero_tag(i)) + 16 + 12 <= 240, "card tagline %d too wide", i);
    }
}

TEST(creator_hero_cards_back_leaves_and_named_hero_skips_them)
{
    town_new(TOWN_STORY, 95, TOWN_POP_PREMADE);
    open_creator(-3);
    CHECK_EQ(cr.phase, CRP_HERO);
    press(BTN_B);
    CHECK(scene_top() == &base_scene);
    memcpy(doomo_town.player.name, "Zed", 4);
    doomo_town.player.status = RES_ALIVE;
    open_creator(-3);
    CHECK_EQ(cr.phase, CRP_MAIN);
}

/* ---------------------------------------------------------------------- */
/* tools                                                                    */
TEST(creator_undo_and_redo)
{
    int h0;
    open_creator(-1);
    h0 = RES_LOOK(&cr.draft, GF_HEIGHT);
    go_row(CR_TAB_BODY, row_index(CR_TAB_BODY, GF_HEIGHT, RK_SLIDER));
    press(BTN_RIGHT);
    frames(40, 0);                      /* separate steps */
    press(BTN_RIGHT);
    frames(40, 0);
    press(BTN_DOWN);
    press(BTN_LEFT);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), h0 + 2);
    CHECK_EQ(cr_undo_count(), 3);
    press(BTN_X);                       /* undo: build */
    press(BTN_X);                       /* undo: height +2 */
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), h0 + 1);
    CHECK_EQ(cr_redo_count(), 2);
    press(BTN_Y);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), h0 + 2);
    press(BTN_X);
    press(BTN_X);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), h0);
    CHECK_EQ(cr_undo_count(), 0);
    /* at least 16 steps are remembered */
    {
        int i;
        for (i = 0; i < 18; i++) {
            press(i & 1 ? BTN_LEFT : BTN_RIGHT);
            frames(35, 0);
        }
        CHECK_MSG(cr_undo_count() >= 16, "undo %d redo %d hcur %d hcount %d", cr_undo_count(),
                  cr_redo_count(), cr.hcur, cr.hcount);
    }
}

TEST(creator_locked_options_are_shown_but_not_chosen)
{
    int eyes = row_index(CR_TAB_FACE, GF_EYE_STYLE, RK_ENUM);
    const CrRow *row = cr_row_at(CR_TAB_FACE, eyes);
    int i, before;
    CHECK(!unlock_option_ok(CLIST_EYE, EYE_GOAT));
    open_creator(-1);
    CHECK(cr_value_locked(row, EYE_GOAT));
    CHECK(cr_lock_hint(row, EYE_GOAT)[0]);
    go_row(CR_TAB_FACE, eyes);
    before = RES_LOOK(&cr.draft, GF_EYE_STYLE);
    /* the picture grid (CR6): walk onto Goat Eyes and try to take them */
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_GRID);
    for (i = 0; i < EYE_COUNT && cr_grid_choice(cr.g_sel) != EYE_GOAT; i++) press(BTN_RIGHT);
    CHECK_EQ(cr_grid_choice(cr.g_sel), EYE_GOAT);
    CHECK_EQ(cr.browse, EYE_GOAT);
    frames(8, 0);
    CHECK(cr.shown_locked);             /* shown on the citizen, in lavender */
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_GRID);       /* refused */
    press(BTN_B);
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_EYE_STYLE), before);
    /* stepping with LEFT/RIGHT never writes a locked value either */
    for (i = 0; i < EYE_COUNT * 2; i++) {
        press(BTN_RIGHT);
        CHECK(!cr_value_locked(row, RES_LOOK(&cr.draft, GF_EYE_STYLE)));
    }
    /* once unlocked it can be chosen (the creator sees new unlocks from its next tick: CR9 memo) */
    unlock_grant_match(UL_CAT_PART, CLIST_EYE, EYE_GOAT);
    frames(1, 0);
    CHECK(!cr_value_locked(row, EYE_GOAT));
}

TEST(creator_keeps_a_locked_option_the_citizen_already_has)
{
    int id;
    DoomoResident r;
    resident_generate(&r, 9);
    RES_SET_LOOK(&r, GF_EYE_STYLE, EYE_GOAT);                 /* locked for new choices */
    doomo_town.res[3] = r;
    id = 3;
    open_creator(id);
    CHECK(!cr_value_locked(cr_row_at(CR_TAB_FACE, row_index(CR_TAB_FACE,
          GF_EYE_STYLE, RK_ENUM)), EYE_GOAT));
    press(BTN_START);
    press(BTN_A);
    CHECK_EQ(RES_LOOK(town_res(id), GF_EYE_STYLE), EYE_GOAT);
}

TEST(creator_randomiser_respects_locks)
{
    int k, t, i;
    open_creator(-2);
    for (k = 0; k < 30; k++) {
        press(BTN_START);
        press(BTN_DOWN);
        press(BTN_A);                            /* randomise all (hellish) */
        CHECK_EQ(cr.phase, CRP_MAIN);
        for (t = 0; t < CR_TAB_COUNT; t++)
            for (i = 0; i < cr_tabs[t].nrows; i++) {
                const CrRow *row = &cr_tabs[t].rows[i];
                if (row->kind == RK_ENUM || row->kind == RK_PAINT || row->kind == RK_SKIN ||
                    row->kind == RK_PAINT_NONE)
                    CHECK_MSG(!cr_value_locked(row, cr_row_value(&cr.draft, row)),
                              "randomise %d picked a locked %s", k, row->label);
            }
        {
            DoomoResident copy = cr.draft;
            copy.status = RES_ALIVE;
            CHECK_EQ(resident_sanitize(&copy), 0);
        }
    }
    CHECK(unlock_has(CRU_RANDOMISE_LOTS));
    /* randomise this tab only touches this tab's fields */
    {
        DoomoResident before = cr.draft;
        go_row(CR_TAB_VOICE, 2);
        press(BTN_START);
        press(BTN_DOWN);
        press(BTN_RIGHT);                        /* Randomise: all -> this tab */
        press(BTN_A);
        CHECK(memcmp(RES_LOOK_VIEW(&before), RES_LOOK_VIEW(&cr.draft), sizeof(DoomoLook)) == 0);
        CHECK(memcmp(RES_PERSONA_VIEW(&before), RES_PERSONA_VIEW(&cr.draft), PERS_COUNT) == 0);
    }
}

TEST(creator_species_change_brings_species_parts)
{
    int species_row = row_index(CR_TAB_BODY, RES_F_SPECIES, RK_ENUM);
    open_creator(-1);
    go_row(CR_TAB_BODY, species_row);
    while (RES_SPECIES(&cr.draft) != SPECIES_FOXKIN) press(BTN_RIGHT);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_EARS), EARS_FOX);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_TAIL), TAIL_FOX);
    while (RES_SPECIES(&cr.draft) != SPECIES_OWLKIN) press(BTN_RIGHT);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_MOUTH_STYLE), MOUTH_BEAK);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_TAIL), TAIL_FEATHER_FAN);
    while (RES_SPECIES(&cr.draft) != SPECIES_IMP) press(BTN_RIGHT);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_TAIL), TAIL_DEVIL);
    CHECK(RES_LOOK(&cr.draft, GF_HORNS) != HORNS_NONE);
    /* a full preset */
    press(BTN_DOWN);
    press(BTN_RIGHT);
    press(BTN_A);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HORNS), HORNS_DEVIL);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_WINGS), WINGS_TINY_BAT);
}

TEST(creator_menu_lists_only_what_applies)
{
    char buf[48];
    open_creator(-1);
    press(BTN_START);
    CHECK_EQ(cr.phase, CRP_MENU);
    /* a fresh citizen: nothing to undo, redo or reset, so none of those are listed */
    CHECK_EQ(cr_menu_index(CM_UNDO), -1);
    CHECK_EQ(cr_menu_index(CM_REDO), -1);
    CHECK_EQ(cr_menu_index(CM_RESET), -1);
    CHECK_EQ(cr_menu_index(CM_DONE), 0);
    CHECK_EQ(cr_menu_index(CM_RANDOM), 1);
    CHECK(cr_menu_index(CM_COPY) > 0 && cr_menu_index(CM_QUIT) > cr_menu_index(CM_COPY));
    /* Randomise is one entry; LEFT / RIGHT on it picks the scope (this tab is only for a tab with rows) */
    press(BTN_DOWN);
    CHECK_EQ(cr.menu_sel, CM_RANDOM);
    CHECK(strstr(cr_menu_label(CM_RANDOM, buf), "all") != NULL);
    press(BTN_RIGHT);
    CHECK(strstr(cr_menu_label(CM_RANDOM, buf), "this tab") != NULL);
    press(BTN_A);                               /* rolls the tab, which is a change */
    CHECK_EQ(cr.phase, CRP_MAIN);
    press(BTN_START);
    CHECK(cr_menu_index(CM_UNDO) > 0);
    CHECK(cr_menu_index(CM_RESET) > 0);
    CHECK_EQ(cr_menu_index(CM_REDO), -1);
    press(BTN_B);
    /* the Home tab has no tab to roll: always everything */
    go_row(CR_TAB_HOME, 0);
    press(BTN_START);
    CHECK(strstr(cr_menu_label(CM_RANDOM, buf), "this tab") == NULL);
}

TEST(creator_copy_a_premade_look)
{
    const DoomoResident *src;
    open_creator(-1);
    press(BTN_START);
    press_n(BTN_DOWN, cr_menu_index(CM_COPY));        /* Undo and Redo are not listed yet */
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_COPY);
    CHECK_EQ(cr.copy_src, 1);           /* empty island: premade citizens */
    press_n(BTN_DOWN, 4);
    src = cr_copy_res(cr_copy_id_at(cr.copy_sel));
    CHECK(src != NULL);
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(RES_SPECIES(&cr.draft), RES_SPECIES(src));
    CHECK_EQ(RES_LOOK(&cr.draft, GF_TOP), RES_LOOK(src, GF_TOP));
    CHECK(unlock_has(CRU_COPY_PREMADE));
}

TEST(creator_nicknames_persist_and_follow_the_citizen)
{
    const DoomoSaveExt *ext = doomo_save_ext_extra;
    uint8_t buf[512];
    uint32_t n;
    int id;
    open_creator(-1);
    go_row(CR_TAB_PROFILE, 0);
    press(BTN_A);
    type_text("Biscuit\n");
    press(BTN_DOWN);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    type_text("Bis\n");
    CHECK_STR(cr.nick, "Bis");
    press(BTN_START);
    press(BTN_A);
    id = creator_last_result();
    CHECK(id >= 0);
    CHECK_STR(creator_nickname(id), "Bis");
    /* the save chunk round-trips */
    CHECK(ext != NULL);
    n = ext->size();
    CHECK(n > 0 && n <= sizeof(buf));
    CHECK_EQ(ext->write(buf, sizeof(buf)), n);
    ext->reset();
    CHECK_STR(creator_nickname(id), "");
    CHECK(ext->read(buf, n, ext->version));
    CHECK_STR(creator_nickname(id), "Bis");
    /* a different citizen in the same slot does not inherit it */
    memcpy(town_res(id)->name, "Somebody", 9);
    CHECK_STR(creator_nickname(id), "");
}

TEST(creator_draws_every_phase_at_both_sizes)
{
    /* smoke: every screen draws at GBA and PS1 sizes without trouble */
    static const int sizes[2][2] = { { 240, 160 }, { 320, 240 } };
    int s, tab;
    for (s = 0; s < 2; s++) {
        NatOptions o;
        memset(&o, 0, sizeof(o));
        o.w = sizes[s][0]; o.h = sizes[s][1]; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
        open_creator(-1);
        CHECK(crl.rows >= 6);
        CHECK(crl.lx + crl.lw <= o.w);
        CHECK(crl.py + crl.ph <= o.h - 12);
        for (tab = 0; tab < CR_TAB_COUNT; tab++) { press(BTN_R); frames(4, 0); }
        press(BTN_A); frames(15, 0); press(BTN_B);
        press(BTN_START); frames(15, 0); press(BTN_B);
        press(BTN_SELECT); frames(15, BTN_RIGHT); frames(3, 0); press(BTN_B);
        CHECK(scene_top() == &SCENE_CREATOR);
    }
    {
        NatOptions o;
        memset(&o, 0, sizeof(o));
        o.w = 240; o.h = 160; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
    }
}

/* ---------------------------------------------------------------------- */
/* CR12: the age gate for parts                                             */
static int age_row_index(void)
{
    int t, i;
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++)
            if (CR_IS_AGE_ROW(&cr_tabs[t].rows[i])) return t * 64 + i;
    return -1;
}

TEST(creator_hides_parts_above_the_drafts_age)
{
    int hat = row_index(CR_TAB_OUTFIT, GF_HAT, RK_ENUM), i, seen = 0;
    const CrRow *row = cr_row_at(CR_TAB_OUTFIT, hat);
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, PAGE_ADULT);
    part_age_test_set(GF_HAT, HAT_TIARA, PAGE_TEEN);
    open_creator(-1);
    resident_set_stage(&cr.draft, LIFE_CHILD);
    CHECK(cr_value_hidden(row, HAT_TRAFFIC_CONE));
    CHECK(cr_value_hidden(row, HAT_TIARA));
    CHECK(!cr_value_hidden(row, HAT_NONE));
    CHECK(!cr_value_hidden(row, HAT_CAP));
    CHECK_EQ(cr_visible_count(row), HAT_COUNT - 2);
    go_row(CR_TAB_OUTFIT, hat);
    for (i = 0; i < HAT_COUNT * 2; i++) {               /* LEFT / RIGHT never land on them */
        press(BTN_RIGHT);
        CHECK(RES_LOOK(&cr.draft, GF_HAT) != HAT_TRAFFIC_CONE && RES_LOOK(&cr.draft, GF_HAT) != HAT_TIARA);
    }
    cr.browse = -1;                                      /* off any locked preview */
    press(BTN_A);                                        /* nor the grid (CR6): not even listed */
    CHECK_EQ(cr.phase, CRP_GRID);
    CHECK_EQ(cr.g_n, HAT_COUNT - 2);
    for (i = 0; i < HAT_COUNT * 2; i++) {
        press(BTN_RIGHT);
        seen |= cr_grid_choice(cr.g_sel) == HAT_TRAFFIC_CONE || cr_grid_choice(cr.g_sel) == HAT_TIARA;
    }
    for (i = 0; i < cr.g_nfilt * cr.g_n + cr.g_nfilt; i++) {     /* no chip lists them either */
        int k;
        if (i % (cr.g_n + 1) == 0) press(BTN_R);
        for (k = 0; k < cr.g_n; k++)
            seen |= cr_grid_choice(k) == HAT_TRAFFIC_CONE || cr_grid_choice(k) == HAT_TIARA;
    }
    CHECK(!seen);
    frames(2, 0);                                        /* the shorter list draws */
    press(BTN_B);
    CHECK_EQ(cr.phase, CRP_MAIN);
    /* a teen may wear the TEEN+ tiara, still not the ADULT cone */
    resident_set_stage(&cr.draft, LIFE_TEEN);
    CHECK(!cr_value_hidden(row, HAT_TIARA));
    CHECK(cr_value_hidden(row, HAT_TRAFFIC_CONE));
    resident_set_stage(&cr.draft, LIFE_ADULT);
    CHECK_EQ(cr_visible_count(row), HAT_COUNT);
    press(BTN_B); press(BTN_A);                          /* leave (discard) */
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, -1);
    part_age_test_set(GF_HAT, HAT_TIARA, -1);
}

TEST(creator_age_change_swaps_grown_up_parts_and_says_so)
{
    int a = age_row_index(), hat = row_index(CR_TAB_OUTFIT, GF_HAT, RK_ENUM), i, id, kid = 0;
    const CrRow *age;
    CHECK(a >= 0);
    age = cr_row_at(a / 64, a % 64);
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, PAGE_ADULT);
    open_creator(-1);
    cr_row_set(&cr.draft, cr_row_at(CR_TAB_OUTFIT, hat), HAT_TRAFFIC_CONE);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HAT), HAT_TRAFFIC_CONE);
    go_row(a / 64, a % 64);
    ui_toast_clear();
    for (i = 0; i < LIFE_COUNT && !kid; i++) {           /* step the Age row until a minor */
        press(BTN_RIGHT);
        kid = LIFE_IS_MINOR(RES_STAGE(&cr.draft));
    }
    CHECK(kid);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HAT), HAT_NONE);     /* swapped for the slot's default */
    CHECK(ui_toast_pending() > 0);
    (void)age;
    /* the record that reaches the town never carries it either */
    cr_row_set(&cr.draft, cr_row_at(CR_TAB_OUTFIT, hat), HAT_TRAFFIC_CONE);
    memcpy(cr.draft.name, "Pip", 4);
    cr.name_ok = 1;
    press(BTN_START);
    CHECK_EQ(cr.phase, CRP_MENU);
    press(BTN_A);                                        /* Move in! */
    frames(400, 0);
    id = creator_last_result();
    CHECK(id >= 0);
    CHECK(LIFE_IS_MINOR(RES_STAGE(town_res(id))));
    CHECK_EQ(RES_LOOK(town_res(id), GF_HAT), HAT_NONE);
    part_age_test_set(GF_HAT, HAT_TRAFFIC_CONE, -1);
}

/* ---------------------------------------------------------------------- */
/* CR6: the picture grid, cameras, rows mode, help lines                    */
static const CrRow *go_part_row(int tab, int field)
{
    int i = row_index(tab, field, RK_ENUM);
    go_row(tab, i);
    return cr_row_at(tab, i);
}

TEST(creator_grid_lists_every_choice_with_its_icon)
{
    int t, i, k;
    open_creator(-1);
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            if (cr_row_slot(row) < 0) continue;
            go_row(t, i);
            press(BTN_A);
            CHECK_MSG(cr.phase == CRP_GRID, "%s does not open the grid", row->label);
            CHECK_EQ(cr.g_n, cr_visible_count(row));
            for (k = 0; k < cr.g_n; k++) {
                int v = cr_grid_choice(k);
                CHECK_MSG(part_icon_index(cr.g_meta[k]) >= 0, "%s choice %d has no icon", row->label, v);
                /* CR9: the lock bits made on opening are the locks */
                CHECK_EQ((cr.g_lock[v >> 3] >> (v & 7)) & 1, cr_value_locked(row, v));
            }
            frames(2, 0);
            press(BTN_B);
            CHECK_EQ(cr.phase, CRP_MAIN);
        }
}

TEST(creator_grid_tries_on_keeps_and_cancels)
{
    const CrRow *row;
    int before, undo, i, v = -1;
    open_creator(-1);
    row = go_part_row(CR_TAB_OUTFIT, GF_HAT);
    before = RES_LOOK(&cr.draft, GF_HAT);
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_GRID);
    CHECK_EQ(cr_grid_choice(cr.g_sel), before);
    for (i = 0; i < cr.g_n; i++) {                       /* the cursor tries an open hat on */
        press(BTN_RIGHT);
        v = cr_grid_choice(cr.g_sel);
        if (v != before && !cr_value_locked(row, v)) break;
    }
    CHECK(v >= 0 && v != before && !cr_value_locked(row, v));
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HAT), v);
    press(BTN_B);                                        /* B puts the old one back */
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HAT), before);
    undo = cr_undo_count();
    press(BTN_A);
    while (cr_grid_choice(cr.g_sel) != v) press(BTN_RIGHT);
    press(BTN_A);                                        /* A keeps it, one undo step */
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HAT), v);
    CHECK_EQ(cr_undo_count(), undo + 1);
}

TEST(creator_grid_filters_and_search)
{
    const CrRow *row;
    int f, k, found = 0;
    open_creator(-1);
    row = go_part_row(CR_TAB_OUTFIT, GF_HAT);
    press(BTN_A);
    CHECK(cr.g_nfilt >= 3);                              /* All, Unlocked, shapes, sets */
    for (f = 1; f < cr.g_nfilt; f++) {
        int kind, arg;
        char b[48];
        press(BTN_R);
        CHECK_EQ(cr.g_filter, f);
        kind = cr.g_filt[f] >> 5;
        arg = cr.g_filt[f] & 31;
        CHECK(cr_grid_chip(f, b)[0]);
        CHECK(cr.g_n > 0);
        for (k = 0; k < cr.g_n; k++) {
            int v = cr_grid_choice(k);
            const PartMeta *m = cr_choice_meta(row, v);
            if (kind == GFK_OPEN) CHECK(!cr_value_locked(row, v));
            if (kind == GFK_SHAPE) CHECK_EQ(m->shape, arg);
            if (kind == GFK_SET) CHECK_EQ(m->set, arg);
        }
    }
    press(BTN_SELECT);                                   /* back to All */
    CHECK_EQ(cr.g_filter, 0);
    CHECK_EQ(cr.g_n, cr_visible_count(row));
    press(BTN_START);                                    /* find "top": the Top Hat */
    type_text("top");
    ui_text_entry_feed('\n');
    frames(2, 0);
    CHECK_STR(cr.g_find, "top");
    for (k = 0; k < cr.g_n; k++) {
        CHECK(part_matches(cr_choice_meta(row, cr_grid_choice(k)), "top"));
        found |= cr_grid_choice(k) == HAT_TOP_HAT;
    }
    CHECK(found);
    {                                                    /* what it found is tried on at once */
        int v = cr_grid_choice(cr.g_sel);
        CHECK(cr.browse == v || RES_LOOK(&cr.draft, GF_HAT) == v);
    }
    press(BTN_SELECT);
    CHECK_EQ(cr.g_find[0], 0);
    press(BTN_B);
}

TEST(creator_camera_follows_the_slot)
{
    open_creator(-1);
    go_part_row(CR_TAB_OUTFIT, GF_HAT);
    CHECK_EQ(cr_camera(), CAM_BUST);
    go_part_row(CR_TAB_OUTFIT, GF_SHOES);
    CHECK_EQ(cr_camera(), CAM_LEGS);
    go_part_row(CR_TAB_OUTFIT, GF_TOP);
    CHECK_EQ(cr_camera(), CAM_FIGURE);
    go_part_row(CR_TAB_SPOOKY, GF_TAIL);
    CHECK_EQ(cr_camera(), CAM_BACK);
    frames(4, 0);                                        /* the back view draws */
    go_part_row(CR_TAB_SPOOKY, GF_HORNS);
    CHECK_EQ(cr_camera(), CAM_BUST);
    go_part_row(CR_TAB_FACE, GF_EYE_STYLE);
    CHECK_EQ(cr_camera(), CAM_BUST);
    go_part_row(CR_TAB_BODY, GF_BODY);
    CHECK_EQ(cr_camera(), CAM_FIGURE);
}

TEST(creator_rows_mode_hides_the_fine_tuning)
{
    int t, i, n_all = 0, n_basic = 0, k;
    open_creator(-1);
    CHECK_EQ(cr.show, CR_SHOW_ALL);
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) n_all += cr_row_shown(t, i);
    /* the START menu's Rows entry */
    press(BTN_START);
    k = cr_menu_index(CM_ROWS);
    CHECK(k >= 0);
    while (cr_menu_index(cr.menu_sel) != k) press(BTN_DOWN);
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(cr.show, CR_SHOW_SIMPLE);
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            n_basic += cr_row_shown(t, i);
            if (row->kind != RK_HEADER) CHECK_EQ(cr_row_shown(t, i), cr_row_level(row) == 0);
        }
    CHECK(n_basic < n_all);
    CHECK(n_basic >= 30);
    /* the cursor never lands on a hidden row */
    for (t = 0; t < CR_TAB_COUNT; t++) {
        for (i = 0; i < 40; i++) {
            press(BTN_DOWN);
            CHECK(cr_row_shown(cr.tab, cr.row[cr.tab]));
            CHECK(cr_cur_row()->kind != RK_HEADER);
        }
        frames(2, 0);
        press(BTN_R);
    }
    /* every part row stays in the basics */
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++)
            if (cr_row_slot(&cr_tabs[t].rows[i]) >= 0) CHECK(cr_row_shown(t, i));
}

TEST(creator_every_row_has_a_help_line_that_fits)
{
    int t, i;
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            const char *h;
            if (row->kind == RK_HEADER) continue;
            h = cr_row_help(row);
            CHECK_MSG(h[0], "row %s has no help line", row->label);
            CHECK_MSG(ui_text_width(h) <= 224, "row %s: help \"%s\" is %d px (GBA bar 224)", row->label, h,
                      ui_text_width(h));
        }
}

/* ---- CR9: the GBA's way (cr_calm) on the host: one layer group a tick ---------------------------- */
static uint32_t calm_tick;
static void cframes(int n, uint32_t b)
{
    while (n-- > 0) {
        portrait_cache_clock(++calm_tick, 1);
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

static void calm_begin(void)
{
    cr_calm = 1;
    calm_tick = 5000;
    portrait_cache_reset();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_CREATOR, -1);
    cframes(3, 0);
}

static void calm_end(void)
{
    scene_set(&base_scene, 0);
    cr_calm = 0;
    portrait_cache_reset();
}

/* CR9: on the GBA a new look is drawn in slices while the last one stays up: the pane keeps the old
 * view for a few ticks, then the new one lands (with a hop) within a bounded time; a held direction
 * never makes a whole figure render in one tick, and the bust's camera waits for the cursor to rest */
TEST(creator_calm_draws_a_new_look_in_slices)
{
    DoomoLook before;
    int i, row, waited = -1, stand_in = 0;
    calm_begin();
    while (cr.tab != CR_TAB_OUTFIT) { cframes(1, BTN_R); cframes(1, 0); }
    row = row_index(CR_TAB_OUTFIT, GF_TOP, RK_ENUM);
    CHECK(row >= 0);
    while (cr.row[cr.tab] != row) { cframes(1, BTN_DOWN); cframes(1, 0); }
    cframes(30, 0);
    CHECK(cr.disp.ok);
    CHECK(memcmp(&cr.disp.look, &cr.shown, sizeof(DoomoLook)) == 0);
    before = cr.disp.look;
    cframes(1, BTN_RIGHT);                               /* a new top (or a locked one to look at) */
    for (i = 0; i < 40; i++) {
        cframes(1, 0);
        if (memcmp(&cr.disp.look, &before, sizeof(DoomoLook)) == 0) stand_in++;
        if (memcmp(&cr.disp.look, &cr.shown, sizeof(DoomoLook)) == 0) { waited = i; break; }
    }
    CHECK(stand_in >= 2);                                /* the old figure stayed up meanwhile */
    CHECK(waited >= 0 && waited < 24);                   /* ... and the new one landed */
    CHECK(cr.hop > 0);
    /* the camera: moving onto the hat row keeps the figure until the cursor rests there */
    row = row_index(CR_TAB_OUTFIT, GF_HAT, RK_ENUM);
    while (cr.row[cr.tab] != row) { cframes(1, BTN_DOWN); cframes(1, 0); }
    CHECK_EQ(cr_camera(), CAM_BUST);
    CHECK_EQ(cr.disp.cam, CAM_FIGURE);
    for (i = 0; i < 40 && cr.disp.cam != CAM_BUST; i++) cframes(1, 0);
    CHECK_EQ(cr.disp.cam, CAM_BUST);
    calm_end();
}

/* CR6: SELECT + UP / DOWN jumps between a tab's sections (and does not open the turntable) */
TEST(creator_select_jumps_between_sections)
{
    int start, i, sec0, jumps = 0;
    open_creator(-1);
    while (cr.tab != CR_TAB_FACE) press(BTN_R);
    start = cr.row[cr.tab];
    sec0 = 0;
    for (i = 0; i < cr_tabs[cr.tab].nrows; i++) sec0 += cr_tabs[cr.tab].rows[i].kind == RK_HEADER;
    CHECK(sec0 >= 2);
    for (i = 0; i < sec0; i++) {                         /* DOWN through every section, back to the start */
        const char *was = cr_row_section(cr.tab, cr.row[cr.tab]);
        frames(1, BTN_SELECT);
        frames(1, BTN_SELECT | BTN_DOWN);
        frames(1, BTN_SELECT);
        frames(1, 0);
        CHECK_EQ(cr.phase, CRP_MAIN);                    /* no turntable */
        CHECK(cr_tabs[cr.tab].rows[cr.row[cr.tab]].kind != RK_HEADER);
        if (strcmp(was, cr_row_section(cr.tab, cr.row[cr.tab])) != 0) jumps++;
    }
    CHECK_EQ(cr.row[cr.tab], start);
    CHECK(jumps >= sec0 - 1);
    press(BTN_DOWN);                                     /* UP: first back to the section's first row */
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_UP);
    frames(1, 0);
    CHECK_EQ(cr.row[cr.tab], start);
    press(BTN_SELECT);                                   /* a plain SELECT still turns the citizen */
    CHECK_EQ(cr.phase, CRP_TURN);
}
