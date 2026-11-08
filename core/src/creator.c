/* creator.c - SCENE_CREATOR, the citizen creator (owner: creator).
 *
 *   scene arg  >= 0  edit that resident (an empty slot: create into it)
 *              -1    new Hillvile (good side) citizen
 *              -2    new Hellvile (bad side) citizen
 *              -3    the story protagonist (town_res(DOOMO_PLAYER_ID))
 *
 * The player edits a draft DoomoResident through nine tabs of rows (see
 * creator_data.c). Every row edits one byte of the draft, which keeps undo
 * (a 20-snapshot timeline), "randomise this tab", copy-a-look and the
 * tests generic. Locked options (unlocks.h) can be looked at - the preview
 * turns into a silhouette with the unlock hint - but never chosen, except
 * a value the citizen already had when the creator opened (grandfathered).
 * Finishing names the citizen, sanitises the record and writes it into the
 * town (new slot via town_alloc, or the edited slot in place), then plays
 * the "moving in!" animation and pops back to whoever opened the creator.
 * Drawing lives in creator_draw.c. See docs/CREATOR.md.
 */
#include "creator_int.h"
#include "doomo/playas.h"
#include "doomo/part_meta.h"

CrState *cr_sp;                         /* the scene scratch (creator_int.h) */
#if !defined(DOOMO_PLATFORM_GBA) && !defined(DOOMO_PLATFORM_PS1)
uint8_t cr_calm = 0;                    /* CR9: CR_CALM (creator_int.h; always 1 on the GBA) */
#endif
static void set_show(int show);
DOOMO_STATIC_ASSERT(sizeof(CrState) <= DOOMO_SCENE_SCRATCH, creator_state_fits_the_scratch);
static int16_t last_result = -1;

/* ====================================================================== */
/* nicknames (not part of DoomoResident: a small table, saved as the
 * 'NICK' save extension chunk; entries remember a hash of the citizen's
 * name so a reused slot never inherits a stale nickname)                  */
typedef struct CrNick {
    uint16_t id;                      /* 0xFFFF = free                     */
    uint16_t hash;                    /* of the citizen's name             */
    char     nick[DOOMO_NAME_LEN];
} CrNick;
static CrNick nicks[CR_NICKS];
static uint8_t nicks_ready;

static uint16_t name_hash(const char *s)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < DOOMO_NAME_LEN && s[i]; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return (uint16_t)(h ^ (h >> 16));
}

static void nicks_reset(void)
{
    int i;
    memset(nicks, 0, sizeof(nicks));
    for (i = 0; i < CR_NICKS; i++) nicks[i].id = 0xFFFF;
    nicks_ready = 1;
}

static void nicks_init(void)
{
    if (!nicks_ready) nicks_reset();
}

static int nick_valid(const CrNick *n)
{
    const DoomoResident *r;
    if (n->id == 0xFFFF || !n->nick[0]) return 0;
    r = town_res(n->id);
    return r && r->status != RES_EMPTY && name_hash(r->name) == n->hash;
}

const char *creator_nickname(int id)
{
    int i;
    nicks_init();
    for (i = 0; i < CR_NICKS; i++)
        if (nicks[i].id == id && nick_valid(&nicks[i])) return nicks[i].nick;
    return "";
}

/* (integration) Copy a nickname into a DOOMO_NAME_LEN draft buffer.
 * creator_nickname() returns the 1-byte "" literal when there is none,
 * so never read DOOMO_NAME_LEN bytes from it (ASan: global overflow). */
static void nick_copy(char *dst, const char *src)
{
    size_t n = 0;
    while (n < DOOMO_NAME_LEN && src[n]) n++;
    memset(dst, 0, DOOMO_NAME_LEN);
    memcpy(dst, src, n);
}

/* Store (or with "" clear) the nickname of resident `id` named `name`.
 * Returns 0 if the table is full. */
static int nick_set(int id, const char *name, const char *nick)
{
    int i, slot = -1;
    nicks_init();
    for (i = 0; i < CR_NICKS; i++)
        if (nicks[i].id == id) nicks[i].id = 0xFFFF;
    if (!nick || !nick[0]) return 1;
    for (i = 0; i < CR_NICKS && slot < 0; i++)
        if (nicks[i].id == 0xFFFF) slot = i;
    for (i = 0; i < CR_NICKS && slot < 0; i++)
        if (!nick_valid(&nicks[i])) slot = i;          /* reuse a stale one */
    if (slot < 0) return 0;
    memset(&nicks[slot], 0, sizeof(nicks[slot]));
    nicks[slot].id = (uint16_t)id;
    nicks[slot].hash = name_hash(name);
    for (i = 0; i < DOOMO_NAME_LEN - 1 && nick[i]; i++) nicks[slot].nick[i] = nick[i];
    return 1;
}

/* ---- save extension 'NICK': [count][id lo hi][hash lo hi][12 name bytes]...
 * CR3 appends the gene bits the v1 save stream has no place for (look_gene.c: the new look fields
 * such as glasses and outer layers, and the high bit of aura, print and eye count) when any record
 * has some: [u16 n][n x 8 B]. The chunk keeps version 1: a build from before CR3 reads the nicknames
 * and skips the trailer (its reader never needed the length to match). A town with neither
 * nicknames nor such parts writes an empty chunk, as before. ---- */
#define NICK_REC (4 + DOOMO_NAME_LEN)
static int nick_count_used(void)
{
    int i, n = 0;
    nicks_init();
    for (i = 0; i < CR_NICKS; i++) if (nicks[i].id != 0xFFFF && nicks[i].nick[0]) n++;
    return n;
}

static uint32_t nickext_size(void)
{
    int n = nick_count_used();
    uint32_t g = look_gene_ext_size();
    return n || g ? (uint32_t)(1 + n * NICK_REC) + g : 0;
}

static uint32_t nickext_write(uint8_t *dst, uint32_t cap)
{
    uint32_t n = nickext_size(), o = 0;
    int i;
    if (n == 0 || n > cap) return 0;
    dst[o++] = (uint8_t)nick_count_used();
    for (i = 0; i < CR_NICKS; i++) {
        if (nicks[i].id == 0xFFFF || !nicks[i].nick[0]) continue;
        dst[o++] = (uint8_t)(nicks[i].id & 255);
        dst[o++] = (uint8_t)(nicks[i].id >> 8);
        dst[o++] = (uint8_t)(nicks[i].hash & 255);
        dst[o++] = (uint8_t)(nicks[i].hash >> 8);
        memcpy(dst + o, nicks[i].nick, DOOMO_NAME_LEN);
        o += DOOMO_NAME_LEN;
    }
    o += look_gene_ext_write(dst + o, cap - o);           /* CR3 (v2) */
    return o;
}

static int nickext_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    int n, i, k, ok = 1;
    nicks_reset();
    if (len < 1) return 1;
    n = src[0];
    if ((uint32_t)(1 + n * NICK_REC) > len || n > CR_NICKS) return 0;
    (void)version;
    if (!look_gene_ext_read(src + 1 + n * NICK_REC, len - (uint32_t)(1 + n * NICK_REC)))
        ok = 0;                                         /* CR3: the gene bits (the town is decoded) */
    for (i = 0; i < n; i++) {
        const uint8_t *b = src + 1 + i * NICK_REC;
        int id = b[0] | (b[1] << 8);
        if (id > DOOMO_PLAYER_ID) { ok = 0; continue; }
        nicks[i].id = (uint16_t)id;
        nicks[i].hash = (uint16_t)(b[2] | (b[3] << 8));
        for (k = 0; k < DOOMO_NAME_LEN - 1; k++) {
            char c = (char)b[4 + k];
            if (!c) break;
            nicks[i].nick[k] = (c < 0x20 || c > 0x7E) ? '?' : c;
        }
    }
    return ok;
}

static const DoomoSaveExt nick_ext = {
    SAVE_EXT_TAG('N', 'I', 'C', 'K'), 1,
    nickext_size, nickext_write, nickext_read, nicks_reset, 0 /* max_size (S0) */
};
const DoomoSaveExt *const doomo_save_ext_extra = &nick_ext;

/* ====================================================================== */
/* small helpers                                                            */
int creator_last_result(void) { return last_result; }

/* A row's `field` is a field id (resident_acc.h), not a byte offset. */
static int cbyte(const DoomoResident *r, int field) { return res_field_get(r, field); }
static void sbyte(DoomoResident *r, int field, int v) { res_field_set(r, field, v); }

/* RK_ENUM rows on part of a byte (aux = bit mask, creator_int.h) */
static int mask_shift(int mask)
{
    int sh = 0;
    while (mask && !(mask & 1)) { mask >>= 1; sh++; }
    return sh;
}
static int enum_get(const DoomoResident *r, const CrRow *row)
{
    int v = cbyte(r, row->field);
    return row->aux ? (v & row->aux) >> mask_shift(row->aux) : v;
}
static void enum_put(DoomoResident *r, const CrRow *row, int v)
{
    int cur = cbyte(r, row->field);
    if (!row->aux) { sbyte(r, row->field, v); return; }
    sbyte(r, row->field, (cur & ~row->aux) | ((v << mask_shift(row->aux)) & row->aux));
}

/* LS0: the youngest stage the Age row may NOT go older than. The story
 * hero is a grown-up; a premade citizen who still carries their roster
 * name keeps the roster's youth (sim_life_stage would anyway). */
static int age_locked(int stage)
{
    if (cr.mode == CR_PLAYER) return !LIFE_IS_ADULT(stage);
    if (cr.mode == CR_EDIT && cr.target >= 0 && cr.target < DOOMO_MAX_RESIDENTS &&
        doomo_town.population == TOWN_POP_PREMADE && roster_name(cr.target)[0] &&
        strncmp(roster_name(cr.target), cr.draft.name, DOOMO_NAME_LEN) == 0)
        return life_stage_youth(stage) < life_stage_youth(roster_stage(cr.target));
    return 0;
}

/* ---- CR12: the age gate for parts ----------------------------------------------------------- */
/* the catalog list of a part row (part_meta.h), or -1 for a row that is not a part */
int cr_row_part_list(const CrRow *row)
{
    if (!row || row->kind != RK_ENUM || CR_IS_AGE_ROW(row) || row->aux) return -1;
    if (row->field == RES_F_SPECIES) return PL_SPECIES;
    if (row->field < GF_COUNT && RES_F_IS_LOOK(row->field)) return row->field;
    return -1;
}

int cr_value_hidden(const CrRow *row, int index)
{
    int list = cr_row_part_list(row), v;
    if (list < 0) return 0;
    v = row->base + index;
    return v != 0 && !part_age_ok(list, v, RES_STAGE(&cr.draft));
}

int cr_row_slot(const CrRow *row)
{
    int list = cr_row_part_list(row);
    const PartSlotInfo *sl = list >= 0 ? part_slot_of_list(list) : NULL;
    return sl ? (int)(sl - doomo_part_slots) : -1;
}

const PartMeta *cr_choice_meta(const CrRow *row, int index)
{
    int list = cr_row_part_list(row);
    return list >= 0 ? part_meta_find(list, row->base + index) : NULL;
}

int cr_visible_count(const CrRow *row)
{
    int n = cr_row_count(row), i, k = 0;
    for (i = 0; i < n; i++) k += !cr_value_hidden(row, i);
    return k;
}

int cr_visible_pos(const CrRow *row, int index)
{
    int i, k = 0;
    for (i = 0; i < index; i++) k += !cr_value_hidden(row, i);
    return k;
}

/* After the Age row changed (or a randomise / copy): every part the draft's stage may not wear
 * goes back to its slot's default; with `say`, a toast names the first one ("Fishnets are for
 * grown-ups - swapped for None"). Returns how many were swapped. */
