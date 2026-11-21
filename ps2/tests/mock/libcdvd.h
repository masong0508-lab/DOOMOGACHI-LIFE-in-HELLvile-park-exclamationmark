/* Host stand-in for only the PS2SDK CDVD functions used by talkdisc.c.
 * The real game is compiled against PS2SDK; this header is TEST-ONLY.
 */
#ifndef DOOMO_PS2_MOCK_LIBCDVD_H
#define DOOMO_PS2_MOCK_LIBCDVD_H
#include <stdint.h>
typedef struct {
    uint32_t lsn;
    uint32_t size;
} sceCdlFILE;
typedef struct {
    unsigned char trycount, spindlctrl, datapattern, pad;
} sceCdRMode;
#define SCECdErNO 0
int sceCdInit(int mode);
int sceCdSearchFile(sceCdlFILE *out,const char *name);
int sceCdRead(uint32_t lsn,uint32_t sectors,void *destination,sceCdRMode *mode);
int sceCdSync(int mode);
int sceCdGetError(void);
#endif
