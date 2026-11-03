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

/* ---- the adults' security code (U5 / E11; user answer 20) ----
 * A 4-digit parental PIN, kept as a salted hash in bytes 32..47 of the machine record (layout in
 * options_store.c). Turning Mature content on asks for it (E11); the first-boot screen sets it (U5).
 * With no code set nothing is asked. Scenes: SCENE_SECURITY_CODE (seccode_scene.c). */
#define SECCODE_LEN 4
int  seccode_is_set(void);
int  seccode_set(const char *code);      /* SECCODE_LEN ASCII digits; 1 = stored, 0 = bad input */
void seccode_clear(void);                /* forget the code (the caller asked for the old one first) */
/* 1 = right (or no code set), 0 = wrong (a try is used), -1 = locked: no tries left. Three wrong
 * in a row lock the session; after a restart a locked machine grants one try per boot. */
int  seccode_check(const char *code);
int  seccode_tries_left(void);

/* SCENE_SECURITY_CODE's arg (scenes.h): SET asks for a new code twice (B on the first digit skips),
 * ASK asks for the existing one (nothing set = passes at once). The outcome of the last run,
 * until the scene is entered again: 1 = a code was set / the right code was entered, 0 = skipped,
 * cancelled or locked. */
enum { SECCODE_SET = 0, SECCODE_ASK = 1 };
int  seccode_scene_result(void);

#endif
