/* loadfade.c - a short fade over a loading freeze (SP5, G-LD; plat_ext.h plat_loading; owner: gba;
 * docs/GBA.md "LD: loading fades"), and (S2-10) a live screen over a save, a load and a town build (plat_ext.h
 * plat_busy / plat_busy_step; docs/GBA.md "Speed wave 2: S2-10").
 *
 * A scene change (scene.c) or a story map load calls plat_loading(): the next frame may take long (a scene's
 * enter builds its caches, a map is unpacked, the island's sim starts; 0.2-2 s on the GBA). Until then the last
 * frame stays on screen, which reads as a hang. So from the call until the next flip, the VBlank IRQ watches the
 * clock: once LD_LATE vblanks (150 ms, more than the slowest normal frame: combat, 100-160 ms) have passed without a flip, it darkens the screen with the hardware fade
 * (BLDCNT brightness decrease on every layer, BLDY) by LD_STEP a vblank, to black in about 130 ms more. The flip
 * that ends the wait shows the new picture at that darkness and lifts it by LD_IN a flip (about 130 ms). A load
 * shorter than LD_LATE shows nothing at all. Registers only: no pixel of any page changes, logic and timing are
 * the same (the core does not know), and a frame with its own blend (postfx.c) keeps the darker of the two.
 *
 * S2-10: a save (10.7 s for a Classic island), a load (6 s) and a New story / new island build (2 s) run inside one
 * tick: the codec and the build rebuild every citizen's base record, 5-6 ms each. The core calls plat_busy(kind)
 * first and plat_busy_step(done, total) once per citizen (save.c, town.c, sim.c), every 32 relationships, once per
 * pass of rel_normalize's sort and (save_flash.c) every flash sector, so the main thread gets control every few ms. plat_busy arms the same IRQ watch
 * with a shorter fuse (BUSY_LATE vblanks) and a target: a save dims the picture to half (BLDY 8), a load or a build
 * fades it to black. The first plat_busy_step after the fuse paints a small card ("Saving..." / "Loading...") into
 * the Mode 4 page on screen (vid_rect / vid_text, the resident IWRAM primitives that draw into gba_back, which points
 * at that page for the calls); WIN0 keeps the card out of the fade and OBJs off it. From then on the IRQ turns the
 * card's spinner one step every BUSY_EVERY vblanks with a few raw pixel writes (no call, no DMA, no overlay code: it
 * runs inside whatever the main thread was doing) and counts each step as a presented frame in the harness mailbox
 * (`drawn`), so the screen shows 30 pictures a second to the end of the wait, whatever code runs meanwhile (a flash
 * sector, a scene's enter). The flip that ends the wait lifts the fade as before and, when a card was painted, tells
 * the page trackers that neither page holds a kept picture (pn / hold / stage, as a tile frame does), because the
 * page that had the card is drawn into next. A frame being built over several ticks (I10: hold_building) or a tile
 * frame on screen (Mode 0) gets the fade only. Nothing happens while the harness hashes every frame (`draw_all`):
 * those runs see the same frames as before S2-10.
 *
 * The VBlank IRQ (irq.iwram.c) calls gba_vbl_hook while it is set: this file's THUMB code in ROM, set only from
 * plat_loading() / plat_busy() to the end of the wait, so the IRQ costs one load and test otherwise.
 * RAM: 20 bytes (S2-10: +8). */
#include "gba_plat.h"
#include "canvas_int.h"
#include "video_int.h"
#include "doomo/plat_ext.h"
#include "doomo/palette.h"

#define LD_LATE 9           /* vblanks (150 ms) without a flip before the fade starts: past any normal slow frame */
#define LD_STEP 2           /* BLDY per vblank while it fades out (0..16) */
#define LD_IN   4           /* BLDY per flip while the new picture fades in */
#define LD_BLDCNT 0x00FFu   /* brightness decrease, first target BG0-3 + OBJ + backdrop */
#define BUSY_LATE 3         /* S2-10: vblanks (50 ms) into a save / load / build before its picture changes: a
                               fast one (an empty island, a story save) shows nothing */