static int age_sweep(int say)
{
    int t, i, n = 0;
    char m[80];
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            int v, list = cr_row_part_list(row);
            if (list < 0) continue;
            v = cr_row_value(&cr.draft, row);
            if (!cr_value_hidden(row, v)) continue;
            if (say && !n) {
                char b0[48], b1[48];
                doomo_snprintf(m, sizeof(m), "%s is for %s - swapped for %s", cr_choice_name(row, v, b0),
                               part_age_class(list, row->base + v) == PAGE_ADULT ? "grown-ups" : "teens and up",
                               cr_choice_name(row, 0, b1));
            }
            cr_row_set(&cr.draft, row, 0);
            n++;
        }
    if (say && n) ui_toast(m);
    return n;
}

const CrRow *cr_row_at(int tab, int i)
{
    if (tab < 0 || tab >= CR_TAB_COUNT || i < 0 || i >= cr_tabs[tab].nrows) return NULL;
    return &cr_tabs[tab].rows[i];
}

const CrRow *cr_cur_row(void) { return cr_row_at(cr.tab, cr.row[cr.tab]); }

int cr_first_selectable(int tab)
{
    int i;
    for (i = 0; i < cr_tabs[tab].nrows; i++)
        if (cr_tabs[tab].rows[i].kind != RK_HEADER) return i;
    return 0;
}

/* The row that edits `field` (first match; not flags), or NULL. */
static const CrRow *row_of_field(int field)
{
    int t, i;
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *r = &cr_tabs[t].rows[i];
            if (r->field == field && r->kind != RK_FLAG && r->kind != RK_RFLAG &&
                r->kind != RK_NAME && r->kind != RK_HEADER)
                return r;
        }
    return NULL;
}

static int is_critter_like(int sp)
{
    return sp == SPECIES_BUNNYKIN || sp == SPECIES_HELLHOUND || sp == SPECIES_BATLING ||
           (sp >= SPECIES_CATFOLK && sp <= SPECIES_OWLKIN);
}

int cr_is_hellish(void)
{
    return cr.district == DISTRICT_HELLVILE || (cr.draft.flags & RF_HELLBORN);
}

int cr_dirty(void)
{
    return memcmp(&cr.draft, &cr.orig, sizeof(cr.draft)) != 0 ||
           strcmp(cr.nick, cr.nick_orig) != 0 ||
           (cr.mode == CR_NEW && cr.district != (cr.arg == -2 ? 1 : (cr.target >= 0 ? RES_DISTRICT(cr.target) : 0)));
}

void cr_home_floor_room(int id, int *floor, int *room)
{
    int k = id % DOOMO_DISTRICT_SIZE;
    if (id < 0) k = 0;
    if (floor) *floor = k / 10 + 1;
    if (room) *room = k % 10 + 1;
}

static void refresh_home(void)
{
    if (cr.mode == CR_NEW)
        cr.home_id = (int16_t)(cr.target >= 0 ? cr.target : town_alloc(cr.district));
    else
        cr.home_id = cr.target;
}

/* ====================================================================== */
/* actions & presets                                                        */
int cr_action_choices(int action)
{
    switch (action) {
    case CA_SPECIES_PRESET: return CR_PRESETS_PER_SPECIES;
    case CA_OUTFIT_PRESET:  return cr_outfit_preset_count;
    case CA_VOICE_PRESET:   return cr_voice_preset_count;
    default:                return 1;
    }
}

int cr_action_index(int action)
{
    switch (action) {
    case CA_SPECIES_PRESET: return cr.preset_i;
    case CA_OUTFIT_PRESET:  return cr.outfit_i;
    case CA_VOICE_PRESET:   return cr.voice_i;
    default:                return 0;
    }
}

static void set_action_index(int action, int i)
{
    switch (action) {
    case CA_SPECIES_PRESET: cr.preset_i = (int8_t)i; break;
    case CA_OUTFIT_PRESET:  cr.outfit_i = (int8_t)i; break;
    case CA_VOICE_PRESET:   cr.voice_i = (int8_t)i; break;
    default: break;
    }
}

const CrPreset *cr_action_preset(int action, int index)
{
    int sp = RES_SPECIES(&cr.draft) % SPECIES_COUNT;
    switch (action) {
    case CA_SPECIES_PRESET:
        index = DOOMO_CLAMP(index, 0, CR_PRESETS_PER_SPECIES - 1);
        return &cr_species_presets[sp * CR_PRESETS_PER_SPECIES + index];
    case CA_OUTFIT_PRESET:
        return &cr_outfit_presets[DOOMO_CLAMP(index, 0, cr_outfit_preset_count - 1)];
    case CA_VOICE_PRESET:
        return &cr_voice_presets[DOOMO_CLAMP(index, 0, cr_voice_preset_count - 1)];
    default:
        return NULL;
    }
}

static void apply_kv(DoomoResident *r, const CrPreset *p, int skip_locked)
{
    int i;
    for (i = 0; i + 1 < CR_PRESET_KV && p->kv[i] != CR_NOFIELD; i += 2) {
        int f = p->kv[i], v = p->kv[i + 1];
        if (f >= RES_F_NAME) continue;   /* the name is text, not a value */
        if (skip_locked) {
            const CrRow *row = row_of_field(f);
            if (row && row->kind == RK_ENUM && v >= row->base &&
                cr_value_locked(row, v - row->base))
                continue;
            if (row && (row->kind == RK_PAINT || row->kind == RK_SKIN) &&
                cr_value_locked(row, v))
                continue;
        }
        sbyte(r, f, v);
    }
}

static int is_base_field(int f)
{
    int i;
    for (i = 0; cr_species_base[i].field != CR_NOFIELD; i++)
        if (cr_species_base[i].field == f) return 1;
    return 0;
}

/* Change the species of `r`: the fields that make a species (skin, ears,
 * tail, nose ...) are reset and set from the species' first preset; the
 * rest of the look (eyes, hair, outfit) is kept. */
static void species_change(DoomoResident *r, int species)
{
    const CrPreset *p;
    int i;
    species = DOOMO_CLAMP(species, 0, SPECIES_COUNT - 1);
    for (i = 0; cr_species_base[i].field != CR_NOFIELD; i++)
        sbyte(r, cr_species_base[i].field, cr_species_base[i].neutral);
    p = &cr_species_presets[species * CR_PRESETS_PER_SPECIES];
    for (i = 0; i + 1 < CR_PRESET_KV && p->kv[i] != CR_NOFIELD; i += 2) {
        int f = p->kv[i], v = p->kv[i + 1];
        const CrRow *row;
        if (!is_base_field(f)) continue;
        row = row_of_field(f);
        if (row && row->kind == RK_ENUM && v >= row->base && cr_value_locked(row, v - row->base))
            continue;
        if (row && row->kind == RK_SKIN && cr_value_locked(row, v)) continue;
        sbyte(r, f, v);
    }
    RES_SET_SPECIES(r, (uint8_t)species);
}

/* Full species preset: species base + the preset itself. */
static void apply_species_preset(DoomoResident *r, const CrPreset *p)
{
    int sp = p->kv[0] == RES_F_SPECIES ? p->kv[1]
                                                                         : RES_SPECIES(r);
    species_change(r, sp);
    apply_kv(r, p, 0);
}

static int lock_compute(const CrRow *row, int index);
int cr_preset_locked(const CrPreset *p, const char **hint)
{
    int i;
    if (hint) *hint = "";
    if (!p) return 0;
    for (i = 0; i + 1 < CR_PRESET_KV && p->kv[i] != CR_NOFIELD; i += 2) {
        int f = p->kv[i], v = p->kv[i + 1];
        const CrRow *row = row_of_field(f);
        int idx;
        if (!row) continue;
        if (row->kind == RK_ENUM) idx = v - row->base;
        else if (row->kind == RK_PAINT || row->kind == RK_SKIN) idx = v;
        else if (row->kind == RK_PAINT_NONE) idx = v == LOOK_NO_COLOR ? 0 : v + 1;
        else continue;
        if (idx >= 0 && lock_compute(row, idx)) {      /* CR9: not through the memo */
            if (hint) *hint = cr_lock_hint(row, idx);
            return 1;
        }
    }
    return 0;
}

/* ====================================================================== */
/* rows                                                                     */
/* CR3: a part row offers every live choice of its catalog list that this build can draw
 * (part_live_drawable: the PS1 leaves the scripted ones out until CR10); other rows their count */
static int part_row_count(const CrRow *row)
{
    int list = cr_row_part_list(row), n;
    if (list >= 0 && list < PART_GENE_FIELDS && !row->base && (n = part_live_drawable(list)) > 0) return n;
    return row->count;
}

int cr_row_count(const CrRow *row)
{
    if (!row) return 1;
    switch (row->kind) {
    case RK_ENUM:
        return part_row_count(row);
    case RK_SLIDER: case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN:
        return row->count;
    case RK_FLAG: case RK_RFLAG: case RK_DISTRICT:
        return 2;
    case RK_ACTION:
        return cr_action_choices(row->aux);
    default:
        return 1;
    }
}

int cr_row_value(const DoomoResident *r, const CrRow *row)
{
    int v;
    if (!row) return 0;
    switch (row->kind) {
    case RK_ENUM:
        v = enum_get(r, row) - row->base;
        return DOOMO_CLAMP(v, 0, part_row_count(row) - 1);
    case RK_SLIDER: case RK_PAINT: case RK_SKIN:
        v = cbyte(r, row->field);
        return DOOMO_CLAMP(v, 0, row->count - 1);
    case RK_PAINT_NONE:
        v = cbyte(r, row->field);
        return v == LOOK_NO_COLOR ? 0 : DOOMO_CLAMP(v + 1, 1, PAL_PAINT_COUNT);
    case RK_FLAG: case RK_RFLAG:
        return (cbyte(r, row->field) & row->aux) != 0;
    case RK_DISTRICT:
        return cr.district;
    case RK_ACTION:
        return cr_action_index(row->aux);
    default:
        return 0;
    }
}

void cr_row_set(DoomoResident *r, const CrRow *row, int index)
{
    int cur;
    if (!row) return;
    index = DOOMO_CLAMP(index, 0, cr_row_count(row) - 1);
    if (row->kind == RK_DISTRICT) { cr.district = (uint8_t)index; refresh_home(); return; }
    if (row->kind == RK_ACTION) { set_action_index(row->aux, index); return; }
    if (row->field == CR_NOFIELD) return;
    cur = cbyte(r, row->field);
    switch (row->kind) {
    case RK_ENUM:
        if (row->field == RES_F_SPECIES) {
            if (RES_SPECIES(r) != row->base + index) species_change(r, row->base + index);
        } else {
            enum_put(r, row, row->base + index);
        }
        break;
    case RK_SLIDER: case RK_PAINT: case RK_SKIN:
        sbyte(r, row->field, index);
        break;
    case RK_PAINT_NONE:
        sbyte(r, row->field, index == 0 ? LOOK_NO_COLOR : index - 1);
        break;
    case RK_FLAG: case RK_RFLAG:
        sbyte(r, row->field, index ? (cur | row->aux) : (cur & ~row->aux));
        break;
    default:
        break;
    }
}

static int lock_compute(const CrRow *row, int index);

/* CR9: the unlock bits' signature: the memo below is valid while it is unchanged */
static uint32_t unlock_sig(void)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < DOOMO_UNLOCK_BITS / 8; i++) h = (h ^ doomo_town.unlocks[i]) * 16777619u;
    return h;
}

/* CR9: called once a tick, after the update and before the draw: a new unlock (granted by the
 * creator during its update, or anywhere before this tick) clears the memo, so the draw and the
 * next update see it. A lock asked later in the same update as its grant still reads the old
 * answer (nothing in the creator does that today). */
void cr_lock_memo_check(void)
{
    uint32_t sig = unlock_sig();
    if (sig != cr.lmemo_sig) { memset(cr.lmemo, 0, sizeof(cr.lmemo)); cr.lmemo_sig = sig; }
}

