/* Exact-content GS upload suppression:
 * - first frame uploads; identical frames need no 256KiB DMA
 * - one changed pixel is enough to upload
 * - 512x256 padding and guard words remain untouched
 * - 1000 randomized tiles agree with reference conversion.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "indexed_present.h"
enum {W=320,H=240,TW=512,TH=256,GUARD=16};
static uint8_t frame[W*H];
static uint16_t texture[TW*TH+GUARD],colors[256];
static uint32_t seed=0x2345FE12u;
static int failures,checks;
#define CHECK(p) do{checks++;if(!(p)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#p);failures++;}}while(0)
static uint32_t rand32(void)
{
    seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
    return seed;
}
static void pad_untouched(void)
{
    int x,y;
    for(y=0;y<TH;y++){
        int start=y<H?W:0;
        for(x=start;x<TW;x++)CHECK(texture[y*TW+x]==0x5A5Au);
    }
    for(x=0;x<GUARD;x++)CHECK(texture[TW*TH+x]==0xBEEFu);
}
int main(void)
{
    int i,x,y;
    for(i=0;i<256;i++)colors[i]=(uint16_t)(0x8000u|((uint16_t)i<<5));
    memset(frame,0,sizeof frame);
    for(i=0;i<TW*TH;i++)texture[i]=0x5A5A;
    for(i=0;i<GUARD;i++)texture[TW*TH+i]=0xBEEF;
    CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==1);
    CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==0);
    frame[47*W+53]=124;
    CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==1);
    CHECK(texture[47*TW+53]==colors[124]);
    CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==0);
    frame[47*W+53]=0;
    CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==1);
    CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==0);
    CHECK(ps2_indexed_update(NULL,W,texture,TW,W,H,colors)==0);
    CHECK(ps2_indexed_update(frame,W,texture,W-1,W,H,colors)==0);
    CHECK(ps2_indexed_update(frame,W,texture,TW,0,H,colors)==0);
    for(i=0;i<1000;i++){
        uint8_t index=(uint8_t)rand32();
        x=(int)(rand32()%W);
        y=(int)(rand32()%H);
        frame[y*W+x]=index;
        /* Any mutation changing the resulting palette color triggers
         * upload. An unchanged index is a correct no-op. */
        {
            int expected=texture[y*TW+x]!=colors[index];
            int dirty=ps2_indexed_update(frame,W,texture,TW,W,H,colors);
            CHECK(dirty==expected);
            CHECK(texture[y*TW+x]==colors[index]);
            CHECK(ps2_indexed_update(frame,W,texture,TW,W,H,colors)==0);
        }
    }
    for(y=0;y<H;y++)for(x=0;x<W;x++)
        CHECK(texture[y*TW+x]==colors[frame[y*W+x]]);
    pad_untouched();
    printf("PS2 exact GS frame update: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
