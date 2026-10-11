/* pet.c - the Doomogachi: tamagotchi rules (owner: life). The LCD art is
 * pet_gfx.c, the screen is pet_scene.c.
 *
 * Egg -> baby -> child -> teen -> adult. Hunger and joy are 4 hearts each
 * and drain by the game minute (pet_tick_minute, driven by the sim). An
 * empty heart, a dark bedroom left lit, or poop piling up raise the
 * attention icon; ignoring it for 15 minutes is a care mistake. Now and
 * then the pet throws a fake tantrum that wants discipline. Sickness needs
 * medicine; left untreated for 6 hours the pet passes on and becomes a
 * little ghost. Evolution checks happen when a stage's timer runs out.
 */
#include "doomo/doomo.h"

const char *const doomo_pet_form_names[PET_FORM_COUNT] = {
    "Mochiblob", "Puffling", "Sprinkle", "Cupcakeling",
    "Halopup", "Seraphuff", "Choirbun", "Archangelo",
    "Scruffle", "Grumblin", "Feralfloof", "Wildbeastie",
    "Impkin", "Hornlet", "Hellmuffin", "Demogorgoo"
};
const char *const doomo_pet_line_names[4] = { "Cute", "Angel", "Feral", "Demon" };
const char *const doomo_pet_stage_names[PET_STAGE_DEAD + 1] = {
    "Egg", "Baby", "Child", "Teen", "Adult", "Departed"
};

/* bits of DoomoPet.asleep */
#define PA_ASLEEP  1
#define PA_DARK    2
#define PA_CALL    4
#define PA_FAKE    8
/* bits of DoomoPet.sick */
#define PS_SICK    1
#define PS_HOURS(p) (((p)->sick >> 1) & 7)
#define PS_DOSES(p) (((p)->sick >> 4) & 3)

static DoomoRng prng;
static uint8_t prng_ok, last_fed, sick_minutes, poop_minutes, holy_minutes, call_minutes;

static DoomoPet *P(void) { return &doomo_town.pet; }

