/* test_pet.c - the Doomogachi (owner: life): hatching, care, attention
 * calls and care mistakes, sickness, every evolution line reachable by a
 * matching care style, combat buffs, and the LCD art for all 16 forms. */
#include "test.h"

/* Care styles driven minute by minute. */
enum { STYLE_PERFECT, STYLE_DECENT, STYLE_SPOIL, STYLE_NEGLECT, STYLE_NONE };

static void care_minute(int style)
{
    const DoomoPet *p = &doomo_town.pet;
    switch (style) {
    case STYLE_PERFECT:
        if (pet_call_is_tantrum()) pet_discipline();
        if (pet_is_asleep() && !pet_lights_off()) pet_lights(1);
        if (!pet_is_asleep()) {
            if (p->hunger < PET_HEARTS) pet_feed(0);
            if (p->joy < PET_HEARTS) pet_play_result(1);
        }
        if (p->poop) pet_clean();
        if (pet_is_sick()) pet_medicine();
        break;
    case STYLE_DECENT:
        /* good care, but a sweet tooth keeps it from being saintly */
        if (pet_call_is_tantrum()) pet_discipline();
        if (pet_is_asleep() && !pet_lights_off()) pet_lights(1);
        if (!pet_is_asleep()) {
            if (p->hunger < 2) pet_feed(0);
            if (p->joy < 2) pet_play_result(1);
            if (p->holiness > 55) pet_feed(1);
        }
        if (p->poop >= 2) pet_clean();
        if (pet_is_sick()) pet_medicine();
        break;
    case STYLE_SPOIL:
        /* snacks for everything, scolded for nothing */
        if (!pet_is_asleep()) {
            if (p->joy < PET_HEARTS || p->hunger < 2) pet_feed(1);
            if (p->hunger < 2) pet_feed(0);
            if (p->holiness > 20) pet_discipline();
        }
        if (pet_is_asleep() && !pet_lights_off()) pet_lights(1);
        if (p->poop) pet_clean();
        if (pet_is_sick()) pet_medicine();
        break;
    case STYLE_NONE:
        break;
    default:
        /* neglect: only the bare minimum to keep it alive */
        if (pet_is_sick() && (p->sick >> 1 & 7) >= 4) pet_medicine();
        if (!pet_is_asleep() && p->hunger == 0 && pet_calling() == 0) pet_feed(0);
        break;
    }
}

static void live(int minutes, int style)
{
    while (minutes-- > 0) {
        doomo_town.minute = (uint16_t)((doomo_town.minute + 1) % 1440);
        if (doomo_town.minute == 0) doomo_town.day++;
        pet_tick_minute();
        if (doomo_town.pet.stage == PET_STAGE_DEAD) return;
        care_minute(style);
    }
}

static int raise_adult(int style)
{
    pet_new("Testy");
    doomo_town.minute = 9 * 60;
    live(PET_EGG_MINUTES + PET_BABY_MINUTES + PET_CHILD_MINUTES + PET_TEEN_MINUTES + 10, style);
    return doomo_town.pet.stage == PET_STAGE_ADULT ? pet_line() : -1;
}

TEST(pet_egg_hatches)
{
    pet_new("Mochi");
    CHECK(pet_exists());
    CHECK_EQ(doomo_town.pet.stage, PET_STAGE_EGG);
    CHECK_STR(doomo_town.pet.name, "Mochi");
    CHECK_EQ(pet_combat_buff(), 0);
    doomo_town.minute = 10 * 60;
    live(PET_EGG_MINUTES, STYLE_PERFECT);
    CHECK_EQ(doomo_town.pet.stage, PET_STAGE_BABY);
    CHECK_EQ(doomo_town.pet.form, 0);
    CHECK_STR(doomo_pet_form_names[doomo_town.pet.form], "Mochiblob");
    CHECK(pet_form_seen(0));
    CHECK(unlock_has(320));
}

TEST(pet_no_pet_means_nothing_happens)
{
    memset(&doomo_town.pet, 0, sizeof(doomo_town.pet));
    CHECK(!pet_exists());
    pet_tick_minute();
    CHECK_EQ(doomo_town.pet.stage, PET_STAGE_EGG);
    CHECK(pet_image(0) != NULL);
    CHECK_EQ(pet_image(0)->w, PET_SPR);
}

