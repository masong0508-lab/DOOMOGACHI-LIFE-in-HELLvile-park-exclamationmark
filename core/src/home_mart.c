/* home_mart.c - SCENE_HOME_MART: HomeMart, the Black Market, the Workshop
 * furniture bench, Town Hall's "Expand" and the neighbours' doors (owner:
 * H4; HF sec.8.4, 3.1, 5.1; SPEC H4).
 *
 * arg 0  HomeMart: 12 seeded pieces a day with a 30% deal, locked rows as
 *        "???", the season's pieces, Last Season at 3x, the Wants tab (what
 *        the last four visitors wished for) and Sell (40%)
 * arg 1  the Hellvile Black Market: the FF_HELL / goth stock, open at night,
 *        and by day once Chapter 4 is done (or in free play)
 * arg 2  the Workshop furniture bench (pushed from SCENE_WORKSHOP): 24
 *        recipes of scrap, gears, hellsteel, stuffing, glitter and horns;
 *        the weld grade (a timing bar) is S / A / B / C. Storage only keeps
 *        counts (HOME.owned), so the grade's reward is a bonus copy at S
 *        and one material back at A instead of HF's gold paint B
 * arg 3  Town Hall "Expand": the next cottage room for H$; the house then
 *        plays the expansion rumble (home_draw.c reads home_mart_expanded)
 * arg 4  "Visit a neighbour": the last visitors, the best friends and a
 *        random door (pushes SCENE_HOME 2 + id)
 *
 * Stock and the deal come from story_rng(HOME_CTX_MART) (daily, C18). The
 * scene keeps ~600 B of scratch ('HMRT'); pushed from the house it costs
 * the house its undo history (documented in docs/HOUSING.md).
 */
#include "home_int.h"
#include "doomo/audio.h"
#include "doomo/tween.h"
#include "doomo/camfx.h"
#include "doomo/items.h"
#include "doomo/story_vm.h"
#include "doomo/font.h"
#include "../gen/furn_ids.h"

#define HM_TAG SAVE_EXT_TAG('H', 'M', 'R', 'T')
#define HM_ROWS 48

enum { HMA_MART, HMA_BLACK, HMA_BENCH, HMA_EXPAND, HMA_DOORS };

/* the timing table (frames at 30 Hz) */
enum { MT_POP, MT_STAMP, MT_WELD_PERIOD, MT_WELD_SHOW, MT_RESULT, MT_COUNT };
static const uint8_t mt[MT_COUNT] = {
    8,      /* MT_POP         a bought row pops (TW_POP_BUBBLE)               */
    14,     /* MT_STAMP       the SOLD / grade stamp (TW_SLAM_STAMP) holds      */
    40,     /* MT_WELD_PERIOD the weld marker's sweep, there and back          */
    45,     /* MT_WELD_SHOW   the grade stays up                               */
    60,     /* MT_RESULT      a caption stays                                  */
};

/* the cottage rooms Town Hall can add, in order, and their prices */
static const uint8_t expand_room[5] = { HOME_ROOM_BEDROOM, HOME_ROOM_KITCHEN, HOME_ROOM_GARDEN,
                                        HOME_ROOM_CELLAR, HOME_ROOM_ANNEX };
static const uint16_t expand_price[5] = { 400, 600, 500, 900, 1200 };
uint8_t home_mart_expanded = 0xFF;           /* .bss: the room the house shows off next */

