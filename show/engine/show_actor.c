/* show_actor.c - the show's actors (WP6; SHOW_ENGINE_SPEC.md sec.4.3, 8.2,
 * 8.4, 9.2, 9.3).
 *
 * An actor is one character drawn by the game's portrait engine: a bust
 * (portrait_image, 48x56), a figure (figure_image, 48x80), a chibi
 * (bodyspr_image, 16x24, 4 dirs x 4 walk frames) or a fighter (fighter_image,
 * 32x48, 5 views). Its look sits in doomo_town.res[look] (show_cast.c).
 *
 * Clocks. Actor clock = beat_clock(clock) + phase: beat.c holds it under
 * FREEZE / STILL and hit-stop and doubles it under PANIC, so `freeze mayor`
 * for 13 f stops the blink, the breath and the wings for exactly 13 f. The
 * clock drives the portrait cache through portrait_cache_clock(clock, 6)
 * before each fetch (sec.8.2: every phase whole on the frame it is due; the
 * patch hunk (f) removes the per-tick budget two actors on one clock value
 * would otherwise share, AT6.5).
 *
 * Channels per frame: expression (op or EXPR track), mouth (MOUTH track of
 * visemes, mapped through the actor's vis[] onto MOUTH_* styles and handed
 * to the engine in portrait_show_mouth, hunks (b)/(b')/(d)) and blink (BLINK
 * track or the blink op, portrait_show_blink, hunk (c)). The overrides are
 * set around each actor's fetch and reset to -1 after it, so no other
 * image ever sees them.
 *
 * Motion and acting run on the shot frame: move, walk (chibi walk frames,
 * a 1 px figure bob), pop (a tween.h table: scale, squash pair or offset),
 * squash (height Q8 held, width = 65536 / height), headturn (quarter turns:
 * a chibi changes direction like the game's NACT_HEAD_TURN; a bust or figure
 * narrows and mirrors through the turn), face, state, flipbook poses.
 *
 * The player calls show_actor_draw() with draw = 0 during a seek's pre-roll:
 * the images are still fetched (and so rendered into the cache) exactly as in
 * a full run.
 */
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/beat.h"
#include "doomo/tween.h"
#include "doomo/town.h"
#include "show/show_actor.h"

int8_t portrait_show_mouth = -1;  /* the patch hunks read these (sec.4.3)     */
int8_t portrait_show_blink = -1;

#define MAX_FLIPS       32
#define MAX_FLIP_FRAMES 160
#define MAX_TRACKS      64

typedef struct Act {
    ShowActorRec r;
    uint8_t  on, hidden, walking, started;
    int8_t   expr, face, status;
    uint8_t  turn;                   /* net quarter turns 0..3              */
    int32_t  x_q8, y_q8;
    /* move / walk */
    int32_t  mx0, my0, mx1, my1;
    int16_t  mt0, mdur;
    uint8_t  mease, mwalk;
    /* blink op */
    int16_t  blink_until;
    /* pop */
    int8_t   pop_tw;
    int16_t  pop_t0;
    /* squash */
    int16_t  sq_from, sq_to, sq_t0, sq_dur;
    /* headturn */
    uint8_t  turn_from, turn_to;
    int16_t  turn_t0, turn_dur;
    /* pose */
    int8_t   pose;
    int16_t  pose_t0, pose_speed;
    ShowActorState st;
} Act;

static Act s_act[SHOW_MAX_ACTORS];
static int s_nact;
static int s_t;

/* tracks */
typedef struct Trk {
    const ShowTrackHdr *h;
    const uint8_t *runs;
    uint16_t cur, cur_t0;            /* cursor: run index and its first frame */
} Trk;
static Trk s_trk[MAX_TRACKS];
static int s_ntrk;

/* flipbooks */
typedef struct Flip {
    const ShowFlipHdr *h;
    const uint8_t *hold;
    uint16_t first;                  /* index into s_fimg                   */
    uint16_t total;                  /* frames of one pass (sum of holds)   */
} Flip;
static Flip s_flip[MAX_FLIPS];
static int s_nflip;
static DoomoImage s_fimg[MAX_FLIP_FRAMES];
static int s_nfimg;

