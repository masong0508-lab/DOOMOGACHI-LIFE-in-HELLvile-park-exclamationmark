/* link_code.h - compact, manually typed GBA <-> PS1 progress transfer.
 * Same format on all ports. Nothing is stored between calls. */
#ifndef DOOMO_LINK_CODE_H
#define DOOMO_LINK_CODE_H
#include <stdint.h>

/* 15 encoded bytes = 24 base32 symbols, shown as six groups of four. */
#define DOOMO_LINK_CODE_SIZE 30 /* 24 symbols, 5 hyphens, final NUL */
typedef struct DoomoLinkProgress {
    uint32_t breach;      /* old seven 3-bit ranks plus best Horde wave */
    uint32_t money;       /* max-merge, never added to avoid repeat imports */
    uint16_t exorcisms;  /* max-merge */
    uint8_t gold;         /* gold trophies for the seven shared levels */
} DoomoLinkProgress;

/* 1 on success, 0 on invalid input, buffer too small or bad checksum.
 * Hyphens/spaces are optional when decoding; lowercase is accepted. */
int doomo_link_encode(const DoomoLinkProgress *data, char *code, int capacity);
int doomo_link_decode(DoomoLinkProgress *data, const char *code);
#endif
