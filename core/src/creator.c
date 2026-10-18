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

CrState *cr_sp;                         /* the scene scratch (creator_int.h) */
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

/* ---- save extension: [count][id lo hi][hash lo hi][12 name bytes]... ---- */
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
    return n ? (uint32_t)(1 + n * NICK_REC) : 0;
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
    return o;
}

static int nickext_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    int n, i, k, ok = 1;
    (void)version;
    nicks_reset();
    if (len < 1) return 1;
    n = src[0];
    if ((uint32_t)(1 + n * NICK_REC) > len || n > CR_NICKS) return 0;
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

static uint8_t *fbyte(DoomoResident *r, int field) { return (uint8_t *)r + field; }
static uint8_t cbyte(const DoomoResident *r, int field) { return ((const uint8_t *)r)[field]; }

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
    int sp = cr.draft.look.species % SPECIES_COUNT;
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
        if (f >= (int)sizeof(DoomoResident)) continue;
        if (skip_locked) {
            const CrRow *row = row_of_field(f);
            if (row && row->kind == RK_ENUM && v >= row->base &&
                cr_value_locked(row, v - row->base))
                continue;
            if (row && (row->kind == RK_PAINT || row->kind == RK_SKIN) &&
                cr_value_locked(row, v))
                continue;
        }
        *fbyte(r, f) = (uint8_t)v;
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
        *fbyte(r, cr_species_base[i].field) = cr_species_base[i].neutral;
    p = &cr_species_presets[species * CR_PRESETS_PER_SPECIES];
    for (i = 0; i + 1 < CR_PRESET_KV && p->kv[i] != CR_NOFIELD; i += 2) {
        int f = p->kv[i], v = p->kv[i + 1];
        const CrRow *row;
        if (!is_base_field(f)) continue;
        row = row_of_field(f);
        if (row && row->kind == RK_ENUM && v >= row->base && cr_value_locked(row, v - row->base))
            continue;
        if (row && row->kind == RK_SKIN && cr_value_locked(row, v)) continue;
        *fbyte(r, f) = (uint8_t)v;
    }
    r->look.species = (uint8_t)species;
}

/* Full species preset: species base + the preset itself. */
static void apply_species_preset(DoomoResident *r, const CrPreset *p)
{
    int sp = p->kv[0] == (uint8_t)offsetof(DoomoResident, look.species) ? p->kv[1]
                                                                         : r->look.species;
    species_change(r, sp);
    apply_kv(r, p, 0);
}

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
        if (idx >= 0 && cr_value_locked(row, idx)) {
            if (hint) *hint = cr_lock_hint(row, idx);
            return 1;
        }
    }
    return 0;
}