TEST(pet_feeding_and_refusing)
{
    pet_new("Nom");
    doomo_town.minute = 10 * 60;
    live(PET_EGG_MINUTES, STYLE_PERFECT);
    doomo_town.pet.hunger = 1;
    pet_feed(0);
    CHECK(pet_last_fed());
    CHECK_EQ(doomo_town.pet.hunger, 2);
    doomo_town.pet.hunger = PET_HEARTS;
    pet_feed(0);
    CHECK(!pet_last_fed());                       /* full: shakes its head */
    {
        int h = doomo_town.pet.holiness, w = doomo_town.pet.weight;
        doomo_town.pet.joy = 1;
        pet_feed(1);
        CHECK_EQ(doomo_town.pet.joy, 2);
        CHECK(doomo_town.pet.holiness < h);       /* sugar is devilish */
        CHECK(doomo_town.pet.weight > w);
    }
}

TEST(pet_ignored_call_is_a_care_mistake)
{
    pet_new("Sad");
    doomo_town.minute = 10 * 60;
    live(PET_EGG_MINUTES, STYLE_PERFECT);
    doomo_town.pet.hunger = 0;
    doomo_town.pet.mistakes = 0;
    pet_tick_minute();
    CHECK(pet_calling());
    CHECK(!pet_call_is_tantrum());
    live(16, STYLE_NEGLECT);
    CHECK(doomo_town.pet.mistakes >= 1);
}

TEST(pet_tantrum_discipline)
{
    int d, h;
    pet_new("Brat");
    doomo_town.minute = 10 * 60;
    live(PET_EGG_MINUTES, STYLE_PERFECT);
    /* fake a tantrum */
    doomo_town.pet.asleep = 4 | 8;
    CHECK(pet_call_is_tantrum());
    d = doomo_town.pet.discipline;
    h = doomo_town.pet.holiness;
    pet_discipline();
    CHECK(!pet_calling());
    CHECK(doomo_town.pet.discipline > d);
    CHECK(doomo_town.pet.holiness > h);
    /* scolding for nothing makes it sad and a bit devilish */
    h = doomo_town.pet.holiness;
    doomo_town.pet.joy = 3;
    pet_discipline();
    CHECK_EQ(doomo_town.pet.joy, 2);
    CHECK(doomo_town.pet.holiness < h);
}

TEST(pet_bedtime_lights)
{
    pet_new("Sleepy");
    doomo_town.minute = 10 * 60;
    live(PET_EGG_MINUTES + PET_BABY_MINUTES + 2, STYLE_PERFECT);
    CHECK(doomo_town.pet.stage >= PET_STAGE_CHILD);
    doomo_town.minute = 21 * 60;
    pet_tick_minute();
    CHECK(pet_is_asleep());
    CHECK(pet_calling());                         /* lights please */
    pet_lights(1);
    CHECK(!pet_calling());
    doomo_town.minute = 8 * 60;
    pet_tick_minute();
    CHECK(!pet_is_asleep());
    CHECK(!pet_lights_off());                     /* good morning */
}

TEST(pet_untreated_sickness_is_fatal_medicine_cures)
{
    pet_new("Coughy");
    doomo_town.minute = 10 * 60;
    live(PET_EGG_MINUTES, STYLE_PERFECT);
    doomo_town.pet.sick = 1 | (2 << 4);           /* two doses needed */
    CHECK(pet_is_sick());
    pet_medicine();
    CHECK(pet_is_sick());
    pet_medicine();
    CHECK(!pet_is_sick());
    doomo_town.pet.sick = 1 | (1 << 4);
    doomo_town.pet.stage_timer = 60000;           /* don't evolve meanwhile */
    live(6 * 60 + 5, STYLE_NONE);
    CHECK_EQ(doomo_town.pet.stage, PET_STAGE_DEAD);
    CHECK(pet_image(0) != NULL);
    CHECK_STR(pet_buff_text(), "Watching over you");
}

TEST(pet_lines_angel)
{
    int line = raise_adult(STYLE_PERFECT);
    CHECK_MSG(line == PET_LINE_ANGEL, "perfect care gave line %d (holiness %d mistakes %d)",
              line, doomo_town.pet.holiness, doomo_town.pet.mistakes);
    CHECK_EQ(doomo_town.pet.form, PET_FORM(PET_LINE_ANGEL, PET_STAGE_ADULT));
    CHECK_STR(doomo_pet_form_names[doomo_town.pet.form], "Archangelo");
    CHECK_EQ(pet_combat_buff(), 15);
    CHECK(progress_get(PROG_PET_EVOLUTIONS) >= 3);
}

