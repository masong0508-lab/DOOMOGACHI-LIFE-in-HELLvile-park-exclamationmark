/* Isolate the actual PS2 game CT16 GS texture path from all IOP services.
 * The 320x240 packed RGB5551 texture is the only thing drawn on the
 * black background. Visible Play! pixels come from texture DMA.
 * Diagnostic ELF only. Do not include this in the gameplay disc.
 */
#include <stdint.h>
#include <string.h>
#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "game2d.h"

enum { W=PS2_GAME_W, H=PS2_GAME_H };
static u16 pixels[W*H] __attribute__((aligned(64)));
static GSTEXTURE tex;

static u16 rgb5(unsigned r,unsigned g,unsigned b)
{
    return (u16)(0x8000u|(r&31u)|((g&31u)<<5)|((b&31u)<<10));
}
static void repaint(unsigned phase)
{
    unsigned x,y;
    unsigned bar=(phase%7u)*34u;
    u16 dark=rgb5(4,19,30),light=rgb5(31,11,23);
    u16 gold=rgb5(31,29,2);
    for(y=0;y<H;y++){
        for(x=0;x<W;x++){
            u16 c=(((x/32u)^(y/32u))&1u)?light:dark;
            if(x>=bar && x<bar+48u && y>=64u && y<172u)c=gold;
            pixels[y*W+x]=c;
        }
    }
}
int main(void)
{
    GSGLOBAL *gs=gsKit_init_global();
    unsigned field=0;
    if(!gs)return 1;
    gs->Mode=GS_MODE_NTSC;
    gs->Interlace=GS_INTERLACED;
    gs->Field=GS_FIELD;
    gs->Width=640;
    gs->Height=448;
    gs->PSM=GS_PSM_CT16;
    gs->PSMZ=GS_PSMZ_16S;
    gs->ZBuffering=GS_SETTING_OFF;
    gs->DoubleBuffering=GS_SETTING_ON;
    /* The earlier alpha-on GS-only A/B control was completely black. */
    gs->PrimAlphaEnable=GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs,GS_ONESHOT);
    gsKit_set_test(gs,GS_ZTEST_OFF);
    memset(&tex,0,sizeof tex);
    memset(pixels,0,sizeof pixels);
    tex.Width=W;tex.Height=H;
    tex.PSM=GS_PSM_CT16;
    tex.Filter=GS_FILTER_NEAREST;
    tex.Mem=(u32 *)(void *)pixels;
    tex.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(W,H,tex.PSM),
                              GSKIT_ALLOC_USERBUFFER);
    if(tex.Vram==GSKIT_ALLOC_ERROR)return 2;
    gsKit_set_clamp(gs,GS_CMODE_CLAMP);
    for(;;){
        if(field%60u==0u){
            repaint(field/60u);
            gsKit_texture_upload(gs,&tex);
        }
        /* The GS clear is deliberately black: only CT16 texels may pass
         * the visible-pixel gate, and the moving bar must change ROI hash. */
        gsKit_clear(gs,GS_SETREG_RGBAQ(0,0,0,0x80,0));
        gsKit_prim_sprite_texture(gs,&tex,
                                  22.0f,0.0f,0.0f,0.0f,
                                  618.0f,448.0f,
                                  (float)W,(float)H,1,
                                  GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        field++;
    }
}
