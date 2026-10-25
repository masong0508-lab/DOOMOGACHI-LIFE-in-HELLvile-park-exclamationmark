/* wp6_data.h - the WP6 PS1 test program's scene table (gen_wp6.py writes the data). */
#ifndef WP6_DATA_H
#define WP6_DATA_H

#include "doomo/doomo.h"
#include "doomo/beat.h"

typedef struct Wp6Op { int16_t t, sop; int16_t a[6]; } Wp6Op;

enum { WP6_PLAY, WP6_SHEET, WP6_AT65, WP6_DX };

typedef struct Wp6Scene {
    const char *name;
    int mode, frames, arg, backdrop;
    const uint8_t *look; int nlook;
    const uint8_t *actr; int nactr;
    const uint8_t *crwd; int ncrwd;
    const uint8_t *trak; int trak_bytes;
    const uint8_t *flip; int flip_bytes;
    const BeatEv *beat; int nbeat;
    const Wp6Op *ops; int nops;
} Wp6Scene;

extern const Wp6Scene wp6_scenes[];
extern const int wp6_nscenes;

#endif
