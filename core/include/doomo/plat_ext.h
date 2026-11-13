/* plat_ext.h - the platform hooks added after platform.h was frozen
 * (owner: K0 contract, LIFESIM_PLAN R3; add-only after K0).
 *
 * platform.h stays as it is: every NEW thing the core asks of a platform
 * is declared here instead, with a weak default in core/src/plat_weak_ext.c,
 * so GBA, PS1, native and Dreamcast keep linking while each port fills in
 * the hooks it supports. A port overrides a hook with a strong definition
 * in its own tree; it never edits plat_weak_ext.c.
 *
 * The defaults say "this platform has none of it": no feature bits, no
 * config storage, no held frame, no content pack, no test flags. Code that
 * calls a hook must behave correctly with the default (it is what the GBA
 * and PS1 builds run until their package lands).
 *
 * Who fills what (LIFESIM_PLAN):
 *   plat_features            U0 (native --features), U1g (GBA), U1p (PS1),
 *                            DC ports (X0 bits)
 *   plat_config_read/write   U0 (native file), U1g (GBA flash sector),
 *                            U1p (PS1 memory card file)
 *   plat_option_changed      U0 calls it from opt_set; U8 (GBA, for G7's
 *                            OPT_3D_FAST), DC display packages implement it
 *   plat_rumble              X2 (DC Puru Puru); camfx_shake calls it
 *   plat_test_flags          U0/U5 (native runner, harnesses)
 *   plat_hold_frame          U4 (held-frame pause backdrop)
 *   plat_strip               SP3 G-I11 (GBA: the island hub's toast band)
 *   plat_scene_scratch       SP3 P-I1 (PS1: the overlay slot's unused tail)
 *   plat_hold_band           SP5 (held dialogue backdrop: GBA, native)
 *   plat_loading             SP5 (loading fades: GBA, PS1)
 *   plat_stage_*             SP7 (a kept backdrop: the GBA's ROM-baked
 *                            exorcism church; native for tests)
 *   plat_pack_blob/read      E0 declares the pack format (pack.h, which
 *                            includes this header); E8 (GBA ROM blob),
 *                            E9 (PS1 disc file), E2 (native loader)
 *   plat_dots, plat_hshapes  SP4 (GBA: the pet's LCD and egg in ARM passes)
 *   plat_view_hold           S2-2 (GBA: the 3D view held under a card)
 */
#ifndef DOOMO_PLAT_EXT_H
#define DOOMO_PLAT_EXT_H

#include "types.h"

/* ---- features ------------------------------------------------------------
 * What this platform can do beyond platform.h, as a bit mask. Option rows,
 * menu entries and content that depend on a feature check its bit; nothing
 * may assume a platform by name. Bits are append-only.
 * There is ONE 3D quality option: G7's OPT_3D_FAST (town.h, id < 64).
 * U0 moves its row test (today fp_options.c's
 * plat_name()[0] == 'G') to PLATF_3D_QUALITY and may mirror the value into
 * the machine config record; no package adds a second 3D quality OPT_*. */
#define PLATF_VMU_LCD     (1u << 0)   /* a second 48x32 1bpp status screen (DC VMU) */
#define PLATF_RUMBLE      (1u << 1)   /* a rumble motor (DC Puru Puru, PS1 DualShock) */
#define PLATF_POINTER     (1u << 2)   /* a mouse / pointer                          */
#define PLATF_KEYBOARD    (1u << 3)   /* a keyboard for text entry                  */
#define PLATF_MULTIPAD    (1u << 4)   /* more than one pad (party modes)            */
#define PLATF_EXCLUSIVE   (1u << 5)   /* platform-exclusive content (DC side area)  */
#define PLATF_3D_QUALITY  (1u << 6)   /* G7's "3D: Faster / Better" row (OPT_3D_FAST) means something */
#define PLATF_DC_DISPLAY  (1u << 7)   /* DC video / picture / HD text option rows   */
#define PLATF_CONFIG      (1u << 8)   /* plat_config_read/write persist (machine settings) */
/* (T0, docs/TALK.md) Talk, the live house conversation: PS1 and Dreamcast
 * (and native for tests). The GBA never sets it and links no Talk code. */
