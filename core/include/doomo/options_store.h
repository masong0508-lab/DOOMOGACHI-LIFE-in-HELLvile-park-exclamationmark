/* options_store.h - the options with ids 64 and up, the 'OPT2' save chunk and the machine config
 * record (U0; core/src/options_store.c). Nothing here is for game code: use opt_get / opt_set
 * (town.h), which take every id. */
#ifndef DOOMO_OPTIONS_STORE_H
#define DOOMO_OPTIONS_STORE_H

#include "town.h"

/* the table of the ids OPT2_FIRST..: doomo_option2_info[opt - OPT2_FIRST] */
extern const DoomoOptionInfo doomo_option2_info[OPT2_COUNT];

int  opt2_get(int opt);                  /* 0 for an unknown id */
void opt2_set(int opt, int value);       /* clamps; runs the option's side effects */
void opt2_defaults(void);                /* all ids >= OPT2_FIRST to their defaults (side effects too) */

/* opt_set calls this after any option changes: a machine-wide one (language, the three volumes,
 * OPT_3D_FAST, OPT_MATURE, OPT_RUMBLE) is written to the machine config record, and the platform is
 * told about the ones it applies itself (plat_option_changed). */
void opt_machine_changed(int opt, int value);

/* The machine config record (plat_ext.h PLAT_CONFIG_MAX = 48 B): bytes 0..31 are ours, 32..47 belong
 * to the adults' security code (U5 / E11) and are kept as they are when we write.
 *   0 'D' 1 'O' 2 version (1) 3 0 | 4 language 5 music 6 sfx 7 voice 8 3D 9 mature 10 rumble |
 *   11..29 0 | 30..31 CRC-16/CCITT of bytes 0..29, little endian */
#define OPT_CONFIG_VERSION 1
#define OPT_CONFIG_SETTINGS_SZ 32u
/* Boot: apply the record if there is a valid one (doomo_init calls it after opt_defaults). 1 = applied. */
int  opt_config_load(void);
/* Write the record now (opt_machine_changed does it on every change). 1 = stored. */
int  opt_config_save(void);
/* U5: 1 when the platform keeps a config record (PLATF_CONFIG) and opt_config_load found none, so this
 * is the machine's first boot and SCENE_LANG_PICK runs once. Always 0 where nothing is kept (native,
 * DC for now). _done() clears it (the picker calls it after it wrote the record). */
int  opt_config_first_boot(void);
void opt_config_first_boot_done(void);

#endif