/* ---- the Workshop furniture bench (HF sec.8.4: TECH 16 + 8 hellsteel) ---------- */
#define R3(f, a, na, b, nb, c, nc) { f, { a, b, c }, { na, nb, nc } }
const HomeBenchRecipe home_bench_recipes[HOME_BENCH_RECIPES] = {
    R3(FURN_TUBE_TV, ITEM_SCRAP, 3, ITEM_GEARS, 2, ITEM_GLITTER, 1),
    R3(FURN_JUKEBOX, ITEM_SCRAP, 4, ITEM_GEARS, 4, ITEM_GLITTER, 3),
    R3(FURN_GAME_CONSOLE, ITEM_SCRAP, 2, ITEM_GEARS, 2, ITEM_GLITTER, 1),
    R3(FURN_WELDING_RIG, ITEM_SCRAP, 3, ITEM_WELD_ROD, 2, ITEM_GEARS, 1),
    R3(FURN_GEAR_CLOCK, ITEM_GEARS, 3, ITEM_SCRAP, 1, ITEM_GLITTER, 0),
    R3(FURN_NEON_ITS_FINE, ITEM_GLITTER, 2, ITEM_SCRAP, 1, ITEM_WELD_ROD, 1),
    R3(FURN_FORKLIFT_PALLET, ITEM_SCRAP, 2, ITEM_WELD_ROD, 1, ITEM_GEARS, 0),
    R3(FURN_WEATHER_VANE, ITEM_SCRAP, 2, ITEM_GEARS, 1, ITEM_WELD_ROD, 1),
    R3(FURN_RECORD_PLAYER, ITEM_GEARS, 2, ITEM_SCRAP, 1, ITEM_GLITTER, 1),
    R3(FURN_VAPOR_DETECTOR_MK2, ITEM_GEARS, 2, ITEM_SCRAP, 2, ITEM_DEMON_HORN, 1),
    R3(FURN_DESK_COMPUTER, ITEM_GEARS, 3, ITEM_SCRAP, 2, ITEM_GLITTER, 2),
    R3(FURN_TOOL_BENCH, ITEM_SCRAP, 3, ITEM_WELD_ROD, 2, ITEM_GEARS, 1),
    R3(FURN_FAIRY_LIGHTS, ITEM_GLITTER, 2, ITEM_SCRAP, 1, ITEM_GEARS, 0),
    R3(FURN_STREETLAMP, ITEM_SCRAP, 3, ITEM_WELD_ROD, 1, ITEM_GLITTER, 1),
    R3(FURN_PAPER_LANTERN, ITEM_GLITTER, 1, ITEM_PLUSH_STUFFING, 1, ITEM_SCRAP, 0),
    R3(FURN_LUMINA_LANTERN, ITEM_SCRAP, 2, ITEM_GLITTER, 2, ITEM_GEARS, 1),
    R3(FURN_HELLSTEEL_SHELVING, ITEM_HELLSTEEL, 2, ITEM_WELD_ROD, 1, ITEM_SCRAP, 1),
    R3(FURN_IRON_BED, ITEM_HELLSTEEL, 3, ITEM_PLUSH_STUFFING, 2, ITEM_WELD_ROD, 1),
    R3(FURN_CHAINED_CHANDELIER, ITEM_HELLSTEEL, 2, ITEM_GEARS, 1, ITEM_GLITTER, 1),
    R3(FURN_WELDED_FENCE, ITEM_HELLSTEEL, 1, ITEM_WELD_ROD, 1, ITEM_SCRAP, 0),
    R3(FURN_BONE_FENCE, ITEM_HELLSTEEL, 1, ITEM_DEMON_HORN, 1, ITEM_SCRAP, 0),
    R3(FURN_CONTRACT_DESK, ITEM_HELLSTEEL, 3, ITEM_DEMON_HORN, 1, ITEM_WELD_ROD, 1),
    R3(FURN_EMBER_BRAZIER, ITEM_HELLSTEEL, 2, ITEM_DEMON_HORN, 1, ITEM_SCRAP, 1),
    R3(FURN_IMP_STOOL, ITEM_HELLSTEEL, 1, ITEM_DEMON_HORN, 1, ITEM_PLUSH_STUFFING, 1),
};
#undef R3

int home_bench_can(int i)
{
    const HomeBenchRecipe *r;
    int k;
    if (i < 0 || i >= HOME_BENCH_RECIPES) return 0;
    r = &home_bench_recipes[i];
    for (k = 0; k < 3; k++)
        if (r->n[k] && inv_count(r->item[k]) < r->n[k]) return 0;
    return home_owned(r->furn) < 15;
}

int home_bench_make(int i, int grade)
{
    const HomeBenchRecipe *r;
    int k;
    if (!home_bench_can(i)) return 0;
    r = &home_bench_recipes[i];
    for (k = 0; k < 3; k++)
        if (r->n[k]) inv_take(r->item[k], r->n[k]);
    if (!home_grant(r->furn)) return 0;
    if (grade >= 3) home_grant(r->furn);                       /* S: a second one */
    else if (grade == 2 && r->n[0]) inv_add(r->item[0], 1);    /* A: a scrap back */
    return 1;
}

/* ---- stock --------------------------------------------------------------------------- */
int home_mart_season(void) { return (int)((doomo_town.day / 28u) % 12u) / 3; }

int home_mart_black_open(void)
{
    if (town_is_night()) return 1;
    return doomo_town.mode != TOWN_STORY || doomo_town.story.chapter >= STORY_CH_DONE;
}

/* the seasonal rows (cat SEASONAL) split into 4 runs in catalog order */
static int season_of(int furn)
{
    static const uint16_t first[4] = { FURN_BLOSSOM_BRANCH, FURN_PADDLING_POOL, FURN_PUMPKIN_LANTERN,
                                       FURN_WINTER_TREE };
    int s;
    for (s = 3; s > 0; s--) if (furn >= first[s]) return s;
    return 0;
}

static int sells(int furn, int black)
{
    const DoomoFurn *f = home_furn(furn);
    if (!f || !f->price || (f->flags2 & (FF_UNSELLABLE | FF_FIXED | FF_GIFT))) return 0;
    if (f->cat == FCAT_SEASONAL || f->cat == FCAT_STORY || f->cat == FCAT_TROPHY) return 0;
    if (black) return (f->flags & FF_HELL) || f->cat == FCAT_GOTHIC;
    return !(f->flags & FF_HELL);
}