static int prand(int n)
{
    if (!prng_ok) {
        rng_seed(&prng, doomo_town.seed ^ 0xD0060ACu ^ ((uint32_t)P()->age_days << 8) ^ P()->care_score);
        prng_ok = 1;
    }
    return n > 1 ? rng_range(&prng, n) : 0;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void holy(int d) { P()->holiness = (uint8_t)clampi(P()->holiness + d, 0, 100); }
static void care(int d) { P()->care_score = (uint16_t)clampi(P()->care_score + d, 0, 65535); }
static void mistake(void) { if (P()->mistakes < 255) P()->mistakes++; }

static int stage_minutes(int stage)
{
    switch (stage) {
    case PET_STAGE_EGG: return PET_EGG_MINUTES;
    case PET_STAGE_BABY: return PET_BABY_MINUTES;
    case PET_STAGE_CHILD: return PET_CHILD_MINUTES;
    case PET_STAGE_TEEN: return PET_TEEN_MINUTES;
    default: return 0;
    }
}

int pet_exists(void) { return P()->name[0] != 0; }
int pet_calling(void) { return (P()->asleep & PA_CALL) != 0; }
int pet_call_is_tantrum(void) { return (P()->asleep & (PA_CALL | PA_FAKE)) == (PA_CALL | PA_FAKE); }
int pet_is_asleep(void) { return (P()->asleep & PA_ASLEEP) != 0; }
int pet_lights_off(void) { return (P()->asleep & PA_DARK) != 0; }
int pet_is_sick(void) { return (P()->sick & PS_SICK) != 0; }
int pet_last_fed(void) { return last_fed; }

int pet_line(void)
{
    const DoomoPet *p = P();
    if (p->stage == PET_STAGE_EGG || p->stage > PET_STAGE_ADULT) return PET_LINE_CUTE;
    return (p->form / 4) & 3;
}

void pet_new(const char *name)
{
    DoomoPet *p = P();
    int i;
    memset(p, 0, sizeof(*p));
    if (!name || !name[0]) name = "Doomo";
    for (i = 0; i < 7 && name[i]; i++) p->name[i] = name[i];
    p->stage = PET_STAGE_EGG;
    p->form = 0;
    p->hunger = 4;
    p->joy = 4;
    p->discipline = 0;
    p->holiness = 50;
    p->weight = 5;
    p->stage_timer = PET_EGG_MINUTES;
    prng_ok = 0;
    last_fed = 0;
    sick_minutes = poop_minutes = holy_minutes = call_minutes = 0;
}

static void call(int fake)
{
    DoomoPet *p = P();
    if (p->asleep & PA_CALL) return;
    p->asleep = (uint8_t)((p->asleep & (PA_ASLEEP | PA_DARK)) | PA_CALL | (fake ? PA_FAKE : 0));
    call_minutes = 0;
}

static void clear_call(void)
{
    P()->asleep &= (uint8_t)(PA_ASLEEP | PA_DARK);
    call_minutes = 0;
}

/* Does the pet have a real reason to call right now? */
static int needs_call(void)
{
    const DoomoPet *p = P();
    if (p->asleep & PA_ASLEEP) return !(p->asleep & PA_DARK);   /* lights! */
    return p->hunger == 0 || p->joy == 0 || p->poop >= 3 || (p->sick & PS_SICK);
}

static int decide_line(void)
{
    const DoomoPet *p = P();
    if (p->holiness >= 75 && p->mistakes <= 2) return PET_LINE_ANGEL;
    if (p->holiness <= 25) return PET_LINE_DEMON;
    if (p->mistakes >= 4) return PET_LINE_FERAL;
    return PET_LINE_CUTE;
}

int pet_line_forecast(void) { return decide_line(); }

int pet_form_seen(int form)
{
    if (form < 0 || form >= PET_FORM_COUNT) return 0;
    return unlock_has(320 + form);
}

static int evolve(void)
{
    DoomoPet *p = P();
    int line, old = p->form;
    if (p->stage >= PET_STAGE_ADULT) return 0;
    line = p->stage == PET_STAGE_EGG ? (p->holiness >= 75 ? PET_LINE_ANGEL :
                                        p->holiness <= 25 ? PET_LINE_DEMON : PET_LINE_CUTE)
                                     : decide_line();
    p->stage++;
    p->form = (uint8_t)PET_FORM(line, p->stage);
    p->mistakes = 0;
    p->care_score = 0;
    p->stage_timer = (uint16_t)stage_minutes(p->stage);
    if (p->stage > PET_STAGE_BABY) progress_add(PROG_PET_EVOLUTIONS, 1);
    unlock_grant_match(UL_CAT_PET, 0, p->form);
    return p->form != old || p->stage == PET_STAGE_BABY;
}

int pet_evolve_now(void)
{
    if (!pet_exists() || P()->stage >= PET_STAGE_ADULT) return 0;
    return evolve();
}

static void pass_away(void)
{
    DoomoPet *p = P();
    p->stage = PET_STAGE_DEAD;
    p->asleep = 0;
    p->sick = 0;
    p->poop = 0;
}

void pet_tick_minute(void)
{
    DoomoPet *p = P();
    int h, asleep_now, bed, wake, decay_h, decay_j;
    if (!pet_exists() || p->stage == PET_STAGE_DEAD) return;
    /* the clock */
    if (doomo_town.minute == 0) {
        if (p->age_days < 255) p->age_days++;
    }
    if (p->stage_timer > 0) p->stage_timer--;
    if (p->stage == PET_STAGE_EGG) {
        if (p->stage_timer == 0) evolve();
        return;
    }
    if (p->stage_timer == 0 && p->stage < PET_STAGE_ADULT) {
        evolve();
        return;
    }
    /* bedtime */
    h = town_hour();
    bed = p->stage <= PET_STAGE_CHILD ? 20 : p->stage == PET_STAGE_TEEN ? 21 : 22;
    wake = 8;
    asleep_now = h >= bed || h < wake;
    if (asleep_now && !(p->asleep & PA_ASLEEP)) {
        p->asleep |= PA_ASLEEP;
        clear_call();
        if (!(p->asleep & PA_DARK)) call(0);           /* "lights, please" */
    } else if (!asleep_now && (p->asleep & PA_ASLEEP)) {
        p->asleep &= (uint8_t)~(PA_ASLEEP | PA_DARK);  /* good morning! */
        clear_call();
    }
    /* hunger and joy drain while awake */
    if (!(p->asleep & PA_ASLEEP)) {
        decay_h = p->stage == PET_STAGE_BABY ? 20 : p->stage == PET_STAGE_CHILD ? 45 :
                  p->stage == PET_STAGE_TEEN ? 60 : 80;
        decay_j = decay_h + 10;
        if (p->sick & PS_SICK) { decay_h /= 2; decay_j /= 2; }
        if (prand(decay_h) == 0 && p->hunger > 0) p->hunger--;
        if (prand(decay_j) == 0 && p->joy > 0) p->joy--;
        /* what goes in must come out */
        if (p->hunger > 0 && ++poop_minutes >= 30 && prand(60) == 0 && p->poop < 4) {
            p->poop++;
            poop_minutes = 0;
        }
        /* a tantrum for attention */
        if (!(p->asleep & PA_CALL) && p->hunger > 0 && p->joy > 0 && prand(240) == 0) call(1);
        /* catching a cold */
        if (!(p->sick & PS_SICK) &&
            (prand(4000) == 0 || (p->poop >= 3 && prand(120) == 0) || (p->weight > 40 && prand(600) == 0))) {
            p->sick = (uint8_t)(PS_SICK | ((1 + prand(2)) << 4));
            sick_minutes = 0;
        }
    }
    /* sickness gets worse untreated */
    if (p->sick & PS_SICK) {
        if (++sick_minutes >= 60) {
            int hrs = PS_HOURS(p) + 1;
            sick_minutes = 0;
            if (hrs >= 6) { pass_away(); return; }
            p->sick = (uint8_t)((p->sick & ~0x0E) | (hrs << 1));
        }
    }
    /* well cared-for pets grow a little holier */
    if (p->hunger >= 2 && p->joy >= 2 && p->poop == 0 && !(p->sick & PS_SICK) && ++holy_minutes >= 120) {
        holy_minutes = 0;
        holy(1);
        care(1);
    }
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter == 3 && prand(180) == 0) holy(-1);
    /* calling for attention */
    if (!(p->asleep & PA_CALL) && needs_call()) call(0);
    if (p->asleep & PA_CALL) {
        if (call_minutes < 255) call_minutes++;
        p->asleep = (uint8_t)((p->asleep & 0x0F) | (DOOMO_MIN(15, call_minutes / 4) << 4));
        if (call_minutes >= 15) {
            /* 15 minutes ignored */
            if (p->asleep & PA_FAKE) {
                p->discipline = (uint8_t)clampi(p->discipline - 10, 0, 100);
                holy(-3);
            } else {
                mistake();
            }
            clear_call();
        }
    }
}