/* the spec's sec.9.3 defaults, used when a record carries no viseme map */
static const uint8_t k_vis_default[VIS_COUNT] = {
    SHOW_NO_MOUTH, MOUTH_FLAT, MOUTH_TEETH, MOUTH_GRIN, MOUTH_OPEN, MOUTH_O, MOUTH_POUT, MOUTH_TEETH
};

static const uint8_t k_size[SHA_KIND_COUNT][2] = {
    { PORTRAIT_W, PORTRAIT_H }, { FIGURE_W, FIGURE_H }, { BODYSPR_W, BODYSPR_H }, { FIGHTER_W, FIGHTER_H }
};

/* ---- load -------------------------------------------------------------------- */
void show_actor_reset(void)
{
    memset(s_act, 0, sizeof(s_act));
    s_nact = 0;
    s_ntrk = 0;
    s_nflip = 0;
    s_nfimg = 0;
    s_t = 0;
    portrait_show_mouth = -1;
    portrait_show_blink = -1;
}

int show_actor_load(const ShowActorRec *recs, int n)
{
    int i;
    if (!recs || n < 0) n = 0;
    if (n > SHOW_MAX_ACTORS) n = SHOW_MAX_ACTORS;
    for (i = 0; i < n; i++) {
        Act *a = &s_act[i];
        int k;
        memset(a, 0, sizeof(*a));
        a->r = recs[i];
        if (a->r.kind >= SHA_KIND_COUNT) a->r.kind = SHA_FIGURE;
        if (a->r.px < 1) a->r.px = 1;
        if (a->r.px > 4) a->r.px = 4;
        if (a->r.clock >= BEAT_ACTORS) a->r.clock = 0xFF;
        if (a->r.vis[VIS_X] != SHOW_NO_MOUTH) {      /* no map given: the defaults */
            int any = 0;
            for (k = 0; k < VIS_COUNT; k++) any |= a->r.vis[k];
            if (!any) memcpy(a->r.vis, k_vis_default, sizeof(a->r.vis));
        }
        a->on = 1;
        a->hidden = (a->r.flags & SHAF_HIDDEN) ? 1 : 0;
        a->expr = (int8_t)(a->r.expr < EXPR_COUNT ? a->r.expr : EXPR_NEUTRAL);
        a->face = (int8_t)(a->r.face & 3);
        a->status = (int8_t)(a->r.status ? a->r.status : RES_ALIVE);
        a->x_q8 = (int32_t)a->r.x * 256;
        a->y_q8 = (int32_t)a->r.y * 256;
        a->pose = -1;
        a->pop_tw = -1;
        a->sq_from = a->sq_to = 256;
        show_cast_set_status(a->r.look, a->status);
        a->st.pose = -1;
    }
    s_nact = n;
    return n;
}

int show_actor_count(void) { return s_nact; }

int show_actor_tracks(const uint8_t *blob, uint32_t bytes)
{
    uint32_t off = 0;
    s_ntrk = 0;
    while (blob && off + sizeof(ShowTrackHdr) <= bytes && s_ntrk < MAX_TRACKS) {
        const ShowTrackHdr *h = (const ShowTrackHdr *)(blob + off);
        uint32_t len = sizeof(ShowTrackHdr) + (uint32_t)h->nruns * 2u;
        if (off + len > bytes) break;
        if (h->actor < SHOW_MAX_ACTORS) {
            Trk *k = &s_trk[s_ntrk++];
            k->h = h;
            k->runs = blob + off + sizeof(ShowTrackHdr);
            k->cur = 0;
            k->cur_t0 = h->start;
        }
        off += (len + 3u) & ~3u;
    }
    return s_ntrk;
}

int show_actor_flips(const uint8_t *blob, uint32_t bytes)
{
    uint32_t off = 0;
    s_nflip = 0;
    s_nfimg = 0;
    while (blob && off + sizeof(ShowFlipHdr) <= bytes && s_nflip < MAX_FLIPS) {
        const ShowFlipHdr *h = (const ShowFlipHdr *)(blob + off);
        uint32_t holds = ((uint32_t)h->n + 3u) & ~3u;
        uint32_t pix = (uint32_t)h->w * h->h;
        uint32_t len = sizeof(ShowFlipHdr) + holds + ((pix * h->n + 3u) & ~3u);
        Flip *f;
        int i;
        if (!h->n || off + len > bytes || s_nfimg + h->n > MAX_FLIP_FRAMES) break;
        f = &s_flip[s_nflip++];
        f->h = h;
        f->hold = blob + off + sizeof(ShowFlipHdr);
        f->first = (uint16_t)s_nfimg;
        f->total = 0;
        for (i = 0; i < h->n; i++) {
            DoomoImage *im = &s_fimg[s_nfimg++];
            im->w = h->w;
            im->h = h->h;
            im->pix = f->hold + holds + pix * (uint32_t)i;
            im->uid = doomo_image_new_uid();
            f->total = (uint16_t)(f->total + (f->hold[i] ? f->hold[i] : 1));
        }
        off += len;
    }
    return s_nflip;
}