static int locked(const DoomoFurn *f)
{
    return f->unlock != 0xFFFF && !unlock_has(f->unlock);
}

int home_mart_rows(int tab, int black, HomeMartRow *out, int cap)
{
    int n = 0, i, total = home_furn_count();
    memset(out, 0, sizeof(*out) * (size_t)cap);
    if (tab == HMT_STOCK) {
        uint16_t cand[FURN_MAX];
        int nc = 0, deal;
        uint32_t x = story_rng(HOME_CTX_MART ^ (uint32_t)black);
        for (i = 0; i < total && nc < FURN_MAX; i++) if (sells(i, black)) cand[nc++] = (uint16_t)i;
        /* a partial Fisher-Yates on the day's seed: the first 12 are the stock */
        for (i = 0; i < nc && n < HOME_MART_STOCK && n < cap; i++) {
            int j;
            uint16_t t;
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            j = i + (int)(x % (uint32_t)(nc - i));
            t = cand[i]; cand[i] = cand[j]; cand[j] = t;
            out[n].furn = cand[i];
            n++;
        }
        /* catalog order reads better than shuffle order */
        for (i = 1; i < n; i++) {
            HomeMartRow r = out[i];
            int j = i - 1;
            while (j >= 0 && out[j].furn > r.furn) { out[j + 1] = out[j]; j--; }
            out[j + 1] = r;
        }
        deal = n ? (int)((x >> 8) % (uint32_t)n) : -1;
        for (i = 0; i < n; i++) {
            const DoomoFurn *f = home_furn(out[i].furn);
            out[i].locked = (uint8_t)locked(f);
            out[i].deal = (uint8_t)(i == deal && !out[i].locked);
            out[i].price = (uint16_t)(out[i].deal ? f->price * 7 / 10 : f->price);
        }
    } else if (tab == HMT_SEASON || tab == HMT_LAST_SEASON) {
        int now = home_mart_season();
        if (black) return 0;
        for (i = 0; i < total && n < cap; i++) {
            const DoomoFurn *f = home_furn(i);
            if (!f || f->cat != FCAT_SEASONAL || !f->price) continue;
            if ((season_of(i) == now) != (tab == HMT_SEASON)) continue;
            out[n].furn = (uint16_t)i;
            out[n].price = (uint16_t)(tab == HMT_SEASON ? f->price : f->price * 3);
            out[n].locked = (uint8_t)locked(f);
            n++;
        }
    } else if (tab == HMT_WANTS) {
        int v;
        for (v = 0; v < 4; v++) {
            int id = home_visit_recent(v), k;
            DoomoResident *r = town_res(id);
            uint8_t w[3];
            if (id < 0 || !r) continue;
            home_visit_wants(id, w);
            for (k = 0; k < 3 && n < cap; k++) {
                int best = -1;
                for (i = 0; i < total && best < 0; i++)
                    if (sells(i, 0) && !locked(home_furn(i)) && home_visit_want_met(w[k], r, i, -1) &&
                        !home_visit_fear_hit(r->fear, i))
                        best = i;
                if (best < 0) continue;
                out[n].furn = (uint16_t)best;
                out[n].price = home_furn(best)->price;
                out[n].owned = (uint8_t)id;              /* who wants it: low byte */
                out[n].tab = (uint8_t)(w[k] | (v << 5)); /* want | visitor slot   */
                n++;
            }
        }
    } else if (tab == HMT_SELL) {
        for (i = 0; i < total && n < cap; i++) {
            const DoomoFurn *f = home_furn(i);
            int avail = home_owned(i) - home_placed(i, NULL, -1);
            if (!f || avail <= 0 || !f->price || (f->flags2 & FF_UNSELLABLE)) continue;
            out[n].furn = (uint16_t)i;
            out[n].price = (uint16_t)(f->price * 2 / 5);
            out[n].owned = (uint8_t)avail;
            n++;
        }
    }
    for (i = 0; i < n; i++) {
        if (tab != HMT_WANTS && tab != HMT_SELL) out[i].owned = (uint8_t)home_owned(out[i].furn);
        if (tab != HMT_WANTS) out[i].tab = (uint8_t)tab;
    }
    return n;
}

int home_mart_buy(const HomeMartRow *row)
{
    if (!row || row->locked || doomo_town.money < row->price) return 0;
    if (!home_grant(row->furn)) return 0;                      /* storage full / cap 1 */
    doomo_town.money -= row->price;
    return 1;
}