void pet_feed(int snack)
{
    DoomoPet *p = P();
    last_fed = 0;
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage == PET_STAGE_DEAD) return;
    if (p->asleep & PA_ASLEEP) return;
    if (snack) {
        if (p->joy < PET_HEARTS) p->joy++;
        if (p->weight < 99) p->weight = (uint8_t)DOOMO_MIN(99, p->weight + 2);
        holy(-3);                                       /* sugar is devilish */
        last_fed = 1;
    } else {
        if (p->hunger >= PET_HEARTS || (p->sick & PS_SICK)) return;     /* refuses */
        p->hunger++;
        if (p->weight < 99) p->weight++;
        care(1);
        last_fed = 1;
    }
    if (!snack) progress_add(PROG_MEALS_SERVED, 1);
    if ((p->asleep & PA_CALL) && !(p->asleep & PA_FAKE) && !needs_call()) clear_call();
}

void pet_play_result(int won)
{
    DoomoPet *p = P();
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage == PET_STAGE_DEAD) return;
    if (p->weight > 1) p->weight--;
    if (won) {
        if (p->joy < PET_HEARTS) p->joy++;
        holy(1);
        care(1);
    }
    if ((p->asleep & PA_CALL) && !(p->asleep & PA_FAKE) && !needs_call()) clear_call();
}