#define PLATF_TALK        (1u << 9)
#define PLATF_ALL_KNOWN   0x3FFu
uint32_t plat_features(void);

/* ---- machine config record -----------------------------------------------
 * One small record of machine-wide settings (language, volumes, OPT_3D_FAST,
 * Mature...; its layout belongs to U0's options_store.c) kept outside the
 * save slots: a GBA flash sector, a PS1 memory card file, a native file.
 * read: copies at most `cap` bytes into dst and returns the number of
 *       bytes read, 0 when there is no record (the default) or it cannot
 *       be read. The caller validates the bytes (magic, CRC).
 * write: stores `len` bytes (len <= PLAT_CONFIG_MAX); 1 ok, 0 failed or
 *       not supported (the default).
 * 48 B: U0's 32 B settings record + 16 B for the adults' security code
 * (parental PIN, U5 / E11; life_contract.h CONFIG_SZ_*). A GBA flash sector
 * (4 KB) and a PS1 card block (8 KB) both hold it with room to spare. */
#define PLAT_CONFIG_MAX 48u
int  plat_config_read(uint8_t *dst, uint32_t cap);
int  plat_config_write(const uint8_t *src, uint32_t len);

/* ---- option side effects ---------------------------------------------------
 * opt_set (U0) tells the platform about the options a platform applies
 * itself (OPT_3D_FAST, the DC display rows, vibration). `opt` is an
 * OPT_* id (town.h), `value` its new value. Default: nothing. */
void plat_option_changed(int opt, int value);

/* ---- rumble ------------------------------------------------------------------
 * A short motor pulse (PLATF_RUMBLE platforms): strength 0..255, length in
 * 30 Hz ticks. camfx_shake() calls it with the shake's own strength and
 * length, already scaled by OPT_SCREEN_SHAKE, so "Screen shake: Off" also
 * silences the motor (a separate option arrives with U0). It may be called
 * every tick (a held gun); a platform without a motor ignores it.
 * Default: nothing. */
void plat_rumble(int strength, int ticks);

/* ---- test flags ------------------------------------------------------------
 * Set by a test harness (native runner, GBA/PS1/DC harnesses) to switch off
 * interactive steps that would block a scripted run. Default 0 (a real
 * console). Bits are append-only. */
#define PLAT_TEST_NO_PICKER (1u << 0)   /* skip U5's first-boot language picker */
#define PLAT_TEST_NO_TIPS   (1u << 1)   /* (CL4) no first-time tip cards (tips.h) unless a script says "tips on" */
uint32_t plat_test_flags(void);

/* ---- held frame (U4) -------------------------------------------------------
 * plat_hold_frame(1) right after the frame to keep was presented: the
 * platform keeps showing it as the backdrop and the core then repaints only
 * its own rects on top. Returns 1 when the frame is held, 0 when the
 * platform cannot (the default; the caller then redraws its backdrop every
 * frame as today). plat_hold_frame(0) releases it (also implied by any
 * scene_set). */
int  plat_hold_frame(int on);

