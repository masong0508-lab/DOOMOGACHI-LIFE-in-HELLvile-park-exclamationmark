/* vmusave.h - VMU saves (DC4; owner: dreamcast).
 *
 * vmusave.c (console only) implements the core's save calls
 * (plat_save_write/read/erase, plat_save_scratch) and the machine config
 * record (plat_config_read/write) on the first VMU found, through the
 * portable file system in vms.c. save_icon.c (portable) draws what the
 * Dreamcast's file manager shows for each file: the Boreagotchi as it is
 * in the save, as an animated 32x32 16-colour icon and a 72x56 eyecatch.
 *
 * Files on the VMU (names keep the code's DOOMO prefix; the descriptions
 * the BIOS shows carry the game's name):
 *   DOOMO0.SYS    slot 0, story        (DOOMO0.NEW while a save is written)
 *   DOOMO1.SYS    slot 1, free play    (DOOMO1.NEW)
 *   DOOMOCFG.SYS  the machine config record (DOOMOCFG.NEW)
 */
#ifndef DC_VMUSAVE_H
#define DC_VMUSAVE_H

#include <stdint.h>
#include "vms.h"

#define DC_VMU_APP_ID   "BOREAGOTCHI"
#define DC_VMU_FRAMES   3                /* icon animation frames             */
#define DC_VMU_ANIM     12               /* 1/30 s per frame                  */

/* The pet as the save holds it: 0..15 a form (pet.h), -1 the egg (also
 * when there is no pet yet), -2 the departed pet's ghost. */
int dc_vmu_pet_form(void);
/* The icon (DC_VMU_FRAMES frames of 512 bytes, its 16 ARGB4444 colours)
 * and the 16-colour eyecatch (72x56 4bpp, its colours) of a pet form. */
void dc_vmu_icon(int form, uint16_t pal[16], uint8_t icon[DC_VMU_FRAMES * VMS_ICON_SIZE]);
void dc_vmu_eyecatch(int form, uint16_t pal[16], uint8_t ec[VMS_EC16_SIZE - 32]);
/* The descriptions the BIOS file manager shows, by slot (0, 1, or
 * DC_VMU_CONFIG for the config record). */
#define DC_VMU_CONFIG 2
const char *dc_vmu_desc_long(int slot);

#endif
