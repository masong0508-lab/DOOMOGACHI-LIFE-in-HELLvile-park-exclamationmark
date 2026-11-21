/* Exact host parity for the PS2 EE 1x composition fast path.
 * Checks transparency, flip, silhouette and guard bytes against a
 * deliberately simple reference for hundreds of seeded images. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "indexed_blit.h"

enum { W=127, H=81, GUARD=32, N=W*H };
static uint8_t fast[GUARD+N+GUARD], reference[GUARD+N+GUARD];
static uint8_t src[64*64];
static uint32_t state=0x2174BA5Du;

static uint32_t rnd(void)
{
    state ^= state<<13; state ^= state>>17; state ^= state<<5;
    return state;
}

static void oracle(uint8_t *dst,int x,int y,int iw,int ih,int flip,
                   int silhouette,uint8_t ink)
{
    int xx,yy;
    for(yy=0;yy<ih;yy++)
        for(xx=0;xx<iw;xx++){
            uint8_t c=src[yy*iw+(flip?iw-1-xx:xx)];
            if(c)dst[(y+yy)*W+x+xx]=silhouette?ink:c;
        }
}

int main(void)
{
    int it,checked=0;
    for(it=0;it<1000;it++){
        int w=1+(int)(rnd()%64),h=1+(int)(rnd()%64);
        int x=(int)(rnd()%(W-w+1)),y=(int)(rnd()%(H-h+1));
        int flip=(int)(rnd()&1u),silhouette=(int)(rnd()&1u);
        uint8_t ink=(uint8_t)rnd();
        int i;
        for(i=0;i<w*h;i++)src[i]=(rnd()%5u==0)?0:(uint8_t)rnd();
        for(i=0;i<(int)sizeof fast;i++)
            fast[i]=reference[i]=(uint8_t)rnd();
        if(!ps2_blit_visible_1x(fast+GUARD,W,H,x,y,src,w,h,flip,silhouette,ink)){
            printf("FAIL: valid image rejected at iteration %d\n",it);
            return 1;
        }
        oracle(reference+GUARD,x,y,w,h,flip,silhouette,ink);
        if(memcmp(fast,reference,sizeof fast)!=0){
            printf("FAIL: fast path differs at iteration %d\n",it);
            return 1;
        }
        checked++;
    }
    /* Clipped and malformed images must leave the whole frame untouched
     * so the caller can safely fall back to the generic renderer. */
    memset(fast,0x6B,sizeof fast);
    if(ps2_blit_visible_1x(fast+GUARD,W,H,-1,0,src,4,4,0,0,0) ||
       ps2_blit_visible_1x(fast+GUARD,W,H,W-2,0,src,4,4,0,0,0) ||
       ps2_blit_visible_1x(fast+GUARD,W,H,0,0,src,0,4,0,0,0) ||
       ps2_blit_visible_1x(fast+GUARD,W,H,0,0,src,W+1,4,0,0,0)) {
        puts("FAIL: invalid image accepted");
        return 1;
    }
    for(it=0;it<(int)sizeof fast;it++)
        if(fast[it]!=0x6B){puts("FAIL: invalid blit modified buffer");return 1;}
    printf("PS2 indexed 1x blit: %d parity and guard cases passed\n",checked);
    return 0;
}