TEST(pet_lines_cute)
{
    int line = raise_adult(STYLE_DECENT);
    CHECK_MSG(line == PET_LINE_CUTE, "decent care gave line %d (holiness %d mistakes %d)",
              line, doomo_town.pet.holiness, doomo_town.pet.mistakes);
    CHECK_STR(doomo_pet_form_names[doomo_town.pet.form], "Cupcakeling");
    CHECK_EQ(pet_combat_buff(), 10);
}

TEST(pet_lines_demon)
{
    int line = raise_adult(STYLE_SPOIL);
    CHECK_MSG(line == PET_LINE_DEMON, "spoiling gave line %d (holiness %d mistakes %d)",
              line, doomo_town.pet.holiness, doomo_town.pet.mistakes);
    CHECK_STR(doomo_pet_form_names[doomo_town.pet.form], "Demogorgoo");
    CHECK_EQ(pet_combat_buff(), 25);
}

TEST(pet_lines_feral)
{
    int line = raise_adult(STYLE_NEGLECT);
    CHECK_MSG(line == PET_LINE_FERAL, "neglect gave line %d (holiness %d mistakes %d stage %d)",
              line, doomo_town.pet.holiness, doomo_town.pet.mistakes, doomo_town.pet.stage);
    CHECK_STR(doomo_pet_form_names[doomo_town.pet.form], "Wildbeastie");
    CHECK_EQ(pet_combat_buff(), 20);
}

TEST(pet_art_every_form_is_distinct)
{
    static uint8_t a[PET_FORM_COUNT + 2][PET_SPR * PET_SPR];
    int f, g, fr;
    for (f = -2; f < PET_FORM_COUNT; f++) {
        int ink = 0, i;
        pet_render(f, PET_ANIM_IDLE0, a[f + 2]);
        for (i = 0; i < PET_SPR * PET_SPR; i++) {
            CHECK(a[f + 2][i] <= 3);
            ink += a[f + 2][i] == 1;
        }
        CHECK_MSG(ink > 30, "form %d has only %d ink dots", f, ink);
        /* every animation frame renders and the frames differ from idle */
        for (fr = 1; fr < PET_ANIM_COUNT; fr++) {
            static uint8_t b[PET_SPR * PET_SPR];
            pet_render(f, fr, b);
            if (f >= 0) CHECK_MSG(memcmp(a[f + 2], b, sizeof(b)) != 0, "form %d frame %d == idle", f, fr);
        }
    }
    for (f = 0; f < PET_FORM_COUNT + 2; f++)
        for (g = f + 1; g < PET_FORM_COUNT + 2; g++)
            CHECK_MSG(memcmp(a[f], a[g], sizeof(a[0])) != 0, "forms %d and %d look the same", f - 2, g - 2);
    {
        const DoomoImage *i1 = pet_form_image(3, 0), *i2 = pet_form_image(3, 0), *i3 = pet_form_image(7, 1);
        CHECK(i1 == i2);                          /* cached */
        CHECK(i3 != i1);
        CHECK(i3->uid != i1->uid);
    }
}

TEST(pet_scene_smoke)
{
    int i;
    pet_new("Scene");
    scene_set(&SCENE_PET, 0);
    for (i = 0; i < 40; i++) { plat_begin_frame(); doomo_frame(i % 7 == 0 ? BTN_RIGHT : 0); plat_end_frame(); }
    for (i = 0; i < 20; i++) { plat_begin_frame(); doomo_frame(i == 3 ? BTN_A : 0); plat_end_frame(); }
    for (i = 0; i < 20; i++) { plat_begin_frame(); doomo_frame(i == 3 ? BTN_SELECT : 0); plat_end_frame(); }
    CHECK(scene_top() == &SCENE_PET);
    for (i = 0; i < 6; i++) { plat_begin_frame(); doomo_frame(i == 2 ? BTN_START : 0); plat_end_frame(); }
    CHECK(scene_top() != &SCENE_PET);
}
