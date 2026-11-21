/* PS2-1 experimental game entry, separate from PS2-0 hardware test.
 * Runs the actual shared doomo_init()/doomo_frame(), not a debug menu.
 * First implementation has real 2D UI + portraits and a GS upload;
 * audio, 3D, world and memory-card backends remain tracked blockers.
 */
#include <stdint.h>
#include <stdio.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "doomo/doomo.h"
#include "padmap.h"
#include "game2d.h"

static unsigned char pad_dma[256] __attribute__((aligned(64)));
static int pad_open;
static int init_pad(void)
{
    SifLoadModule("rom0:SIO2MAN",0,NULL);
    SifLoadModule("rom0:PADMAN",0,NULL);
    if(padInit(0)<0)return 0;
    return padPortOpen(0,0,pad_dma)!=0;
}
static uint32_t read_pad(void)
{
    struct padButtonStatus st;
    int state;
    if(!pad_open)return 0;
    state=padGetState(0,0);
    if(state!=PAD_STATE_STABLE && state!=PAD_STATE_FINDCTP1)return 0;
    if(!padRead(0,0,&st))return 0;
    input_set_analog(1,(int)st.ljoy_h-128,(int)st.ljoy_v-128,
                      (int)st.rjoy_h-128,(int)st.rjoy_v-128,0);
    return ps2_buttons_to_core((uint16_t)~st.btns) |
           input_stick_dpad((int)st.ljoy_h-128,(int)st.ljoy_v-128);
}
int main(void)
{
    GSGLOBAL *gs;
    unsigned field=0;
    SifInitRpc(0);
    pad_open=init_pad();
    gs=gsKit_init_global();
    if(!gs)return 1;
    gs->Mode=GS_MODE_NTSC;gs->Interlace=GS_INTERLACED;
    gs->Field=GS_FIELD;gs->Width=640;gs->Height=448;
    gs->ZBuffering=GS_SETTING_OFF;
    gs->DoubleBuffering=GS_SETTING_ON;
    gs->PrimAlphaEnable=GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs,GS_ONESHOT);
    gsKit_set_test(gs,GS_ZTEST_OFF);
    if(!ps2_game2d_init(gs)){printf("PS2 GS texture memory exhausted\n");return 2;}
    doomo_init();
    printf("PS2 shared-core prototype: doomo_init complete, 30Hz logic\n");
    for(;;){
        /* Input is sampled each game tick; 60Hz GS fields never advance
         * the simulation twice. Present the last complete 8bpp image. */
        if((field&1u)==0){
            uint32_t buttons=read_pad();
            plat_begin_frame();
            doomo_frame(buttons);
            plat_end_frame();
            ps2_game2d_present(gs);
        }else{
            /* Current frame pixels remain in GS memory. This is a GS
             * redraw, never a second simulation update or a DMA upload. */
            gsKit_clear(gs,GS_SETREG_RGBAQ(12,9,20,0x80,0));
        }
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        field++;
    }
    return 0;
}
