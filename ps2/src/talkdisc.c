/* PS2-4: disc-backed Talk language files using one 2048-byte sector.
 * The existing core's 256-byte read-through window amortizes small
 * reads. The entire 0.9MB English model stays on the optical disc.
 * Nonexistent/invalid media fails closed; no simulated conversation bank.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <libcdvd.h>
#include "doomo/plat_ext.h"
#include "talk_path.h"

#define SECTOR_BYTES 2048u
#define TALK_DISC_LIMIT (4u*1024u*1024u)
static uint8_t sector_data[SECTOR_BYTES] __attribute__((aligned(64)));
static uint32_t bank_lsn,bank_size;
static int cached_sector=-1,cd_ready,english_present;
static char active_code[8];

static int open_language(const char *code)
{
    sceCdlFILE file;
    char path[24];
    bank_size=0;
    cached_sector=-1;
    active_code[0]=0;
    if(!cd_ready || !ps2_talk_path(path,code))return 0;
    if(!sceCdSearchFile(&file,path) ||
       file.size<20u || file.size>TALK_DISC_LIMIT)return 0;
    bank_lsn=file.lsn;
    bank_size=file.size;
    strcpy(active_code,code); /* ps2_talk_path verified length <= 6 */
    return 1;
}
void ps2_talk_disc_init(void)
{
    cd_ready=(sceCdInit(0)!=0);
    if(!cd_ready){
        printf("PS2 Talk: CDVD init failed; disc conversations unavailable\n");
        return;
    }
    english_present=open_language("en");
    if(!english_present)
        printf("PS2 Talk: no TALK_EN.BIN disc bank; conversations disabled\n");
    else
        printf("PS2 Talk: disc bank TALK_EN.BIN, %lu bytes\n",
               (unsigned long)bank_size);
}
int ps2_talk_disc_available(void){return english_present;}
uint32_t plat_talk_load(const char *code)
{
    if(!code || !cd_ready)return 0;
    if(bank_size && strcmp(active_code,code)==0)return bank_size;
    return open_language(code)?bank_size:0;
}
uint32_t plat_talk_read(uint32_t off,void *dst,uint32_t len)
{
    uint8_t *out=(uint8_t *)dst;
    uint32_t at=0;
    if(!dst || !bank_size || off>bank_size || len>bank_size-off)return 0;
    while(at<len){
        uint32_t pos=off+at;
        uint32_t block=pos/SECTOR_BYTES;
        uint32_t in=pos%SECTOR_BYTES;
        uint32_t n=SECTOR_BYTES-in;
        if(n>len-at)n=len-at;
        if(cached_sector!=(int)block){
            sceCdRMode mode;
            memset(&mode,0,sizeof mode);
            mode.trycount=3;
            cached_sector=-1;
            if(!sceCdRead(bank_lsn+block,1u,sector_data,&mode) ||
               sceCdSync(0)!=0 || sceCdGetError()!=SCECdErNO)
                return 0;
            cached_sector=(int)block;
        }
        memcpy(out+at,sector_data+in,n);
        at+=n;
    }
    return len;
}
uint32_t plat_features(void)
{
    /* Advertise Talk only when its disc bank was actually discovered.
     * Native/PS2 share the same conversation engine, but an ELF boot
     * without a disc must not offer a dead menu action. */
    return (english_present?PLATF_TALK:0u)|PLATF_3D_QUALITY;
}