int cr_value_locked(const CrRow *row, int index)
{
    uint32_t sig = 0;
    int k, key;
    CrLockMemo *m;
    if (!row) return 0;
    switch (row->kind) {
    case RK_ENUM:
        if (CR_IS_AGE_ROW(row) || row->clist == CR_NOLIST) return lock_compute(row, index);
        break;
    case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN: case RK_ACTION:
        break;
    default:
        return lock_compute(row, index);
    }
    /* a species preset depends on the draft's species too */
    key = row->kind == RK_ACTION && row->aux == CA_SPECIES_PRESET ? index + 8 * RES_SPECIES(&cr.draft) : index;
    (void)sig;
    k = (int)(((uintptr_t)row >> 2) * 7u + (unsigned)key) & (CR_LMEMO - 1);
    m = &cr.lmemo[k];
    if (m->ok && m->row == row && m->idx == key) return m->val;
    m->row = row;
    m->idx = (int16_t)key;
    m->val = (uint8_t)lock_compute(row, index);
    m->ok = 1;
    return m->val;
}

static int lock_compute(const CrRow *row, int index)
{
    int v;
    if (!row) return 0;
    switch (row->kind) {
    case RK_ENUM:
        if (CR_IS_AGE_ROW(row)) return age_locked(row->base + index);
        if (row->clist == CR_NOLIST) return 0;
        v = row->base + index;
        if (unlock_option_ok(row->clist, v)) return 0;
        return cbyte(&cr.orig, row->field) != v;
    case RK_PAINT:
        if (unlock_paint_ok(index)) return 0;
        return cbyte(&cr.orig, row->field) != index;
    case RK_PAINT_NONE:
        if (index == 0 || unlock_paint_ok(index - 1)) return 0;
        return cbyte(&cr.orig, row->field) != index - 1;
    case RK_SKIN:
        if (index < PAL_SKIN_COUNT || unlock_paint_ok(index - 8)) return 0;
        return cbyte(&cr.orig, row->field) != index;
    case RK_ACTION:
        return cr_preset_locked(cr_action_preset(row->aux, index), NULL);
    case RK_DISTRICT:
        return cr.district_fixed && index != cr.district;
    default:
        return 0;
    }
}

const char *cr_lock_hint(const CrRow *row, int index)
{
    const DoomoUnlock *u = NULL;
    const char *h = "";
    if (!row || !cr_value_locked(row, index)) return "";
    if (CR_IS_AGE_ROW(row))
        return cr.mode == CR_PLAYER ? "The hero is a grown-up" : "Canon says: still young!";
    switch (row->kind) {
    case RK_ENUM:
        u = unlock_find(UL_CAT_PART, row->clist, row->base + index);
        break;
    case RK_PAINT:
        u = unlock_find(UL_CAT_PAINT, 0, index);
        break;
    case RK_PAINT_NONE:
        u = unlock_find(UL_CAT_PAINT, 0, index - 1);
        break;
    case RK_SKIN:
        u = unlock_find(UL_CAT_PAINT, 0, index - 8);
        break;
    case RK_ACTION:
        cr_preset_locked(cr_action_preset(row->aux, index), &h);
        return h;
    case RK_DISTRICT:
        return "Already has a home";
    default:
        break;
    }
    return u && u->how ? u->how : "Keep playing to find out!";
}

const char *cr_row_label(const CrRow *row)
{
    if (!row) return "";
    if (row->kind == RK_SKIN) {
        switch (RES_SPECIES(&cr.draft)) {
        case SPECIES_SLIME:    return "Goo";
        case SPECIES_ROBOT:    return "Plating";
        case SPECIES_SKELETON: return "Bones";
        case SPECIES_GHOST:    return "Glow";
        case SPECIES_CACOBLOB: return "Hide";
        case SPECIES_PLUSHIE:  return "Fabric";
        case SPECIES_MUMMY:    return "Wraps";
        case SPECIES_MOTHKIN:  return "Fuzz";
        case SPECIES_ANIMATRONIC: return "Casing";
        default: return is_critter_like(RES_SPECIES(&cr.draft)) ? "Fur" : "Skin";
        }
    }
    if (row->kind == RK_DISTRICT && cr.mode == CR_PLAYER) return "Lives in";
    return row->label;
}

const char *cr_choice_name(const CrRow *row, int index, char *buf)
{
    const CrPreset *p;
    buf[0] = 0;
    if (!row) return buf;
    switch (row->kind) {
    case RK_ENUM:
        /* CR3: a part past the v1 name table (or a row with none) is named by the catalog */
        if (row->names && index >= 0 && index < row->count) return row->names[index];
        {
            const PartMeta *m = cr_choice_meta(row, index);
            if (m) return part_meta_name(m);
        }
        if (row->names) return row->names[DOOMO_CLAMP(index, 0, row->count - 1)];
        doomo_snprintf(buf, 48, "%d", row->base + index);
        return buf;
    case RK_SLIDER:
        doomo_snprintf(buf, 48, "%d", index);
        return buf;
    case RK_PAINT:
        return doomo_paint_names[DOOMO_CLAMP(index, 0, PAL_PAINT_COUNT - 1)];
    case RK_PAINT_NONE:
        if (index == 0) return row->field == GF_FUR_COLOR3 ? "Auto" : "None";   /* CR3: auto = marking colour */
        return doomo_paint_names[DOOMO_CLAMP(index - 1, 0, PAL_PAINT_COUNT - 1)];
    case RK_SKIN:
        return index < PAL_SKIN_COUNT ? doomo_skin_names[index]
                                      : doomo_paint_names[DOOMO_CLAMP(index - 8, 0, PAL_PAINT_COUNT - 1)];
    case RK_FLAG:
        return index ? "On" : "Off";
    case RK_RFLAG:
        return row->names ? row->names[index & 1] : (index ? "Yes" : "No");
    case RK_NAME:
        return cr.draft.name[0] ? cr.draft.name : "(tap A!)";
    case RK_NICK:
        return cr.nick[0] ? cr.nick : "(none)";
    case RK_DISTRICT:
        if (cr.mode == CR_PLAYER) return "HaHaland";
        return cr_district_names[index & 1];
    case RK_ACTION:
        switch (row->aux) {
        case CA_TEST_VOICE: return STR_ICON_NOTE " Play";
        case CA_MATCH_FUR:  return "Apply";
        default:
            p = cr_action_preset(row->aux, index);
            return p ? p->name : "";
        }
    default:
        return buf;
    }
}

/* ====================================================================== */
/* undo timeline                                                            */
#define HIST(i) cr.hist[(cr.hbase + (i)) % CR_UNDO]

static void hist_reset(void)
{
    cr.hbase = 0;
    cr.hcount = 1;
    cr.hcur = 0;
    cr.hist[0] = cr.draft;
    cr.last_key = -1;
}

int cr_undo_count(void) { return cr.hcur; }
int cr_redo_count(void) { return cr.hcount - 1 - cr.hcur; }

static void on_changed(void)
{
    if (!CR_SLICE) cr.hop = 9;          /* CR9: on the GBA / PS1 the hop comes when the new look is up */
    cr.happy = 40;
    cr.changed_t = cr.t;
}

/* Record the draft as a new step. Consecutive changes to the same row
 * within a second (holding a direction) merge into one undo step. */
static void commit(int key)
{
    if (memcmp(&HIST(cr.hcur), &cr.draft, sizeof(cr.draft)) == 0) return;
    if (key >= 0 && key == cr.last_key && cr.t - cr.last_commit_t < 30 && cr.hcur > 0) {
        HIST(cr.hcur) = cr.draft;
    } else {
        if (cr.hcur + 1 < CR_UNDO) {
            cr.hcur++;
        } else {
            cr.hbase = (uint8_t)((cr.hbase + 1) % CR_UNDO);   /* forget the oldest */
        }
        cr.hcount = (uint8_t)(cr.hcur + 1);
        HIST(cr.hcur) = cr.draft;
    }
    cr.last_key = (int16_t)key;
    cr.last_commit_t = cr.t;
    on_changed();
}

static int do_undo(void)
{
    if (cr.hcur == 0) { audio_sfx(SFX_ERROR); return 0; }
    cr.hcur--;
    cr.draft = HIST(cr.hcur);
    cr.last_key = -1;
    cr.browse = -1;
    audio_sfx(SFX_BLIP);
    cr_sparkle_burst(4, 0);
    on_changed();
    if (++cr.n_undo >= 30) unlock_grant(CRU_UNDO_LOTS);
    return 1;
}

static int do_redo(void)
{
    if (cr.hcur + 1 >= cr.hcount) { audio_sfx(SFX_ERROR); return 0; }
    cr.hcur++;
    cr.draft = HIST(cr.hcur);
    cr.last_key = -1;
    cr.browse = -1;
    audio_sfx(SFX_BLIP);
    cr_sparkle_burst(4, 0);
    on_changed();
    return 1;
}

/* ====================================================================== */
/* sparkles, voice test                                                     */
void cr_sparkle_burst(int n, int kind)
{
    static const uint8_t cols[6] = {
        PAL_UI_GOLD, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_SKY, 2),
        PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_LAVENDER, 2), PAL_WHITE
    };
    int i, k = 0;
    uint32_t h = doomo_hash32(cr.t * 7919u + (uint32_t)n);
    for (i = 0; i < CR_SPARKS && k < n; i++) {
        CrSpark *s = &cr.spark[i];
        if (s->life) continue;
        h = doomo_hash32(h + (uint32_t)i);
        s->x = (int16_t)(((int)(h & 63) - 32) * 8);
        s->y = (int16_t)(-(int)((h >> 6) & 63) * 12 - 80);
        s->vx = (int8_t)((int)((h >> 12) & 15) - 8);
        s->vy = (int8_t)(-6 - (int)((h >> 16) & 7));
        s->life = (uint8_t)(18 + ((h >> 20) & 15));
        s->kind = (uint8_t)(kind >= 0 ? kind : (int)((h >> 24) % 3));
        s->color = cols[(h >> 27) % 6];
        k++;
    }
}

static void sparks_update(void)
{
    int i;
    for (i = 0; i < CR_SPARKS; i++) {
        CrSpark *s = &cr.spark[i];
        if (!s->life) continue;
        s->life--;
        s->x = (int16_t)(s->x + s->vx);
        s->y = (int16_t)(s->y + s->vy);
        if (s->kind == 3) { if (s->vy < 10) s->vy++; }          /* confetti falls */
        else if ((cr.t & 3) == 0 && s->vy < -1) s->vy++;          /* sparkles drift */
    }
}

static void start_talk(void)
{
    const char *s = doomo_catchphrases[RES_CATCHPHRASE(&cr.draft) % CATCHPHRASE_COUNT];
    cr.talk = s;
    cr.talk_pos = 0;
    cr.talk_len = (uint8_t)strlen(s);
    cr.talk_t = 0;
    if (++cr.n_voice >= 10) unlock_grant(CRU_VOICE_TESTS);
}

static void talk_update(void)
{
    int interval;
    if (!cr.talk) return;
    cr.talk_t++;
    interval = 3 - RES_VOICE(&cr.draft, VOICE_SPEED) / 12;           /* 1..3 ticks */
    if (interval < 1) interval = 1;
    if (cr.talk_pos < cr.talk_len) {
        if (cr.talk_t % interval == 0) {
            audio_voice_char(RES_VOICE_VIEW(&cr.draft), cr.talk[cr.talk_pos]);
            cr.talk_pos++;
        }
    } else if (cr.talk_t > (uint16_t)(cr.talk_len * interval + 45)) {
        cr.talk = NULL;
    }
}

/* ====================================================================== */
/* preview                                                                  */
/* CR3: a record's look and its gene-only fields (glasses, outer layer ...) as the preview draws them */
static void view_of(const DoomoResident *r, DoomoLook *out, DoomoDeep *dout)
{
    res_get_look(r, out);
    *dout = doomo_deep_zero;
    look_gene_deep(r, LV_ALL, dout);
}

static void target_look(DoomoLook *out, DoomoDeep *dout, uint8_t *locked)
{
    DoomoResident tmp;
    const CrRow *row = cr_cur_row();
    *locked = 0;
    if (cr.phase == CRP_COPY) {
        const DoomoResident *src = cr_copy_res(cr_copy_id_at(cr.copy_sel));
        view_of(src ? src : &cr.draft, out, dout);
        return;
    }
    view_of(&cr.draft, out, dout);
    if (cr.browse < 0 || !row) return;
    tmp = cr.draft;
    if (row->kind == RK_ACTION) {
        const CrPreset *p = cr_action_preset(row->aux, cr.browse);
        if (!p) return;
        if (row->aux == CA_SPECIES_PRESET) apply_species_preset(&tmp, p);
        else apply_kv(&tmp, p, 0);
    } else if (row->field != CR_NOFIELD && RES_F_IS_LOOK(row->field)) {
        cr_row_set(&tmp, row, cr.browse);
    } else {
        return;
    }
    view_of(&tmp, out, dout);
    *locked = (uint8_t)cr_value_locked(row, cr.browse);
}