int home_mart_sell(int furn)
{
    const DoomoFurn *f = home_furn(furn);
    HomeState *s = home_state();
    int n;
    if (!f || !s || !f->price || (f->flags2 & FF_UNSELLABLE)) return 0;
    if (home_owned(furn) - home_placed(furn, NULL, -1) <= 0) return 0;
    n = home_owned(furn) - 1;
    s->owned[furn >> 1] = (uint8_t)((s->owned[furn >> 1] & (0xF0 >> ((furn & 1) * 4))) | (n << ((furn & 1) * 4)));
    doomo_town.money += (uint32_t)(f->price * 2 / 5);
    s->ver_flags |= HVF_TOUCHED;
    return 1;
}

int home_expand_next(int *price)
{
    int i;
    for (i = 0; i < 5; i++) {
        int room = expand_room[i];
        if (home_room_open(room)) continue;
        /* the story opens its rooms itself (home_story_rooms, HF sec.1.1) */
        if (doomo_town.mode == TOWN_STORY) return -1;
        if (price) *price = expand_price[i];
        return room;
    }
    return -1;
}

int home_expand_buy(void)
{
    int price = 0, room = home_expand_next(&price);
    HomeState *s = home_state();
    if (room < 0 || !s || !home_moved_in() || doomo_town.money < (uint32_t)price) return -1;
    doomo_town.money -= (uint32_t)price;
    home_unlock_room(room);
    s->active_room = (uint8_t)room;
    home_mart_expanded = (uint8_t)room;
    return room;
}

/* ---- the scene ------------------------------------------------------------------------ */
static const char *const tab_names[HMT_COUNT] = { "Today", "Season", "Last Season", "Wants", "Sell" };
static const char *const grade_names[4] = { "C", "B", "A", "S" };

typedef struct MartScn {
    uint8_t  kind, tab, ntabs, closed;
    int16_t  sel, top, n;
    uint16_t t, pop_age, stamp_age, msg_age;
    int8_t   pop_row;
    uint8_t  weld, weld_t, grade, stamp_kind;    /* the bench's timing bar          */
    const char *msg;
    HomeMartRow row[HM_ROWS];
    uint16_t doors[16];
} MartScn;

static MartScn *M;
static uint8_t mart_kind;                    /* .bss: survives the house taking the scratch */

static uint8_t *mart_cache(uint32_t *cap)
{
    uint32_t used = ((uint32_t)sizeof(MartScn) + 255u) & ~255u;
    *cap = DOOMO_SCENE_SCRATCH > used ? DOOMO_SCENE_SCRATCH - used : 0;
    return (uint8_t *)M + used;
}

static void say(const char *m) { M->msg = m; M->msg_age = 0; }

static void rebuild(void)
{
    int i;
    M->n = 0;
    if (M->kind == HMA_MART || M->kind == HMA_BLACK) {
        M->n = (int16_t)home_mart_rows(M->tab, M->kind == HMA_BLACK, M->row, HM_ROWS);
    } else if (M->kind == HMA_BENCH) {
        for (i = 0; i < HOME_BENCH_RECIPES; i++) {
            M->row[i].furn = home_bench_recipes[i].furn;
            M->row[i].locked = (uint8_t)!home_bench_can(i);
            M->row[i].owned = (uint8_t)home_owned(home_bench_recipes[i].furn);
        }
        M->n = HOME_BENCH_RECIPES;
    } else if (M->kind == HMA_EXPAND) {
        for (i = 0; i < 5; i++) {
            M->row[i].furn = expand_room[i];
            M->row[i].price = expand_price[i];
            M->row[i].owned = (uint8_t)home_room_open(expand_room[i]);
            M->row[i].locked = (uint8_t)(!M->row[i].owned && home_expand_next(NULL) != expand_room[i]);
        }
        M->n = 5;
    } else {
        /* the doors: the visitor log, the best friends, a random door */
        int k, best[8], nb = 0, id;
        for (k = 0; k < 4; k++) if ((id = home_visit_recent(k)) >= 0) M->doors[M->n++] = (uint16_t)id;
        for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
            DoomoResident *r = town_res(id);
            int j, dup = 0;
            if (!r || r->status == RES_EMPTY || r->level < 20) continue;
            for (j = 0; j < M->n; j++) dup |= M->doors[j] == id;
            if (dup) continue;
            if (nb < 8) best[nb++] = id;
            else for (j = 0; j < 8; j++) if (town_res(best[j])->level < r->level) { best[j] = id; break; }
        }
        for (k = 0; k < nb && M->n < 15; k++) M->doors[M->n++] = (uint16_t)best[k];
        M->doors[M->n++] = 0xFFFF;                         /* a random door */
    }
    if (M->sel >= M->n) M->sel = (int16_t)(M->n ? M->n - 1 : 0);
}

