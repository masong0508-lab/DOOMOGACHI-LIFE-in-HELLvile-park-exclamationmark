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
 *   plat_pack_blob/read      E0 declares the pack format (pack.h, which
 *                            includes this header); E8 (GBA ROM blob),
 *                            E9 (PS1 disc file), E2 (native loader)
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
 *   PLAT_BUILD_HIDDEN  back to the hidden page (the build's target) */
enum { PLAT_BUILD_START = 1, PLAT_BUILD_SHOWN = 2, PLAT_BUILD_HIDDEN = 3 };
int  plat_build_frame(int op);

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

#endif