/* CR6: what a locked part is compared with to show it alone in lavender: the draft with the row's
 * part taken off (its slot's choice 0) */
void cr_lock_base(DoomoLook *out, DoomoDeep *dout)
{
    DoomoResident tmp = cr.draft;
    const CrRow *row = cr_cur_row();
    if (row && row->kind == RK_ENUM && row->field != CR_NOFIELD && RES_F_IS_LOOK(row->field)) cr_row_set(&tmp, row, 0);
    view_of(&tmp, out, dout);
}

int cr_preview_status(void)
{
    if (cr.spooky) return RES_POSSESSED;
    if (cr.mode == CR_EDIT && (cr.draft.status == RES_POSSESSED || cr.draft.status == RES_GHOST ||
                               cr.draft.status == RES_SOUL_LOST))
        return cr.draft.status;
    return RES_ALIVE;
}

int cr_preview_expr(void)
{
    static const uint8_t cycle[6] = { EXPR_NEUTRAL, 0xFF, EXPR_HAPPY, EXPR_NEUTRAL,
                                      0xFF, EXPR_LOVE };
    int e;
    if (cr_preview_status() == RES_POSSESSED) return EXPR_POSSESSED;
    if (cr.phase == CRP_MOVEIN) return (cr.phase_t / 40) & 1 ? EXPR_LOVE : EXPR_HAPPY;
    if (cr.talk && cr.talk_pos < cr.talk_len) return EXPR_TALK;
    if (cr.happy) return EXPR_HAPPY;
    if (cr.tab == CR_TAB_PERSONA) return cr_persona_expr[persona_type(&cr.draft)];
    e = cycle[cr.expr_i % 6];
    if (e == 0xFF) e = cr_persona_expr[persona_type(&cr.draft)];
    return e;
}

static void preview_update(void)
{
    DoomoLook L;
    DoomoDeep D;
    uint8_t locked;
    target_look(&L, &D, &locked);
    if (memcmp(&L, &cr.shown, sizeof(L)) != 0 || memcmp(&D, &cr.shown_d, sizeof(D)) != 0 ||
        locked != cr.shown_locked) {
        /* Throttle: a new look costs a full figure render (~2 GBA ticks),
         * so while a direction is held we refresh every few ticks only.
         * CR9: on the GBA (CR_CALM) the pane draws a new look in slices
         * while the last one stays up (creator_draw.c pane_view): no throttle. */
        if (CR_SLICE || cr.t - cr.shown_t >= 5 || cr.t - cr.changed_t >= 2 || cr.phase == CRP_COPY) {
            cr.shown = L;
            cr.shown_d = D;
            cr.shown_locked = locked;
            cr.shown_t = cr.t;
            /* what the locked part is compared with, made with the look itself (CR6) */
            if (locked) cr_lock_base(&cr.shown_base, &cr.shown_base_d);
            else { memset(&cr.shown_base, 0, sizeof(cr.shown_base)); cr.shown_base_d = doomo_deep_zero; }
        }
    }
    if (cr.hop) cr.hop--;
    if (cr.happy) cr.happy--;
    if (++cr.expr_t >= 80) { cr.expr_t = 0; cr.expr_i++; }
    if (cr.roll) cr.roll--;
    if (cr.spin && (cr.t % 10) == 0) cr.view8 = (uint8_t)((cr.view8 + 1) & 7);
    sparks_update();
    talk_update();
}

/* ====================================================================== */
/* locks & randomising                                                      */
/* Replace every locked value of the draft (a new random roll, a copied
 * look) by something allowed: the original value, else a safe default. */
static void fix_locks(DoomoResident *r)
{
    int t, i;
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            int v;
            if (row->field == CR_NOFIELD) continue;
            switch (row->kind) {
            case RK_ENUM: case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN:
                v = cr_row_value(r, row);
                if (!cr_value_locked(row, v)) break;
                v = cr_row_value(&cr.orig, row);
                if (cr_value_locked(row, v))
                    v = row->kind == RK_SKIN ? SKIN_PEACH :
                        row->kind == RK_PAINT ? PAINT_WHITE : 0;
                if (row->field == RES_F_SPECIES) {
                    RES_SET_SPECIES(r, (uint8_t)v);
                } else {
                    cr_row_set(r, row, v);
                }
                break;
            default:
                break;
            }
        }
    age_sweep(0);                               /* CR12: nothing above the draft's stage */
}

static uint32_t roll_seed(void)
{
    return doomo_hash32(plat_entropy() ^ (cr.t * 2654435761u) ^ doomo_frame_count() ^
                        ((uint32_t)cr.n_random << 20));
}

/* Copy the identity fields (what the creator edits) from src to dst. */
static void copy_editable(DoomoResident *dst, const DoomoResident *src, int keep_name)
{
    int f;
    res_set_look(dst, RES_LOOK_VIEW(src));
    for (f = GF_PUPIL; f <= GF_HANDEDNESS; f++)            /* CR3: the gene-only look fields too */
        if (RES_F_IS_GENE_LOOK(f)) res_gset(dst, f, res_gget(src, f));
    if (!keep_name) memcpy(dst->name, src->name, DOOMO_NAME_LEN);
    res_set_voice(dst, RES_VOICE_VIEW(src));
    res_set_persona(dst, RES_PERSONA_VIEW(src));
    RES_SET_PRONOUN(dst, RES_PRONOUN(src));
    /* the preference only: the life stage (Age row) is never rolled or
     * copied by accident; write_town() stores the draft's on purpose */
    resident_set_romance(dst, RES_ROMANCE(src));
    RES_SET_BDAY_MONTH(dst, RES_BDAY_MONTH(src));
    RES_SET_BDAY_DAY(dst, RES_BDAY_DAY(src));
    RES_SET_LIKE_FOOD(dst, RES_LIKE_FOOD(src));
    RES_SET_LIKE_HOBBY(dst, RES_LIKE_HOBBY(src));
    RES_SET_LIKE_MUSIC(dst, RES_LIKE_MUSIC(src));
    RES_SET_LIKE_COLOR(dst, RES_LIKE_COLOR(src));
    RES_SET_HATE_FOOD(dst, RES_HATE_FOOD(src));
    RES_SET_FEAR(dst, RES_FEAR(src));
    RES_SET_CATCHPHRASE(dst, RES_CATCHPHRASE(src));
}

static void gen_random(DoomoResident *r)
{
    uint32_t seed = roll_seed();
    if (cr_is_hellish()) resident_generate_hellborn(r, seed);
    else resident_generate(r, seed);
}

static void randomise_all(void)
{
    DoomoResident r;
    gen_random(&r);
    copy_editable(&cr.draft, &r, cr.name_ok);
    fix_locks(&cr.draft);
    cr.n_random++;
    if (cr.n_random >= 25) unlock_grant(CRU_RANDOMISE_LOTS);
    commit(-1);
    cr.browse = -1;
    cr_sparkle_burst(10, -1);
    audio_sfx(SFX_SPARKLE);
}

static void randomise_tab(void)
{
    DoomoResident r;
    int i;
    gen_random(&r);
    for (i = 0; i < cr_tabs[cr.tab].nrows; i++) {
        const CrRow *row = &cr_tabs[cr.tab].rows[i];
        switch (row->kind) {
        case RK_ENUM:
            if (CR_IS_AGE_ROW(row)) break;         /* LS0: age is chosen, never rolled */
            enum_put(&cr.draft, row, enum_get(&r, row));
            break;
        case RK_SLIDER: case RK_PAINT: case RK_PAINT_NONE:
        case RK_SKIN: case RK_FLAG:
            sbyte(&cr.draft, row->field, cbyte(&r, row->field));
            break;
        case RK_NAME:
            if (!cr.name_ok) memcpy(cr.draft.name, r.name, DOOMO_NAME_LEN);
            break;
        case RK_ACTION:
            if (row->aux == CA_OUTFIT_PRESET)
                cr.outfit_i = (int8_t)(RES_LOOK(&r, GF_TOP) % cr_outfit_preset_count);
            break;
        default:
            break;
        }
    }
    if (cr.tab == CR_TAB_BODY)   /* keep the species coherent */
        for (i = 0; cr_species_base[i].field != CR_NOFIELD; i++)
            sbyte(&cr.draft, cr_species_base[i].field, cbyte(&r, cr_species_base[i].field));
    fix_locks(&cr.draft);
    cr.n_random++;
    if (cr.n_random >= 25) unlock_grant(CRU_RANDOMISE_LOTS);
    commit(-1);
    cr.browse = -1;
    cr_sparkle_burst(8, -1);
    audio_sfx(SFX_SPARKLE);
}

/* ====================================================================== */
/* new-citizen templates                                                    */
static void make_template(DoomoResident *r, int hell)
{
    static const uint8_t cute_food[] = { 0, 5, 13, 15, 21, 23, 24 };
    resident_default(r);
    if (hell) {
        apply_species_preset(r, &cr_species_presets[SPECIES_IMP * CR_PRESETS_PER_SPECIES]);
        apply_kv(r, &cr_outfit_presets[13], 1);           /* Hellvile Edge */
        apply_kv(r, &cr_voice_presets[7], 0);             /* Grumbly Imp   */
        RES_SET_PERSONA(r, PERS_SPEECH, 11); RES_SET_PERSONA(r, PERS_ATTITUDE, 5);
        RES_SET_PERSONA(r, PERS_QUIRK, 10);
        RES_SET_LIKE_FOOD(r, 6); RES_SET_HATE_FOOD(r, 7); RES_SET_LIKE_HOBBY(r, 28); RES_SET_LIKE_MUSIC(r, 14);
        RES_SET_LIKE_COLOR(r, PAINT_CHERRY); RES_SET_FEAR(r, 19); RES_SET_CATCHPHRASE(r, 48);
        r->flags = RF_HELLBORN;
        r->stat[STAT_CORRUPTION] = 25;
        RES_SET_BDAY_MONTH(r, 9); RES_SET_BDAY_DAY(r, 13);
    } else {
        apply_species_preset(r, &cr_species_presets[SPECIES_CATFOLK * CR_PRESETS_PER_SPECIES]);
        apply_kv(r, &cr_outfit_presets[0], 1);            /* Comfy Hoodie  */
        /* a Sweet Sunbeam: slowish, polite, expressive, easygoing */
        RES_SET_PERSONA(r, PERS_MOVEMENT, 6); RES_SET_PERSONA(r, PERS_SPEECH, 4);
        RES_SET_PERSONA(r, PERS_EXPRESSION, 11); RES_SET_PERSONA(r, PERS_ATTITUDE, 11);
        RES_SET_PERSONA(r, PERS_QUIRK, 3);
        RES_SET_LIKE_FOOD(r, cute_food[(doomo_frame_count() >> 3) % DOOMO_COUNTOF(cute_food)]);
    }
    memset(r->name, 0, sizeof(r->name));
    r->status = RES_ALIVE;
}

/* ====================================================================== */
/* copy picker                                                              */
static int copy_count(void)
{
    int i, n = 0;
    if (cr.copy_src) return ROSTER_COUNT;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status != RES_EMPTY) n++;
    return n;
}

int cr_copy_id_at(int k)
{
    int id, n;
    if (cr.copy_src) return DOOMO_CLAMP(k, 0, ROSTER_COUNT - 1);
    if (k < 0 || k >= cr.copy_count) return -1;
    /* walk from the memo (lists draw consecutive entries) */
    if (cr.copy_memo_k >= 0 && cr.copy_memo_k <= k) { id = cr.copy_memo_id; n = cr.copy_memo_k; }
    else { id = -1; n = -1; }
    while (n < k) {
        id++;
        if (id >= DOOMO_MAX_RESIDENTS) return -1;
        if (doomo_town.res[id].status != RES_EMPTY) n++;
    }
    cr.copy_memo_k = (int16_t)k;
    cr.copy_memo_id = (int16_t)id;
    return id;
}

