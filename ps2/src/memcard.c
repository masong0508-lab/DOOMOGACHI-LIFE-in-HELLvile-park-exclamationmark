/* PS2-2: real memory-card transport for the shared DMOO save codec.
 * Two checksum-verified banks per slot. A write replaces ONLY the
 * older/invalid bank; an interrupted write preserves the newest valid
 * bank. No new framebuffer/heap allocation and no "saved to RAM" lie.
 *
 * libmc uses asynchronous IOP RPC. Every operation is explicitly synced
 * before the caller may reuse a buffer. The 1KiB aligned transfer buffer
 * is static and only used from the game's single-threaded save callbacks.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <libmc.h>
#include <loadfile.h>
#include "doomo/save.h"
#include "save_journal.h"

#define MC_PORT 0
#define MC_SLOT 0
#define MC_DIR "/DOOMOGACHI"
#define IO_SIZE 1024u

static uint8_t io[IO_SIZE] __attribute__((aligned(64)));
static uint8_t mc_ready;
/* Old MCMAN/MCSERV always reports fmt=0, even on formatted cards.
 * Retain the last confirmed formatted-card state across r=0 polls. */
static uint8_t mc_formatted;
static const char *const bank_name[2][2] = {
    { MC_DIR "/STORYA", MC_DIR "/STORYB" },
    { MC_DIR "/ISLANDA", MC_DIR "/ISLANDB" }
};

static int sync_result(void)
{
    int result = -1;
    if(mcSync(0,NULL,&result)<0) return -1;
    return result;
}
static int card_ready(void)
{
    int type=0,free_space=0,fmt=0,r;
    if(!mc_ready || mcGetInfo(MC_PORT,MC_SLOT,&type,&free_space,&fmt)<0)
        return 0;
    r=sync_result();
    /* MCMAN/MCSERV always sets fmt=0, even on a formatted card.
     * -1 confirms a newly inserted formatted card; 0 is unchanged,
     * so preserve the last known status rather than rejecting every
     * subsequent save. XMCSERV may additionally provide fmt!=0.
     * Removal/unformatted/error invalidates the remembered status. */
    if(type!=2 || r < -1) {
        mc_formatted=0;
        return 0;
    }
    if(r==-1 || fmt!=0) mc_formatted=1;
    return mc_formatted!=0;
}
int ps2_memcard_init(void)
{
    if(SifLoadModule("rom0:SIO2MAN",0,NULL)<0) {
        /* A pad driver may already have loaded this module. Continue
         * only if MCMAN and MCSERV genuinely initialize below. */
    }
    /* Some launchers pre-load MCMAN/MCSERV; a second SifLoadModule can
     * return "already resident". The actual mcInit RPC decides whether
     * the service is usable, not the module loader's status alone. */
    (void)SifLoadModule("rom0:MCMAN",0,NULL);
    (void)SifLoadModule("rom0:MCSERV",0,NULL);
    mc_formatted=0;
    mc_ready=(uint8_t)(mcInit(MC_TYPE_MC)>=0);
    return mc_ready && card_ready();
}
static int open_bank(int slot,int bank,int flags)
{
    if(mcOpen(MC_PORT,MC_SLOT,bank_name[slot][bank],flags)<0)
        return -1;
    return sync_result();
}
static int seek_bank(int fd,int off,int origin)
{
    if(mcSeek(fd,off,origin)<0)return -1;
    return sync_result();
}
static int close_bank(int fd)
{
    if(mcClose(fd)<0)return 0;
    return sync_result()>=0;
}
/* Read exactly n into the aligned IOP buffer. If a caller wants data,
 * copy only after each RPC finishes; no DMA references its destination. */
static int read_exact(int fd,void *out,uint32_t n)
{
    uint8_t *dst=(uint8_t *)out;
    while(n){
        uint32_t chunk=n>IO_SIZE?IO_SIZE:n;
        int got;
        if(mcRead(fd,io,(int)chunk)<0)return 0;
        got=sync_result();
        if(got!=(int)chunk)return 0;
        if(dst){memcpy(dst,io,chunk);dst+=chunk;}
        n-=chunk;
    }
    return 1;
}
static int write_exact(int fd,const void *data,uint32_t n)
{
    const uint8_t *src=(const uint8_t *)data;
    while(n){
        uint32_t chunk=n>IO_SIZE?IO_SIZE:n;
        int wrote;
        memcpy(io,src,chunk);
        if(mcWrite(fd,io,(int)chunk)<0)return 0;
        wrote=sync_result();
        if(wrote!=(int)chunk)return 0;
        src+=chunk;
        n-=chunk;
    }
    return 1;
}
/* Validate the entire journal bank, INCLUDING its streamed CRC. The
 * caller can request the whole image or only its 48-byte DMOO header.
 * Validating both banks before choosing guarantees a partial newer write
 * cannot hide the last valid save. */
