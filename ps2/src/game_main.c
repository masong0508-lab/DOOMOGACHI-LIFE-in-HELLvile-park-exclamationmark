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
int ps2_memcard_init(void);

static unsigned char pad_dma[256] __attribute__((aligned(64)));
static int pad_open;
static int pad_ready;  /* library loaded; the physical pad port can be retried */
static int init_pad(void)
{
    SifLoadModule("rom0:SIO2MAN",0,NULL);
    SifLoadModule("rom0:PADMAN",0,NULL);
    if(padInit(0)<0)return 0;
    pad_ready=1;
    return padPortOpen(0,0,pad_dma)!=0;
}
static uint32_t read_pad(void)
{
    struct padButtonStatus st;
    int state,lx,ly,rx,ry;
    uint32_t stick;
    static unsigned retry_tick;
    /* A disconnected or resynchronizing DualShock must also release
     * its previous analog vector. Otherwise a scene can keep walking
     * after unplugging the controller, despite zero digital buttons. */
    if(!pad_open){
        /* A disconnected pad at first boot must not permanently lock the
         * real game out of its menus. Retry opening the port once per
         * second of 30Hz game ticks; never reload PADMAN each frame. */
        if(pad_ready && (++retry_tick%30u)==0u)
            pad_open=padPortOpen(0,0,pad_dma)!=0;
        goto absent;
    }
    state=padGetState(0,0);
    if(state!=PAD_STATE_STABLE && state!=PAD_STATE_FINDCTP1)goto absent;
    if(!padRead(0,0,&st))goto absent;
    lx=(int)st.ljoy_h-128; ly=(int)st.ljoy_v-128;
    rx=(int)st.rjoy_h-128; ry=(int)st.rjoy_v-128;
    stick=input_stick_dpad(lx,ly);
    /* Report which digital directions are stick-generated so the core
     * can distinguish analog walking from D-pad presses. */
    input_set_analog(1,lx,ly,rx,ry,stick);
    return ps2_buttons_to_core((uint16_t)~st.btns) | stick;
absent:
    input_set_analog(0,0,0,0,0,0);
    return 0;
}
int main(void)
{
    GSGLOBAL *gs;
    unsigned field=0;
    SifInitRpc(0);
    /* Memory card initialization before entering the game ensures the
     * shared save UI sees real media errors, not a fake in-RAM success. */
    if(!ps2_memcard_init())
        printf("PS2: no ready formatted memory card in slot 1; saves unavailable\n");
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
    /* PS2 follows the familiar Cross/Circle/Square/Triangle help
     * labels until a dedicated PS2 glyph-label profile is added. */
    input_set_pad(PAD_PS1);
    input_set_feel(1);
    printf("PS2 shared-core prototype: doomo_init complete, 30Hz logic\n");
    for(;;){
        /* Input is sampled each game tick; 60Hz GS fields never advance
         * the simulation twice. Present the last complete 8bpp image. */
        if((field&1u)==0){
            uint32_t buttons=read_pad();
            plat_begin_frame();
            doomo_frame(buttons);
            plat_end_frame();
            ps2_game2d_present(gs,1);
        }else{
            /* Draw the same completed image from GS VRAM: no simulation
             * tick or upload on alternate fields, and no black flicker. */
            ps2_game2d_present(gs,0);
        }
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        field++;
    }
    return 0;
}
