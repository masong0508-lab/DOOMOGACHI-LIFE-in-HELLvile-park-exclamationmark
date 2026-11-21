/* Test the *real* PS2 Talk CD transport, not an independently copied
 * reader. Mock sceCdRead verifies one-sector caching, bounds checks,
 * disc absence, and failure not exposing stale DMA bytes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcdvd.h"
#include "doomo/plat_ext.h"

#define SECTOR 2048u
#define NBYTES (3u*SECTOR+17u)
#define BASE_LSN 12u
static uint8_t disc[NBYTES+SECTOR];
static int present=1,init_ok=1,fail_sector=-1,read_count,last_error;
static int failures,checks;

#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);failures++;}}while(0)

int sceCdInit(int mode){(void)mode;return init_ok;}
int sceCdSearchFile(sceCdlFILE *out,const char *name)
{
    if(!present || strcmp(name,"\\TALK_EN.BIN;1"))return 0;
    out->lsn=BASE_LSN;
    out->size=NBYTES;
    return 1;
}
int sceCdRead(uint32_t lsn,uint32_t sectors,void *destination,sceCdRMode *mode)
{
    uint32_t ix;
    read_count++;
    if(mode->trycount!=3u || sectors!=1u || lsn<BASE_LSN){
        last_error=1;return 0;
    }
    ix=lsn-BASE_LSN;
    if((int)ix==fail_sector || ix>3u){
        last_error=1;return 0;
    }
    memcpy(destination,disc+ix*SECTOR,SECTOR);
    last_error=0;
    return 1;
}
int sceCdSync(int mode){(void)mode;return 0;}
int sceCdGetError(void){return last_error;}
void ps2_talk_disc_init(void);

static void verify_read(uint32_t offset,uint32_t n)
{
    uint8_t dst[96];
    if(n>sizeof dst){CHECK(0);return;}
    memset(dst,0xA5,sizeof dst);
    CHECK(plat_talk_read(offset,dst,n)==n);
    CHECK(memcmp(dst,disc+offset,n)==0);
    CHECK(dst[n]==0xA5);
}

int main(void)
{
    uint32_t i;
    uint8_t dst[96];
    for(i=0;i<sizeof disc;i++) disc[i]=(uint8_t)(i*73u+(i>>3));
    /* No CDVD service: no phantom Talk menu. */
    init_ok=0;
    ps2_talk_disc_init();
    CHECK((plat_features()&PLATF_TALK)==0);
    CHECK(plat_talk_load("en")==0);
    init_ok=1;present=1;
    ps2_talk_disc_init();
    CHECK((plat_features()&PLATF_TALK)!=0);
    CHECK(plat_talk_load("en")==NBYTES);
    CHECK(read_count==0);
    verify_read(10,30);
    CHECK(read_count==1);
    verify_read(17,40);
    CHECK(read_count==1); /* cached 2048-byte sector */
    verify_read(SECTOR-8,48);
    CHECK(read_count==2);
    verify_read(SECTOR-8,48);
    CHECK(read_count==4); /* alternates across two sectors */
    verify_read(3u*SECTOR,17u);
    CHECK(read_count==5);
    CHECK(plat_talk_read(NBYTES-1,dst,2)==0);
    CHECK(plat_talk_read(NBYTES,dst,1)==0);
    CHECK(plat_talk_read(0,NULL,2)==0);
    fail_sector=2;
    memset(dst,0x5C,sizeof dst);
    CHECK(plat_talk_read(2u*SECTOR,dst,16)==0);
    CHECK(dst[0]==0x5C); /* failed sector does not copy old data */
    fail_sector=-1;
    verify_read(2u*SECTOR,16);
    CHECK(plat_talk_load("../en")==0);
    CHECK(plat_talk_read(0,dst,16)==0);
    CHECK(plat_talk_load("en")==NBYTES);
    verify_read(0,16);
    /* Disc removed after a successful session: stale file metadata and
     * the old loaded sector must never survive a failed re-init. */
    init_ok=0;
    ps2_talk_disc_init();
    CHECK((plat_features()&PLATF_TALK)==0);
    CHECK(plat_talk_read(0,dst,16)==0);
    printf("PS2 Talk CDVD transport: %d checks, %d failed\n",checks,failures);
    return failures?1:0;
}
