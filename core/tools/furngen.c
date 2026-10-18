/* furngen.c - the dream house's furniture generator (host tool, owner: H4;
 * HF sec.6.2, SPEC R19, C3).
 *
 *   furngen --out-root DIR    write DIR/core/gen/{furn_data.c, furn_grant.c,
 *                             furn_iso.c, furn_ids.h} from the repository's
 *                             core/data/furn/catalog.txt (H1: stats) and
 *                             core/data/furn/art/ *.txt (H4: recipes)
 *   furngen --sheet FILE.png  a contact sheet of every baked facing
 *   furngen --check           parse, join and bake only
 *   furngen --repo DIR        where catalog.txt and art/ are (default: the
 *                             repository this tool was built in)
 *
 * The two inputs are joined by key: an art block whose key is not in the
 * catalog fails the build (so does a duplicate key, a bad token or a fixed
 * material that uses a marker colour). A catalog row without art (H4 M1
 * draws 60 pieces; M2 all of them) gets a placeholder box from its
 * footprint and paints; `--strict` makes that an error.
 *
 * Every piece is baked into two facings (rot 0 and rot 1) by casting one
 * ray per pixel along the iso view direction (1,1,1) against its prims:
 * boxes, ellipsoid blobs, vertical cylinders and cones. Shading is the
 * 3-tone rule (top light, +y face mid, +x face dark; curved prims by their
 * normal), soft prims dither their tone edges, shiny ones get a highlight;
 * then coloured selective outlines (the darkest tone of the local colour,
 * black only on the contact row) and a rim light on the top-left edge.
 * Facings are span encoded (furn.h). Integer maths only (128-bit for the
 * quadratics), so the output is byte-identical on every host.
 *
 * This tool may use stdio and malloc: it never ships in a ROM.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/types.h"
#include "doomo/palette.h"
#include "doomo/furn.h"
#include "doomo/resident.h"

#ifndef FURNGEN_REPO
#define FURNGEN_REPO "."
#endif

__extension__ typedef __int128 i128;
__extension__ typedef unsigned __int128 u128;

/* ---- errors --------------------------------------------------------------- */
static int fg_errors;
static char fg_lasterr[256];
static void fg_err(const char *where, int line, const char *msg, const char *tok)
{
    snprintf(fg_lasterr, sizeof fg_lasterr, "%s:%d: %s%s%s", where, line, msg,
             tok ? " " : "", tok ? tok : "");
    fprintf(stderr, "furngen: %s\n", fg_lasterr);
    fg_errors++;
}

