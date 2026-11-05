/* settings.h - contract for the global settings record (customisation v2,
 * SPEC 1.7). P0.M0a lands the layout and the weak stubs; the real settings_load_best() and
 * settings_commit() come with M1 (and the GBA journal with U1g). */
#ifndef DOOMO_SETTINGS_H
#define DOOMO_SETTINGS_H

#include "types.h"

enum { IC_MENU, IC_WORLD, IC_SHOOTER, IC_COUNT };

typedef struct DoomoSettings {           /* copy in every save header + GBA journal */
    uint32_t gen; uint16_t len, crc16;   /* newest valid gen wins                   */
    uint8_t  v[288];                     /* one byte per global option              */
    uint8_t  remap[3][IC_COUNT][7];      /* 3 profiles x 3 contexts x 14 nibbles    */
    uint8_t  profile;                    /* 0 = preset from OPT_CONTROL_SCHEME      */
    uint8_t  fav[16][3];                 /* creator favourites (lf, choice)         */
    uint8_t  seen[8];                    /* tutorial + seen-cutscene bits           */
} DoomoSettings;
DOOMO_STATIC_ASSERT(sizeof(DoomoSettings) == 416, settings_is_416_bytes);

/* Declared by P0 (M0) so P4 and the scenes can call them before their owners land; every one is a
 * weak stub in stubs.c until then (settings: M1; input_context: P5.M1). */
int  settings_load_best(void);           /* 1 if a valid record was loaded, 0 for defaults */
int  settings_commit(void);              /* 1 on success                                    */
void input_context(int ic);              /* IC_*: which remap table the input layer uses    */

#endif
