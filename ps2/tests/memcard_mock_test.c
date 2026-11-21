/* Mock PS2SDK/libmc integration for the REAL memcard.c transport.
 * No PS2 headers or hardware required. Specifically tests failed writes,
 * corrupt newest bank, card removal, header-only save menu and slots. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "libmc.h"
#include "loadfile.h"
#include "doomo/save.h"

enum { NBANK=4, MAX_FILE=96*1024 };
typedef struct {
    const char *name;
    uint8_t data[MAX_FILE];
    int live,size;
} Bank;
static Bank bank[NBANK] = {
    { .name="/DOOMOGACHI/STORYA" },
    { .name="/DOOMOGACHI/STORYB" },
    { .name="/DOOMOGACHI/ISLANDA" },
    { .name="/DOOMOGACHI/ISLANDB" }
};
static int pending,open_id=-1,offset,card_present=1,card_formatted=1;
static int card_seen; /* MCMAN's first poll returns -1, later 0. */
static int writes,fail_write_after=-1;
static int failures,checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)

int SifLoadModule(const char *name,int argc,const char *argv)
{(void)name;(void)argc;(void)argv;return 1;}
int mcInit(int type){card_seen=0;return type==MC_TYPE_MC?0:-1;}
int mcSync(int mode,int *cmd,int *result)
{(void)mode;(void)cmd;if(result)*result=pending;return 1;}
int mcGetInfo(int port,int slot,int *type,int *free_space,int *format)
{
    (void)port;(void)slot;
    if(type)*type=card_present?2:0;
    if(free_space)*free_space=8192;
    /* The real ROM MCMAN returns 0 for *format even for formatted
     * cards. Reproduce the initial -1 and subsequent 0 status path. */
    if(format)*format=0;
    if(!card_present){pending=-10;card_seen=0;}
    else if(!card_formatted){pending=-2;card_seen=0;}
    else if(!card_seen){pending=-1;card_seen=1;}
    else pending=0;
    return 0;
}
int mcOpen(int port,int slot,const char *name,int mode)
{
    int i;
    (void)port;(void)slot;
    pending=-4;
    if(!card_present || !card_formatted)return 0;
    for(i=0;i<NBANK;i++)if(!strcmp(bank[i].name,name))break;
    if(i==NBANK)return 0;
    if(!bank[i].live && !(mode&O_CREAT))return 0;
    if(!bank[i].live){bank[i].live=1;bank[i].size=0;}
    if(mode&O_TRUNC)bank[i].size=0;
    open_id=i;offset=0;
    pending=i+1;
    return 0;
}
int mcClose(int fd)
{
    pending=(open_id==fd-1)?0:-4;
    open_id=-1;
    return 0;
}
int mcSeek(int fd,int off,int origin)
{
    int to;
    if(open_id!=fd-1){pending=-4;return 0;}
    to=origin==SEEK_END?bank[open_id].size+off:off;
    if(to<0 || to>MAX_FILE){pending=-4;return 0;}
    offset=to;pending=offset;
    return 0;
}
int mcRead(int fd,void *data,int n)
{
    Bank *b;
    if(open_id!=fd-1 || !card_present){pending=-4;return 0;}
    b=&bank[open_id];
    if(n<0 || offset+n>b->size){pending=-4;return 0;}
    memcpy(data,b->data+offset,(size_t)n);
    offset+=n;pending=n;
    return 0;
}
int mcWrite(int fd,const void *data,int n)
{
    Bank *b;
    if(open_id!=fd-1 || !card_present){pending=-4;return 0;}
    writes++;
    if(fail_write_after>=0 && writes>=fail_write_after){
        pending=-3;return 0;
    }
    b=&bank[open_id];
    if(n<0 || offset+n>MAX_FILE){pending=-3;return 0;}
    memcpy(b->data+offset,data,(size_t)n);
    offset+=n;
    if(offset>b->size)b->size=offset;
    pending=n;return 0;
}
int mcFlush(int fd){pending=open_id==fd-1?0:-4;return 0;}
int mcMkDir(int port,int slot,const char *name)
{(void)port;(void)slot;(void)name;pending=0;return 0;}
int mcDelete(int port,int slot,const char *name)
{
    int i;
    (void)port;(void)slot;
    pending=-4;
    for(i=0;i<NBANK;i++)if(!strcmp(bank[i].name,name) && bank[i].live){
        bank[i].live=0;bank[i].size=0;pending=0;break;
    }
    return 0;
}

int ps2_memcard_init(void);
int plat_save_write(int slot,const void *data,uint32_t size);
int plat_save_read(int slot,void *data,uint32_t cap,uint32_t *size);
int plat_save_erase(int slot);

