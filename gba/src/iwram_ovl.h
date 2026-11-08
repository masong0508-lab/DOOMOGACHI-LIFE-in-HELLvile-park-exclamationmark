/* iwram_ovl.h - the IWRAM code overlay (owner: gba; G4, docs/GBA.md
 * section 3 "IWRAM code overlay").
 *
 * IWRAM has one slot (gba.ld OVERLAY, right after the resident ARM code)
 * that holds ONE group of hot ARM code at a time:
 *   OVL_3D  the raycaster: r3d.iwram.c + r3d_loops.s (section .iwram_3d)
 *   OVL_2D  the tile-mode 2D loops (section .iwram_2d, IWRAM_2D_CODE)
 *   OVL_ROOM (H8) the dream house's lamp-pool loops (section .iwram_room,
 *           and SP7 stage.c stage_rows, the kept backdrop restore;
 *           IWRAM_ROOM_CODE; room.c, loaded by plat_room())
 *   OVL_UI  (SP4) loops of the bitmap scenes that use none of the above
 *           (section .iwram_ui, IWRAM_UI_CODE): the pet LCD's dot expand
 *           (dots.c, loaded by plat_dots())
 * crt0 loads no group. Before running any code of a group, call
 * iwram_ovl_use(group): a compare when the group is already in, else one
 * DMA from ROM (the 3D group's 10.4 KB: ~0.6 ms, once per mode change).
 *
 * Rules (a broken one crashes, it does not degrade):
 *   - the entry point of each mode calls it every time it runs (plat_view3d
 *     for OVL_3D; the tile-mode entry for OVL_2D), never a helper deep
 *     inside, so a 3D frame inside a 2D scene reloads 3D and the next 2D
 *     frame reloads 2D;
 *   - never from an IRQ handler (the interrupted code may be running in
 *     the slot; the call halts with a log line if it sees the IRQ stack),
 *     and never from code that is itself in the slot;
 *   - IRQ handlers never call slot code (irq.iwram.c is resident);
 *   - .data / .bss never go in a group (gba.ld): only code and const data,
 *     which the next load restores byte for byte.
 */
#ifndef GBA_IWRAM_OVL_H
#define GBA_IWRAM_OVL_H

#include <stdint.h>

enum { OVL_NONE = 0, OVL_3D = 1, OVL_2D = 2, OVL_ROOM = 3, OVL_UI = 4 };

/* Load group g (OVL_3D / OVL_2D / OVL_ROOM / OVL_UI) into the slot unless it is there. */
void iwram_ovl_use(int g);
/* The group in the slot now (OVL_NONE after boot). */
int iwram_ovl_current(void);
/* Loads so far: a change means the slot was rewritten. The bytes of the
 * slot past the 2D group (iwram_ovl_2d_tail) are scratch while the 2D
 * group is in, and are lost on the next load (G6: ow_obj.c). */
uint32_t iwram_ovl_loads(void);
uint8_t *iwram_ovl_2d_tail(uint32_t *bytes);

/* A single ARM function of the 2D group (like IWRAM_CODE for resident
 * code). Callers must have called iwram_ovl_use(OVL_2D). */
#define IWRAM_2D_CODE __attribute__((section(".iwram_2d"), target("arm"), long_call, noinline))
/* (H8) a single ARM function of the room group; callers must have called
 * iwram_ovl_use(OVL_ROOM). They call nothing outside the group. */
#define IWRAM_ROOM_CODE __attribute__((section(".iwram_room"), target("arm"), long_call, noinline))
/* (SP4) a single ARM function of the UI group; callers must have called
 * iwram_ovl_use(OVL_UI). They call nothing outside the group. */
#define IWRAM_UI_CODE __attribute__((section(".iwram_ui"), target("arm"), long_call, noinline))

#if defined(ENGINE_TEST)
/* engine test (enginetest.c): a 2D-group function, so the test ROM swaps
 * a real group in and out between 3D frames. Returns x * 3 + 0x2D. */
uint32_t iwram_ovl_probe(uint32_t x) __attribute__((long_call));
#endif

#endif