/* ---- a frame built over several ticks (I10, docs/ISLAND_SPEED.md) --------------
 * Only while a frame is held (plat_hold_frame returned 1 and plat_hold_frame(4) still does): the shown page keeps
 * showing the held picture while the core draws a NEW picture into the other page over several ticks, so no tick
 * has to draw it all. A tick in the build presents the shown page again (no flip); the tick that ends the build
 * (plat_hold_frame(0), which every other end of a hold implies too) flips as usual.
 *   PLAT_BUILD_START   begin (or go on): returns 1 when it can, 0 when it cannot (nothing held, a skipped tick,
 *                      a platform without the call: the default); the core then draws the picture in one go
 *   PLAT_BUILD_SHOWN   the next 2D calls draw into the SHOWN page (the news strip, which keeps moving), with
 *                      retained panels off; only valid inside a build, and only early in the tick (before the
 *                      beam reaches what is drawn)
 *   PLAT_BUILD_HIDDEN  back to the hidden page (the build's target)
 *   PLAT_BUILD_BUSY    (S2-11) asked after plat_room(): 1 when the platform is building that picture over several
 *                      ticks on its own (the GBA house's view turn) and this tick's frame will not be presented:
 *                      the old picture stays on screen, and the caller skips whatever it would draw over the room
 *                      on this tick, as on a skipped tick (plat_draw_skipped); 0 otherwise (the default) */
enum { PLAT_BUILD_START = 1, PLAT_BUILD_SHOWN = 2, PLAT_BUILD_HIDDEN = 3, PLAT_BUILD_BUSY = 4 };
int  plat_build_frame(int op);

/* ---- a dot-matrix screen in one call (SP4, G-P1: the pet's LCD) -------------
 * Draw a dw x dh grid of dots at (x, y), each dot an s x s square: `dots`
 * holds one value (0..7) a dot, row by row; value v fills its square with
 * fill[v], and when s >= 3 the square's last column and last row are
 * edge[v] (an LCD's dot gaps). Those are exactly the pixels of one
 * plat_rect(x + i*s, y + j*s, s, s, fill[v]) a dot followed, when s >= 3,
 * by its last row and column in edge[v]. Returns 1 when drawn (or when the
 * frame is not drawn at all), 0 when the platform cannot (the default):
 * the caller then draws the same pixels with plat_rect. */
int  plat_dots(int x, int y, int s, const uint8_t *dots, int dw, int dh,
               const uint8_t fill[8], const uint8_t edge[8]);

/* ---- round shapes in one call (SP4: the pet's egg shell) -------------------
 * n shapes painted in order, each row by row: shape s covers, on every
 * row y from s.cy - s.top to s.cy + s.bot, the pixels cx - hw .. cx + hw
 * (inclusive) with hw = ht[cy - y] above cy and hb[y - cy] from cy down:
 * exactly one plat_rect(cx - hw, y, 2 * hw + 1, 1, c) a row on the screen.
 * Returns 1 when drawn, 0 when the platform cannot (the default; the GBA
 * off a plain Mode 4 frame): the caller then draws the rows itself. */
typedef struct DoomoHShape {
    int16_t cx, cy, top, bot;           /* centre; rows above and below it */
    const uint8_t *ht, *hb;             /* half widths: ht[0..top], hb[0..bot] */
    uint8_t c, pad[3];
} DoomoHShape;
int  plat_hshapes(const DoomoHShape *s, int n);

/* ---- a clean copy of a band of rows (SP3 / G-I11, docs/ISLAND_SPEED.md I11) -----------------------------------
 * A scene that holds its frame (plat_hold_frame) and has something small move over it (the island hub's toast
 * sliding in and out) keeps a clean copy of the rows that thing can touch, so a held tick puts the band back and
 * draws only the moving thing instead of the whole frame.
 *   PLAT_STRIP_SAVE     copy rows [y, y + h) of the frame being drawn, as drawn so far, into the platform's band
 *                       store (the GBA: the two Mode 4 page gaps, so h * 240 <= 4,800 B: PLAT_STRIP_MAX_H rows)
 *   PLAT_STRIP_RESTORE  copy the store back into the same rows of the frame being drawn
 * Returns 1 when done, 0 when the platform cannot (the default; a skipped tick; h too big; RESTORE without a SAVE
 * of the same rows). The store is valid only until another scene draws (the core uses it only while its own
 * hold lasts, which any scene change ends). */
enum { PLAT_STRIP_SAVE = 1, PLAT_STRIP_RESTORE = 2 };
#define PLAT_STRIP_MAX_H 20
int  plat_strip(int op, int y, int h);