/* ---- ops --------------------------------------------------------------------- */
static int cur_x(const Act *a) { return (int)(a->x_q8 >> 8); }

void show_actor_op(int i, int op, int a, int b, int c, int d)
{
    Act *ac;
    if (i < 0 || i >= s_nact) return;
    ac = &s_act[i];
    switch (op) {
    case SHOP_EXPR:
        if (a >= 0 && a < EXPR_COUNT) ac->expr = (int8_t)a;
        break;
    case SHOP_BLINK:
        ac->blink_until = (int16_t)(s_t + (a > 0 ? a : RUNG_R3));
        break;
    case SHOP_FACE:
        ac->face = (int8_t)(a & 3);
        break;
    case SHOP_MOVE:
    case SHOP_WALK:
        ac->mx0 = ac->x_q8;
        ac->my0 = ac->y_q8;
        ac->mx1 = (int32_t)a * 256;
        ac->my1 = op == SHOP_WALK ? ac->y_q8 : (int32_t)b * 256;
        ac->mt0 = (int16_t)s_t;
        ac->mdur = (int16_t)(c > 0 ? c : 1);
        ac->mease = (uint8_t)(op == SHOP_WALK ? SHE_LINEAR : d);
        ac->mwalk = op == SHOP_WALK;
        if (op == SHOP_WALK && a != cur_x(ac))
            ac->face = (int8_t)(a < cur_x(ac) ? SHFACE_L : SHFACE_R);
        break;
    case SHOP_POP:
        ac->pop_tw = (int8_t)(a >= 0 && a < TWT_COUNT ? a : -1);
        ac->pop_t0 = (int16_t)s_t;
        break;
    case SHOP_SQUASH: {
        int cur = ac->sq_to;
        if (ac->sq_dur > 0 && s_t - ac->sq_t0 < ac->sq_dur)
            cur = tw_lerp(ac->sq_from, ac->sq_to, tw_smooth(tw_t(s_t - ac->sq_t0, ac->sq_dur)));
        ac->sq_from = (int16_t)cur;
        ac->sq_to = (int16_t)DOOMO_CLAMP(a, 64, 640);
        ac->sq_t0 = (int16_t)s_t;
        ac->sq_dur = (int16_t)(c > 0 ? c : 1);
        break; }
    case SHOP_HEADTURN:
        ac->turn_from = ac->turn;
        ac->turn_to = (uint8_t)((ac->turn + ((a & 0xFF) + 32) / 64) & 3);
        ac->turn_t0 = (int16_t)s_t;
        ac->turn_dur = (int16_t)(c > 0 ? c : 1);
        ac->turn = ac->turn_to;
        break;
    case SHOP_POSE:
        ac->pose = (int8_t)(a >= 0 && a < s_nflip ? a : -1);
        ac->pose_t0 = (int16_t)s_t;
        ac->pose_speed = (int16_t)(b > 0 ? b : 256);
        break;
    case SHOP_STATE:
        ac->status = (int8_t)(a >= RES_ALIVE && a <= RES_GHOST ? a : RES_ALIVE);
        show_cast_set_status(ac->r.look, ac->status);
        break;
    case SHOP_SHOW:
        ac->hidden = a ? 0 : 1;
        break;
    case SHOP_TWITCH:                 /* deferred (DEFERRED.md): POSSESSED twitches */
    default:
        break;
    }
}

/* ---- per frame --------------------------------------------------------------- */
static int ease(int e, fix16 u)
{
    switch (e) {
    case SHE_SMOOTH: return tw_smooth(u);
    case SHE_IN:     return tw_in2(u);
    case SHE_OUT:    return tw_out2(u);
    case SHE_BOUNCE: return tw_bounce(u);
    default:         return u;
    }
}

