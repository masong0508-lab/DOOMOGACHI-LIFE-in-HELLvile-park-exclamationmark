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
    uint8_t covered[sizeof(DoomoResident)];
    int t, i;
    memset(covered, 0, sizeof(covered));
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *r = &cr_tabs[t].rows[i];
            if (r->field == CR_NOFIELD) continue;
            CHECK(r->field < sizeof(DoomoResident));
            covered[r->field] = 1;
            if (r->kind == RK_NAME)
                memset(covered + r->field, 1, DOOMO_NAME_LEN);
        }
    /* every byte of the look ... */
    for (i = 0; i < (int)sizeof(DoomoLook); i++)
        CHECK_MSG(covered[i], "look byte %d has no creator row", i);
    /* ... and every identity field of the resident */
    for (i = 0; i < VOICE_COUNT; i++) CHECK(covered[offsetof(DoomoResident, voice) + i]);
    for (i = 0; i < PERS_COUNT; i++) CHECK(covered[offsetof(DoomoResident, persona) + i]);
    CHECK(covered[offsetof(DoomoResident, name)]);
    CHECK(covered[offsetof(DoomoResident, pronoun)]);
    CHECK(covered[offsetof(DoomoResident, romance)]);
    CHECK(covered[offsetof(DoomoResident, bday_month)]);
    CHECK(covered[offsetof(DoomoResident, bday_day)]);
    CHECK(covered[offsetof(DoomoResident, like_food)]);
    CHECK(covered[offsetof(DoomoResident, like_hobby)]);
    CHECK(covered[offsetof(DoomoResident, like_music)]);
    CHECK(covered[offsetof(DoomoResident, like_color)]);
    CHECK(covered[offsetof(DoomoResident, hate_food)]);
    CHECK(covered[offsetof(DoomoResident, fear)]);
    CHECK(covered[offsetof(DoomoResident, catchphrase)]);
    CHECK(covered[offsetof(DoomoResident, flags)]);
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
        if (p->kv[i] >= sizeof(DoomoResident)) return 0;
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
        CHECK_EQ(p->kv[0], offsetof(DoomoResident, look.species));
        CHECK_EQ(p->kv[1], i / CR_PRESETS_PER_SPECIES);
        /* applying it gives a valid resident */
        resident_default(&r);
        {
            int k;
            for (k = 0; k + 1 < CR_PRESET_KV && p->kv[k] != CR_NOFIELD; k += 2)
                ((uint8_t *)&r)[p->kv[k]] = p->kv[k + 1];
        }
        CHECK_MSG(resident_sanitize(&r) == 0, "preset %s produces invalid values", p->name);
    }
    for (i = 0; i < cr_outfit_preset_count; i++) CHECK(preset_ok(&cr_outfit_presets[i]));
    for (i = 0; i < cr_voice_preset_count; i++) CHECK(preset_ok(&cr_voice_presets[i]));
    /* switching species on a fresh island never brings in a locked part
     * (a base part another team locks falls back to a neutral one) */
    open_creator(-1);
    go_row(CR_TAB_BODY, row_index(CR_TAB_BODY, offsetof(DoomoResident, look.species), RK_ENUM));
    for (sp = 0; sp < SPECIES_COUNT; sp++) {
        int t, k;
        press(BTN_RIGHT);
        for (t = 0; t < CR_TAB_COUNT; t++)
            for (k = 0; k < cr_tabs[t].nrows; k++) {
                const CrRow *row = &cr_tabs[t].rows[k];
                if (row->kind == RK_ENUM || row->kind == RK_SKIN || row->kind == RK_PAINT)
                    CHECK_MSG(!cr_value_locked(row, cr_row_value(&cr.draft, row)),
                              "species %d brought a locked %s", cr.draft.look.species, row->label);
            }
    }
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
    int species_row = row_index(CR_TAB_BODY, offsetof(DoomoResident, look.species), RK_ENUM);
    int start_species, id;
    const DoomoResident *r;
    CHECK(species_row >= 0);
    CHECK_EQ(count_living(), 0);
    open_creator(-1);
    CHECK(scene_top() == &SCENE_CREATOR);
    CHECK_EQ(cr.mode, CR_NEW);
    CHECK_EQ(cr.district, DISTRICT_HILLVILE);
    start_species = cr.draft.look.species;
    /* species: two to the right */
    go_row(CR_TAB_BODY, species_row);
    press_n(BTN_RIGHT, 2);
    CHECK_EQ(cr.draft.look.species, (start_species + 2) % SPECIES_COUNT);
    /* height slider up */
    go_row(CR_TAB_BODY, row_index(CR_TAB_BODY, offsetof(DoomoResident, look.height), RK_SLIDER));
    press_n(BTN_RIGHT, 3);
    CHECK_EQ(cr.draft.look.height, 10);
    /* top colour through the swatch grid: two right, one down */
    go_row(CR_TAB_OUTFIT, row_index(CR_TAB_OUTFIT, offsetof(DoomoResident, look.top_color), RK_PAINT));
    {
        int before = cr.draft.look.top_color;
        press(BTN_A);
        CHECK_EQ(cr.phase, CRP_COLOR);
        press_n(BTN_RIGHT, 2);
        press(BTN_DOWN);
        press(BTN_A);
        CHECK_EQ(cr.phase, CRP_MAIN);
        CHECK_EQ(cr.draft.look.top_color, (before + 2 + 8) % PAL_PAINT_COUNT);
    }
    /* blush off */
    go_row(CR_TAB_FACE, row_index(CR_TAB_FACE, offsetof(DoomoResident, look.cheeks), RK_FLAG));
    CHECK(cr.draft.look.cheeks & CHEEK_BLUSH);
    press(BTN_A);
    CHECK(!(cr.draft.look.cheeks & CHEEK_BLUSH));
    /* personality: quirk to the max */
    go_row(CR_TAB_PERSONA, 4);
    frames(80, BTN_RIGHT);
    frames(1, 0);
    CHECK_EQ(cr.draft.persona[PERS_QUIRK], 15);
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
    CHECK_EQ(r->look.species, (start_species + 2) % SPECIES_COUNT);
    CHECK_EQ(r->look.height, 10);
    CHECK(!(r->look.cheeks & CHEEK_BLUSH));
    CHECK_EQ(r->persona[PERS_QUIRK], 15);
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
    top_row = row_index(CR_TAB_BODY, offsetof(DoomoResident, look.height), RK_SLIDER);
    go_row(CR_TAB_BODY, top_row);
    press(before.look.height < 15 ? BTN_RIGHT : BTN_LEFT);
    go_row(CR_TAB_FACE, row_index(CR_TAB_FACE, offsetof(DoomoResident, look.cheeks), RK_FLAG));
    press(BTN_A);
    press(BTN_START);
    press(BTN_A);                       /* named already: straight to the finale */
    CHECK_EQ(cr.phase, CRP_MOVEIN);
    CHECK_EQ(creator_last_result(), id);
    {
        const DoomoResident *r = town_res(id);
        DoomoResident copy = *r;
        CHECK_EQ(r->look.height, before.look.height < 15 ? before.look.height + 1
                                                          : before.look.height - 1);
        CHECK_EQ(r->look.cheeks & CHEEK_BLUSH, (before.look.cheeks & CHEEK_BLUSH) ^ CHEEK_BLUSH);
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

/* ---------------------------------------------------------------------- */
/* tools                                                                    */
TEST(creator_undo_and_redo)
{
    int h0;
    open_creator(-1);
    h0 = cr.draft.look.height;
    go_row(CR_TAB_BODY, row_index(CR_TAB_BODY, offsetof(DoomoResident, look.height), RK_SLIDER));
    press(BTN_RIGHT);
    frames(40, 0);                      /* separate steps */
    press(BTN_RIGHT);
    frames(40, 0);
    press(BTN_DOWN);
    press(BTN_LEFT);
    CHECK_EQ(cr.draft.look.height, h0 + 2);
    CHECK_EQ(cr_undo_count(), 3);
    press(BTN_X);                       /* undo: build */
    press(BTN_X);                       /* undo: height +2 */
    CHECK_EQ(cr.draft.look.height, h0 + 1);
    CHECK_EQ(cr_redo_count(), 2);
    press(BTN_Y);
    CHECK_EQ(cr.draft.look.height, h0 + 2);
    press(BTN_X);
    press(BTN_X);
    CHECK_EQ(cr.draft.look.height, h0);
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
    int eyes = row_index(CR_TAB_FACE, offsetof(DoomoResident, look.eye_style), RK_ENUM);
    const CrRow *row = cr_row_at(CR_TAB_FACE, eyes);
    int i, before;
    CHECK(!unlock_option_ok(CLIST_EYE, EYE_GOAT));
    open_creator(-1);
    CHECK(cr_value_locked(row, EYE_GOAT));
    CHECK(cr_lock_hint(row, EYE_GOAT)[0]);
    go_row(CR_TAB_FACE, eyes);
    before = cr.draft.look.eye_style;
    /* the picker: walk onto Goat Eyes and try to take them */
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_PICK);
    while (cr.pick_sel != EYE_GOAT) press(BTN_DOWN);
    CHECK_EQ(cr.browse, EYE_GOAT);
    frames(8, 0);
    CHECK(cr.shown_locked);             /* silhouette preview */
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_PICK);       /* refused */
    press(BTN_B);
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(cr.draft.look.eye_style, before);
    /* stepping with LEFT/RIGHT never writes a locked value either */
    for (i = 0; i < EYE_COUNT * 2; i++) {
        press(BTN_RIGHT);
        CHECK(!cr_value_locked(row, cr.draft.look.eye_style));
    }
    /* once unlocked it can be chosen */
    unlock_grant_match(UL_CAT_PART, CLIST_EYE, EYE_GOAT);
    CHECK(!cr_value_locked(row, EYE_GOAT));
}