/* ---- scene scratch (SP3 / P-I1, docs/ISLAND_SPEED.md I12) ---------------------------------------------------------
 * RAM that belongs to the running scene's code only: on the PS1, the part of the scene overlay slot after the
 * group that is loaded (the island group leaves ~70 KB of it unused). plat_scene_scratch(bytes, &gen) returns
 * `bytes` bytes of it (4-aligned) or NULL when there is no such room (the default: other platforms keep their
 * tables in BSS). What is written there survives only while *gen keeps the value this call returned: any overlay
 * load (another scene's code, even for one call) changes it, so the caller must call again every frame before it
 * uses the memory and start over when gen changed. Only callable from the scene's own code. */
void *plat_scene_scratch(uint32_t bytes, uint32_t *gen);

/* ---- a held picture under an overlay band (SP5, G-D1; docs/GBA.md "D: the held dialogue backdrop") ---------
 * For an overlay that redraws a band at the bottom of the screen over a picture that does not change (the
 * dialogue box over the story overworld): the platform keeps a clean copy of the picture under the band, so a
 * held frame puts it back and the overlay draws on top as on a full frame (its drop shadows are not shaded twice).
 * The band is rows [y, screen height) without the core rect [cx0, cx1) x [cy0, cy1), which the overlay promises
 * to cover with opaque pixels on every frame (a platform may save more of it: a narrower core is always safe).
 *   PLAT_BAND_SAVE     on a frame being drawn (not a skipped tick), after the picture and before the overlay:
 *                      keep the band. y_need is the top row the overlay needs; the platform may save from as high
 *                      as y_want (<= y_need) when it has the room. Returns the top row it saved, or -1 (it
 *                      cannot: no room, a frame kind it cannot read back, the default)
 *   PLAT_BAND_RESTORE  on a held frame (plat_hold_frame(2) made both pages carry the shown picture): put the
 *                      band back into the picture being drawn. 1 = done; 0 = the copy was lost (the caller then
 *                      drops the hold and draws in full)
 *   PLAT_BAND_DROP     forget the copy (its memory goes back to its owner; on the GBA the overworld's lit-tile
 *                      cache, which rebuilds). Always 0
 * The core holds only while nothing in the picture would change (docs/GBA.md); everything else is the core's. */
enum { PLAT_BAND_DROP = 0, PLAT_BAND_SAVE = 1, PLAT_BAND_RESTORE = 2 };
typedef struct { int16_t y_need, y_want, cx0, cy0, cx1, cy1; } PlatBand;
int  plat_hold_band(int op, const PlatBand *band);

/* ---- a short fade over a loading freeze (SP5, G-LD; docs/GBA.md "LD", docs/PS1.md "Loading fades") ---------
 * Called when the scene stack changes (scene.c) and when the story overworld loads a map: the next frame may be
 * late (a scene's enter, an overlay read from the disc, a map unpacked). A platform may darken the picture that
 * stays on screen meanwhile once it is clearly late, so a load reads as a quick fade instead of a stuck frame,
 * and bring the next picture in from there. Draw-side only: no logic, timing or saved state sees it.
 * Default: nothing. */
void plat_loading(void);