const DoomoResident *cr_copy_res(int id)
{
    if (id < 0) return NULL;
    if (cr.copy_src) return roster_premade(id);
    return town_res(id);
}

static void open_copy(int src)
{
    cr.copy_src = (uint8_t)src;
    cr.copy_count = (int16_t)copy_count();
    cr.copy_sel = 0;
    cr.copy_top = 0;
    cr.copy_memo_k = -1;
    cr.copy_memo_id = -1;
}

/* ====================================================================== */
/* leaving & finishing                                                      */
static void go_phase(int phase)
{
    cr.phase = (uint8_t)phase;
    cr.phase_t = 0;
}

static void leave_scene(int result)
{
    cr.result = (int16_t)result;
    last_result = (int16_t)result;
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

static int trimmed_len(const char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && s[n - 1] == ' ') n--;
    return n;
}

static void finish(void);

static void cb_name(int ok)
{
    int n;
    if (ok) {
        n = trimmed_len(cr.namebuf);
        while (*cr.namebuf == ' ' && n > 0) {            /* trim the front too */
            memmove(cr.namebuf, cr.namebuf + 1, DOOMO_NAME_LEN - 1);
            cr.namebuf[DOOMO_NAME_LEN - 1] = 0;
            n--;
        }
        if (n > 0) {
            memset(cr.draft.name, 0, DOOMO_NAME_LEN);
            memcpy(cr.draft.name, cr.namebuf, (size_t)DOOMO_MIN(n, DOOMO_NAME_LEN - 1));
            cr.name_ok = 1;
            commit(-2);
            cr_sparkle_burst(6, 1);
            audio_sfx(SFX_HAPPY);
            if (cr.finishing) { cr.finishing = 0; finish(); }
            return;
        }
    }
    if (cr.finishing) {
        cr.finishing = 0;
        ui_toast("Every citizen needs a name!");
        cr.tab = CR_TAB_PROFILE;
        cr.row[CR_TAB_PROFILE] = 0;
        cr.top[CR_TAB_PROFILE] = 0;
        cr.tab_t = 0;
    }
}

static void open_name_entry(void)
{
    memcpy(cr.namebuf, cr.draft.name, DOOMO_NAME_LEN);
    cr.namebuf[DOOMO_NAME_LEN - 1] = 0;
    if (cr.mode == CR_PLAYER && !cr.name_ok) memset(cr.namebuf, 0, DOOMO_NAME_LEN);
    ui_text_entry(cr.mode == CR_PLAYER ? "What's your name?" : "Name your citizen!",
                  cr.namebuf, DOOMO_NAME_LEN, cb_name);
}

static void cb_nick(int ok)
{
    int n;
    if (!ok) return;
    n = trimmed_len(cr.nickbuf);
    memset(cr.nick, 0, sizeof(cr.nick));
    memcpy(cr.nick, cr.nickbuf, (size_t)DOOMO_MIN(n, DOOMO_NAME_LEN - 1));
    cr.happy = 30;
    cr_sparkle_burst(5, 1);
}

static void open_nick_entry(void)
{
    memcpy(cr.nickbuf, cr.nick, DOOMO_NAME_LEN);
    cr.nickbuf[DOOMO_NAME_LEN - 1] = 0;
    ui_text_entry("Nickname? (empty = none)", cr.nickbuf, DOOMO_NAME_LEN, cb_nick);
}

/* Write the draft into the town. Returns the id written or -1. */
static int write_town(void)
{
    DoomoResident rec = cr.draft;
    DoomoResident *dst;
    int id, first = 0;
    resident_sanitize(&rec);
    part_age_filter_resident(&rec, -1);         /* CR12: never store a part above their stage */
    if (cr.mode == CR_NEW) {
        id = cr.target >= 0 ? cr.target : town_alloc(cr.district);
        if (id < 0 || !town_res(id) || town_res(id)->status != RES_EMPTY) {
            if (cr.target >= 0) id = -1;
            if (id < 0) return -1;
        }
        dst = town_res(id);
        rec.status = RES_ALIVE;
        rec.flags = (uint8_t)((rec.flags & (RF_HELLBORN | RF_FAVORITE)) | RF_CUSTOM);
        rec.exorcism = 0;
        rec.level = 0;
        *dst = rec;
        first = 1;
    } else if (cr.mode == CR_PLAYER) {
        id = DOOMO_PLAYER_ID;
        dst = town_res(id);
        if (dst->status == RES_EMPTY) {
            rec.status = RES_ALIVE;
            rec.flags = (uint8_t)(rec.flags & (RF_HELLBORN | RF_FAVORITE));
            *dst = rec;
            first = 1;
        } else {
            copy_editable(dst, &rec, 0);
            dst->flags = (uint8_t)((dst->flags & ~(RF_HELLBORN | RF_FAVORITE)) |
                                   (rec.flags & (RF_HELLBORN | RF_FAVORITE)));
        }
        dst->flags |= RF_PLAYER | RF_CUSTOM;
        /* LS0: the story hero is always a grown-up */
        if (!LIFE_IS_ADULT(RES_STAGE(&rec))) resident_set_stage(&rec, LIFE_ADULT);
        resident_set_stage(dst, RES_STAGE(&rec));
    } else {
        id = cr.target;
        dst = town_res(id);
        if (!dst) return -1;
        copy_editable(dst, &rec, 0);
        resident_set_stage(dst, RES_STAGE(&rec));
        dst->flags = (uint8_t)((dst->flags & ~(RF_HELLBORN | RF_FAVORITE)) |
                               (rec.flags & (RF_HELLBORN | RF_FAVORITE)) | RF_CUSTOM);
    }
    resident_sanitize(dst);
    /* LS0: a new age may forbid a romance they already had (a dating adult
     * made a kid): undo it quietly before the sim sees it */
    if (cr.mode != CR_NEW) sim_romance_sweep(id);
    portrait_invalidate(id);
    nick_set(id, dst->name, cr.nick);
    if (first) progress_add(PROG_CITIZENS_CREATED, 1);
    if (cr.mode == CR_NEW && RES_DISTRICT(id) == DISTRICT_HELLVILE) unlock_grant(CRU_MOVE_TO_HELLVILE);
    if (cr.mode == CR_EDIT) unlock_grant(CRU_MAKEOVER);
    if (cr.mode == CR_PLAYER) unlock_grant(CRU_SELF_PORTRAIT);
    return id;
}

static void finish(void)
{
    int id;
    if (!cr.name_ok || trimmed_len(cr.draft.name) == 0) {
        cr.finishing = 1;
        open_name_entry();
        return;
    }
    id = write_town();
    if (id < 0) {
        char m[48];
        audio_sfx(SFX_ERROR);
        doomo_snprintf(m, sizeof(m), "No free homes in %s!", cr_district_names[cr.district & 1]);
        ui_toast(m);
        return;
    }
    cr.result = (int16_t)id;
    last_result = (int16_t)id;
    cr.home_id = (int16_t)id;
    doomo_snprintf(cr.msg, sizeof(cr.msg),
                   cr.mode == CR_EDIT ? "%s got a makeover!" :
                   cr.mode == CR_PLAYER ? "Looking good, %s!" :
                   cr_move_in_lines[(cr.t >> 2) & 7], town_res(id)->name);
    cr.browse = -1;
    cr.spooky = 0;
    cr.talk = NULL;
    go_phase(CRP_MOVEIN);
    audio_sfx(SFX_CONFETTI);
    cr_sparkle_burst(CR_SPARKS, 3);
}

static void cb_discard(int yes)
{
    if (yes) { audio_sfx(SFX_CANCEL); leave_scene(-1); }
}

static void ask_quit(void)
{
    if (!cr_dirty()) { audio_sfx(SFX_CANCEL); leave_scene(-1); return; }
    ui_confirm(cr.mode == CR_EDIT ? "Discard your changes?" : "Discard this citizen?",
               cb_discard);
}

static void cb_reset(int yes)
{
    if (!yes) return;
    cr.draft = cr.orig;
    memcpy(cr.nick, cr.nick_orig, sizeof(cr.nick));
    cr.browse = -1;
    commit(-1);
    cr_sparkle_burst(8, 0);
}

/* ====================================================================== */
/* START menu                                                               */
const char *cr_menu_label(int i, char *buf)
{
    switch (i) {
    case CM_DONE:       return cr.mode == CR_NEW ? "Move in!" : "All done!";
    case CM_RANDOM:
        if (cr.tab == CR_TAB_HOME)
            return cr_is_hellish() ? "Randomise (hellish)" : "Randomise all";
        doomo_snprintf(buf, 48, STR_ICON_ARROW_L " %s " STR_ICON_ARROW_R,
                       cr.rand_tab ? "Randomise this tab" : cr_is_hellish() ? "Randomise (hellish)" : "Randomise all");
        return buf;
    case CM_UNDO:  doomo_snprintf(buf, 48, "Undo (%d)", cr_undo_count()); return buf;
    case CM_REDO:  doomo_snprintf(buf, 48, "Redo (%d)", cr_redo_count()); return buf;
    case CM_COPY:       return "Copy a look...";
    case CM_SPOOKY:     return cr.spooky ? "Possessed peek: ON" : "Possessed peek: off";
    case CM_ROWS:
        doomo_snprintf(buf, 48, STR_ICON_ARROW_L " Rows: %s " STR_ICON_ARROW_R,
                       cr.show == CR_SHOW_SIMPLE ? "just the basics" : "every detail");
        return buf;
    case CM_RESET:      return cr.mode == CR_NEW ? "Start over" : "Reset changes";
    case CM_QUIT:       return "Quit";
    default:            return "";
    }
}

int cr_menu_enabled(int i)
{
    switch (i) {
    case CM_UNDO: return cr_undo_count() > 0;
    case CM_REDO: return cr_redo_count() > 0;
    case CM_RESET: return cr_dirty();
    default: return 1;
    }
}

int cr_menu_list(uint8_t *out)
{
    int i, n = 0;
    for (i = 0; i < CM_COUNT; i++)
        if (cr_menu_enabled(i)) out[n++] = (uint8_t)i;
    return n;
}

int cr_menu_index(int cm)
{
    uint8_t l[CM_COUNT];
    int i, n = cr_menu_list(l);
    for (i = 0; i < n; i++) if (l[i] == cm) return i;
    return -1;
}

int cr_menu_group(int cm)
{
    switch (cm) {
    case CM_DONE: return 0;
    case CM_RANDOM: return 1;
    case CM_UNDO: case CM_REDO: return 2;
    case CM_COPY: case CM_SPOOKY: case CM_ROWS: return 3;
    default: return 4;
    }
}

static void menu_run(int i)
{
    switch (i) {
    case CM_DONE:
        go_phase(CRP_MAIN);
        audio_sfx(SFX_SELECT);
        finish();
        break;
    case CM_RANDOM:
        go_phase(CRP_MAIN);
        if (cr.rand_tab && cr.tab != CR_TAB_HOME) randomise_tab();
        else randomise_all();
        break;
    case CM_UNDO: do_undo(); break;
    case CM_REDO: do_redo(); break;
    case CM_COPY:
        open_copy(town_count(-1) > 0 ? 0 : 1);
        go_phase(CRP_COPY);
        audio_sfx(SFX_SELECT);
        break;
    case CM_ROWS:
        set_show(cr.show == CR_SHOW_SIMPLE ? CR_SHOW_ALL : CR_SHOW_SIMPLE);
        audio_sfx(SFX_BLIP);
        go_phase(CRP_MAIN);
        break;
    case CM_SPOOKY:
        cr.spooky ^= 1;
        audio_sfx(cr.spooky ? SFX_POSSESS : SFX_HOLY);
        if (cr.spooky) unlock_grant(CRU_POSSESSED_PEEK);
        go_phase(CRP_MAIN);
        break;
    case CM_RESET:
        go_phase(CRP_MAIN);
        ui_confirm(cr.mode == CR_NEW ? "Start this citizen over?" : "Undo all changes?", cb_reset);
        break;
    case CM_QUIT:
        go_phase(CRP_MAIN);
        ask_quit();
        break;
    default:
        break;
    }
}