static int read_bank(int slot,int bank,uint32_t *gen,
                     void *out,uint32_t cap,uint32_t *actual)
{
    uint8_t h[PS2_SJ_HEADER];
    uint32_t len=0,crc=0,epoch=0,pos=0,calc=0xFFFFFFFFu;
    int fd,filesize,ok=0;
    uint8_t *dst=(uint8_t *)out;
    fd=open_bank(slot,bank,O_RDONLY);
    if(fd<0)return 0;
    filesize=seek_bank(fd,0,SEEK_END);
    if(filesize<(int)PS2_SJ_HEADER || seek_bank(fd,0,SEEK_SET)!=0)
        goto end;
    if(!read_exact(fd,h,sizeof h) ||
       !ps2_sj_decode(h,slot?DOOMO_SAVE_SLOT1_MAX:DOOMO_SAVE_SLOT0_MAX,&epoch,&len,&crc) ||
       (uint32_t)filesize!=PS2_SJ_HEADER+len)
        goto end;
    if(dst && cap<len && cap!=DOOMO_SAVE_HEADER_SIZE)goto end;
    while(pos<len){
        uint32_t chunk=len-pos;
        if(chunk>IO_SIZE)chunk=IO_SIZE;
        if(mcRead(fd,io,(int)chunk)<0 || sync_result()!=(int)chunk)goto end;
        calc=ps2_sj_crc_step(calc,io,chunk);
        if(dst && pos<cap){
            uint32_t n=cap-pos;
            if(n>chunk)n=chunk;
            memcpy(dst+pos,io,n);
        }
        pos+=chunk;
    }
    if((calc^0xFFFFFFFFu)!=crc)goto end;
    if(actual)*actual=dst && cap==DOOMO_SAVE_HEADER_SIZE ?
                     DOOMO_SAVE_HEADER_SIZE : len;
    if(gen)*gen=epoch;
    ok=1;
end:
    if(!close_bank(fd))return 0;
    return ok;
}
static int latest_bank(int slot,uint32_t *generation)
{
    uint32_t ga=0,gb=0;
    int a=read_bank(slot,0,&ga,NULL,0,NULL);
    int b=read_bank(slot,1,&gb,NULL,0,NULL);
    if(!a && !b)return -1;
    if(b && (!a || ps2_sj_newer(gb,ga))){
        if(generation)*generation=gb;
        return 1;
    }
    if(generation)*generation=ga;
    return 0;
}

int plat_save_read(int slot,void *data,uint32_t cap,uint32_t *size)
{
    int b;
    if(size)*size=0;
    if(slot<0 || slot>1 || !data || !size || !card_ready())return 0;
    b=latest_bank(slot,NULL);
    if(b<0)return 0;
    if(read_bank(slot,b,NULL,data,cap,size))return 1;
    /* If the preferred bank changes or becomes corrupt between the
     * validity scan and its second read, the surviving bank can still
     * restore the player's island rather than losing a valid save. */
    return read_bank(slot,1-b,NULL,data,cap,size);
}
int plat_save_write(int slot,const void *data,uint32_t size)
{
    uint32_t generation=0;
    uint8_t h[PS2_SJ_HEADER];
    int newest,target,fd,ok;
    if(slot<0 || slot>1 || !data ||
       size<DOOMO_SAVE_HEADER_SIZE ||
       size>(slot?DOOMO_SAVE_SLOT1_MAX:DOOMO_SAVE_SLOT0_MAX) ||
       !card_ready())return 0;
    newest=latest_bank(slot,&generation);
    target=newest<0?0:1-newest;
    ps2_sj_header(h,generation+1u,size,ps2_sj_crc(data,size));
    /* Safe to try an existing folder; "already exists" is not a
     * failure. A later mcOpen/write/verify is the real success check. */
    if(mcMkDir(MC_PORT,MC_SLOT,MC_DIR)>=0)(void)sync_result();
    fd=open_bank(slot,target,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0)return 0;
    ok=write_exact(fd,h,sizeof h) && write_exact(fd,data,size);
    if(ok && mcFlush(fd)>=0)ok=sync_result()>=0;
    else ok=0;
    if(!close_bank(fd))ok=0;
    /* Even after a successful RPC, require a full disk readback and CRC.
     * If verification fails, the untouched bank stays loadable. */
    return ok && read_bank(slot,target,NULL,NULL,0,NULL);
}
int plat_save_erase(int slot)
{
    int b;
    if(slot<0 || slot>1 || !card_ready())return 0;
    for(b=0;b<2;b++){
        if(mcDelete(MC_PORT,MC_SLOT,bank_name[slot][b])>=0)
            (void)sync_result();
    }
    return latest_bank(slot,NULL)<0;
}