static void mart_enter(int arg)
{
    M = (MartScn *)scene_scratch(HM_TAG, (uint32_t)sizeof(MartScn));
    if (!M) return;
    memset(M, 0, sizeof(*M));
    camfx_reset();
    M->kind = (uint8_t)(arg >= HMA_MART && arg <= HMA_DOORS ? arg : HMA_MART);
    mart_kind = M->kind;
    M->ntabs = M->kind == HMA_MART ? HMT_COUNT : M->kind == HMA_BLACK ? 1 : 1;
    M->pop_row = -1;
    M->pop_age = M->stamp_age = M->msg_age = 0xFFFF;
    if (M->kind == HMA_BLACK) {
        M->closed = (uint8_t)!home_mart_black_open();
        if (!M->closed)                                 /* a raw bit: no row, no toast (home_int.h) */
            doomo_town.unlocks[HUL_RAW_BLACK_MARKET >> 3] |= (uint8_t)(1 << (HUL_RAW_BLACK_MARKET & 7));
    }
    if (!home_moved_in() && M->kind != HMA_DOORS) home_move_in();
    rebuild();
    audio_music(M->kind == HMA_BLACK ? SONG_HELLVILE : M->kind == HMA_BENCH ? SONG_WORKSHOP : SONG_SHOP);
}

static void mart_resume_check(void)
{
    if (!M || !scene_scratch_mine(HM_TAG)) mart_enter(mart_kind);   /* the scratch went to the house */
}

static void act(void)
{
    HomeMartRow *r = &M->row[M->sel];
    if (M->n <= 0) return;
    if (M->kind == HMA_DOORS) {
        int id = M->doors[M->sel];
        if (id == 0xFFFF) {
            int k;
            uint32_t x = story_rng(HOME_CTX_VISIT ^ M->t);
            for (k = 0; k < 64; k++) {
                id = (int)((x >> 3) % DOOMO_MAX_RESIDENTS);
                x = x * 1103515245u + 12345u;
                if (town_res(id) && town_res(id)->status != RES_EMPTY) break;
            }
        }
        audio_sfx(SFX_DOOR);
        scene_push(&SCENE_HOME, HOME_ARG_VISIT(id));
        return;
    }
    if (M->kind == HMA_BENCH) {
        if (r->locked) { audio_sfx(SFX_ERROR); say("You need more materials."); return; }
        M->weld = 1;
        M->weld_t = 0;
        audio_sfx(SFX_WELD);
        return;
    }
    if (M->kind == HMA_EXPAND) {
        int room;
        if (r->owned) { say("That room is already yours."); return; }
        if (r->locked) { audio_sfx(SFX_ERROR); say("Expand in order: the next room first."); return; }
        if (doomo_town.money < r->price) { audio_sfx(SFX_ERROR); say("Not enough H$."); return; }
        room = home_expand_buy();
        if (room < 0) { audio_sfx(SFX_ERROR); say("Move in first."); return; }
        audio_sfx(SFX_FANFARE);
        scene_push(&SCENE_HOME, HOME_ARG_OWN(HOME_COTTAGE));
        return;
    }
    if (M->tab == HMT_SELL) {
        if (home_mart_sell(r->furn)) { audio_sfx(SFX_PICKUP); M->stamp_kind = 1; M->stamp_age = 0; rebuild(); }
        return;
    }
    if (r->locked) { audio_sfx(SFX_ERROR); say("Not yet. Keep playing!"); return; }
    if (doomo_town.money < r->price) { audio_sfx(SFX_ERROR); say("Not enough H$."); return; }
    if (!home_mart_buy(r)) { audio_sfx(SFX_ERROR); say("Storage is full."); return; }
    audio_sfx(SFX_PICKUP);
    M->pop_row = (int8_t)M->sel;
    M->pop_age = 0;
    M->stamp_kind = 0;
    M->stamp_age = 0;
    r->owned = (uint8_t)home_owned(r->furn);
}

