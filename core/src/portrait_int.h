/* portrait_int.h - internals of the procedural resident renderer
 * (owner: portrait). Not part of the public contract.
 *
 * Coordinates handed to the raster primitives are in 1/16 pixel units
 * ("Q4"), so every slider step moves something by a sub-pixel amount and
 * still lands on a different pixel somewhere. Feature code works in
 * normalised head / body space (Q8: 256 = one radius) and converts with
 * the PHead / PBody helpers.
 *
 * Rendering model (all integer, no allocation, bounded cost):
 *   1. parts are filled back to front; every solid part gets a new part
 *      id in a per-pixel id buffer, decals keep the id underneath;
 *   2. pc_contours(): where a part sits in front of another, the back
 *      part gets a 1px darker line in its own colour family (inner lines
 *      + soft ambient occlusion);
 *   3. pc_outline(): a 1px coloured selective outline around the
 *      silhouette (dark shade of the local colour, lighter on the lit
 *      top-left, darkest underneath);
 *   4. effects (auras, glows, tears) are drawn last without outlines.
 */
#ifndef DOOMO_PORTRAIT_INT_H
#define DOOMO_PORTRAIT_INT_H

#include "doomo/portrait.h"
#include "doomo/palette.h"
#include "doomo/catalog.h"

#define PQ        16                    /* sub-pixel units per pixel */
#define PX(p)     ((p) * PQ)            /* pixels -> Q4               */
#define PMAXW     48
#define PMAXH     80

/* ---- materials -------------------------------------------------------- */
/* c[0] deep shadow, c[1] shadow, c[2] base, c[3] light, c[4] highlight;
 * line = silhouette outline colour, cont = (unused, reserved). */
typedef struct PMat { uint8_t c[5]; uint8_t line; } PMat;

void pm_from3(PMat *m, uint8_t dark, uint8_t base, uint8_t light);
void pm_paint(PMat *m, int paint);
void pm_ramp(PMat *m, int ramp, int base_shade);   /* base_shade 1..6 */
void pm_skin(PMat *m, const DoomoLook *l);
void pm_flat(PMat *m, uint8_t c);                  /* one colour, dark line */
uint8_t pm_line_of(uint8_t c);                     /* outline colour for c  */

/* ---- shading modes ---------------------------------------------------- */
enum {
    SH_BALL,        /* soft sphere lit from the top-left (+ rim light)       */
    SH_FLAT,        /* base colour only                                      */
    SH_VGRAD,       /* vertical gradient dark (top) -> light (bottom), eyes  */
    SH_GLOSS,       /* sphere with a bigger specular (slime, metal, gems)    */
    SH_SOFT,        /* sphere, lighter overall (pastel cloth / fluff)        */
    SH_HGRAD,       /* vertical gradient light (top) -> dark (bottom)        */
    SH_COUNT
};

/* ---- part flags ------------------------------------------------------- */
enum {
    PF_NOLINE = 1,  /* no silhouette outline around this part               */
    PF_NOCAST = 2,  /* does not cast an inner contour onto parts behind it  */
    PF_NOCONT = 4,  /* never receives inner contours                        */
    PF_FUZZ   = 8,  /* soft 50% fur dither on the shading band edges        */
    PF_DARKLINE = 16 /* contours drawn two steps darker (strong inner line) */
};

#define PID_FX      254     /* effect pixels: no outline, no contour       */
#define PID_LINE    255     /* outline pixels written by pc_outline()      */

typedef struct PCanvas {
    uint8_t *px;            /* output colours                              */
    int w, h;
    uint8_t id[PMAXW * PMAXH];
    uint8_t part;           /* id of the part being drawn                  */
    uint8_t decal;          /* 1: draw without touching ids                */
    uint8_t shade;          /* SH_*                                        */
    uint8_t flags;
    PMat m;
    uint8_t pline[256];     /* outline colour per part id                  */
    uint8_t pflag[256];
    /* shading frame (Q4 centre, Q8-per-Q4 inverse radii) */
    int fon, fcx, fcy, fiu, fiv;
    /* pattern fill */
    const uint8_t *pat;
    PMat pm;
    /* mask: draw only on pixels whose id bit is set */
    int masked;
    uint32_t mask[8];
    /* pixel clip rectangle (inclusive x0,y0 / exclusive x1,y1) */
    int x0, y0, x1, y1;
    /* gradient band shift (-2..2) applied to every shaded pixel */
    int bias;
    uint32_t ops;           /* rough pixel-visit counter (perf checks)     */
} PCanvas;