/* ====================================================================== */
/* rows                                                                     */
int cr_row_count(const CrRow *row)
{
    if (!row) return 1;
    switch (row->kind) {
    case RK_ENUM: case RK_SLIDER: case RK_PAINT: case RK_PAINT_NONE: case RK_SKIN:
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
        v = cbyte(r, row->field) - row->base;
        return DOOMO_CLAMP(v, 0, row->count - 1);
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
    uint8_t *p;
    if (!row) return;
    index = DOOMO_CLAMP(index, 0, cr_row_count(row) - 1);
    if (row->kind == RK_DISTRICT) { cr.district = (uint8_t)index; refresh_home(); return; }
    if (row->kind == RK_ACTION) { set_action_index(row->aux, index); return; }
    if (row->field == CR_NOFIELD) return;
    p = fbyte(r, row->field);
    switch (row->kind) {
    case RK_ENUM:
        if (row->field == offsetof(DoomoResident, look.species)) {
            if (r->look.species != row->base + index) species_change(r, row->base + index);
        } else {
            *p = (uint8_t)(row->base + index);
        }
        break;
    case RK_SLIDER: case RK_PAINT: case RK_SKIN:
        *p = (uint8_t)index;
        break;
    case RK_PAINT_NONE:
        *p = index == 0 ? LOOK_NO_COLOR : (uint8_t)(index - 1);
        break;
    case RK_FLAG: case RK_RFLAG:
        if (index) *p |= row->aux; else *p &= (uint8_t)~row->aux;
        break;
    default:
        break;
    }
}

int cr_value_locked(const CrRow *row, int index)
{
    int v;
    if (!row) return 0;
    switch (row->kind) {
    case RK_ENUM:
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
        switch (cr.draft.look.species) {
        case SPECIES_SLIME:    return "Goo";
        case SPECIES_ROBOT:    return "Plating";
        case SPECIES_SKELETON: return "Bones";
        case SPECIES_GHOST:    return "Glow";
        case SPECIES_CACOBLOB: return "Hide";
        case SPECIES_PLUSHIE:  return "Fabric";
        case SPECIES_MUMMY:    return "Wraps";
        case SPECIES_MOTHKIN:  return "Fuzz";
        default: return is_critter_like(cr.draft.look.species) ? "Fur" : "Skin";
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
        if (row->names) return row->names[DOOMO_CLAMP(index, 0, row->count - 1)];
        doomo_snprintf(buf, 48, "%d", row->base + index);
        return buf;
    case RK_SLIDER:
        doomo_snprintf(buf, 48, "%d", index);
        return buf;
    case RK_PAINT:
        return doomo_paint_names[DOOMO_CLAMP(index, 0, PAL_PAINT_COUNT - 1)];
    case RK_PAINT_NONE:
        return index == 0 ? "None" : doomo_paint_names[DOOMO_CLAMP(index - 1, 0, PAL_PAINT_COUNT - 1)];
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
    cr.hop = 9;
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
    const char *s = doomo_catchphrases[cr.draft.catchphrase % CATCHPHRASE_COUNT];
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
    interval = 3 - cr.draft.voice[VOICE_SPEED] / 12;           /* 1..3 ticks */
    if (interval < 1) interval = 1;
    if (cr.talk_pos < cr.talk_len) {
        if (cr.talk_t % interval == 0) {
            audio_voice_char(cr.draft.voice, cr.talk[cr.talk_pos]);
            cr.talk_pos++;
        }
    } else if (cr.talk_t > (uint16_t)(cr.talk_len * interval + 45)) {
        cr.talk = NULL;
    }
}

/* ====================================================================== */
/* preview                                                                  */
static void target_look(DoomoLook *out, uint8_t *locked)
{
    DoomoResident tmp;
    const CrRow *row = cr_cur_row();
    *locked = 0;
    if (cr.phase == CRP_COPY) {
        const DoomoResident *src = cr_copy_res(cr_copy_id_at(cr.copy_sel));
        *out = src ? src->look : cr.draft.look;
        return;
    }
    *out = cr.draft.look;
    if (cr.browse < 0 || !row) return;
    tmp = cr.draft;
    if (row->kind == RK_ACTION) {
        const CrPreset *p = cr_action_preset(row->aux, cr.browse);
        if (!p) return;
        if (row->aux == CA_SPECIES_PRESET) apply_species_preset(&tmp, p);
        else apply_kv(&tmp, p, 0);
    } else if (row->field != CR_NOFIELD && row->field < sizeof(DoomoLook)) {
        cr_row_set(&tmp, row, cr.browse);
    } else {
        return;
    }
    *out = tmp.look;
    *locked = (uint8_t)cr_value_locked(row, cr.browse);
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
    uint8_t locked;
    target_look(&L, &locked);
    if (memcmp(&L, &cr.shown, sizeof(L)) != 0 || locked != cr.shown_locked) {
        /* Throttle: a new look costs a full figure render (~2 GBA ticks),
         * so while a direction is held we refresh every few ticks only. */
        if (cr.t - cr.shown_t >= 5 || cr.t - cr.changed_t >= 2 || cr.phase == CRP_COPY) {
            cr.shown = L;
            cr.shown_locked = locked;
            cr.shown_t = cr.t;
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
                if (row->field == offsetof(DoomoResident, look.species)) {
                    r->look.species = (uint8_t)v;
                } else {
                    cr_row_set(r, row, v);
                }
                break;
            default:
                break;
            }
        }
}

static uint32_t roll_seed(void)
{
    return doomo_hash32(plat_entropy() ^ (cr.t * 2654435761u) ^ doomo_frame_count() ^
                        ((uint32_t)cr.n_random << 20));
}

/* Copy the identity fields (what the creator edits) from src to dst. */
static void copy_editable(DoomoResident *dst, const DoomoResident *src, int keep_name)
{
    dst->look = src->look;
    if (!keep_name) memcpy(dst->name, src->name, DOOMO_NAME_LEN);
    memcpy(dst->voice, src->voice, VOICE_COUNT);
    memcpy(dst->persona, src->persona, PERS_COUNT);
    dst->pronoun = src->pronoun;
    dst->romance = src->romance;
    dst->bday_month = src->bday_month;
    dst->bday_day = src->bday_day;
    dst->like_food = src->like_food;
    dst->like_hobby = src->like_hobby;
    dst->like_music = src->like_music;
    dst->like_color = src->like_color;
    dst->hate_food = src->hate_food;
    dst->fear = src->fear;
    dst->catchphrase = src->catchphrase;
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
        case RK_ENUM: case RK_SLIDER: case RK_PAINT: case RK_PAINT_NONE:
        case RK_SKIN: case RK_FLAG:
            *fbyte(&cr.draft, row->field) = cbyte(&r, row->field);
            break;
        case RK_NAME:
            if (!cr.name_ok) memcpy(cr.draft.name, r.name, DOOMO_NAME_LEN);
            break;
        case RK_ACTION:
            if (row->aux == CA_OUTFIT_PRESET)
                cr.outfit_i = (int8_t)(r.look.top % cr_outfit_preset_count);
            break;
        default:
            break;
        }
    }
    if (cr.tab == CR_TAB_BODY)   /* keep the species coherent */
        for (i = 0; cr_species_base[i].field != CR_NOFIELD; i++)
            *fbyte(&cr.draft, cr_species_base[i].field) = cbyte(&r, cr_species_base[i].field);
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
        r->persona[PERS_SPEECH] = 11; r->persona[PERS_ATTITUDE] = 5;
        r->persona[PERS_QUIRK] = 10;
        r->like_food = 6; r->hate_food = 7; r->like_hobby = 28; r->like_music = 14;
        r->like_color = PAINT_CHERRY; r->fear = 19; r->catchphrase = 48;
        r->flags = RF_HELLBORN;
        r->stat[STAT_CORRUPTION] = 25;
        r->bday_month = 9; r->bday_day = 13;
    } else {
        apply_species_preset(r, &cr_species_presets[SPECIES_CATFOLK * CR_PRESETS_PER_SPECIES]);
        apply_kv(r, &cr_outfit_presets[0], 1);            /* Comfy Hoodie  */
        /* a Sweet Sunbeam: slowish, polite, expressive, easygoing */
        r->persona[PERS_MOVEMENT] = 6; r->persona[PERS_SPEECH] = 4;
        r->persona[PERS_EXPRESSION] = 11; r->persona[PERS_ATTITUDE] = 11;
        r->persona[PERS_QUIRK] = 3;
        r->like_food = cute_food[(doomo_frame_count() >> 3) % DOOMO_COUNTOF(cute_food)];
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
    } else {
        id = cr.target;
        dst = town_res(id);
        if (!dst) return -1;
        copy_editable(dst, &rec, 0);
        dst->flags = (uint8_t)((dst->flags & ~(RF_HELLBORN | RF_FAVORITE)) |
                               (rec.flags & (RF_HELLBORN | RF_FAVORITE)) | RF_CUSTOM);
    }
    resident_sanitize(dst);
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
    case CM_RANDOM_ALL: return cr_is_hellish() ? "Randomise (hellish)" : "Randomise all";
    case CM_RANDOM_TAB: return "Randomise this tab";
    case CM_UNDO:  doomo_snprintf(buf, 48, "Undo (%d)", cr_undo_count()); return buf;
    case CM_REDO:  doomo_snprintf(buf, 48, "Redo (%d)", cr_redo_count()); return buf;
    case CM_COPY:       return "Copy a look...";
    case CM_SPOOKY:     return cr.spooky ? "Possessed peek: ON" : "Possessed peek: off";
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
    case CM_RANDOM_TAB: return cr.tab != CR_TAB_HOME;
    case CM_RESET: return cr_dirty();
    default: return 1;
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
    case CM_RANDOM_ALL: go_phase(CRP_MAIN); randomise_all(); break;
    case CM_RANDOM_TAB: go_phase(CRP_MAIN); randomise_tab(); break;
    case CM_UNDO: do_undo(); break;
    case CM_REDO: do_redo(); break;
    case CM_COPY:
        open_copy(town_count(-1) > 0 ? 0 : 1);
        go_phase(CRP_COPY);
        audio_sfx(SFX_SELECT);
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
    if (btn_repeat(BTN_UP | BTN_DOWN)) {
        int d = btn_down(BTN_UP) ? CM_COUNT - 1 : 1;
        cr.menu_sel = (uint8_t)((cr.menu_sel + d) % CM_COUNT);
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_A)) {
        if (!cr_menu_enabled(cr.menu_sel)) { audio_sfx(SFX_ERROR); return; }
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
static void ensure_visible(void)
{
    int vis = crl.rows > 0 ? crl.rows : 6;
    int r = cr.row[cr.tab], top = cr.top[cr.tab];
    int n = cr_tabs[cr.tab].nrows;
    if (r - 1 < top && r > 0 && cr_tabs[cr.tab].rows[r - 1].kind == RK_HEADER) top = r - 1;
    if (r < top) top = r;
    if (r >= top + vis) top = r - vis + 1;
    top = DOOMO_CLAMP(top, 0, DOOMO_MAX(0, n - vis));
    cr.top[cr.tab] = (uint8_t)top;
}

static void move_row(int d)
{
    int n = cr_tabs[cr.tab].nrows, r = cr.row[cr.tab], k;
    for (k = 0; k < n; k++) {
        r = (r + d + n) % n;
        if (cr_tabs[cr.tab].rows[r].kind != RK_HEADER) break;
    }
    cr.row[cr.tab] = (uint8_t)r;
    cr.browse = -1;
    ensure_visible();
    audio_sfx(SFX_CURSOR);
}

static void switch_tab(int d)
{
    cr.tab = (uint8_t)((cr.tab + d + CR_TAB_COUNT) % CR_TAB_COUNT);
    cr.tab_t = 0;
    cr.browse = -1;
    if (cr_tabs[cr.tab].rows[cr.row[cr.tab]].kind == RK_HEADER)
        cr.row[cr.tab] = (uint8_t)cr_first_selectable(cr.tab);
    ensure_visible();
    audio_sfx(SFX_CURSOR);
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
        if (cr.draft.look.skin < PAL_SKIN_COUNT) {
            static const uint8_t natural[PAL_SKIN_COUNT] = {
                PAINT_PEACH, PAINT_PEACH, PAINT_CARAMEL, PAINT_CARAMEL, PAINT_HONEY,
                PAINT_CHOCOLATE, PAINT_CHOCOLATE, PAINT_CHARCOAL };
            cr.draft.look.hair_color = natural[cr.draft.look.skin];
        } else {
            cr.draft.look.hair_color = (uint8_t)(cr.draft.look.skin - 8);
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
        v = (cur + d + n) % n;
    }
    if (cr_value_locked(row, v)) {
        cr.browse = (int16_t)v;
        audio_sfx(SFX_CURSOR);
        return;
    }
    cr.browse = -1;
    cr_row_set(&cr.draft, row, v);
    commit(key);
    if (row->kind != RK_SLIDER) cr_sparkle_burst(3, -1);
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
        cr.hop = 6;
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
    if (btn_released(BTN_SELECT) && btn_held_ticks(BTN_X | BTN_Y) == 0) {
        cr.view8 = 0;
        cr.walking = 0;
        cr.spin = 0;
        go_phase(CRP_TURN);
        audio_sfx(SFX_SELECT);
    }
}

/* ====================================================================== */
/* pickers                                                                  */
static void pick_preview(const CrRow *row)
{
    if (cr_value_locked(row, cr.pick_sel)) {
        cr.browse = cr.pick_sel;
    } else {
        cr.browse = -1;
        cr_row_set(&cr.draft, row, cr.pick_sel);
        cr.changed_t = cr.t;
    }
}

static void pick_close(const CrRow *row, int ok)
{
    if (ok) {
        cr.browse = -1;
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
    int n = cr_row_count(row), vis = cr_pick_rows(), old = cr.pick_sel;
    if (cr.pick_err) cr.pick_err--;
    if (btn_repeat(BTN_UP)) cr.pick_sel = (int16_t)((cr.pick_sel + n - 1) % n);
    if (btn_repeat(BTN_DOWN)) cr.pick_sel = (int16_t)((cr.pick_sel + 1) % n);
    if (btn_repeat(BTN_LEFT | BTN_L)) cr.pick_sel = (int16_t)DOOMO_MAX(0, cr.pick_sel - vis);
    if (btn_repeat(BTN_RIGHT | BTN_R)) cr.pick_sel = (int16_t)DOOMO_MIN(n - 1, cr.pick_sel + vis);
    if (cr.pick_sel < cr.pick_top) cr.pick_top = cr.pick_sel;
    if (cr.pick_sel >= cr.pick_top + vis) cr.pick_top = (int16_t)(cr.pick_sel - vis + 1);
    cr.pick_top = (int16_t)DOOMO_CLAMP(cr.pick_top, 0, DOOMO_MAX(0, n - vis));
    if (cr.pick_sel != old) {
        pick_preview(row);
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_A)) {
        if (cr_value_locked(row, cr.pick_sel)) { audio_sfx(SFX_ERROR); cr.pick_err = 14; return; }
        pick_close(row, 1);
    } else if (btn_pressed(BTN_B)) {
        pick_close(row, 0);
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
        pick_close(row, 1);
    } else if (btn_pressed(BTN_B)) {
        pick_close(row, 0);
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
        cr.draft.look = src->look;
        fix_locks(&cr.draft);
        commit(-1);
        if (cr.copy_src) unlock_grant(CRU_COPY_PREMADE);
        cr_sparkle_burst(10, -1);
        audio_sfx(SFX_SPARKLE);
        go_phase(CRP_MAIN);
    }
}

static void update_turn(void)
{
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
            cr.draft.pronoun = PRONOUN_THEY;
        }
        nick_copy(cr.nick, creator_nickname(DOOMO_PLAYER_ID));
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
    for (i = 0; i < CR_TAB_COUNT; i++) cr.row[i] = (uint8_t)cr_first_selectable(i);
    cr.preset_i = 0;
    refresh_home();
    cr.shown = cr.draft.look;
    cr.img.uid = 0;
    audio_music(SONG_CREATOR);
    cr_sparkle_burst(8, -1);
}

static void cr_leave(void)
{
    if (scene_scratch_mine(CR_SCRATCH_TAG)) cr.talk = NULL;
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
    default: break;
    }
    if (scene_top() == &SCENE_CREATOR) preview_update();
}

const DoomoScene SCENE_CREATOR = { "CREATOR", cr_enter, cr_leave, cr_update, cr_draw, 0 };