static void mart_update(void)
{
    mart_resume_check();
    if (!M) { scene_pop(); return; }
    camfx_tick();
    M->t++;
    if (M->pop_age != 0xFFFF && ++M->pop_age > mt[MT_POP]) M->pop_age = 0xFFFF;
    if (M->stamp_age != 0xFFFF && ++M->stamp_age > mt[MT_STAMP]) M->stamp_age = 0xFFFF;
    if (M->msg_age != 0xFFFF && ++M->msg_age > mt[MT_RESULT]) M->msg_age = 0xFFFF;
    if (M->weld == 1) {
        /* the timing bar: A in the middle */
        M->weld_t++;
        if (btn_pressed(BTN_A)) {
            int p = M->weld_t % mt[MT_WELD_PERIOD], h = mt[MT_WELD_PERIOD] / 2;
            int pos = p < h ? p : mt[MT_WELD_PERIOD] - p, d = pos - h / 2;
            if (d < 0) d = -d;
            M->grade = (uint8_t)(d <= 1 ? 3 : d <= 3 ? 2 : d <= 6 ? 1 : 0);
            home_bench_make(M->sel, M->grade);
            M->weld = 2;
            M->weld_t = 0;
            M->stamp_kind = 2;
            M->stamp_age = 0;
            audio_sfx(M->grade == 3 ? SFX_FANFARE : SFX_THUD);
            rebuild();
        } else if (btn_pressed(BTN_B)) {
            M->weld = 0;
        }
        return;
    }
    if (M->weld == 2) {
        if (++M->weld_t > mt[MT_WELD_SHOW] || btn_pressed(BTN_A)) M->weld = 0;
        return;
    }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); scene_pop(); return; }
    if (M->closed) return;
    if (M->ntabs > 1 && (btn_pressed(BTN_L) || btn_pressed(BTN_R))) {
        M->tab = (uint8_t)((M->tab + M->ntabs + (btn_pressed(BTN_R) ? 1 : -1)) % M->ntabs);
        M->sel = M->top = 0;
        audio_sfx(SFX_BLIP);
        rebuild();
        return;
    }
    if (M->n > 0) {
        if (btn_repeat(BTN_UP)) { M->sel = (int16_t)((M->sel + M->n - 1) % M->n); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { M->sel = (int16_t)((M->sel + 1) % M->n); audio_sfx(SFX_CURSOR); }
    }
    if (btn_pressed(BTN_A)) act();
}

static const char *row_name(int i, char *buf, int cap)
{
    const HomeMartRow *r = &M->row[i];
    if (M->kind == HMA_DOORS) {
        DoomoResident *res = M->doors[i] == 0xFFFF ? NULL : town_res(M->doors[i]);
        if (!res) return "Knock on a random door";
        doomo_snprintf(buf, cap, "%s%s", res->name, home_visited(M->doors[i]) ? "" : "  (new)");
        return buf;
    }
    if (M->kind == HMA_EXPAND) return home_room_defs[r->furn].name;
    if (r->locked && M->kind != HMA_BENCH) return "???";
    return home_furn(r->furn) ? doomo_furn_text(home_furn(r->furn)->name, buf, cap) : "?";
}

/* s cut to fit maxw pixels ("Stained-Glass Wi.") */
static const char *fit_text(const char *s, char *out, int cap, int maxw)
{
    int n;
    if (ui_text_width(s) <= maxw) return s;
    doomo_snprintf(out, cap, "%s", s);
    for (n = (int)strlen(out); n > 2 && ui_text_width(out) > maxw; n--) {
        out[n - 2] = '.';
        out[n - 1] = 0;
    }
    return out;
}

static void draw_list(int x, int y, int w, int rows, const UiStyle *st)
{
    int i, rh = 12;
    char buf[48], pb[16], fit[48];
    if (M->sel < M->top) M->top = M->sel;
    if (M->sel >= M->top + rows) M->top = (int16_t)(M->sel - rows + 1);
    for (i = M->top; i < M->n && i < M->top + rows; i++) {
        const HomeMartRow *r = &M->row[i];
        int ry = y + (i - M->top) * rh, sel = i == M->sel, dx = 0;
        uint8_t col = r->locked ? st->text_dim : st->text;
        if (i == M->pop_row && M->pop_age != 0xFFFF) dx = (tw_tab(&TW_POP_BUBBLE, M->pop_age) - 256) / 32;
        if (sel) ui_round_rect(x - 2, ry - 1, w + 4, rh, 3, st->fill_hi);
        pb[0] = 0;
        if (M->kind == HMA_MART || M->kind == HMA_BLACK || M->kind == HMA_EXPAND) {
            if (M->kind == HMA_EXPAND && r->owned) doomo_snprintf(pb, sizeof(pb), STR_ICON_HEART);
            else doomo_snprintf(pb, sizeof(pb), "H$%u", (unsigned)r->price);
        } else if (M->kind == HMA_BENCH) {
            doomo_snprintf(pb, sizeof(pb), "x%d", r->owned);
        }
        {
            int pw = ui_text_width(pb), room = w - 6 - pw - (r->deal ? 30 : 0) - dx;
            ui_text(x + 2 + dx, ry + 1, fit_text(row_name(i, buf, sizeof(buf)), fit, sizeof(fit), room), col);
            ui_text(x + w - pw - 2, ry + 1, pb, r->deal ? PAL_UI_RED : PAL_UI_GOLD);
            if (r->deal) {
                int wig = ui_wave((int)M->t, 16, 1);
                ui_round_rect(x + w - pw - 31, ry + wig, 26, 9, 2, PAL_UI_RED);
                plat_text(x + w - pw - 29, ry + 1 + wig, "-30%", PAL_WHITE);
            }
        }
    }
    ui_scroll_arrows(x + w - 4, y - 6, y + rows * rh, M->top > 0, M->top + rows < M->n);
}