static void scopy(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* ---- names ------------------------------------------------------------------- */
typedef struct { const char *n; int v; } Name;
static const Name cat_names[] = {
    { "PASTEL", FCAT_PASTEL }, { "GOTHIC", FCAT_GOTHIC }, { "HELLISH", FCAT_HELLISH },
    { "PLUSH", FCAT_PLUSH }, { "KITCHEN", FCAT_KITCHEN }, { "TECH", FCAT_TECH },
    { "GARDEN", FCAT_GARDEN }, { "LIGHT", FCAT_LIGHT }, { "WALL", FCAT_WALL },
    { "RUG", FCAT_RUG }, { "TROPHY", FCAT_TROPHY }, { "SEASONAL", FCAT_SEASONAL },
    { "STORY", FCAT_STORY }, { NULL, 0 } };
static const Name flag_names[] = {
    { "WALL", FF_WALL }, { "RUG", FF_RUG }, { "SURFACE", FF_SURFACE }, { "SMALL", FF_SMALL },
    { "LIGHT", FF_LIGHT }, { "ANIM", FF_ANIM }, { "HELL", FF_HELL }, { "BILLBOARD", FF_BILLBOARD },
    { NULL, 0 } };
static const Name flag2_names[] = {
    { "PORTAL", FF_PORTAL }, { "CURSED", FF_CURSED }, { "NO_SQUASH", FF_NO_SQUASH },
    { "UNSELLABLE", FF_UNSELLABLE }, { "GIFT", FF_GIFT }, { "STARTER", FF_STARTER }, { NULL, 0 } };
static const Name fi_names[] = {
    { "NONE", FI_NONE }, { "SIT", FI_SIT }, { "SLEEP", FI_SLEEP }, { "EAT", FI_EAT },
    { "COOK", FI_COOK }, { "PLAY", FI_PLAY }, { "WATCH", FI_WATCH }, { "MUSIC", FI_MUSIC },
    { "READ", FI_READ }, { "BATHE", FI_BATHE }, { "PRAY", FI_PRAY }, { "SUMMON", FI_SUMMON },
    { "EXAMINE", FI_EXAMINE }, { "TURRET", FI_TURRET }, { NULL, 0 } };
static const Name role_names[] = {
    { "NONE", FR_NONE }, { "DOOR", FR_DOOR }, { "WINDOW", FR_WINDOW }, { "HEARTH", FR_HEARTH },
    { "BED", FR_BED }, { "SEAT", FR_SEAT }, { "TABLE", FR_TABLE }, { "LAMP", FR_LAMP },
    { "CANDLE", FR_CANDLE }, { "MIRROR", FR_MIRROR }, { "PLUSH", FR_PLUSH },
    { "PENTAGRAM", FR_PENTAGRAM }, { "PLAIN_HUMAN", FR_PLAIN_HUMAN }, { "TURRET", FR_TURRET },
    { "PLATE", FR_PLATE }, { "NEST", FR_NEST }, { "FENCE", FR_FENCE }, { "PLANT", FR_PLANT },
    { NULL, 0 } };
static const Name theme_names[] = {
    { "PASTEL", THEME_PASTEL }, { "CLOUD", THEME_CLOUD }, { "CANDY", THEME_CANDY },
    { "COTTAGE", THEME_COTTAGE }, { "GOTH", THEME_GOTH }, { "BONE", THEME_BONE },
    { "LAVA", THEME_LAVA }, { "VAPOR", THEME_VAPOR }, { "PICNIC", THEME_PICNIC },
    { "WORKSHOP", THEME_WORKSHOP }, { NULL, 0 } };
static const Name need_names[] = {
    { "HUNGER", STAT_HUNGER }, { "HAPPY", STAT_HAPPY }, { "HYGIENE", STAT_HYGIENE },
    { "ENERGY", STAT_ENERGY }, { "HEALTH", STAT_HEALTH }, { "SANITY", STAT_SANITY },
    { "SOCIAL", STAT_SOCIAL }, { NULL, 0 } };
static const Name paint_names[] = {
    { "WHITE", PAINT_WHITE }, { "SILVER", PAINT_SILVER }, { "CHARCOAL", PAINT_CHARCOAL },
    { "JET", PAINT_JET }, { "CHERRY", PAINT_CHERRY }, { "BLOOD", PAINT_BLOOD },
    { "ROSE", PAINT_ROSE }, { "BUBBLEGUM", PAINT_BUBBLEGUM }, { "PEACH", PAINT_PEACH },
    { "TANGERINE", PAINT_TANGERINE }, { "RUST", PAINT_RUST }, { "HONEY", PAINT_HONEY },
    { "LEMON", PAINT_LEMON }, { "CHARTREUSE", PAINT_CHARTREUSE }, { "LIME", PAINT_LIME },
    { "MINT", PAINT_MINT }, { "FOREST", PAINT_FOREST }, { "TEAL", PAINT_TEAL },
    { "CYAN", PAINT_CYAN }, { "SKY", PAINT_SKY }, { "COBALT", PAINT_COBALT },
    { "NAVY", PAINT_NAVY }, { "LAVENDER", PAINT_LAVENDER }, { "GRAPE", PAINT_GRAPE },
    { "VAPE_PURPLE", PAINT_VAPE_PURPLE }, { "MAGENTA", PAINT_MAGENTA },
    { "CHOCOLATE", PAINT_CHOCOLATE }, { "CARAMEL", PAINT_CARAMEL }, { "BONE", PAINT_BONE },
    { "SLIME", PAINT_SLIME }, { "HELLFIRE", PAINT_HELLFIRE }, { "ECTOPLASM", PAINT_ECTOPLASM },
    { NULL, 0 } };
static const Name ramp_names[] = {
    { "STONE", RAMP_STONE }, { "WOOD", RAMP_WOOD }, { "GRASS", RAMP_GRASS },
    { "PINKRAMP", RAMP_PASTEL_PINK }, { "SKYRAMP", RAMP_SKY }, { "VAPE", RAMP_VAPE },
    { "CANDYRAMP", RAMP_CANDY }, { "MINTRAMP", RAMP_MINT }, { "PEACHRAMP", RAMP_PEACH },
    { "BLOODRAMP", RAMP_BLOOD }, { "LAVA", RAMP_LAVA }, { "FLESH", RAMP_FLESH },
    { "BONERAMP", RAMP_BONE }, { "SICK", RAMP_SICK }, { "STEEL", RAMP_STEEL }, { NULL, 0 } };

static int name_find(const Name *t, const char *s)
{
    int i;
    for (i = 0; t[i].n; i++) if (strcmp(t[i].n, s) == 0) return t[i].v;
    return -1;
}

static int flags_parse(const Name *t, const char *s, int *out)
{
    char buf[128], *p, *q;
    int v = 0;
    scopy(buf, sizeof buf, s);
    for (p = buf; p && *p; p = q) {
        int f;
        q = strchr(p, '|');
        if (q) *q++ = 0;
        f = name_find(t, p);
        if (f < 0) return 0;
        v |= f;
    }
    *out = v;
    return 1;
}

/* ---- the tokenizer: words and "quoted strings"; # starts a comment --------- */
typedef struct { const char *src; int pos, line; const char *file; } Lex;
static int lex_next(Lex *L, char *out, int cap, int *quoted, int *newline)
{
    const char *s = L->src;
    int n = 0;
    *quoted = 0;
    *newline = 0;
    for (;;) {
        char c = s[L->pos];
        if (c == '\n') { L->line++; L->pos++; *newline = 1; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { L->pos++; continue; }
        if (c == '#') { while (s[L->pos] && s[L->pos] != '\n') L->pos++; continue; }
        break;
    }
    if (!s[L->pos]) return 0;
    if (s[L->pos] == '"') {
        L->pos++;
        while (s[L->pos] && s[L->pos] != '"' && s[L->pos] != '\n') {
            if (n < cap - 1) out[n++] = s[L->pos];
            L->pos++;
        }
        if (s[L->pos] == '"') L->pos++;
        *quoted = 1;
    } else {
        while (s[L->pos] && s[L->pos] != ' ' && s[L->pos] != '\t' && s[L->pos] != '\n' &&
               s[L->pos] != '\r' && s[L->pos] != '#') {
            if (n < cap - 1) out[n++] = s[L->pos];
            L->pos++;
        }
    }
    out[n] = 0;
    return 1;
}

static int to_int(const char *s, int *v)
{
    char *e;
    long x = strtol(s, &e, 10);
    if (!*s || *e) return 0;
    *v = (int)x;
    return 1;
}

/* ---- the catalog ------------------------------------------------------------- */
#define FG_MAX FURN_MAX
typedef struct Row {
    char key[40], name[32], desc[64], reward[8];
    int cat, price, unlock, giver, flags, flags2, fi, role, fw, fd, fh;
    int pa, pb, cute, spooky, comfy, need, seat, light, theme, mass;
    int line;
} Row;
static Row rows[FG_MAX];
static int nrows;

static int row_find(const char *key)
{
    int i;
    for (i = 0; i < nrows; i++) if (strcmp(rows[i].key, key) == 0) return i;
    return -1;
}

static int paint_or_fixed(const char *s)
{
    if (strcmp(s, "FIXED") == 0) return 0xFF;
    return name_find(paint_names, s);
}

/* Parse catalog text. Returns the number of errors it added. */
static int parse_catalog(const char *text, const char *file)
{
    Lex L;
    char tok[128];
    int q, nl, e0 = fg_errors;
    Row *r = NULL;
    L.src = text; L.pos = 0; L.line = 1; L.file = file;
    nrows = 0;
#define NEXT() (lex_next(&L, tok, sizeof tok, &q, &nl))
#define NEED_INT(dst) do { int v_; if (!NEXT() || !to_int(tok, &v_)) { fg_err(file, L.line, "expected a number after", attr); goto fail; } (dst) = v_; } while (0)
    while (NEXT()) {
        char attr[32];
        if (tok[0] == '[') {
            char *end = strchr(tok, ']');
            if (!end || end == tok + 1) { fg_err(file, L.line, "bad key", tok); goto fail; }
            *end = 0;
            if (nrows >= FG_MAX) { fg_err(file, L.line, "more than FURN_MAX rows", NULL); goto fail; }
            if (row_find(tok + 1) >= 0) { fg_err(file, L.line, "duplicate key", tok + 1); goto fail; }
            r = &rows[nrows++];
            memset(r, 0, sizeof(*r));
            scopy(r->key, sizeof r->key, tok + 1);
            r->line = L.line;
            r->unlock = 0xFFFF; r->giver = 0xFFFF; r->need = 0xFF; r->pa = r->pb = 0xFF;
            r->fw = r->fd = 1; r->fh = 4; r->theme = THEME_PASTEL;
            if (!NEXT() || (r->cat = name_find(cat_names, tok)) < 0) { fg_err(file, L.line, "bad category", tok); goto fail; }
            if (!NEXT() || !to_int(tok, &r->price) || r->price < 0 || r->price > 65535) { fg_err(file, L.line, "bad price", tok); goto fail; }
            if (!NEXT() || !q) { fg_err(file, L.line, "expected a quoted name", tok); goto fail; }
            if (strlen(tok) > 20 || !tok[0]) { fg_err(file, L.line, "name longer than 20", tok); goto fail; }
            scopy(r->name, sizeof r->name, tok);
            continue;
        }
        if (!r) { fg_err(file, L.line, "attribute before the first [key]", tok); goto fail; }
        scopy(attr, sizeof attr, tok);
        if (!strcmp(attr, "desc")) {
            if (!NEXT() || !q || strlen(tok) > 40) { fg_err(file, L.line, "desc must be quoted, <= 40", tok); goto fail; }
            scopy(r->desc, sizeof r->desc, tok);
        } else if (!strcmp(attr, "foot")) {
            NEED_INT(r->fw); NEED_INT(r->fd); NEED_INT(r->fh);
            if (r->fw < 1 || r->fw > 4 || r->fd < 1 || r->fd > 4 || r->fh < 0 || r->fh > 40) { fg_err(file, L.line, "bad footprint for", r->key); goto fail; }
        } else if (!strcmp(attr, "flags")) {
            if (!NEXT() || !flags_parse(flag_names, tok, &r->flags)) { fg_err(file, L.line, "bad flags", tok); goto fail; }
        } else if (!strcmp(attr, "flags2")) {
            if (!NEXT() || !flags_parse(flag2_names, tok, &r->flags2)) { fg_err(file, L.line, "bad flags2", tok); goto fail; }
        } else if (!strcmp(attr, "fi")) {
            if (!NEXT() || (r->fi = name_find(fi_names, tok)) < 0) { fg_err(file, L.line, "bad fi", tok); goto fail; }
        } else if (!strcmp(attr, "role")) {
            if (!NEXT() || (r->role = name_find(role_names, tok)) < 0) { fg_err(file, L.line, "bad role", tok); goto fail; }
        } else if (!strcmp(attr, "paints")) {
            if (!NEXT() || (r->pa = paint_or_fixed(tok)) < 0) { fg_err(file, L.line, "bad paint", tok); goto fail; }
            if (!NEXT() || (r->pb = paint_or_fixed(tok)) < 0) { fg_err(file, L.line, "bad paint", tok); goto fail; }
        } else if (!strcmp(attr, "score")) {
            NEED_INT(r->cute); NEED_INT(r->spooky); NEED_INT(r->comfy);
            if (r->cute < -10 || r->cute > 10 || r->spooky < -10 || r->spooky > 10 || r->comfy < -10 || r->comfy > 10) { fg_err(file, L.line, "score outside -10..10 for", r->key); goto fail; }
        } else if (!strcmp(attr, "theme")) {
            if (!NEXT() || (r->theme = name_find(theme_names, tok)) < 0) { fg_err(file, L.line, "bad theme", tok); goto fail; }
        } else if (!strcmp(attr, "need")) {
            if (!NEXT() || (r->need = name_find(need_names, tok)) < 0) { fg_err(file, L.line, "bad need", tok); goto fail; }
        } else if (!strcmp(attr, "mass")) { NEED_INT(r->mass);
        } else if (!strcmp(attr, "seat")) { NEED_INT(r->seat);
        } else if (!strcmp(attr, "light")) { NEED_INT(r->light);
        } else if (!strcmp(attr, "giver")) { NEED_INT(r->giver);
        } else if (!strcmp(attr, "unlock")) { NEED_INT(r->unlock);
        } else if (!strcmp(attr, "reward")) {
            if (!NEXT() || tok[0] != 'S' || strlen(tok) > 5) { fg_err(file, L.line, "bad reward", tok); goto fail; }
            scopy(r->reward, sizeof r->reward, tok);
        } else {
            fg_err(file, L.line, "unknown attribute", attr);
            goto fail;
        }
        if ((r->flags & (FF_WALL | FF_SMALL | FF_RUG)) && r->mass) { fg_err(file, L.line, "wall, small and rug pieces have no mass:", r->key); goto fail; }
    }
#undef NEED_INT
#undef NEXT
fail:
    return fg_errors - e0;
}

/* ---- the art ----------------------------------------------------------------- */
enum { MAT_A = -1, MAT_B = -2 };
typedef struct Prim {
    int kind, frames;            /* FP_*, frame mask (0 = all)                     */
    int x, y, z, w, d, h;        /* box: min + size; blob: centre + radii;
                                    cyl/cone: centre x y, z bottom, w radius, h   */
    int mat, style;              /* MAT_A / MAT_B / a palette index (the dark tone);
                                    ramp materials store -100 - ramp              */
} Prim;
#define FG_MAX_PRIMS 24
typedef struct Art {
    int has, frames, nprim, line;
    Prim p[FG_MAX_PRIMS];
    char file[64];
} Art;
static Art arts[FG_MAX];

static int mat_parse(const char *s, int *mat)
{
    int v;
    if (!strcmp(s, "A")) { *mat = MAT_A; return 1; }
    if (!strcmp(s, "B")) { *mat = MAT_B; return 1; }
    if (s[0] == '~') {
        v = name_find(ramp_names, s + 1);
        if (v < 0) return 0;
        *mat = -100 - v;
        return 1;
    }
    v = name_find(paint_names, s);
    if (v < 0) return 0;
    if (v == FURN_MARK_A || v == FURN_MARK_B) return -1;   /* the markers are reserved */
    *mat = v;
    return 1;
}

static int parse_art(const char *text, const char *file)
{
    Lex L;
    char tok[128];
    int q, nl, e0 = fg_errors, cur = -1;
    L.src = text; L.pos = 0; L.line = 1; L.file = file;
    if (!lex_next(&L, tok, sizeof tok, &q, &nl)) return 0;
    for (;;) {
        if (tok[0] == '[') {
            char *end = strchr(tok, ']');
            if (!end) { fg_err(file, L.line, "bad key", tok); return fg_errors - e0; }
            *end = 0;
            cur = row_find(tok + 1);
            if (cur < 0) { fg_err(file, L.line, "art key not in catalog.txt:", tok + 1); return fg_errors - e0; }
            if (arts[cur].has) { fg_err(file, L.line, "art defined twice for", tok + 1); return fg_errors - e0; }
            arts[cur].has = 1;
            arts[cur].frames = 1;
            arts[cur].line = L.line;
            snprintf(arts[cur].file, sizeof arts[cur].file, "%s", file);
            if (!lex_next(&L, tok, sizeof tok, &q, &nl)) break;
            continue;
        }
        if (cur < 0) { fg_err(file, L.line, "prim before the first [key]", tok); return fg_errors - e0; }
        if (!strcmp(tok, "frames")) {
            int v;
            if (!lex_next(&L, tok, sizeof tok, &q, &nl) || !to_int(tok, &v) || v < 1 || v > 4) { fg_err(file, L.line, "frames 1..4", tok); return fg_errors - e0; }
            arts[cur].frames = v;
            if (!lex_next(&L, tok, sizeof tok, &q, &nl)) break;
            continue;
        }
        {
            Prim *p;
            char kind[32], *at;
            int args[6], na, i, mr;
            if (arts[cur].nprim >= FG_MAX_PRIMS) { fg_err(file, L.line, "too many prims in", rows[cur].key); return fg_errors - e0; }
            p = &arts[cur].p[arts[cur].nprim++];
            memset(p, 0, sizeof(*p));
            scopy(kind, sizeof kind, tok);
            at = strchr(kind, '@');
            if (at) {
                *at++ = 0;
                for (; *at; at++) {
                    if (*at < '0' || *at > '3') { fg_err(file, L.line, "bad frame mask", tok); return fg_errors - e0; }
                    p->frames |= 1 << (*at - '0');
                }
            }
            if (!strcmp(kind, "box")) { p->kind = FP_BOX; na = 6; }
            else if (!strcmp(kind, "blob")) { p->kind = FP_BLOB; na = 6; }
            else if (!strcmp(kind, "cyl")) { p->kind = FP_CYL; na = 5; }
            else if (!strcmp(kind, "cone")) { p->kind = FP_CONE; na = 5; }
            else { fg_err(file, L.line, "unknown prim", kind); return fg_errors - e0; }
            for (i = 0; i < na; i++)
                if (!lex_next(&L, tok, sizeof tok, &q, &nl) || !to_int(tok, &args[i])) { fg_err(file, L.line, "prim needs numbers:", kind); return fg_errors - e0; }
            p->x = args[0]; p->y = args[1]; p->z = args[2];
            if (na == 6) { p->w = args[3]; p->d = args[4]; p->h = args[5]; }
            else { p->w = args[3]; p->d = args[3]; p->h = args[4]; }
            if (p->w <= 0 || p->d <= 0 || p->h <= 0 || p->z < 0 || p->x < -64 || p->x > 127 || p->y < -64 || p->y > 127 ||
                p->z > 200 || p->w > 127 || p->d > 127 || p->h > 160) { fg_err(file, L.line, "prim out of range in", rows[cur].key); return fg_errors - e0; }
            if (!lex_next(&L, tok, sizeof tok, &q, &nl)) { fg_err(file, L.line, "prim needs a material", NULL); return fg_errors - e0; }
            mr = mat_parse(tok, &p->mat);
            if (mr < 0) { fg_err(file, L.line, "fixed material uses a marker paint (BUBBLEGUM/MINT):", tok); return fg_errors - e0; }
            if (!mr) { fg_err(file, L.line, "bad material", tok); return fg_errors - e0; }
            /* styles until the next prim keyword / key */
            for (;;) {
                if (!lex_next(&L, tok, sizeof tok, &q, &nl)) goto done;
                if (!strcmp(tok, "soft")) p->style |= FPS_SOFT;
                else if (!strcmp(tok, "shiny")) p->style |= FPS_SHINY;
                else if (!strcmp(tok, "glow")) p->style |= 4;
                else break;
            }
        }
    }
done:
    return fg_errors - e0;
}

/* A placeholder for a row without art: its footprint as a box. */
static void art_default(int i)
{
    Row *r = &rows[i];
    Art *a = &arts[i];
    Prim *p = &a->p[0];
    int W = r->fw * 16, D = r->fd * 16, H = r->fh * 4;
    memset(a, 0, sizeof(*a));
    a->frames = 1;
    a->nprim = 1;
    p->kind = FP_BOX;
    p->mat = r->pa != 0xFF ? MAT_A : PAINT_CARAMEL;
    if (r->flags & FF_WALL) { p->x = 1; p->y = 0; p->z = 0; p->w = W - 2; p->d = 2; p->h = H ? H : 8; }
    else if (r->flags & FF_RUG) { p->x = 1; p->y = 1; p->z = 0; p->w = W - 2; p->d = D - 2; p->h = 1; }
    else if (r->flags & FF_SMALL) { p->x = 5; p->y = 5; p->w = 6; p->d = 6; p->h = H ? H : 6; }
    else { p->x = 2; p->y = 2; p->w = W - 4; p->d = D - 4; p->h = H ? H : 8; }
    if (r->flags & FF_SMALL) { p->kind = FP_BLOB; p->x = 8; p->y = 8; p->z = p->h / 2; p->w = 4; p->d = 4; p->h = p->h / 2 + 1; }
}

/* ---- baking ------------------------------------------------------------------- */
#define BW 192
#define BH 256
#define BOX0 96          /* screen u = 0 at buffer column BOX0     */
#define BOY0 176         /* screen v = 0 at buffer row BOY0        */
#define Q 16             /* sub-pixel precision                    */
typedef struct Hit { int64_t t; int tone256, prim, zq; } Hit;

/* the material's tones: [0] dark .. [2] light, [3] highlight, [4] outline */
static void mat_tones(int mat, uint8_t t[5])
{
    int p, i;
    if (mat <= -100) {
        int r = -100 - mat;
        t[0] = PAL_RAMP(r, 2); t[1] = PAL_RAMP(r, 4); t[2] = PAL_RAMP(r, 6); t[3] = PAL_RAMP(r, 7);
        t[4] = PAL_RAMP(r, 0);
        return;
    }
    p = mat == MAT_A ? FURN_MARK_A : mat == MAT_B ? FURN_MARK_B : mat;
    for (i = 0; i < 3; i++) t[i] = PAL_PAINT(p, i);
    t[3] = (mat < 0) ? t[2] : doomo_art_lighter[t[2]];
    t[4] = doomo_art_darker[t[0]];
}

/* The 8 indices the recolour LUT rewrites (furn.h: markers + their outlines). */
static int reserved(uint8_t c)
{
    return c == PAL_PAINT(FURN_MARK_A, 0) || c == PAL_PAINT(FURN_MARK_A, 1) || c == PAL_PAINT(FURN_MARK_A, 2) ||
           c == PAL_PAINT(FURN_MARK_B, 0) || c == PAL_PAINT(FURN_MARK_B, 1) || c == PAL_PAINT(FURN_MARK_B, 2) ||
           c == doomo_art_darker[PAL_PAINT(FURN_MARK_A, 0)] || c == doomo_art_darker[PAL_PAINT(FURN_MARK_B, 0)];
}

static uint8_t unreserve(uint8_t c)
{
    int k;
    for (k = 0; k < 8 && reserved(c); k++) c = doomo_art_darker[c];
    return reserved(c) ? PAL_BLACK : c;
}

static u128 isqrt128(u128 v)
{
    u128 r = 0, bit = (u128)1 << 126;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return r;
}

static int64_t isqrt64(int64_t v) { return v <= 0 ? 0 : (int64_t)isqrt128((u128)v); }

/* light direction (unnormalised x, y, z): top light, +y mid, +x dark */
#define LX 30
#define LY 55
#define LZ 78
static int shade_of(int64_t nx, int64_t ny, int64_t nz)
{
    int64_t len = isqrt64(nx * nx + ny * ny + nz * nz);
    int64_t d;
    if (!len) return 128;
    d = (nx * LX + ny * LY + nz * LZ) * 256 / (len * 100);   /* -256..256 */
    return (int)d;
}

/* Ray of buffer pixel (px, py): P(t) = (X0 + t, Y0 + t, t), all Q16. */
static int intersect(const Prim *p, int64_t X0, int64_t Y0, Hit *h)
{
    int64_t tmin = -((int64_t)1 << 40), tmax = (int64_t)1 << 40;
    if (p->kind == FP_BOX) {
        int64_t x0 = (int64_t)p->x * Q, x1 = (int64_t)(p->x + p->w) * Q;
        int64_t y0 = (int64_t)p->y * Q, y1 = (int64_t)(p->y + p->d) * Q;
        int64_t z0 = (int64_t)p->z * Q, z1 = (int64_t)(p->z + p->h) * Q;
        int64_t a0 = x0 - X0, a1 = x1 - X0, b0 = y0 - Y0, b1 = y1 - Y0;
        int face;
        tmin = a0; if (b0 > tmin) tmin = b0; if (z0 > tmin) tmin = z0;
        tmax = a1; face = 0;
        if (b1 < tmax) { tmax = b1; face = 1; }
        if (z1 < tmax) { tmax = z1; face = 2; }
        if (tmin >= tmax) return 0;
        h->t = tmax;
        h->tone256 = face == 2 ? shade_of(0, 0, 1) : face == 1 ? shade_of(0, 1, 0) : shade_of(1, 0, 0);
        h->zq = (int)tmax;
        return 1;
    }
    if (p->kind == FP_BLOB) {
        /* ((X0+t-cx)/rx)^2 + ((Y0+t-cy)/ry)^2 + ((t-cz)/rz)^2 = 1, Q16 */
        i128 rx2 = (i128)p->w * p->w * Q * Q, ry2 = (i128)p->d * p->d * Q * Q, rz2 = (i128)p->h * p->h * Q * Q;
        i128 dx = X0 - (int64_t)p->x * Q, dy = Y0 - (int64_t)p->y * Q, dz = -(int64_t)p->z * Q;
        i128 A = ry2 * rz2 + rx2 * rz2 + rx2 * ry2;
        i128 B = 2 * (dx * ry2 * rz2 + dy * rx2 * rz2 + dz * rx2 * ry2);
        i128 C = dx * dx * ry2 * rz2 + dy * dy * rx2 * rz2 + dz * dz * rx2 * ry2 - rx2 * ry2 * rz2;
        i128 disc, sq;
        int64_t t, hx, hy, hz;
        /* scale down to keep B^2 inside 128 bits */
        while (B > ((i128)1 << 60) || B < -((i128)1 << 60) || A > ((i128)1 << 60) || C > ((i128)1 << 60) || C < -((i128)1 << 60)) {
            A >>= 4; B >>= 4; C >>= 4;
        }
        if (A <= 0) return 0;
        disc = B * B - 4 * A * C;
        if (disc < 0) return 0;
        sq = (i128)isqrt128((u128)disc);
        t = (int64_t)((-B + sq) / (2 * A));
        hx = dx + t; hy = dy + t; hz = dz + t;    /* offsets from the centre */
        h->t = t;
        h->tone256 = shade_of((int64_t)(hx * 1024 / ((int64_t)p->w * p->w)), (int64_t)(hy * 1024 / ((int64_t)p->d * p->d)),
                              (int64_t)(hz * 1024 / ((int64_t)p->h * p->h)));
        h->zq = (int)t;
        return 1;
    }
    {
        /* vertical cylinder / cone: (X0+t-cx)^2 + (Y0+t-cy)^2 = r(t)^2 */
        int64_t cx = (int64_t)p->x * Q, cy = (int64_t)p->y * Q, z0 = (int64_t)p->z * Q, z1 = (int64_t)(p->z + p->h) * Q;
        int64_t r = (int64_t)p->w * Q, dx = X0 - cx, dy = Y0 - cy;
        int64_t best = -((int64_t)1 << 40);
        int got = 0, tone = 0;
        if (p->kind == FP_CYL) {
            i128 A = 2, B = 2 * ((i128)dx + dy), C = (i128)dx * dx + (i128)dy * dy - (i128)r * r;
            i128 disc = B * B - 4 * A * C;
            /* the top cap */
            {
                int64_t ex = dx + z1, ey = dy + z1;
                if ((i128)ex * ex + (i128)ey * ey <= (i128)r * r) { best = z1; got = 1; tone = shade_of(0, 0, 1); }
            }
            if (disc >= 0) {
                int64_t t = (int64_t)((-B + (i128)isqrt128((u128)disc)) / (2 * A));
                if (t >= z0 && t <= z1 && t > best) {
                    best = t; got = 1;
                    tone = shade_of(dx + t, dy + t, 0);
                }
            }
        } else {
            /* r(t) = r * (z1 - t) / hh: hh^2 [(dx+t)^2 + (dy+t)^2] - r^2 (z1-t)^2 = 0 */
            int64_t hh = z1 - z0;
            i128 h2 = (i128)hh * hh, r2 = (i128)r * r;
            i128 A = 2 * h2 - r2, B = 2 * h2 * ((i128)dx + dy) + 2 * r2 * z1;
            i128 C = h2 * ((i128)dx * dx + (i128)dy * dy) - r2 * (i128)z1 * z1;
            i128 disc;
            int64_t t;
            while (B > ((i128)1 << 60) || B < -((i128)1 << 60) || C > ((i128)1 << 60) || C < -((i128)1 << 60) ||
                   A > ((i128)1 << 60) || A < -((i128)1 << 60)) { A >>= 4; B >>= 4; C >>= 4; }
            disc = B * B - 4 * A * C;
            if (disc >= 0 && A != 0) {
                i128 sq = (i128)isqrt128((u128)disc), t1 = (-B + sq) / (2 * A), t2 = (-B - sq) / (2 * A);
                int64_t c[2];
                int k;
                c[0] = (int64_t)t1; c[1] = (int64_t)t2;
                for (k = 0; k < 2; k++) {
                    t = c[k];
                    if (t >= z0 && t <= z1 && t > best) {
                        best = t; got = 1;
                        tone = shade_of(dx + t, dy + t, r * r / (hh ? hh : 1));
                    }
                }
            }
        }
        if (!got) return 0;
        h->t = best;
        h->tone256 = tone;
        h->zq = (int)best;
        return 1;
    }
}

typedef struct Img { int w, h, ox, oy; uint8_t *pix; } Img;

static int rot_prim(const Prim *in, Prim *out, int W)
{
    /* rot 1: x' = y, y' = W - x (front +y becomes +x) */
    *out = *in;
    if (in->kind == FP_BOX) {
        out->x = in->y;
        out->y = W - (in->x + in->w);
        out->w = in->d;
        out->d = in->w;
    } else {
        out->x = in->y;
        out->y = W - in->x;
        if (in->kind == FP_BLOB) { out->w = in->d; out->d = in->w; }
    }
    return 1;
}

static uint8_t buf_c[BH][BW];
static uint8_t buf_m[BH][BW];   /* prim + 1 */
static int32_t buf_z[BH][BW];

static void bake(int fi, int facing, int frame, Img *out)
{
    const Row *r = &rows[fi];
    const Art *a = &arts[fi];
    Prim pr[FG_MAX_PRIMS];
    int np = 0, i, x, y, minx = BW, miny = BH, maxx = -1, maxy = -1;
    int W = r->fw * 16;
    for (i = 0; i < a->nprim; i++) {
        if (a->p[i].frames && !(a->p[i].frames & (1 << frame))) continue;
        if (facing) rot_prim(&a->p[i], &pr[np], W); else pr[np] = a->p[i];
        np++;
    }
    memset(buf_c, 0, sizeof buf_c);
    memset(buf_m, 0, sizeof buf_m);
    for (y = 0; y < BH; y++) {
        for (x = 0; x < BW; x++) {
            /* pixel centre in screen space, Q16 */
            int64_t u = (int64_t)(x - BOX0) * Q + Q / 2, v = (int64_t)(y - BOY0) * Q + Q / 2;
            int64_t X0 = v + u / 2, Y0 = v - u / 2;
            Hit best, h;
            int k, got = 0;
            for (k = 0; k < np; k++)
                if (intersect(&pr[k], X0, Y0, &h) && (!got || h.t > best.t)) { best = h; best.prim = k; got = 1; }
            if (!got) continue;
            {
                uint8_t tn[5];
                int s = best.tone256, tone;
                mat_tones(pr[best.prim].mat, tn);
                if (pr[best.prim].style & 4) s += 90;                       /* glow */
                tone = s > 150 ? 2 : s > 95 ? 1 : 0;
                if ((pr[best.prim].style & FPS_SOFT) && ((x + y) & 1)) {     /* dithered fluff edges */
                    if (s > 135 && s <= 165) tone = 2;
                    else if (s > 80 && s <= 110) tone = 1;
                }
                if ((pr[best.prim].style & FPS_SHINY) && s > 225) tone = 3;
                buf_c[y][x] = tn[tone];
                buf_m[y][x] = (uint8_t)(best.prim + 1);
                buf_z[y][x] = (int32_t)best.zq;
            }
        }
    }
    /* coloured selective outlines: the silhouette, and seams between prims of
     * different materials where the depth jumps */
    {
        static uint8_t outc[BH][BW];
        memset(outc, 0, sizeof outc);
        for (y = 1; y < BH - 1; y++)
            for (x = 1; x < BW - 1; x++) {
                int m = buf_m[y][x], k, edge = 0, seam = 0;
                static const int8_t nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                if (!m) continue;
                for (k = 0; k < 4; k++) {
                    int m2 = buf_m[y + nb[k][1]][x + nb[k][0]];
                    if (!m2) edge = 1;
                    else if (m2 != m && pr[m2 - 1].mat != pr[m - 1].mat &&
                             buf_z[y + nb[k][1]][x + nb[k][0]] - buf_z[y][x] > 3 * Q) seam = 1;
                }
                if (edge || seam) {
                    uint8_t tn[5];
                    mat_tones(pr[m - 1].mat, tn);
                    /* black only where it touches the floor at the bottom */
                    outc[y][x] = (edge && !buf_m[y + 1][x] && buf_z[y][x] < 2 * Q) ? PAL_BLACK : tn[4];
                }
            }
        for (y = 0; y < BH; y++)
            for (x = 0; x < BW; x++)
                if (outc[y][x]) buf_c[y][x] = outc[y][x];
        /* rim light: the first lit pixel inside the top-left silhouette */
        for (y = 1; y < BH - 1; y++)
            for (x = 1; x < BW - 1; x++) {
                int m = buf_m[y][x];
                uint8_t tn[5];
                if (!m || outc[y][x] || !outc[y - 1][x] || buf_m[y - 1][x]) continue;
                if (buf_m[y - 2 < 0 ? 0 : y - 2][x]) continue;
                mat_tones(pr[m - 1].mat, tn);
                if (buf_c[y][x] == tn[2] || buf_c[y][x] == tn[1]) buf_c[y][x] = tn[3];
            }
    }
    /* fixed colours never use the recolour LUT's indices */
    for (y = 0; y < BH; y++)
        for (x = 0; x < BW; x++) {
            int m = buf_m[y][x];
            if (!m) continue;
            if (pr[m - 1].mat >= 0 || pr[m - 1].mat <= -100) buf_c[y][x] = unreserve(buf_c[y][x]);
            else if (!reserved(buf_c[y][x]) && buf_c[y][x] != PAL_BLACK) buf_c[y][x] = unreserve(buf_c[y][x]);
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    if (maxx < 0) { minx = maxx = BOX0; miny = maxy = BOY0; }
    if (minx > BOX0) minx = BOX0;            /* ox, oy stay >= 0 */
    if (miny > BOY0) miny = BOY0;
    out->w = maxx - minx + 1;
    out->h = maxy - miny + 1;
    out->ox = BOX0 - minx;
    out->oy = BOY0 - miny;
    if (out->w > 255 || out->h > 255) { fg_err(r->key, 0, "facing larger than 255 px", NULL); out->w = out->h = 1; }
    out->pix = (uint8_t *)malloc((size_t)out->w * out->h);
    for (y = 0; y < out->h; y++)
        for (x = 0; x < out->w; x++) out->pix[y * out->w + x] = buf_c[miny + y][minx + x];
}

/* span encode: h u16 row offsets, then [skip][len][pixels].. 0xFF per row */
static int span_encode(const Img *im, uint8_t *dst)
{
    int y, n = im->h * 2;
    for (y = 0; y < im->h; y++) {
        const uint8_t *row = im->pix + y * im->w;
        int x = 0;
        dst[y * 2] = (uint8_t)(n & 0xFF);
        dst[y * 2 + 1] = (uint8_t)(n >> 8);
        while (x < im->w) {
            int skip = 0, len = 0;
            while (x + skip < im->w && !row[x + skip]) skip++;
            if (x + skip >= im->w) break;
            while (x + skip + len < im->w && row[x + skip + len] && len < 254) len++;
            dst[n++] = (uint8_t)skip;
            dst[n++] = (uint8_t)len;
            memcpy(dst + n, row + x + skip, (size_t)len);
            n += len;
            x += skip + len;
        }
        dst[n++] = 0xFF;
    }
    return n;
}

/* ---- output -------------------------------------------------------------------- */
#ifndef FURNGEN_LIB
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *s;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    s = (char *)malloc((size_t)n + 1);
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return NULL; }
    s[n] = 0;
    fclose(f);
    return s;
}

static void cstr(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

static void upper_key(const char *k, char *out)
{
    int i;
    for (i = 0; k[i] && i < 39; i++) out[i] = (char)((k[i] >= 'a' && k[i] <= 'z') ? k[i] - 32 : k[i]);
    out[i] = 0;
}

static const char *art_files[] = { "pastel", "story", "gothic", "hellish", "plush", "kitchen", "tech",
                                   "garden", "light", "wall", "rug", "trophy", "seasonal", NULL };

int nat_png_encode(const char *path, const uint8_t *pix, int w, int h, const uint8_t *rgb_palette);

int main(int argc, char **argv)
{
    const char *out_root = NULL, *sheet = NULL, *repo = FURNGEN_REPO;
    int check = 0, strict = 0, i, f, k, ndefault = 0;
    char path[1024];
    char *text;
    static Img imgs[FG_MAX][8];
    static uint8_t data[4 * 1024 * 1024];
    uint32_t off[FG_MAX][8];
    int ndata = 0, nprims = 0;
    FILE *o;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out-root") && i + 1 < argc) out_root = argv[++i];
        else if (!strcmp(argv[i], "--sheet") && i + 1 < argc) sheet = argv[++i];
        else if (!strcmp(argv[i], "--repo") && i + 1 < argc) repo = argv[++i];
        else if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--strict")) strict = 1;
        else { fprintf(stderr, "usage: furngen [--out-root DIR] [--sheet F.png] [--check] [--strict] [--repo DIR]\n"); return 2; }
    }
    if (!out_root && !sheet && !check) out_root = repo;
    snprintf(path, sizeof path, "%s/core/data/furn/catalog.txt", repo);
    text = read_file(path);
    if (!text) { fprintf(stderr, "furngen: cannot read %s\n", path); return 1; }
    parse_catalog(text, "catalog.txt");
    free(text);
    for (k = 0; art_files[k]; k++) {
        char name[64];
        snprintf(path, sizeof path, "%s/core/data/furn/art/%s.txt", repo, art_files[k]);
        snprintf(name, sizeof name, "art/%s.txt", art_files[k]);
        text = read_file(path);
        if (!text) continue;
        parse_art(text, name);
        free(text);
    }
    for (i = 0; i < nrows; i++)
        if (!arts[i].has) {
            if (strict) fg_err("catalog.txt", rows[i].line, "no art for", rows[i].key);
            art_default(i);
            ndefault++;
        }
    if (fg_errors) { fprintf(stderr, "furngen: %d error(s)\n", fg_errors); return 1; }
    /* bake */
    for (i = 0; i < nrows; i++)
        for (f = 0; f < arts[i].frames; f++)
            for (k = 0; k < 2; k++) {
                Img *im = &imgs[i][f * 2 + k];
                bake(i, k, f, im);
                off[i][f * 2 + k] = (uint32_t)ndata;
                ndata += span_encode(im, data + ndata);
            }
    if (fg_errors) return 1;
    if (sheet) {
        /* the pieces with art first, then the placeholder boxes */
        static int order[FG_MAX];
        int no = 0, j, cols = 12, cw = 72, ch = 88, rowsn, W, H, x, y;
        uint8_t *pix, pal[768];
        for (j = 0; j < nrows; j++) if (arts[j].has) order[no++] = j;
        for (j = 0; j < nrows; j++) if (!arts[j].has) order[no++] = j;
        rowsn = (nrows + cols - 1) / cols; W = cols * cw; H = rowsn * ch;
        pix = (uint8_t *)calloc((size_t)W * H, 1);
        for (i = 0; i < 256; i++) palette_rgb((uint8_t)i, &pal[i * 3], &pal[i * 3 + 1], &pal[i * 3 + 2]);
        for (i = 0; i < W * H; i++) pix[i] = PAL_RAMP(RAMP_STONE, 6);
        for (j = 0; j < nrows; j++) {
            int bx = (j % cols) * cw, by = (j / cols) * ch;
            i = order[j];
            for (k = 0; k < 2; k++) {
                Img *im = &imgs[i][k];
                int dx = bx + 2 + k * 35 + (34 - im->w) / 2, dy = by + 84 - im->h;
                for (y = 0; y < im->h; y++)
                    for (x = 0; x < im->w; x++) {
                        int sx = dx + x, sy = dy + y;
                        uint8_t c = im->pix[y * im->w + x];
                        if (c && sx >= bx && sx < bx + cw && sy >= by && sy < by + ch) pix[sy * W + sx] = c;
                    }
            }
            if (!arts[i].has) for (x = 0; x < 4; x++) pix[(by + 1) * W + bx + 1 + x] = PAL_UI_RED;
        }
        nat_png_encode(sheet, pix, W, H, pal);
        free(pix);
    }
    fprintf(stderr, "furngen: %d pieces (%d placeholder boxes), %d bytes of iso sprites\n", nrows, ndefault, ndata);
    if (!out_root) return 0;

    /* ---- furn_ids.h ---- */
    snprintf(path, sizeof path, "%s/core/gen/furn_ids.h", out_root);
    o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "/* GENERATED by core/tools/furngen.c from core/data/furn/catalog.txt - do not edit.\n"
               " * Furniture ids are saved: they only ever grow (catalog.txt is append-only). */\n"
               "#ifndef DOOMO_GEN_FURN_IDS_H\n#define DOOMO_GEN_FURN_IDS_H\n\n");
    for (i = 0; i < nrows; i++) {
        char up[40];
        upper_key(rows[i].key, up);
        fprintf(o, "#define FURN_%-26s %3d", up, i);
        if (rows[i].reward[0]) fprintf(o, "   /* %s */", rows[i].reward);
        fputc('\n', o);
    }
    fprintf(o, "\n#define FURN_GEN_COUNT %d\n\n#endif\n", nrows);
    fclose(o);

    /* ---- furn_data.c ---- */
    snprintf(path, sizeof path, "%s/core/gen/furn_data.c", out_root);
    o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "/* GENERATED by core/tools/furngen.c from core/data/furn/catalog.txt and\n"
               " * core/data/furn/art/ - do not edit. The catalog, the strings and the PS1\n"
               " * recipes (the `home` overlay group on the PS1, SPEC C3). */\n"
               "#include \"doomo/furn.h\"\n\n");
    fprintf(o, "const DoomoFurnPrim doomo_furn_prims[] = {\n");
    for (i = 0; i < nrows; i++)
        for (k = 0; k < arts[i].nprim; k++) {
            const Prim *p = &arts[i].p[k];
            int mat = p->mat == MAT_A ? FM_A : p->mat == MAT_B ? FM_B :
                      p->mat <= -100 ? PAL_RAMP(-100 - p->mat, 4) : PAL_PAINT(p->mat, 1);
            int w = p->kind == FP_BOX ? p->w : p->w * 2, d = p->kind == FP_BOX ? p->d : p->kind == FP_BLOB ? p->d * 2 : 0;
            int h = p->kind == FP_BLOB ? p->h * 2 : p->h;
            fprintf(o, "    { %d, %d, %d, %d, %d, %d, %d, %d },%s\n",
                    p->kind | ((p->style & 3) << 2) | (p->frames << 4), mat, p->x, p->y, p->z,
                    w > 255 ? 255 : w, d > 255 ? 255 : d, h > 255 ? 255 : h, k == 0 ? "" : "");
        }
    fprintf(o, "};\n\nconst DoomoFurn doomo_furn[%d] = {\n", nrows);
    for (i = 0; i < nrows; i++) {
        const Row *r = &rows[i];
        fprintf(o, "    { ");
        cstr(o, r->name);
        fprintf(o, ", ");
        cstr(o, r->desc);
        fprintf(o, ", %d, 0x%04X, 0x%04X, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },\n",
                r->price, r->unlock, r->giver, r->cat, r->flags, r->flags2, r->fi, r->fw, r->fd, r->fh,
                r->pa, r->pb, r->cute, r->spooky, r->comfy, r->need, r->seat, r->light, r->theme, r->mass,
                arts[i].nprim, nprims, arts[i].frames, r->role);
        nprims += arts[i].nprim;
    }
    fprintf(o, "};\nconst uint16_t doomo_furn_count = %d;\n", nrows);
    fclose(o);

    /* ---- furn_grant.c ---- */
    snprintf(path, sizeof path, "%s/core/gen/furn_grant.c", out_root);
    o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "/* GENERATED by core/tools/furngen.c - do not edit. Resident on the PS1\n"
               " * (SPEC C3): the pieces an unlock grants, and which pieces are capped at one. */\n"
               "#include \"doomo/furn.h\"\n\nconst DoomoFurnGrant doomo_furn_grants[] = {\n");
    for (i = 0; i < nrows; i++)
        if (rows[i].unlock != 0xFFFF) {
            fprintf(o, "    { %d, %d, ", rows[i].unlock, i);
            cstr(o, rows[i].name);
            fprintf(o, " },\n");
        }
    fprintf(o, "    { 0, 0, 0 }\n};\n\nconst uint8_t doomo_furn_cap1[FURN_MAX / 8] = {\n   ");
    for (k = 0; k < FURN_MAX / 8; k++) {
        int b = 0, j;
        for (j = 0; j < 8; j++) {
            int id = k * 8 + j;
            if (id < nrows && (rows[id].flags2 & (FF_UNSELLABLE | FF_GIFT | FF_PORTAL))) b |= 1 << j;
        }
        fprintf(o, " 0x%02X,%s", b, (k % 12) == 11 ? "\n   " : "");
    }
    fprintf(o, "\n};\n");
    fclose(o);

    /* ---- furn_iso.c ---- */
    snprintf(path, sizeof path, "%s/core/gen/furn_iso.c", out_root);
    o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "/* GENERATED by core/tools/furngen.c - do not edit. The span-encoded iso\n"
               " * facings (furn.h) for the GBA and the native reference renderer; the PS1\n"
               " * draws the recipes and links none of this (SPEC C3). %d bytes. */\n"
               "#include \"doomo/furn.h\"\n\n#if !defined(DOOMO_PLATFORM_PS1)\n", ndata);
    {
        int nimg = 0;
        fprintf(o, "const uint16_t doomo_furn_iso_first[%d] = {", nrows);
        for (i = 0; i < nrows; i++) {
            fprintf(o, "%s%d,", (i % 16) ? " " : "\n    ", nimg);
            nimg += arts[i].frames * 2;
        }
        fprintf(o, "\n};\nconst uint16_t doomo_furn_iso_count = %d;\nconst DoomoFurnIso doomo_furn_iso[%d] = {\n", nimg, nimg);
        for (i = 0; i < nrows; i++)
            for (k = 0; k < arts[i].frames * 2; k++)
                fprintf(o, "    { %d, %d, %d, %d, %u },\n", imgs[i][k].w, imgs[i][k].h, imgs[i][k].ox, imgs[i][k].oy,
                        (unsigned)off[i][k]);
        fprintf(o, "};\nconst uint8_t doomo_furn_iso_data[%d] = {", ndata);
        for (i = 0; i < ndata; i++) fprintf(o, "%s%d,", (i % 24) ? "" : "\n", data[i]);
        fprintf(o, "\n};\n");
    }
    fprintf(o, "#else\ntypedef int doomo_furn_iso_not_on_ps1;\n#endif\n");
    fclose(o);
    for (i = 0; i < nrows; i++) for (k = 0; k < arts[i].frames * 2; k++) free(imgs[i][k].pix);
    return 0;
}
#endif /* FURNGEN_LIB */
