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
#define PLATF_ALL_KNOWN   0x1FFu
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

/* ---- test flags ------------------------------------------------------------
 * Set by a test harness (native runner, GBA/PS1/DC harnesses) to switch off
 * interactive steps that would block a scripted run. Default 0 (a real
 * console). Bits are append-only. */
#define PLAT_TEST_NO_PICKER (1u << 0)   /* skip U5's first-boot language picker */
uint32_t plat_test_flags(void);

/* ---- held frame (U4) -------------------------------------------------------
 * plat_hold_frame(1) right after the frame to keep was presented: the
 * platform keeps showing it as the backdrop and the core then repaints only
 * its own rects on top. Returns 1 when the frame is held, 0 when the
 * platform cannot (the default; the caller then redraws its backdrop every
 * frame as today). plat_hold_frame(0) releases it (also implied by any
 * scene_set). */
int  plat_hold_frame(int on);

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

#endif