/* value of a track at frame t: SHOW_TRACK_KEEP outside it */
static int track_at(Trk *k, int t)
{
    const ShowTrackHdr *h = k->h;
    if (t < h->start || t >= h->start + h->frames) return SHOW_TRACK_KEEP;
    if (t < k->cur_t0) { k->cur = 0; k->cur_t0 = h->start; }   /* rewound (tests) */
    while (k->cur < h->nruns) {
        int len = k->runs[k->cur * 2] ? k->runs[k->cur * 2] : 1;
        if (t < k->cur_t0 + len) return k->runs[k->cur * 2 + 1];
        k->cur_t0 = (uint16_t)(k->cur_t0 + len);
        k->cur++;
    }
    return SHOW_TRACK_KEEP;
}

/* a pop table's effect at `age`: scale x/y Q8 and offsets (px at scale 1) */
static void pop_fx(int tw, int age, int *sx, int *sy, int *dx, int *dy)
{
    const TwTab *t = tw_table(tw);
    int v;
    *sx = *sy = 256;
    *dx = *dy = 0;
    if (!t || age >= t->n) return;
    v = tw_tab(t, age);
    if (tw <= TWT_SLAM_STICK2) { *sx = *sy = v; return; }       /* uniform scale */
    if (tw >= TWT_DROP_TITLE_SX && tw <= TWT_DROP_SOFT_DY) {    /* sx / sy / dy triple */
        int g = TWT_DROP_TITLE_SX + ((tw - TWT_DROP_TITLE_SX) / 3) * 3;
        *sx = tw_tab(tw_table(g), age);
        *sy = tw_tab(tw_table(g + 1), age);
        *dy = tw_tab(tw_table(g + 2), age);
        return;
    }
    switch (tw) {
    case TWT_HEART_LAND_SQ: *sx = 256 + v; *sy = 256 - (v * 9) / 10; break;
    case TWT_CLUNK: case TWT_CLAP: *sy = v; *sx = 512 - v; break;
    case TWT_BRAKE_DX: *dx = v; break;
    default: *dy = v; break;                                   /* the dy tables */
    }
}

static int pop_len(int tw)
{
    const TwTab *t = tw_table(tw);
    return t ? t->n : 0;
}