static void draw_preview(int x, int y, int w, int h, const UiStyle *st)
{
    const HomeMartRow *r = &M->row[M->sel];
    char buf[64];
    if (M->n <= 0) {
        ui_text_wrap(x + 4, y + 4, w - 8, M->kind == HMA_MART && M->tab == HMT_WANTS ?
                     "Nobody has visited you yet. Invite someone over!" :
                     M->kind == HMA_MART && M->tab == HMT_SELL ? "Nothing in storage to sell." : "Nothing here today.",
                     st->text_dim, 1);
        return;
    }
    if (M->kind == HMA_DOORS || M->kind == HMA_EXPAND) {
        if (M->kind == HMA_DOORS) {
            int id = M->doors[M->sel];
            if (id != 0xFFFF && town_res(id)) ui_portrait(x + w / 2 - 16, y + 6, id, 1, 0);
            ui_text_wrap(x + 4, y + h - 30, w - 8, id == 0xFFFF ? "Somebody's home. Probably." :
                         home_visited(id) ? "You have been here before." : "You have never been inside.", st->text, 1);
        } else {
            const HomeRoomDef *d = &home_room_defs[r->furn];
            doomo_snprintf(buf, sizeof(buf), "%dx%d %s", d->w, d->d, d->kind == ROOMK_GARDEN ? "garden" : "room");
            ui_text(x + 4, y + 6, buf, st->text);
            ui_text_wrap(x + 4, y + 20, w - 8, r->owned ? "Yours already." : r->locked ? "Expand in order." :
                         "The builders knock a wall through. Loudly.", st->text_dim, 1);
        }
        return;
    }
    {
        uint32_t cap;
        uint8_t *cache = mart_cache(&cap);
        DoomoImage img;
        int ox, oy, k, n, fy, dy, ah;
        const DoomoFurn *f = home_furn(r->furn);
        char dbuf[48];
        const char *desc;
        if (!f) return;
        if (r->locked && M->kind != HMA_BENCH) {
            ui_text_big(x + w / 2 - 8, y + h / 3, "?", 3, st->text_dim, st->fill_lo);
            ui_text_wrap(x + 4, y + h - 28, w - 8, "Unlock it somewhere in town.", st->text_dim, 1);
            return;
        }
        /* the footer: the bench's materials (a line each), who wants it, or the count */
        if (M->kind == HMA_BENCH) {
            const HomeBenchRecipe *b = &home_bench_recipes[M->sel];
            for (k = n = 0; k < 3; k++) n += b->n[k] != 0;
            fy = y + h - 2 - n * FONT_LINE;
            for (k = n = 0; k < 3; k++) {
                if (!b->n[k]) continue;
                doomo_snprintf(buf, sizeof(buf), "%s %d/%d", doomo_items[b->item[k]].name, inv_count(b->item[k]), b->n[k]);
                ui_text(x + 4, fy + n++ * FONT_LINE, buf, inv_count(b->item[k]) >= b->n[k] ? PAL_UI_MINT_D : PAL_UI_RED);
            }
        } else if (M->tab == HMT_WANTS) {
            DoomoResident *who = town_res(home_visit_recent(r->tab >> 5));
            doomo_snprintf(buf, sizeof(buf), "%s wants %s", who ? who->name : "?", home_visit_want_text(r->tab & 31));
            fy = y + h - 2 - ui_text_wrap(x + 4, 0, w - 8, buf, 0, 0) * FONT_LINE;
            ui_text_wrap(x + 4, fy, w - 8, buf, PAL_UI_PINK_D, 1);
        } else {
            fy = y + h - 2 - FONT_LINE;
            doomo_snprintf(buf, sizeof(buf), "Owned: %d", home_owned(r->furn));
            ui_text(x + 4, fy, buf, st->text_dim);
        }
        desc = doomo_furn_text(f->desc, dbuf, sizeof(dbuf));
        dy = fy - 3 - ui_text_wrap(x + 4, 0, w - 8, desc, 0, 0) * FONT_LINE;
        ui_text_wrap(x + 4, dy, w - 8, desc, st->text, 1);
        ah = dy - 3 - y;
        if (room_ref_sprite(r->furn, (M->t >> 6) & 1, (M->t >> 3) & 3, -1, -1, 15, cache, cap, &img, &ox, &oy)) {
            /* 2x where the panel has the room for it (the PS1's 320x240) */
            int sc = img.w * 2 + 8 <= w && img.h * 2 + 4 <= ah ? 2 : 1, bob = ui_wave((int)M->t, 39, 1) * sc;
            plat_image(x + (w - img.w * sc) / 2, y + 1 + (ah - img.h * sc) / 2 + bob, &img, sc, IMG_SHADOW, 0);
        } else if (!room_ref_maquette(r->furn, -1, -1, x, y + 1, w, ah)) {   /* no sprites (the PS1) */
            ui_round_rect(x + w / 2 - 12, y + ah / 2 - 12, 24, 24, 4, PAL_PAINT(f->paint_a < 32 ? f->paint_a : PAINT_CARAMEL, 1));
        }
    }
}