int main(void)
{
    static uint8_t a[6000],b[6000],c[6000],out[6000];
    uint32_t size=0;
    int i;
    for(i=0;i<6000;i++){
        a[i]=(uint8_t)(i*3+7);
        b[i]=(uint8_t)(i*5+11);
        c[i]=(uint8_t)(i*7+13);
    }
    CHECK(ps2_memcard_init());
    /* The SECOND and later polls return r=0 with fmt=0 under MCMAN.
     * This previously made subsequent saving permanently fail. */
    CHECK(!plat_save_read(0,out,sizeof out,&size));
    CHECK(plat_save_write(0,a,sizeof a));
    CHECK(plat_save_read(0,out,sizeof out,&size));
    CHECK(size==sizeof a && memcmp(out,a,sizeof a)==0);
    CHECK(plat_save_read(0,out,DOOMO_SAVE_HEADER_SIZE,&size));
    CHECK(size==DOOMO_SAVE_HEADER_SIZE && memcmp(out,a,size)==0);

    CHECK(plat_save_write(0,b,sizeof b));
    CHECK(plat_save_read(0,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);
    /* Third write targets the older bank, and fails in the middle of
     * its payload. The last valid complete B must still be chosen. */
    writes=0;fail_write_after=3;
    CHECK(!plat_save_write(0,c,sizeof c));
    fail_write_after=-1;
    CHECK(plat_save_read(0,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);

    CHECK(plat_save_write(0,c,sizeof c));
    CHECK(plat_save_read(0,out,sizeof out,&size));
    CHECK(size==sizeof c && memcmp(out,c,sizeof c)==0);
    /* Newest C is corrupted on the card: load B, never a torn C. */
    bank[0].data[32]^=0x80u;
    CHECK(plat_save_read(0,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);

    card_present=0;
    CHECK(!plat_save_read(0,out,sizeof out,&size));
    CHECK(!plat_save_write(0,c,sizeof c));
    card_present=1;
    CHECK(plat_save_read(0,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);

    card_formatted=0;
    CHECK(!plat_save_write(1,a,sizeof a));
    card_formatted=1;
    CHECK(plat_save_write(1,a,sizeof a));
    CHECK(plat_save_read(1,out,sizeof out,&size));
    CHECK(size==sizeof a && memcmp(out,a,sizeof a)==0);
    CHECK(plat_save_erase(0));
    CHECK(!plat_save_read(0,out,sizeof out,&size));
    CHECK(plat_save_read(1,out,sizeof out,&size));
    CHECK(size==sizeof a && memcmp(out,a,sizeof a)==0);

    /* Reinitialize the real libmc transport as it would be after a fresh
     * boot, but do not clear the mock's memory-card files. A successful
     * write must survive re-creating the RPC client, not just the
     * process-local 'ready' flag or the one-frame save UI. */
    CHECK(ps2_memcard_init());
    memset(out,0x00,sizeof out);
    size=0;
    CHECK(plat_save_read(1,out,sizeof out,&size));
    CHECK(size==sizeof a && memcmp(out,a,sizeof a)==0);
    CHECK(!plat_save_read(0,out,sizeof out,&size)); /* erased slot stays gone */

    /* Both journal banks survive independent boots; verify newest-bank
     * selection and recovery from an interrupted next save after reboot. */
    CHECK(plat_save_write(1,b,sizeof b));
    CHECK(ps2_memcard_init());
    memset(out,0x00,sizeof out);
    CHECK(plat_save_read(1,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);
    writes=0;fail_write_after=3;
    CHECK(!plat_save_write(1,c,sizeof c));
    fail_write_after=-1;
    CHECK(ps2_memcard_init());
    CHECK(plat_save_read(1,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);

    /* Reboot with missing/unformatted media must reject success instead of
     * trusting an earlier ready or formatted state. A subsequent reboot
     * with restored media must recover the same verified saved bytes. */
    card_present=0;
    CHECK(!ps2_memcard_init());
    CHECK(!plat_save_read(1,out,sizeof out,&size));
    card_present=1;card_formatted=0;
    CHECK(!ps2_memcard_init());
    CHECK(!plat_save_write(1,c,sizeof c));
    card_formatted=1;
    CHECK(ps2_memcard_init());
    memset(out,0x00,sizeof out);
    CHECK(plat_save_read(1,out,sizeof out,&size));
    CHECK(size==sizeof b && memcmp(out,b,sizeof b)==0);
    printf("PS2 memory-card host integration: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