void show_actor_tick(int t)
{
    int i;
    s_t = t;
    for (i = 0; i < s_nact; i++) {
        Act *a = &s_act[i];
        ShowActorState *st = &a->st;
        int clk, sx = 256, sy = 256, dx = 0, dy = 0, flip, k;
        int expr = a->expr, mouth = -1, blink = -1, vis = SHOW_TRACK_KEEP;
        if (!a->on) continue;
        /* idle mode at frame 0 (beat_start cleared every clock mode) */
        if (!a->started) {
            a->started = 1;
            if (a->r.clock < BEAT_ACTORS) {
                if (a->r.idle == SHI_STILL) beat_set_actor_clock(a->r.clock, BCLK_STILL);
                if (a->r.idle == SHI_FROZEN) beat_set_actor_clock(a->r.clock, BCLK_FROZEN);
            }
        }
        /* the clock */
        if (a->r.idle == SHI_NONE) clk = 0;
        else if (a->r.clock < BEAT_ACTORS) clk = beat_clock(a->r.clock);
        else clk = t;
        st->clock = clk + a->r.phase;
        /* motion */
        a->walking = 0;
        if (a->mdur > 0) {
            int age = t - a->mt0;
            fix16 u = ease(a->mease, tw_t(age, a->mdur));
            a->x_q8 = tw_lerp(a->mx0, a->mx1, u);
            a->y_q8 = tw_lerp(a->my0, a->my1, u);
            if (age < a->mdur) a->walking = a->mwalk;
            else a->mdur = 0;
        }
        /* tracks: the last track that covers t wins */
        for (k = 0; k < s_ntrk; k++) {
            Trk *tk = &s_trk[k];
            int v;
            if (tk->h->actor != i) continue;
            v = track_at(tk, t);
            if (v == SHOW_TRACK_KEEP) continue;
            if (tk->h->chan == SHT_MOUTH) vis = v;
            else if (tk->h->chan == SHT_EXPR && v < EXPR_COUNT) expr = v;
            else if (tk->h->chan == SHT_BLINK) blink = v ? 1 : 0;
        }
        if (blink < 0 && t < a->blink_until) blink = 1;
        if (vis != SHOW_TRACK_KEEP && vis < VIS_COUNT) {
            if (a->r.kind == SHA_CHIBI) {          /* chibis flap A / X only (sec.9.3) */
                if (vis != VIS_X && vis != VIS_M)
                    mouth = a->r.vis[VIS_A] != SHOW_NO_MOUTH ? a->r.vis[VIS_A] : MOUTH_OPEN;
            } else if (a->r.vis[vis] != SHOW_NO_MOUTH) {
                mouth = a->r.vis[vis];
            }
        }
        /* acting: pop, squash, headturn */
        if (a->pop_tw >= 0) {
            int age = t - a->pop_t0;
            if (age >= pop_len(a->pop_tw)) a->pop_tw = -1;
            else pop_fx(a->pop_tw, age, &sx, &sy, &dx, &dy);
        }
        {
            int q = a->sq_to;
            if (a->sq_dur > 0 && t - a->sq_t0 < a->sq_dur)
                q = tw_lerp(a->sq_from, a->sq_to, tw_smooth(tw_t(t - a->sq_t0, a->sq_dur)));
            if (q != 256) {
                int qx = (int)(65536 / (q > 0 ? q : 1));
                sy = sy * q / 256;
                sx = sx * qx / 256;
            }
        }
        flip = a->face == SHFACE_L;
        st->dir = (int8_t)(a->face == SHFACE_L ? 1 : a->face == SHFACE_R ? 2 : a->face == SHFACE_BACK ? 3 : 0);
        {
            int turn = a->turn, age = t - a->turn_t0;
            if (a->turn_dur > 0 && age < a->turn_dur) {
                if (age * 2 + 1 < a->turn_dur) turn = a->turn_from;     /* mirrors mid-turn */
                if (a->r.kind != SHA_CHIBI && a->turn_from != a->turn_to) {
                    /* narrow to 62.5 % at the middle of the turn and back */
                    int p = ((2 * age + 1) * 256) / a->turn_dur - 256;
                    int dip = 256 - (p < 0 ? -p : p);
                    sx = sx * (256 - (dip * 96) / 256) / 256;
                }
            }
            if (a->r.kind == SHA_CHIBI || a->r.kind == SHA_FIGHTER) {
                static const uint8_t cw[4] = { 1, 3, 0, 2 };   /* down->left->up->right */
                int d = st->dir, j;
                for (j = 0; j < (turn & 3); j++) d = cw[d];
                st->dir = (int8_t)d;
                flip = 0;
            } else if (turn & 1) {
                flip = !flip;
            }
        }
        if (a->walking && a->r.kind == SHA_FIGURE) dy -= ((t - a->mt0) >> 2) & 1;   /* the step bob */
        st->frame = (int8_t)((a->walking && (a->r.kind == SHA_CHIBI || a->r.kind == SHA_FIGHTER))
                             ? ((clk >> 2) & 3) : 0);
        /* pose */
        st->pose = -1;
        if (a->pose >= 0) {
            const Flip *f = &s_flip[a->pose];
            int age = (int)(((int32_t)(t - a->pose_t0) * a->pose_speed) >> 8), j;
            if (f->h->flags & SHFL_LOOP) age %= f->total;
            for (j = 0; j < f->h->n; j++) {
                int hld = f->hold[j] ? f->hold[j] : 1;
                if (age < hld) break;
                age -= hld;
            }
            if (j >= f->h->n) j = f->h->n - 1;
            st->pose = (int8_t)j;
        }
        st->x = (int16_t)(a->x_q8 >> 8);
        st->y = (int16_t)(a->y_q8 >> 8);
        st->x = (int16_t)(st->x + dx * a->r.px);
        st->dy = (int16_t)(dy * a->r.px);
        st->sx = (int16_t)(flip ? -sx : sx);
        st->sy = (int16_t)sy;
        st->expr = (int8_t)expr;
        st->mouth = (int8_t)mouth;
        st->blink = (int8_t)blink;
    }
}