#define BUSY_DIM  8         /* a save dims the screen to half (a load or a build: black) */
#define BUSY_EVERY 2        /* vblanks per picture: 30 a second */
#define CARD_W 112
#define CARD_H 22
#define CARD_X ((SCREEN_W - CARD_W) / 2)
#define CARD_Y ((SCREEN_H - CARD_H) / 2)
#define SPIN_N 8            /* spinner dots */

void (*volatile gba_vbl_hook)(void);
static volatile uint32_t ld_v0;      /* gba_vblanks at plat_loading() / plat_busy() */
static volatile uint8_t ld_level;    /* darkness on screen now, 0..16 */
static volatile uint8_t ld_kind;     /* S2-10: PLAT_BUSY_* (0: a scene change) */
static volatile uint8_t ld_card;     /* 1 once the card is painted on the page on screen: the IRQ turns its spinner */
static volatile uint8_t ld_spin;     /* the lit spinner dot */
static uint8_t ld_dirty;             /* a card was painted since the last flip: the pages lost their kept pictures */
static volatile uint8_t ld_seen;     /* the fade level at the last counted picture */
static volatile uint32_t ld_pic;     /* gba_vblanks at the last counted picture */

/* the spinner's dots: a ring of 8 around (SPIN_CX, SPIN_CY), 2x2 each (x stays even: one halfword a row) */
static const int8_t spin_dx[SPIN_N] = { 0, 4, 6, 4, 0, -4, -6, -4 };
static const int8_t spin_dy[SPIN_N] = { -6, -4, 0, 4, 6, 4, 0, -4 };
#define SPIN_CX (CARD_X + CARD_W - 12)
#define SPIN_CY (CARD_Y + CARD_H / 2 - 1)

/* a dot with two halfword stores a row, straight into the Mode 4 page on screen (IRQ-safe: no call) */
static void spin_put(int i, uint32_t color)
{
    volatile uint16_t *p = (volatile uint16_t *)((REG_DISPCNT & DCNT_PAGE) ? VRAM_PAGE1 : VRAM_PAGE0) +
                           ((SPIN_CY + spin_dy[i]) * SCREEN_W + SPIN_CX + spin_dx[i]) / 2;
    uint16_t v = (uint16_t)(color | color << 8);
    p[0] = v;
    p[SCREEN_W / 2] = v;
}

static void ld_vblank(void)
{
    uint32_t late = ld_kind ? BUSY_LATE : LD_LATE;
    int target = ld_kind == PLAT_BUSY_SAVE ? BUSY_DIM : 16;
    if (gba_vblanks - ld_v0 < late) return;
    if (ld_level < target) ld_level = (uint8_t)DOOMO_MIN(target, ld_level + LD_STEP);
    REG_BLDCNT = LD_BLDCNT;
    REG_BLDY = ld_level;
    if (ld_kind && gba_vblanks - ld_pic >= BUSY_EVERY) {         /* S2-10: a picture every BUSY_EVERY vblanks */
        int changed = ld_level != ld_seen;
        ld_seen = ld_level;
        if (ld_card) {
            int i = ld_spin;
            spin_put(i, PAL_UI_PINK_D);
            i = (i + 1) & (SPIN_N - 1);
            spin_put(i, PAL_WHITE);
            ld_spin = (uint8_t)i;
            changed = 1;
        }
        if (changed) { ld_pic = gba_vblanks; gba_mailbox.drawn++; }
    }
}

static void arm(int kind)
{
    if (gba_mailbox.draw_all) return;            /* the harness hashes every frame (vhash-seq): never touch them */
    if (gba_vbl_hook == ld_vblank && ld_kind && !kind) return;   /* a scene change inside a save keeps its card */
    gba_vbl_hook = 0;
    ld_kind = (uint8_t)kind;
    ld_card = 0;
    ld_spin = 0;
    ld_seen = 0;
    ld_v0 = gba_vblanks;
    ld_pic = gba_vblanks;
    gba_vbl_hook = ld_vblank;
}

void plat_loading(void) { arm(0); }

/* S2-10 (plat_ext.h): a save, a load or a town build runs in this tick */
void plat_busy(int kind) { arm(kind); }