/* ---- a kept backdrop and a retained stage (SP7, docs/EXORCISM_SPEED.md) ----------------
 * A scene whose picture is a full-screen BACKDROP that depends only on a small key (the
 * exorcism's church: its dread level and its slow animation steps), with LAYERS drawn over
 * it, can let the platform keep the backdrop (backdrop.h lists the families and their keys):
 *   plat_stage_begin(id, key, draw, info)  the first call of the scene's draw. This frame's
 *       backdrop is what draw(key) paints over the whole screen; id is its family
 *       (DOOMO_BACKDROP_*). Returns 1 when the platform holds that backdrop (the GBA bakes
 *       every key into the ROM at build time with this same `draw`; native keeps a copy it
 *       drew with it when nat_stage_on is set) and can put any box of it back with
 *       plat_stage_restore(); `info` then says what the buffer this frame is drawn into
 *       holds: `held`, the serial of the frame whose picture it holds (0: unknown, or drawn
 *       over by something that was not a stage frame since), `serial`, this frame's serial,
 *       and the box [dx0, dx1) x [dy0, dy1) that 2D calls after that frame's
 *       plat_stage_end() covered (dialogue, toasts, hints, fades; empty when dx0 >= dx1).
 *       Returns 0 (the default) when it cannot: the core draws the backdrop and every layer
 *       itself, as before.
 *   plat_stage_restore(x, y, w, h)  put that box of the backdrop back into the picture.
 *   plat_stage_end()  the stage's own drawing is done for this frame: from here until the
 *       frame is presented the platform records the box of every 2D call (the next frame
 *       drawn into this buffer gets it as info.dirt).
 * Serials count the frames a platform presents; a skipped tick has none. Weak defaults
 * (core/src/backdrop.c): begin returns 0, restore and end do nothing. */
typedef struct PlatStageInfo {
    uint32_t held, serial;
    int16_t  dx0, dy0, dx1, dy1;
} PlatStageInfo;
int  plat_stage_begin(int id, uint32_t key, void (*draw)(uint32_t key), PlatStageInfo *info);
void plat_stage_restore(int x, int y, int w, int h);
void plat_stage_end(void);

/* ---- content packs (E0 pack.h, E2, E7-E10) -----------------------------------
 * blob: a pack that is directly addressable (the GBA mature ROM variant's
 *       baked blob, a native file loaded whole): its bytes and size, or
 *       NULL when there is none (the default; size is then set to 0).
 * read: a pack that streams (PS1/DC disc file): copies `len` bytes at byte
 *       offset `off` of the pack into dst and returns the number of bytes
 *       copied; 0 when there is no pack (the default) or on error.
 * The pack's format and validation live in pack.h / pack.c; these hooks
 * only move bytes. */
const uint8_t *plat_pack_blob(uint32_t *size);
uint32_t plat_pack_read(uint32_t off, void *dst, uint32_t len);
/* (E2) A scratch buffer for the mslot scene to decode a content pack's still into (PACK_CG_W x
 * PACK_CG_H bytes at most; `cap` is set to its size). The default is NULL and cap 0, so LZ and
 * streamed art is skipped and nothing is reserved. */
uint8_t *plat_mslot_buf(uint32_t *cap);
/* (E9) The buffer a streamed pack's slot script is read into (4-aligned, PACK_SCRIPT_CAP_PS1 bytes
 * on the PS1). The default is NULL and cap 0: a streamed pack then has no scripts and every slot
 * plays the fade-to-black. */
uint8_t *plat_mslot_script_buf(uint32_t *cap);

/* ---- Talk (T0, docs/TALK.md; weak defaults in core/src/weak_talk.c and,
 * for plat_room_project, core/src/room_ref.c) ------------------------------
 * load: make TALK_<code>.BIN ("en", "es", ...) readable through
 *       plat_talk_read and return its size in bytes; 0 when there is none
 *       (the default); PLAT_TALK_LOADING while a platform still stages it
 *       over several updates (PS1: CD -> SPU RAM, TP1): call it again on a
 *       later update. Loading another language replaces the first.
 * read: copy `len` bytes at byte offset `off` of the loaded file into dst;
 *       returns the number of bytes copied (0 on error, nothing loaded).
 *       Reads are small (a row, a string): the PS1 serves them by SpuRead.
 * Native: core/build/talk/TALK_<code>.BIN (make -C core talk), or the
 * directory in $DOOMO_TALK_DIR. */
#define PLAT_TALK_LOADING 0xFFFFFFFFu
uint32_t plat_talk_load(const char *code);
uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len);
/* Where a point of the room is on the screen this frame: tile space x, y
 * (16.16, the DoomoRoomActor convention), height z in px, to screen
 * pixels. Talk anchors its speech bubbles with it (the core cannot know
 * the projection; the PS1 uses the GTE frame's matrices, the GBA its iso
 * blit). The weak default is room_ref.c's iso projection of view v (what
 * native draws). */