extern PCanvas pcv;

void pc_begin(uint8_t *out, int w, int h);
void pc_part(const PMat *m, int shade, int flags);   /* new solid part   */
void pc_decal(const PMat *m, int shade);             /* keep ids         */
void pc_same(const PMat *m, int shade);              /* same id, new mat */
void pc_frame(int cx, int cy, int rx, int ry);       /* shading frame    */
void pc_noframe(void);
void pc_clip(int x0, int y0, int x1, int y1);        /* pixels           */
void pc_noclip(void);
void pc_mask_none(void);
void pc_mask_add(int id);
void pc_mask_range(int lo, int hi);
void pc_pattern(const uint8_t *tile, const PMat *pm);   /* NULL = off */

/* primitives (Q4 coordinates) */
void pc_ellipse(int cx, int cy, int rx, int ry);
void pc_ellipse_sh(int cx, int cy, int rx, int ry, int shear);  /* y += x*shear/256 */
void pc_tri(int x0, int y0, int x1, int y1, int x2, int y2);
void pc_quad(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
void pc_capsule(int x0, int y0, int x1, int y1, int r0, int r1);
void pc_bezier(int x0, int y0, int x1, int y1, int x2, int y2, int r0, int r1, int segs);
void pc_rrect(int cx, int cy, int hw, int hh, int r);
void pc_rect(int x0, int y0, int x1, int y1);        /* Q4 box           */

/* pixel-space helpers (direct colour, keep ids unless noted) */
void pc_px(int x, int y, uint8_t c);
void pc_pxs(int x, int y, uint8_t c);                /* only on opaque   */
void pc_line(int x0, int y0, int x1, int y1, uint8_t c);     /* pixels  */
void pc_lineq(int x0, int y0, int x1, int y1, uint8_t c);    /* Q4 ends */
void pc_fx(int x, int y, uint8_t c);                 /* effect pixel     */
int  pc_get(int x, int y);                           /* colour or 0      */
int  pc_idat(int x, int y);
void pc_darken_part(int id);                         /* whole part 1 step */

void pc_contours(void);
void pc_outline(void);
void pc_flipx(void);

/* ---- math helpers ------------------------------------------------------ */
int  p_isqrt(uint32_t v);
int  p_abs(int v);
/* sin/cos of a 256-step angle, Q8 (256 = 1.0) */
int  p_sin(int a);
int  p_cos(int a);
uint32_t p_hash(uint32_t x);

/* ---- pattern tiles (8x8: 0 = base, 1 = pattern colour, 2 = pattern dark) */
extern const uint8_t p_pattern_tiles[PATTERN_COUNT][64];

/* ---- shared render context -------------------------------------------- */
enum { LOD_TINY = 0, LOD_MID = 1, LOD_BIG = 2 };

/* yaw: 0 front, 1 front 3/4 (facing viewer's left), 2 left profile,
 * 3 back 3/4, 4 back. Right-facing views are drawn left and mirrored. */
typedef struct PHead {
    int cx, cy, rx, ry;     /* Q4                                      */
    int yaw;
    int lod;
    int cosv, sinv;         /* Q8 yaw rotation                         */
} PHead;

/* Everything a part painter needs to know about the resident. */
typedef struct PCtx {
    const DoomoLook *l;
    int status, expr, anim;
    int blink, breath, twitch, wag, fxf;     /* animation phases        */
    int creepy;          /* 0 normal, 1 possessed, 2 soul lost         */
    int ghost;           /* RES_GHOST or SPECIES_GHOST                 */
    int dead;            /* RES_DEAD or EXPR_DEAD                      */
    int critter;         /* furry species                              */
    PMat skin, skin2, hair, hairhi, eye, horn, mark, top, bot, shoe, hat,
         acc, patm;
    uint8_t outline_bias;
    PHead h;
    /* body layout (Q4) */
    int bx, by;          /* torso centre                               */
    int bw, bh;          /* torso half width / half height             */
    int ground;          /* feet y                                     */
    int byaw;            /* body view (same scheme as head yaw)        */
    int view_kind;       /* VK_* below                                 */
    int step;            /* walk phase -1, 0, +1                        */
    int legs;            /* leg length (Q4)                            */
    int arm_pose;        /* AP_*                                       */
    int armlx, armly, armrx, armry;  /* hand positions (Q4) if posed   */
    int head_id;         /* part id of the head                        */
    int body_id;         /* part id of the torso                       */
} PCtx;

enum { VK_PORTRAIT, VK_FIGURE, VK_FIGHTER, VK_CHIBI };
enum { AP_REST, AP_PAWS, AP_HOLD, AP_WAVE, AP_UP };

void pctx_init(PCtx *c, const DoomoLook *l, int status, int expr, int anim);
void phead_set(PHead *h, int cx, int cy, int rx, int ry, int yaw, int lod);
/* project a normalised head-surface point (front surface if front=1,
 * back surface if -1, silhouette if 0); returns z' (Q8) visibility */
int  ph_proj(const PHead *h, int nx, int ny, int front, int *ox, int *oy);
#define HXS(h, n) ((n) * (h)->rx / 256)
#define HYS(h, n) ((n) * (h)->ry / 256)

/* head & face (portrait_head.c) */
typedef struct PHSpace { PCtx *c; PHead *h; int s, z; } PHSpace;
extern PHSpace phs;
int  hx(int n);
int  hy(int n);
int  hr(int n);
int  hry(int n);
void hE(int x, int y, int rx, int ry);
void hT(int ax, int ay, int bx, int by, int cx, int cy);
void hQ(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy);
void hC(int ax, int ay, int bx, int by, int r0, int r1);
void hB(int ax, int ay, int bx, int by, int cx, int cy, int r0, int r1, int segs);
void hL(int ax, int ay, int bx, int by, uint8_t c);
void hP(int x, int y, uint8_t c);
void hFrame(int x, int y, int rx, int ry);
void hHeadFrame(int grow);
int  ph_is_critter(int species);
void ph_head_base(PCtx *c);      /* head silhouette (+ fur tufts)            */
void ph_face(PCtx *c);           /* species marks, eyes, brows, nose, mouth  */
int  ph_eye_geom(PCtx *c, int s, int *X, int *Y, int *erx, int *ery);
void ph_species_over(PCtx *c);   /* species bits drawn over hair / hats      */
void ph_face_over(PCtx *c);      /* third eye + forehead marks over the hair */
/* hair, ears, horns, hats, face accessories, halo (portrait_hair.c) */
void ph_hair_back(PCtx *c);      /* long hair behind the head                */
void ph_hair_front(PCtx *c);     /* cap + bangs in front of the head         */
void ph_ears(PCtx *c, int front);/* front=0: behind head, 1: in front        */
void ph_horns(PCtx *c);
void ph_hat(PCtx *c);
void ph_face_acc(PCtx *c);
void ph_halo(PCtx *c);
int  ph_top_extent(const DoomoLook *l);   /* how far up things reach (Q8) */
int  species_has_muzzle(int sp);
void ph_heart(int cx, int cy, int r, uint8_t c, uint8_t hi);
void ph_star(int cx, int cy, int r, uint8_t c);
/* body (portrait_body.c) */
void pb_layout(PCtx *c, int view_kind, int byaw, int step, int anim);
void pb_draw_behind(PCtx *c);    /* wings, tail, cape, long hair behind body */
void pb_draw_body(PCtx *c);      /* legs, torso, outfit, arms                */
void pb_draw_front(PCtx *c);     /* held paws, teddy, front accessories      */
void pb_draw_tail(PCtx *c, int front);
void pb_draw_wings(PCtx *c, int front);
/* effects (portrait_fx.c) */
void pf_aura_back(PCtx *c);
void pf_aura_front(PCtx *c);
void pf_status(PCtx *c);         /* possessed veins, ghost tint, glitch     */
void pf_expression_fx(PCtx *c);  /* tears, sweat, zzz, anger marks, hearts  */
void pf_recolor_ghost(void);
void pf_recolor_creepy(int level);
/* weapons (portrait_gun.c) */
void pg_draw_weapon(PCtx *c, int wframe, int view, int anim, int hx, int hy, int front);
void pg_draw_flash(int anim);
/* body layout queries (portrait_body.c) */
int  pb_head_y(PCtx *c, int ry);
int  pb_torso_top(void);
void pb_shift(int dy);
void pb_set_neck(int y);
void pb_draw_neck(PCtx *c);  /* chest fluff, ruffs, neckwear (after head) */
int  pb_ground(void);
int  pb_nolegs(void);
void pb_hand(int s, int *x, int *y);
void pb_set_hand(int s, int x, int y);

#endif