/* ---- the card (painted once by the main thread, from plat_busy_step) ------------------------------------- */
static void spin_dot(int i, uint32_t color)
{
    int x = SPIN_CX + spin_dx[i], y = SPIN_CY + spin_dy[i];
    vid_rect(x, y, x + 2, y + 2, color);
}

/* the page on screen as the target of vid_*: 0 when it is not a Mode 4 page (a tile frame is up) */
static int on_shown_page(uint16_t **was)
{
    uint16_t dc = REG_DISPCNT;
    if ((dc & 7u) != 4u) return 0;
    *was = gba_back;
    gba_back = (uint16_t *)((dc & DCNT_PAGE) ? VRAM_PAGE1 : VRAM_PAGE0);
    return 1;
}

static void card_paint(void)
{
    uint16_t *was;
    int i;
    if (!on_shown_page(&was)) return;
    vid_rect(CARD_X, CARD_Y, CARD_X + CARD_W, CARD_Y + CARD_H, PAL_UI_PINK_D);
    vid_rect(CARD_X + 1, CARD_Y + 1, CARD_X + CARD_W - 1, CARD_Y + CARD_H - 1, PAL_UI_PINK);
    vid_rect(CARD_X + 2, CARD_Y + CARD_H - 3, CARD_X + CARD_W - 2, CARD_Y + CARD_H - 2, PAL_UI_PINK_D);
    vid_text(CARD_X + 8, CARD_Y + 7, ld_kind == PLAT_BUSY_SAVE ? "Saving..." : "Loading...", PAL_WHITE);
    for (i = 0; i < SPIN_N; i++) spin_dot(i, i == ld_spin ? PAL_WHITE : PAL_UI_PINK_D);
    gba_back = was;
    /* WIN0 = the card: no fade and no OBJ inside it, everything (faded) outside */
    REG_WIN0H = (uint16_t)(CARD_X << 8 | (CARD_X + CARD_W));
    REG_WIN0V = (uint16_t)(CARD_Y << 8 | (CARD_Y + CARD_H));
    REG_WININ = 0x0004;                          /* BG2 only */
    REG_WINOUT = 0x003F;                         /* every layer + the fade */
    REG_BG2PA = 0x100; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 0x100;   /* no squash under the card */
    REG_BG2X = 0; REG_BG2Y = 0;
    REG_DISPCNT = (uint16_t)(REG_DISPCNT | DCNT_WIN0);
    ld_dirty = 1;
    ld_card = 1;                                 /* last: from here the IRQ turns the spinner */
}

/* plat_ext.h: called by the codec, the build and the flash code every few ms. Once the watch is late, the first
 * call paints the card; the IRQ does the rest (see the top). */
void plat_busy_step(int done, int total)
{
    (void)done; (void)total;
    if (!ld_kind || ld_card || gba_vbl_hook != ld_vblank) return;
    if (gba_vblanks - ld_v0 < BUSY_LATE || hold_building()) return;   /* I10 draws into the shown page: fade only */
    card_paint();
}

/* video_flip, before the page trackers hear of the flip: 1 when a card was painted since the last flip (the
 * page it is on lost its kept picture); clears it */
int ld_take_dirty(void)
{
    int d = ld_dirty;
    ld_dirty = 0;
    return d;
}

/* postfx_vblank, in the vblank of a flip, with the frame's own BLDCNT / BLDY about to be written: the wait is
 * over; the new picture comes in from the darkness reached, keeping a darker blend of its own */
void ld_flip(int *bldcnt, int *bldy)
{
    gba_vbl_hook = 0;
    if (ld_card) {
        REG_DISPCNT = (uint16_t)(REG_DISPCNT & ~DCNT_WIN0);
        ld_card = 0;
    }
    ld_kind = 0;
    if (!ld_level) return;
    if ((*bldcnt & 0x00C0) != 0x00C0 || *bldy < ld_level) {   /* none, white, or lighter than ours: ours */
        *bldcnt = (int)LD_BLDCNT;
        *bldy = ld_level;
    }
    ld_level = ld_level > LD_IN ? (uint8_t)(ld_level - LD_IN) : 0;
}