struct DoomoRoomView;
void plat_room_project(const struct DoomoRoomView *v, fix16 x, fix16 y, int z,
                       int16_t *sx, int16_t *sy);

/* ---- (SP6) a bitmap scene's 2D loops -------------------------------------
 * A scene that draws a plain 2D bitmap frame (no 3D view, no tile frame, no
 * room view) may call this at the start of its draw: the platform makes its
 * faster 2D loops ready for this frame (GBA: the IWRAM overlay's UI group,
 * whose scale-2 image loop then draws plat_image(.., 2, ..) without a flip;
 * docs/GBA.md "SP6"). The pixels are exactly the same either way. Returns 1
 * when the loops are in. Weak default: 0, nothing changes (native, PS1).
 * (S2-5) A scene may also call it right before a scaled image, mid-frame
 * (the cutscenes' demons and eggs, the weapons wing's plans, the welding
 * icon): on the GBA the group's loop then draws plat_image at any scale 2..4,
 * flipped or not; under a tile frame it changes nothing and returns 0. */
int plat_ui_loops(void);
/* (SP6) The concert's light beam (concert_gfx.c beam()): for y = ay, ay + 3,
 * ... below ty, with f = (y - ay) * 256 / (ty - ay), centre cx = ax + (tx -
 * ax) * f / 256 and half width hw = hw0 + (hw1 - hw0) * f / 256, the 3-row
 * plat_checker band from x0 = cx - hw to cx + hw, x0 one pixel later when
 * (x0 + y) is odd. Returns 1 when the platform drew every band (exactly those
 * pixels), 0 to have the core draw them. Weak default: 0. GBA: one ARM call
 * of the UI group while plat_ui_loops() has it in. */
int plat_beam(int ax, int ay, int tx, int ty, int hw0, int hw1, uint8_t color);

/* ---- (S2-2) the 3D view held under a card -----------------------------------
 * A scene whose 3D view stands still (an intro card, a pause menu, a results or death screen: the world does not
 * tick, the level select's preview between two level changes) need not render it on every frame:
 *   PLAT_VIEW_KEEP  right after plat_view3d(v), before any 2D call of the frame: keep a copy of the picture the
 *                   view drew, under `key` (core/src/combat_int.h combat_view_key(): a hash of everything the
 *                   picture depends on). Returns 1 when kept, 0 when not (a skipped tick, a frame kind the platform
 *                   cannot read back, no room, the default)
 *   PLAT_VIEW_PUT   in place of plat_view3d(v): put the kept picture back into the frame being drawn. Returns 1
 *                   when done; 0 (nothing kept under this key, or another 3D view, overworld, room or scene change
 *                   since: the copy is gone; a skipped tick; the default) and the caller renders plat_view3d(v)
 *   PLAT_VIEW_PUT_SHADED(level)  the same, the picture darkened as plat_shade_rect(0, 0, screen w, screen h,
 *                   level) would darken it (0..15): for a card whose first 2D call is that full-screen shade, which
 *                   the caller then skips. 0 when the platform cannot (also: the view does not cover the screen);
 *                   the caller then PUTs and shades as before
 *   PLAT_VIEW_DROP  forget the copy. Always 0
 * The 2D drawn over the view (the HUD, the card) is drawn again on every frame as before, so a PUT frame is the
 * picture plat_view3d would have drawn, pixel for pixel. Weak default: 0 (PS1, DC, native render every frame). */
enum { PLAT_VIEW_DROP = 0, PLAT_VIEW_KEEP = 1, PLAT_VIEW_PUT = 2 };
#define PLAT_VIEW_PUT_SHADED(level) (PLAT_VIEW_PUT | ((level) + 1) << 8)
int plat_view_hold(int op, uint32_t key);

#endif