static const DoomoImage *fetch(Act *a)
{
    const ShowActorState *st = &a->st;
    const DoomoImage *im = NULL;
    int rid = a->r.look;
    portrait_cache_clock((uint32_t)st->clock, 6);
    portrait_show_mouth = st->mouth;
    portrait_show_blink = st->blink;
    switch (a->r.kind) {
    case SHA_BUST:
        im = portrait_image(rid, st->expr);
        break;
    case SHA_FIGURE:
        im = figure_image(&doomo_town.res[rid].look, show_cast_status(rid), st->expr);
        break;
    case SHA_CHIBI:
        im = bodyspr_image(rid, a->r.view && a->r.view <= 4 ? a->r.view - 1 : st->dir, st->frame);
        break;
    case SHA_FIGHTER: {
        /* views 0 back, 2 left, 4 front; the right side mirrors the left */
        static const uint8_t view_of_dir[4] = { 4, 2, 2, 0 };
        int view = a->r.view && a->r.view <= 5 ? a->r.view - 1 : view_of_dir[st->dir & 3];
        im = fighter_image(rid, view, st->frame, a->r.wframe == 0xFF ? -1 : a->r.wframe);
        break; }
    default: break;
    }
    portrait_show_mouth = -1;
    portrait_show_blink = -1;
    return im;
}

static void blit(const DoomoImage *im, int x, int y, int px, int sx, int sy)
{
    if (!im) return;
    if (sx == 256 && sy == 256)
        plat_image(x - (im->w * px) / 2, y - im->h * px, im, px, 0, 0);
    else if (sx == -256 && sy == 256)
        plat_image(x - (im->w * px) / 2, y - im->h * px, im, px, IMG_FLIPX, 0);
    else
        plat_image_xform(x, y, im, sx * px, sy * px, 0, 0);
}

void show_actor_draw(int ox, int oy, int draw)
{
    int i;
    for (i = 0; i < s_nact; i++) {
        Act *a = &s_act[i];
        ShowActorState *st = &a->st;
        const DoomoImage *im;
        int x, y, px = a->r.px;
        if (!a->on) continue;
        im = fetch(a);                        /* also while hidden or pre-rolling */
        st->uid = im ? im->uid : 0;
        if (!draw || a->hidden) continue;
        x = ox + st->x;
        y = oy + st->y + st->dy;
        if (a->r.kind == SHA_CHIBI || a->r.kind == SHA_FIGHTER) {
            /* the right-facing fighter is its left view mirrored */
            int mir = a->r.kind == SHA_FIGHTER && st->dir == 2;
            int sx = st->sx < 0 ? -st->sx : st->sx;
            if (mir) sx = -sx;
            if (st->pose < 0 || s_flip[a->pose].h->mode == SHFM_OVER) blit(im, x, y, px, sx, st->sy);
        } else if (st->pose < 0 || s_flip[a->pose].h->mode == SHFM_OVER) {
            blit(im, x, y, px, st->sx, st->sy);
        }
        if (st->pose >= 0) {
            const Flip *f = &s_flip[a->pose];
            const DoomoImage *fi = &s_fimg[f->first + st->pose];
            int fdx = f->h->dx * px, fdy = f->h->dy * px;
            if (st->sx < 0) fdx = -fdx;
            blit(fi, x + fdx, y + fdy, px, st->sx, st->sy);
        }
    }
    portrait_cache_clock((uint32_t)s_t, 6);   /* the shot clock for whoever fetches next */
}

const ShowActorState *show_actor_state(int i)
{
    if (i < 0 || i >= s_nact) return NULL;
    return &s_act[i].st;
}

/* FNV-1a over every channel (never the uid: uids depend on what earlier
 * shots allocated, and a seek starts from boot) */
uint32_t show_actor_hash(uint32_t h)
{
    int i;
    for (i = 0; i < s_nact; i++) {
        const ShowActorState *st = &s_act[i].st;
        int32_t v[13];
        int j;
        v[0] = st->x; v[1] = st->y; v[2] = st->dy; v[3] = st->sx; v[4] = st->sy;
        v[5] = st->clock; v[6] = st->expr; v[7] = st->mouth; v[8] = st->blink;
        v[9] = st->dir; v[10] = st->frame; v[11] = st->pose; v[12] = s_act[i].hidden;
        for (j = 0; j < 13; j++) { h ^= (uint32_t)v[j]; h *= 16777619u; }
    }
    return h;
}
