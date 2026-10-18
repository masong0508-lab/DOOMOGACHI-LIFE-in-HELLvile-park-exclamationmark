/* mus_int.h - internals shared by the music engine's files (mus_*.c,
 * audio_seq.c) (owner: MU0 skeleton, MU1 maintainer; docs/design/music/
 * SPEC.md sec.4, 6). Not part of the contract: only music.h, mus_data.h
 * and platform.h are. */
#ifndef DOOMO_MUS_INT_H
#define DOOMO_MUS_INT_H

#include "doomo/doomo.h"
#include "doomo/music.h"

/* the legacy bridge's view of voice ch (plat_weak_music.c) */
const DoomoVoice *mus_bridge_voice(int ch);

#endif