static void mart_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), style, lw, rows, y;
    const UiStyle *st;
    char buf[48];
    mart_resume_check();                         /* the frame a pushed house pops: it had the scratch */
    if (!M) { plat_clear(PAL_BLACK); return; }
    style = M->kind == HMA_BLACK ? WIN_HELL : M->kind == HMA_BENCH ? WIN_DARK : WIN_CUTE;
    st = ui_style(style);
    ui_backdrop(style, M->t);
    if (plat_draw_skipped()) return;
    plat_rect(0, 0, sw, 14, st->fill);
    plat_rect(0, 13, sw, 1, st->outline);
    {
        static const char *const titles[5] = { "HomeMart", "Black Market", "Furniture Bench", "Expand the house", "Visit a neighbour" };
        ui_text(4, 3, titles[M->kind], st->accent);
    }
    doomo_snprintf(buf, sizeof(buf), "H$%u", (unsigned)doomo_town.money);
    ui_text(sw - ui_text_width(buf) - 4, 3, buf, PAL_UI_GOLD);
    y = 18;
    if (M->closed) {
        ui_window(sw / 2 - 90, sh / 2 - 24, 180, 48, style);
        ui_text_center(sw / 2, sh / 2 - 14, "CLOSED", st->accent);
        ui_text_center(sw / 2, sh / 2, "Come back after dark.", st->text);
        ui_hint(STR_ICON_B " leave", style);
        return;
    }
    if (M->ntabs > 1) {
        ui_tabs(4, y, tab_names, M->ntabs, M->tab, style);
        y += 16;
    }
    lw = sw >= 300 ? 170 : 128;
    rows = (sh - y - 22) / 12;
    ui_window(2, y, lw + 8, rows * 12 + 6, style);
    draw_list(6, y + 4, lw, rows, st);
    ui_window(lw + 12, y, sw - lw - 14, rows * 12 + 6, style);
    draw_preview(lw + 14, y + 2, sw - lw - 18, rows * 12 + 2, st);
    if (M->weld) {
        /* the weld bar: a sweep there and back, A in the middle */
        int bw = sw - 60, bx = 30, by = sh / 2 - 10, p = M->weld_t % mt[MT_WELD_PERIOD], hh = mt[MT_WELD_PERIOD] / 2;
        int pos = p < hh ? p : mt[MT_WELD_PERIOD] - p;
        ui_window(bx - 8, by - 18, bw + 16, 44, WIN_DARK);
        ui_text_center(sw / 2, by - 12, M->weld == 1 ? "WELD! Press A in the middle" : "Done!", PAL_WHITE);
        plat_rect(bx, by, bw, 8, PAL_RAMP(RAMP_STEEL, 2));
        plat_rect(bx + bw / 2 - bw / 20, by, bw / 10, 8, PAL_RAMP(RAMP_LAVA, 5));
        plat_rect(bx + bw / 2 - 2, by, 4, 8, PAL_RAMP(RAMP_LAVA, 7));
        if (M->weld == 1) plat_rect(bx + pos * bw / hh - 1, by - 3, 3, 14, PAL_WHITE);
    }
    if (M->stamp_age != 0xFFFF) {
        int s = tw_tab(&TW_SLAM_STAMP, M->stamp_age);
        const char *label = M->stamp_kind == 1 ? "SOLD!" : M->stamp_kind == 2 ? grade_names[M->grade & 3] : "MINE!";
        int scale = s >= 400 ? 3 : 2, tw = ui_text_big_width(label, scale);
        ui_text_big(sw / 2 - tw / 2, sh / 2 + 14, label, scale, PAL_UI_RED, PAL_WHITE);
        if (M->stamp_age == 2) camfx_shake(SHK_STAMP);
    }
    if (M->msg_age != 0xFFFF && M->msg) {
        int w = ui_text_width(M->msg) + 10;
        ui_round_rect((sw - w) / 2, sh - 30, w, 13, 4, st->fill_hi);
        ui_text_center(sw / 2, sh - 27, M->msg, st->text);
    }
    if (M->kind == HMA_MART) ui_hint(STR_ICON_A " buy  L/R tab  " STR_ICON_B " leave", style);
    else if (M->kind == HMA_BENCH) ui_hint(STR_ICON_A " weld  " STR_ICON_B " leave", style);
    else if (M->kind == HMA_DOORS) ui_hint(STR_ICON_A " knock  " STR_ICON_B " leave", style);
    else ui_hint(STR_ICON_A " buy  " STR_ICON_B " leave", style);
    camfx_apply();
}

static void mart_leave(void) { M = NULL; }

const DoomoScene SCENE_HOME_MART = { "HOME_MART", mart_enter, mart_leave, mart_update, mart_draw, 0 };
