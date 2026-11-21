/* Host verification of the exact header/CRC helpers used by libmc.
 * Exercises interruption, corruption, generation wrap and clipping.
 * Real IOP RPC + memory-card persistence still requires PCSX2/hardware. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "save_journal.h"

static uint32_t state=0x77CB108Au;
static uint32_t rnd(void)
{
    state^=state<<13;state^=state>>17;state^=state<<5;
    return state;
}
static int valid(const uint8_t *head,const uint8_t *data,uint32_t n,
                 uint32_t *generation)
{
    uint32_t len=0,crc=0;
    return ps2_sj_decode(head,88u*1024u,generation,&len,&crc) &&
           len==n && ps2_sj_crc(data,len)==crc;
}
int main(void)
{
    static uint8_t a[2048],b[2048],head_a[16],head_b[16];
    uint32_t ga,gb;
    int i,checks=0;
    if(ps2_sj_crc((const uint8_t *)"123456789",9)!=0xCBF43926u) {
        puts("FAIL: CRC-32 standard vector");return 1;
    }
    checks++;
    for(i=0;i<1000;i++){
        uint32_t n=48u+(rnd()%1800u),first=rnd();
        uint32_t newer=first+1u,k;
        int choose;
        for(k=0;k<n;k++){a[k]=(uint8_t)rnd();b[k]=(uint8_t)rnd();}
        ps2_sj_header(head_a,first,n,ps2_sj_crc(a,n));
        ps2_sj_header(head_b,newer,n,ps2_sj_crc(b,n));
        if(!valid(head_a,a,n,&ga) || !valid(head_b,b,n,&gb) ||
           !ps2_sj_newer(gb,ga)) {
            printf("FAIL: valid banks %d\n",i);return 1;
        }
        choose=ps2_sj_newer(gb,ga)?1:0;
        if(choose!=1){puts("FAIL: latest generation");return 1;}
        checks++;
        /* Interrupted newer write must not replace the valid previous
         * copy even if its generation header was already committed. */
        b[rnd()%n]^=0x80u;
        if(valid(head_b,b,n,NULL) || !valid(head_a,a,n,NULL)){
            puts("FAIL: corruption not detected");return 1;
        }
        choose=valid(head_b,b,n,NULL)?1:0;
        if(choose!=0){puts("FAIL: fallback missing");return 1;}
        checks++;
        if(ps2_sj_decode(head_a,47,NULL,NULL,NULL) ||
           !ps2_sj_decode(head_a,88u*1024u,NULL,NULL,NULL)){
            puts("FAIL: size validation");return 1;
        }
        checks++;
        head_a[0]^=0x01u;
        if(valid(head_a,a,n,NULL)){puts("FAIL: invalid magic");return 1;}
        checks++;
    }
    if(!ps2_sj_newer(0u,0xFFFFFFFFu) ||
       ps2_sj_newer(0xFFFFFFFFu,0u)) {
        puts("FAIL: wraparound comparison");return 1;
    }
    checks++;
    printf("PS2 save journal: %d CRC, rollback, length and wrap checks passed\n",checks);
    return 0;
}