static void update_menu(void)
{
    uint8_t l[CM_COUNT];
    int n = cr_menu_list(l), k = 0, i;
    for (i = 0; i < n; i++) if (l[i] <= cr.menu_sel) k = i;     /* an entry that went away: the one before */
    cr.menu_sel = l[k];
    if (btn_repeat(BTN_UP | BTN_DOWN)) {
        k = (k + (btn_down(BTN_UP) ? n - 1 : 1)) % n;
        cr.menu_sel = l[k];
        audio_sfx(SFX_CURSOR);
    }
    if (cr.menu_sel == CM_RANDOM && cr.tab != CR_TAB_HOME && btn_pressed(BTN_LEFT | BTN_RIGHT)) {
        cr.rand_tab ^= 1;
        audio_sfx(SFX_BLIP);
    }
    if (cr.menu_sel == CM_ROWS && btn_pressed(BTN_LEFT | BTN_RIGHT)) {
        set_show(cr.show == CR_SHOW_SIMPLE ? CR_SHOW_ALL : CR_SHOW_SIMPLE);
        audio_sfx(SFX_BLIP);
    }
    if (btn_pressed(BTN_A)) {
        menu_run(cr.menu_sel);
    } else if (btn_pressed(BTN_B | BTN_START)) {
        audio_sfx(SFX_CANCEL);
        go_phase(CRP_MAIN);
    } else if (btn_pressed(BTN_X)) {
        do_undo();
    } else if (btn_pressed(BTN_Y)) {
        do_redo();
    }
}

/* ====================================================================== */
/* main list                                                                */
/* CR6: rows the tab shows in the current mode. A section caption shows while any row of it does. */
int cr_row_shown(int tab, int i)
{
    const CrTab *t = &cr_tabs[tab];
    int k;
    if (i < 0 || i >= t->nrows) return 0;
    if (t->rows[i].kind != RK_HEADER) return cr_row_level(&t->rows[i]) <= cr.show;
    for (k = i + 1; k < t->nrows && t->rows[k].kind != RK_HEADER; k++)
        if (cr_row_level(&t->rows[k]) <= cr.show) return 1;
    return 0;
}

const char *cr_row_section(int tab, int i)
{
    for (; i >= 0; i--)
        if (cr_tabs[tab].rows[i].kind == RK_HEADER) return cr_tabs[tab].rows[i].label;
    return "";
}

/* position of row i among the rows the tab shows */
static int shown_ord(int tab, int i)
{
    int k, n = 0;
    for (k = 0; k < i; k++) n += cr_row_shown(tab, k);
    return n;
}

static int shown_at(int tab, int ord)
{
    int k;
    for (k = 0; k < cr_tabs[tab].nrows; k++)
        if (cr_row_shown(tab, k) && ord-- == 0) return k;
    return 0;
}

static int first_shown(int tab)
{
    int i;
    for (i = 0; i < cr_tabs[tab].nrows; i++)
        if (cr_tabs[tab].rows[i].kind != RK_HEADER && cr_row_shown(tab, i)) return i;
    return cr_first_selectable(tab);
}

static void ensure_visible(void)
{
    int vis = crl.rows > 0 ? crl.rows : 6;
    int r = cr.row[cr.tab], tab = cr.tab;
    int ro = shown_ord(tab, r), to = shown_ord(tab, cr.top[tab]), n = shown_ord(tab, cr_tabs[tab].nrows);
    if (!cr_row_shown(tab, cr.top[tab])) to = DOOMO_MIN(to, ro);
    if (ro - 1 < to && ro > 0 && r > 0 && cr_tabs[tab].rows[r - 1].kind == RK_HEADER && cr_row_shown(tab, r - 1))
        to = ro - 1;
    if (ro < to) to = ro;
    if (ro >= to + vis) to = ro - vis + 1;
    to = DOOMO_CLAMP(to, 0, DOOMO_MAX(0, n - vis));
    cr.top[tab] = (uint8_t)shown_at(tab, to);
}

static void move_row(int d)
{
    int n = cr_tabs[cr.tab].nrows, r = cr.row[cr.tab], k;
    for (k = 0; k < n; k++) {
        r = (r + d + n) % n;
        if (cr_tabs[cr.tab].rows[r].kind != RK_HEADER && cr_row_shown(cr.tab, r)) break;
    }
    cr.row[cr.tab] = (uint8_t)r;
    cr.browse = -1;
    ensure_visible();
    audio_sfx(SFX_CURSOR);
}

/* CR6: sections (a tab's rows between its captions): the first row shown after caption h (h = -1:
 * the rows before the first caption), -1 if none; and the caption a row sits under (-1: none) */
static int section_first(int h)
{
    const CrTab *tab = &cr_tabs[cr.tab];
    int k;
    for (k = h + 1; k < tab->nrows && tab->rows[k].kind != RK_HEADER; k++)
        if (cr_row_shown(cr.tab, k)) return k;
    return -1;
}

static int section_of(int r)
{
    while (r >= 0 && cr_tabs[cr.tab].rows[r].kind != RK_HEADER) r--;
    return r;
}

/* SELECT + UP / DOWN: DOWN goes to the first row of the next section, UP to the first row of this
 * one (or, from there, of the one before); both wrap around the tab */
static void jump_section(int d)
{
    const CrTab *tab = &cr_tabs[cr.tab];
    int n = tab->nrows, h = section_of(cr.row[cr.tab]), target = -1, c = h, k;
    if (d < 0 && section_first(h) != cr.row[cr.tab]) target = section_first(h);
    for (k = 0; target < 0 && k <= n; k++) {
        c += d;                                   /* the captions in order: -1, then each header */
        if (c >= n) c = -1;
        if (c < -1) c = n - 1;
        if (c == h) break;
        if (c >= 0 && tab->rows[c].kind != RK_HEADER) continue;
        target = section_first(c);
    }
    if (target < 0 || target == cr.row[cr.tab]) return;
    cr.row[cr.tab] = (uint8_t)target;
    cr.browse = -1;
    ensure_visible();
    audio_sfx(SFX_CURSOR);
}

/* CR6: switch the rows the tabs show; a row that is gone moves to the first one shown */
static void set_show(int show)
{
    int t;
    cr.show = (uint8_t)DOOMO_CLAMP(show, 0, CR_SHOW_COUNT - 1);
    cr.browse = -1;
    for (t = 0; t < CR_TAB_COUNT; t++) {
        uint8_t keep = cr.tab;
        if (!cr_row_shown(t, cr.row[t])) cr.row[t] = (uint8_t)first_shown(t);
        cr.tab = (uint8_t)t;
        ensure_visible();
        cr.tab = keep;
    }
}

static void switch_tab(int d)
{
    cr.tab = (uint8_t)((cr.tab + d + CR_TAB_COUNT) % CR_TAB_COUNT);
    cr.tab_t = 0;
    cr.browse = -1;
    if (cr_tabs[cr.tab].rows[cr.row[cr.tab]].kind == RK_HEADER || !cr_row_shown(cr.tab, cr.row[cr.tab]))
        cr.row[cr.tab] = (uint8_t)first_shown(cr.tab);
    ensure_visible();
    audio_sfx(SFX_CURSOR);
}

/* CR6: the slot-aware camera. Head parts zoom to the bust, shoes and bottoms to the legs, tails
 * and wings turn the citizen round; a colour or size row follows its part. */
int cr_camera(void)
{
    const CrRow *row = cr_cur_row();
    int f;
    if (cr.phase == CRP_COPY || cr.phase == CRP_HERO || cr.phase == CRP_MOVEIN || !row) return CAM_FIGURE;
    f = row->field;
    if (row->kind == RK_PAINT && (row->aux & CR_AUX_LEGS)) return CAM_LEGS;      /* CR3: legwear colour */
    switch (cr_row_part_list(row) >= 0 ? cr_row_part_list(row) : (int)f) {
    case GF_SHOES: case GF_BOTTOM: case GF_SHOES_COLOR: case GF_BOTTOM_COLOR: case GF_LEGWEAR:
        return CAM_LEGS;
    case GF_TAIL: case GF_WINGS:
        return CAM_BACK;
    case GF_HAT: case GF_HAT_COLOR: case GF_HORNS: case GF_HORN_COLOR: case GF_HORN_SIZE: case GF_HALO:
    case GF_GLASSES: case GF_HOOD_UP:
        return CAM_BUST;
    default:
        break;
    }
    if (cr.tab == CR_TAB_FACE || cr.tab == CR_TAB_HAIR) return CAM_BUST;
    return CAM_FIGURE;
}

static void roll_name(void)
{
    DoomoRng rng;
    const char *first;
    int i, hell = cr_is_hellish();
    rng_seed(&rng, roll_seed());
    for (i = 0; i < 4; i++) {
        first = hell ? doomo_first_names[160 + rng_range(&rng, FIRST_NAME_COUNT - 160)]
                     : doomo_first_names[rng_range(&rng, 160)];
        if (strcmp(first, cr.draft.name) != 0) break;
    }
    memset(cr.draft.name, 0, DOOMO_NAME_LEN);
    for (i = 0; i < DOOMO_NAME_LEN - 1 && first[i]; i++) cr.draft.name[i] = first[i];
    cr.name_ok = 1;
    cr.n_random++;
    commit(-2);
    audio_sfx(SFX_BLIP);
}

static void roll_nick(int d)
{
    static const char *const pats[8] = {
        "%s-chan", "Lil %s", "%s%s", "%sy", "%s-pie", "Sir %s", "%sbun", "Big %s"
    };
    char syl[6], full[DOOMO_NAME_LEN], out[24];
    int i, n, k;
    const char *name = cr.draft.name[0] ? cr.draft.name : "Pal";
    static uint8_t pi;
    pi = (uint8_t)((pi + (d > 0 ? 1 : 7)) & 7);
    n = (int)strlen(name);
    k = DOOMO_MIN(n, 3);
    for (i = 0; i < k; i++) syl[i] = name[i];
    syl[k] = 0;
    /* first syllable: stop after the first vowel that follows a consonant */
    for (i = 1; i < k; i++)
        if (strchr("aeiouyAEIOUY", name[i])) { syl[i + 1 <= k ? i + 1 : k] = 0; break; }
    memcpy(full, name, DOOMO_NAME_LEN);
    full[DOOMO_NAME_LEN - 1] = 0;
    for (k = 0; k < 8; k++) {
        const char *p = pats[(pi + k) & 7];
        if (p[2] == 's' && p[3] == '%') doomo_snprintf(out, sizeof(out), p, syl, syl);
        else if (p[0] == '%' && (p[2] == 'y' || p[2] == 'b')) doomo_snprintf(out, sizeof(out), p, syl);
        else doomo_snprintf(out, sizeof(out), p, full);
        if ((int)strlen(out) <= DOOMO_NAME_LEN - 1) break;
    }
    if ((int)strlen(out) > DOOMO_NAME_LEN - 1) out[DOOMO_NAME_LEN - 1] = 0;
    memset(cr.nick, 0, sizeof(cr.nick));
    memcpy(cr.nick, out, strlen(out));
    cr.happy = 20;
    audio_sfx(SFX_BLIP);
}