TEST(creator_keeps_a_locked_option_the_citizen_already_has)
{
    int id;
    DoomoResident r;
    resident_generate(&r, 9);
    r.look.eye_style = EYE_GOAT;                 /* locked for new choices */
    doomo_town.res[3] = r;
    id = 3;
    open_creator(id);
    CHECK(!cr_value_locked(cr_row_at(CR_TAB_FACE, row_index(CR_TAB_FACE,
          offsetof(DoomoResident, look.eye_style), RK_ENUM)), EYE_GOAT));
    press(BTN_START);
    press(BTN_A);
    CHECK_EQ(town_res(id)->look.eye_style, EYE_GOAT);
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
        press_n(BTN_DOWN, 2);
        press(BTN_A);
        CHECK(memcmp(&before.look, &cr.draft.look, sizeof(DoomoLook)) == 0);
        CHECK(memcmp(before.persona, cr.draft.persona, PERS_COUNT) == 0);
    }
}

TEST(creator_species_change_brings_species_parts)
{
    int species_row = row_index(CR_TAB_BODY, offsetof(DoomoResident, look.species), RK_ENUM);
    open_creator(-1);
    go_row(CR_TAB_BODY, species_row);
    while (cr.draft.look.species != SPECIES_FOXKIN) press(BTN_RIGHT);
    CHECK_EQ(cr.draft.look.ears, EARS_FOX);
    CHECK_EQ(cr.draft.look.tail, TAIL_FOX);
    while (cr.draft.look.species != SPECIES_OWLKIN) press(BTN_RIGHT);
    CHECK_EQ(cr.draft.look.mouth_style, MOUTH_BEAK);
    CHECK_EQ(cr.draft.look.tail, TAIL_FEATHER_FAN);
    while (cr.draft.look.species != SPECIES_IMP) press(BTN_RIGHT);
    CHECK_EQ(cr.draft.look.tail, TAIL_DEVIL);
    CHECK(cr.draft.look.horns != HORNS_NONE);
    /* a full preset */
    press(BTN_DOWN);
    press(BTN_RIGHT);
    press(BTN_A);
    CHECK_EQ(cr.draft.look.horns, HORNS_DEVIL);
    CHECK_EQ(cr.draft.look.wings, WINGS_TINY_BAT);
}

TEST(creator_copy_a_premade_look)
{
    const DoomoResident *src;
    open_creator(-1);
    press(BTN_START);
    press_n(BTN_DOWN, CM_COPY);
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_COPY);
    CHECK_EQ(cr.copy_src, 1);           /* empty island: premade citizens */
    press_n(BTN_DOWN, 4);
    src = cr_copy_res(cr_copy_id_at(cr.copy_sel));
    CHECK(src != NULL);
    press(BTN_A);
    CHECK_EQ(cr.phase, CRP_MAIN);
    CHECK_EQ(cr.draft.look.species, src->look.species);
    CHECK_EQ(cr.draft.look.top, src->look.top);
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