void pet_clean(void)
{
    DoomoPet *p = P();
    if (!pet_exists() || p->stage == PET_STAGE_DEAD) return;
    if (p->poop > 0) {
        holy(2);
        care(1);
    }
    p->poop = 0;
    if ((p->asleep & PA_CALL) && !(p->asleep & PA_FAKE) && !needs_call()) clear_call();
}

void pet_medicine(void)
{
    DoomoPet *p = P();
    int d;
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage == PET_STAGE_DEAD) return;
    if (!(p->sick & PS_SICK)) {
        if (p->joy > 0) p->joy--;                       /* yuck! */
        return;
    }
    d = PS_DOSES(p);
    if (d > 1) {
        p->sick = (uint8_t)((p->sick & 0x0F) | ((d - 1) << 4));
        return;
    }
    p->sick = 0;
    sick_minutes = 0;
    holy(2);
    care(2);
    if ((p->asleep & PA_CALL) && !(p->asleep & PA_FAKE) && !needs_call()) clear_call();
}

void pet_discipline(void)
{
    DoomoPet *p = P();
    if (!pet_exists() || p->stage == PET_STAGE_EGG || p->stage == PET_STAGE_DEAD) return;
    if (pet_call_is_tantrum()) {
        p->discipline = (uint8_t)clampi(p->discipline + 25, 0, 100);
        holy(4);
        care(2);
        clear_call();
    } else {
        /* scolding for nothing */
        if (p->joy > 0) p->joy--;
        holy(-6);
    }
}

void pet_lights(int off)
{
    DoomoPet *p = P();
    if (!pet_exists()) return;
    if (off) {
        p->asleep |= PA_DARK;
        if ((p->asleep & PA_ASLEEP) && (p->asleep & PA_CALL) && !(p->asleep & PA_FAKE)) {
            clear_call();
            holy(2);
            care(1);
        }
    } else {
        p->asleep &= (uint8_t)~PA_DARK;
    }
}

int pet_combat_buff(void)
{
    static const uint8_t buff[4] = { 10, 15, 20, 25 };
    const DoomoPet *p = P();
    if (!pet_exists() || p->stage != PET_STAGE_ADULT) return 0;
    return buff[pet_line()];
}

const char *pet_buff_text(void)
{
    static const char *const t[4] = {
        "+10% damage (cupcake cheer)", "+15% damage (holy blessing)",
        "+20% damage (feral fury)", "+25% damage (demonic rage)"
    };
    const DoomoPet *p = P();
    if (!pet_exists()) return "No Doomogachi yet";
    if (p->stage == PET_STAGE_DEAD) return "Watching over you";
    if (p->stage != PET_STAGE_ADULT) return "Grows up to help you fight";
    return t[pet_line()];
}

const DoomoImage *pet_image(int frame)
{
    const DoomoPet *p = P();
    if (!pet_exists()) return pet_form_image(-1, frame);
    if (p->stage == PET_STAGE_EGG) return pet_form_image(-1, frame);
    if (p->stage == PET_STAGE_DEAD) return pet_form_image(-2, frame);
    return pet_form_image(p->form, frame);
}