static void run_action(const CrRow *row)
{
    const CrPreset *p;
    const char *hint;
    switch (row->aux) {
    case CA_TEST_VOICE:
        start_talk();
        audio_sfx(SFX_BLIP);
        return;
    case CA_MATCH_FUR:
        if (RES_LOOK(&cr.draft, GF_SKIN) < PAL_SKIN_COUNT) {
            static const uint8_t natural[PAL_SKIN_COUNT] = {
                PAINT_PEACH, PAINT_PEACH, PAINT_CARAMEL, PAINT_CARAMEL, PAINT_HONEY,
                PAINT_CHOCOLATE, PAINT_CHOCOLATE, PAINT_CHARCOAL };
            RES_SET_LOOK(&cr.draft, GF_HAIR_COLOR, natural[RES_LOOK(&cr.draft, GF_SKIN)]);
        } else {
            RES_SET_LOOK(&cr.draft, GF_HAIR_COLOR, (uint8_t)(RES_LOOK(&cr.draft, GF_SKIN) - 8));
        }
        commit(-1);
        cr_sparkle_burst(5, 0);
        audio_sfx(SFX_SPARKLE);
        return;
    default:
        break;
    }
    p = cr_action_preset(row->aux, cr_action_index(row->aux));
    if (!p) return;
    if (cr_preset_locked(p, &hint)) {
        (void)hint;                  /* the hint is on screen already */
        ui_toast("That one is still locked!");
        audio_sfx(SFX_ERROR);
        cr.pick_err = 12;
        return;
    }
    if (row->aux == CA_SPECIES_PRESET) apply_species_preset(&cr.draft, p);
    else apply_kv(&cr.draft, p, 0);
    cr.browse = -1;
    commit(-1);
    cr_sparkle_burst(8, -1);
    audio_sfx(SFX_SPARKLE);
    if (row->aux == CA_VOICE_PRESET) start_talk();
}

/* LEFT/RIGHT on a row: step through its choices. Locked ones are shown
 * (silhouette + hint) but never written into the draft. */
static void step_value(const CrRow *row, int d)
{
    int n = cr_row_count(row), cur, v;
    int key = cr.tab * 64 + cr.row[cr.tab];
    switch (row->kind) {
    case RK_HEADER:
        return;
    case RK_NAME:
        roll_name();
        return;
    case RK_NICK:
        roll_nick(d);
        return;
    case RK_FLAG: case RK_RFLAG:
        cr_row_set(&cr.draft, row, !cr_row_value(&cr.draft, row));
        commit(key);
        audio_sfx(SFX_CURSOR);
        cr_sparkle_burst(3, 0);
        return;
    case RK_DISTRICT:
        if (cr.district_fixed) { audio_sfx(SFX_ERROR); cr.pick_err = 10; return; }
        cr_row_set(&cr.draft, row, !cr.district);
        /* a new citizen's heritage follows the side they move to */
        if (cr.mode == CR_NEW) {
            if (cr.district) cr.draft.flags |= RF_HELLBORN;
            else cr.draft.flags &= (uint8_t)~RF_HELLBORN;
            commit(key);
        }
        on_changed();
        audio_sfx(SFX_CURSOR);
        return;
    case RK_ACTION:
        if (n <= 1) return;
        cur = cr.browse >= 0 ? cr.browse : cr_action_index(row->aux);
        v = (cur + d + n) % n;
        set_action_index(row->aux, v);
        cr.browse = (int16_t)v;           /* preview the preset before A */
        audio_sfx(SFX_CURSOR);
        return;
    default:
        break;
    }
    cur = cr.browse >= 0 ? cr.browse : cr_row_value(&cr.draft, row);
    if (row->kind == RK_SLIDER) {
        v = DOOMO_CLAMP(cur + d, 0, n - 1);
        if (v == cur) return;
    } else {
        int guard = n;
        v = (cur + d + n) % n;
        while (cr_value_hidden(row, v) && guard-- > 0) v = (v + d + n) % n;   /* CR12 */
    }
    if (cr_value_locked(row, v)) {
        cr.browse = (int16_t)v;
        audio_sfx(SFX_CURSOR);
        return;
    }
    cr.browse = -1;
    cr_row_set(&cr.draft, row, v);
    if (CR_IS_AGE_ROW(row)) age_sweep(1);       /* CR12: a younger stage sheds grown-up parts */
    commit(key);
    if (row->kind != RK_SLIDER && !CR_SLICE) cr_sparkle_burst(3, -1);   /* CR9: GBA / PS1: when it lands */
    audio_sfx(SFX_CURSOR);
}

static void open_picker(const CrRow *row)
{
    cr.pick_orig = (int16_t)cr_row_value(&cr.draft, row);
    cr.pick_sel = cr.browse >= 0 ? cr.browse : cr.pick_orig;
    cr.pick_top = 0;
    cr.pick_err = 0;
    cr.browse = cr_value_locked(row, cr.pick_sel) ? cr.pick_sel : -1;
    go_phase(row->kind == RK_ENUM ? CRP_PICK : CRP_COLOR);
    audio_sfx(SFX_SELECT);
}

static void press_a(const CrRow *row)
{
    switch (row->kind) {
    case RK_ENUM:
        if (cr_row_slot(row) >= 0) {             /* CR6: a part row opens the picture grid */
            cr_grid_open(row);
            go_phase(CRP_GRID);
            audio_sfx(SFX_SELECT);
            break;
        }
        open_picker(row);
        break;
    case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN:
        open_picker(row);
        break;
    case RK_FLAG: case RK_RFLAG: case RK_DISTRICT:
        step_value(row, 1);
        break;
    case RK_NAME:
        audio_sfx(SFX_SELECT);
        open_name_entry();
        break;
    case RK_NICK:
        audio_sfx(SFX_SELECT);
        open_nick_entry();
        break;
    case RK_ACTION:
        if (cr.browse >= 0) set_action_index(row->aux, cr.browse);
        run_action(row);
        break;
    case RK_SLIDER:
        if (!CR_SLICE) cr.hop = 6;
        audio_sfx(SFX_BLIP);
        break;
    default:
        break;
    }
}

static void update_main(void)
{
    const CrRow *row = cr_cur_row();
    if (btn_pressed(BTN_L)) { switch_tab(-1); return; }
    if (btn_pressed(BTN_R)) { switch_tab(1); return; }
    /* CR6: SELECT + UP / DOWN jumps a section (and SELECT then does not open the turntable) */
    if (btn_down(BTN_SELECT) && btn_repeat(BTN_UP | BTN_DOWN)) {
        jump_section(btn_down(BTN_DOWN) ? 1 : -1);
        cr.sel_used = 1;
        return;
    }
    if (btn_repeat(BTN_UP)) { move_row(-1); return; }
    if (btn_repeat(BTN_DOWN)) { move_row(1); return; }
    if (!row) return;
    if (btn_repeat(BTN_LEFT)) { step_value(row, -1); return; }
    if (btn_repeat(BTN_RIGHT)) { step_value(row, 1); return; }
    if (btn_pressed(BTN_A)) {
        if (cr.browse >= 0 && row->kind != RK_ACTION) {
            audio_sfx(SFX_ERROR);
            cr.pick_err = 12;
            return;
        }
        press_a(row);
        return;
    }
    if (btn_pressed(BTN_B)) {
        if (cr.browse >= 0) { cr.browse = -1; audio_sfx(SFX_CANCEL); return; }
        ask_quit();
        return;
    }
    if (btn_pressed(BTN_START)) {
        cr.browse = -1;
        cr.menu_sel = 0;
        go_phase(CRP_MENU);
        audio_sfx(SFX_SELECT);
        return;
    }
    if (btn_pressed(BTN_X)) { do_undo(); return; }
    if (btn_pressed(BTN_Y)) { do_redo(); return; }
    /* SELECT alone (not as part of SELECT+A / SELECT+B on the GBA) */
    if (btn_down(BTN_SELECT) && (btn_down(BTN_X | BTN_Y))) cr.pick_err = 0;
    if (btn_released(BTN_SELECT) && cr.sel_used) { cr.sel_used = 0; return; }
    if (btn_released(BTN_SELECT) && btn_held_ticks(BTN_X | BTN_Y) == 0) {
        cr.view8 = 0;
        cr.walking = 0;
        cr.spin = 0;
        cr.stick_peek = 0;
        go_phase(CRP_TURN);
        audio_sfx(SFX_SELECT);
        return;
    }
    {
        /* (ST1) the right stick spins the citizen: the turntable opens as a peek and
         * closes again once the stick has been let go for a second */
        int rx, ry;
        if (input_rstick(&rx, &ry) && (rx > 96 || rx < -96)) {
            cr.view8 = 0;
            cr.walking = 0;
            cr.spin = 0;
            cr.stick_peek = 1;
            cr.stick_idle = 0;
            cr.stick_acc = 0;
            go_phase(CRP_TURN);
        }
    }
}

/* ====================================================================== */
/* pickers                                                                  */
/* a picker's cursor is on choice `index`: put it on the draft (try-on), or show a locked one */
void cr_pick_preview(const CrRow *row, int index)
{
    if (cr_value_locked(row, index)) {
        cr.browse = (int16_t)index;
    } else {
        cr.browse = -1;
        cr_row_set(&cr.draft, row, index);
        cr.changed_t = cr.t;
    }
}

static void pick_preview(const CrRow *row) { cr_pick_preview(row, cr.pick_sel); }

void cr_pick_close(const CrRow *row, int ok)
{
    if (ok) {
        cr.browse = -1;
        if (CR_IS_AGE_ROW(row)) age_sweep(1);   /* CR12 */
        commit(cr.tab * 64 + cr.row[cr.tab] + 4096);
        cr_sparkle_burst(6, -1);
        audio_sfx(SFX_SELECT);
    } else {
        cr.browse = -1;
        cr_row_set(&cr.draft, row, cr.pick_orig);
        audio_sfx(SFX_CANCEL);
    }
    go_phase(CRP_MAIN);
}

int cr_pick_rows(void)
{
    return crl.big ? 13 : 7;
}

static void update_pick(void)
{
    const CrRow *row = cr_cur_row();
    int n = cr_row_count(row), vis = cr_pick_rows(), old = cr.pick_sel, k;
    if (cr.pick_err) cr.pick_err--;
    /* CR12: the cursor and the list skip choices above the draft's age (cr_value_hidden) */
    if (btn_repeat(BTN_UP))
        for (k = 0; k < n && (k == 0 || cr_value_hidden(row, cr.pick_sel)); k++)
            cr.pick_sel = (int16_t)((cr.pick_sel + n - 1) % n);
    if (btn_repeat(BTN_DOWN))
        for (k = 0; k < n && (k == 0 || cr_value_hidden(row, cr.pick_sel)); k++)
            cr.pick_sel = (int16_t)((cr.pick_sel + 1) % n);
    if (btn_repeat(BTN_LEFT | BTN_L))
        for (k = 0; k < vis && cr.pick_sel > 0; ) {
            cr.pick_sel--;
            if (!cr_value_hidden(row, cr.pick_sel)) k++;
        }
    if (btn_repeat(BTN_RIGHT | BTN_R))
        for (k = 0; k < vis && cr.pick_sel < n - 1; ) {
            cr.pick_sel++;
            if (!cr_value_hidden(row, cr.pick_sel)) k++;
        }
    while (cr.pick_sel > 0 && cr_value_hidden(row, cr.pick_sel)) cr.pick_sel--;
    if (cr.pick_sel < cr.pick_top) cr.pick_top = cr.pick_sel;
    while (cr_visible_pos(row, cr.pick_sel) - cr_visible_pos(row, cr.pick_top) >= vis) cr.pick_top++;
    cr.pick_top = (int16_t)DOOMO_CLAMP(cr.pick_top, 0, DOOMO_MAX(0, n - 1));
    if (cr.pick_sel != old) {
        pick_preview(row);
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_A)) {
        if (cr_value_locked(row, cr.pick_sel)) { audio_sfx(SFX_ERROR); cr.pick_err = 14; return; }
        cr_pick_close(row, 1);
    } else if (btn_pressed(BTN_B)) {
        cr_pick_close(row, 0);
    }
}

/* swatch grid: columns per row kind */
int cr_color_cols(const CrRow *row)
{
    return row && row->kind == RK_PAINT_NONE ? 11 : 8;
}

static void update_color(void)
{
    const CrRow *row = cr_cur_row();
    int n = cr_row_count(row), cols = cr_color_cols(row), old = cr.pick_sel, p = cr.pick_sel;
    if (cr.pick_err) cr.pick_err--;
    if (btn_repeat(BTN_LEFT)) p = (p + n - 1) % n;
    if (btn_repeat(BTN_RIGHT)) p = (p + 1) % n;
    if (btn_repeat(BTN_UP)) { p -= cols; if (p < 0) { p += ((n + cols - 1) / cols) * cols; while (p >= n) p -= cols; } }
    if (btn_repeat(BTN_DOWN)) { p += cols; if (p >= n) p %= cols; }
    cr.pick_sel = (int16_t)p;
    if (p != old) {
        pick_preview(row);
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_A)) {
        if (cr_value_locked(row, p)) { audio_sfx(SFX_ERROR); cr.pick_err = 14; return; }
        cr_pick_close(row, 1);
    } else if (btn_pressed(BTN_B)) {
        cr_pick_close(row, 0);
    }
}

static void update_copy(void)
{
    int vis = cr_pick_rows(), old = cr.copy_sel, n = cr.copy_count;
    if (btn_pressed(BTN_SELECT) || (btn_pressed(BTN_L | BTN_R) && cr.phase_t > 1)) {
        open_copy(!cr.copy_src);
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); go_phase(CRP_MAIN); return; }
    if (n <= 0) return;
    if (btn_repeat(BTN_UP)) cr.copy_sel = (int16_t)((cr.copy_sel + n - 1) % n);
    if (btn_repeat(BTN_DOWN)) cr.copy_sel = (int16_t)((cr.copy_sel + 1) % n);
    if (btn_repeat(BTN_LEFT)) cr.copy_sel = (int16_t)DOOMO_MAX(0, cr.copy_sel - vis);
    if (btn_repeat(BTN_RIGHT)) cr.copy_sel = (int16_t)DOOMO_MIN(n - 1, cr.copy_sel + vis);
    if (cr.copy_sel < cr.copy_top) cr.copy_top = cr.copy_sel;
    if (cr.copy_sel >= cr.copy_top + vis) cr.copy_top = (int16_t)(cr.copy_sel - vis + 1);
    if (cr.copy_sel != old) { audio_sfx(SFX_CURSOR); return; }
    if (btn_pressed(BTN_A)) {
        const DoomoResident *src = cr_copy_res(cr_copy_id_at(cr.copy_sel));
        if (!src) { audio_sfx(SFX_ERROR); return; }
        res_set_look(&cr.draft, RES_LOOK_VIEW(src));
        fix_locks(&cr.draft);
        commit(-1);
        if (cr.copy_src) unlock_grant(CRU_COPY_PREMADE);
        cr_sparkle_burst(10, -1);
        audio_sfx(SFX_SPARKLE);
        go_phase(CRP_MAIN);
    }
}

/* (ST1) smooth stick spin in steps: adds v (-256..256) to *acc and returns how many whole
 * steps of per_step it crossed (signed); let go (v = 0) forgets the partial step */
static int stick_steps(int16_t *acc, int v, int per_step)
{
    int a = *acc + v, n = 0;
    if (!v) { *acc = 0; return 0; }
    while (a >= per_step) { a -= per_step; n++; }
    while (a <= -per_step) { a += per_step; n--; }
    *acc = (int16_t)a;
    return n;
}

static void update_turn(void)
{
    {
        /* (ST1) the right stick turns the citizen smoothly (8 views): a full push is one
         * view every 3 ticks, a small one creeps */
        int rx, ry, n;
        input_rstick(&rx, &ry);
        n = stick_steps(&cr.stick_acc, stick_curve(rx), 768);
        if (n) { cr.view8 = (uint8_t)((cr.view8 + n) & 7); cr.spin = 0; }
        if (rx) cr.stick_idle = 0;
        else if (cr.stick_peek && ++cr.stick_idle >= 30) {
            cr.stick_peek = 0;
            go_phase(CRP_MAIN);
            return;
        }
        if (btn_pressed(~(uint32_t)0)) cr.stick_peek = 0;     /* a button: the turntable stays */
    }
    if (btn_repeat(BTN_LEFT)) { cr.view8 = (uint8_t)((cr.view8 + 7) & 7); cr.spin = 0; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_RIGHT)) { cr.view8 = (uint8_t)((cr.view8 + 1) & 7); cr.spin = 0; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_UP)) { cr.walking ^= 1; audio_sfx(SFX_BLIP); }
    if (btn_pressed(BTN_DOWN)) { cr.spin ^= 1; audio_sfx(SFX_BLIP); }
    if (btn_pressed(BTN_A)) { cr.roll = 16; audio_sfx(SFX_SQUEAK); }
    if (btn_pressed(BTN_B | BTN_START) || btn_released(BTN_SELECT)) {
        audio_sfx(SFX_CANCEL);
        cr.spin = 0;
        go_phase(CRP_MAIN);
    }
}

/* PA2: the three-card first page of a brand-new hero: SUNNY, MARVIN or
 * make your own. The preview shows the highlighted card; A on a preset
 * saves it as the hero (and the hero kind) and moves in, A on "make your
 * own" opens the editor on the blank template. B leaves. */
static void hero_preview(void)
{
    const DoomoResident *p = cr.hero_sel < 2 ? hero_preset(cr.hero_sel == 0 ? HERO_SUNNY : HERO_MARVIN) : NULL;
    cr.draft = p ? *p : cr.orig;
    cr.browse = -1;
    cr.happy = 20;
}

static void update_hero(void)
{
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); leave_scene(-1); return; }
    if (btn_repeat(BTN_UP) || btn_repeat(BTN_LEFT)) {
        cr.hero_sel = (uint8_t)((cr.hero_sel + 2) % 3); audio_sfx(SFX_CURSOR); hero_preview();
    }
    if (btn_repeat(BTN_DOWN) || btn_repeat(BTN_RIGHT)) {
        cr.hero_sel = (uint8_t)((cr.hero_sel + 1) % 3); audio_sfx(SFX_CURSOR); hero_preview();
    }
    if (btn_pressed(BTN_A)) {
        audio_sfx(SFX_SELECT);
        if (cr.hero_sel < 2) {
            playas_set_hero_kind(cr.hero_sel == 0 ? HERO_SUNNY : HERO_MARVIN);
            hero_preview();
            cr.name_ok = 1;
            finish();
        } else {
            playas_set_hero_kind(HERO_CUSTOM);
            hero_preview();
            cr_sparkle_burst(8, -1);
            go_phase(CRP_MAIN);
        }
    }
}

static void update_movein(void)
{
    if ((cr.phase_t % 14) == 0 && cr.phase_t < 80) cr_sparkle_burst(5, 3);
    if (cr.phase_t == 20) audio_sfx(SFX_BELL);
    if ((cr.phase_t > 24 && btn_pressed(BTN_A | BTN_B | BTN_START)) || cr.phase_t >= 150)
        leave_scene(cr.result);
}

/* ====================================================================== */
/* the scene                                                                */
static void cr_enter(int arg)
{
    DoomoResident *r;
    int i;
    cr_sp = (CrState *)scene_scratch(CR_SCRATCH_TAG, (uint32_t)sizeof(CrState));
    memset(&cr, 0, sizeof(cr));
    nicks_init();
    cr_layout();
    cr.arg = (int16_t)arg;
    cr.result = -1;
    cr.browse = -1;
    cr.copy_memo_k = -1;
    cr.target = -1;
    if (arg == -3) {
        cr.mode = CR_PLAYER;
        cr.target = DOOMO_PLAYER_ID;
        cr.district_fixed = 1;
        r = town_res(DOOMO_PLAYER_ID);
        if (r && r->status != RES_EMPTY) {
            cr.draft = *r;
            /* town_new() calls the protagonist "You" until they pick a name */
            cr.name_ok = r->name[0] != 0 && strcmp(r->name, "You") != 0;
        } else {
            make_template(&cr.draft, 0);
            RES_SET_PRONOUN(&cr.draft, PRONOUN_THEY);
        }
        nick_copy(cr.nick, creator_nickname(DOOMO_PLAYER_ID));
        /* PA2: a hero nobody has made or named yet starts on the cards */
        if (!cr.name_ok) cr.phase = CRP_HERO;
    } else if (arg >= 0 && arg < DOOMO_MAX_RESIDENTS) {
        r = town_res(arg);
        cr.target = (int16_t)arg;
        cr.district = (uint8_t)RES_DISTRICT(arg);
        cr.district_fixed = 1;
        if (r->status == RES_EMPTY) {
            cr.mode = CR_NEW;
            make_template(&cr.draft, cr.district);
        } else {
            cr.mode = CR_EDIT;
            cr.draft = *r;
            cr.name_ok = 1;
            nick_copy(cr.nick, creator_nickname(arg));
        }
    } else {
        cr.mode = CR_NEW;
        cr.district = arg == -2 ? DISTRICT_HELLVILE : DISTRICT_HILLVILE;
        /* in the story the wall stays shut until the last chapter */
        cr.district_fixed = doomo_town.mode == TOWN_STORY && doomo_town.story.chapter < 4;
        make_template(&cr.draft, cr.district);
    }
    cr.nick[DOOMO_NAME_LEN - 1] = 0;
    resident_sanitize(&cr.draft);
    if (!cr.name_ok && cr.mode != CR_PLAYER)
        memset(cr.draft.name, 0, DOOMO_NAME_LEN);                /* sanitize said "Nameless" */
    cr.orig = cr.draft;
    memcpy(cr.nick_orig, cr.nick, DOOMO_NAME_LEN);
    hist_reset();
    cr.show = CR_SHOW_ALL;
    memset(cr.lmemo, 0, sizeof(cr.lmemo));      /* CR9: cr.orig is new */
    for (i = 0; i < CR_TAB_COUNT; i++) cr.row[i] = (uint8_t)cr_first_selectable(i);
    cr.preset_i = 0;
    refresh_home();
    if (cr.phase == CRP_HERO) hero_preview();
    view_of(&cr.draft, &cr.shown, &cr.shown_d);
    cr.img.uid = 0;
    cr.disp.ok = cr.pend.ok = 0;
    if (CR_CALM) {
        portrait_hold(1);            /* CR9 (GBA): the figure holds one pose; a new face is drawn in slices */
        portrait_hold_patient(1);    /* ... at the pace the frame allows (SP2's 4-tick deadline is not for here) */
    }
    audio_music(SONG_CREATOR);
    cr_sparkle_burst(8, -1);
}

static void cr_leave(void)
{
    if (scene_scratch_mine(CR_SCRATCH_TAG)) cr.talk = NULL;
    portrait_hold(0);                /* CR9: (a no-op when the creator held nothing) */
    portrait_hold_patient(0);
}

static void cr_update(void)
{
    if (!scene_scratch_mine(CR_SCRATCH_TAG)) {
        /* cannot happen: the creator only opens modals over itself */
        plat_log("creator: scene scratch lost, closing");
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        return;
    }
    cr.t++;
    cr.tab_t++;
    cr.phase_t++;
    cr_layout();
    if (cr.pick_err && cr.phase == CRP_MAIN) cr.pick_err--;
    switch (cr.phase) {
    case CRP_MAIN:   update_main(); break;
    case CRP_PICK:   update_pick(); break;
    case CRP_COLOR:  update_color(); break;
    case CRP_MENU:   update_menu(); break;
    case CRP_COPY:   update_copy(); break;
    case CRP_TURN:   update_turn(); break;
    case CRP_MOVEIN: update_movein(); break;
    case CRP_HERO:   update_hero(); break;
    case CRP_GRID:   cr_grid_update(); break;
    default: break;
    }
    cr_lock_memo_check();                       /* CR9: after this tick's grants, before its drawing */
    {                                           /* CR9: ticks the cursor has rested on its row */
        uint16_t at = (uint16_t)(cr.tab << 8 | cr.row[cr.tab]);
        if (at != cr.row_at) { cr.row_at = at; cr.row_t = 0; }
        else if (cr.row_t < 0xFFFF) cr.row_t++;
    }
    if (scene_top() == &SCENE_CREATOR) preview_update();
}

const DoomoScene SCENE_CREATOR = { "CREATOR", cr_enter, cr_leave, cr_update, cr_draw, 0 };
