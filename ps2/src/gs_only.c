/* PS2 GS-only execution probe. No IOP modules, SIF RPC, PADMAN, libmc,
 * CDVD, SPU2, DMA upload texture, or shared core at ANY point. Instead
 * draw high-contrast changing colors with gsKit native GS commands.
 *
 * If this ELF is also black in an emulator while status shows FPS,
 * it is a graphics/ELF-emulation setup issue and NOT evidence against
 * the game's Talk, memory card, or audio mechanics.
 * Never package this diagnostic as a gameplay executable.
 */
#include <stdint.h>
#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>

int main(void)
{
    GSGLOBAL *gs=gsKit_init_global();
    uint32_t field=0;
    if(!gs)return 1;
    gs->Mode=GS_MODE_NTSC;
    gs->Interlace=GS_INTERLACED;
    gs->Field=GS_FIELD;
    gs->Width=640;
    gs->Height=448;
    gs->PSM=GS_PSM_CT32;
    gs->ZBuffering=GS_SETTING_OFF;
    gs->DoubleBuffering=GS_SETTING_ON;
    gs->PrimAlphaEnable=GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_set_test(gs,GS_ZTEST_OFF);
    gsKit_mode_switch(gs,GS_ONESHOT);

    for(;;){
        u64 bg=(field/60u)&1u
             ?GS_SETREG_RGBAQ(22,69,131,0x80,0)
             :GS_SETREG_RGBAQ(103,27,69,0x80,0);
        u64 white=GS_SETREG_RGBAQ(245,245,224,0x80,0);
        u64 yellow=GS_SETREG_RGBAQ(251,209,39,0x80,0);
        float x=40.0f+(float)((field*3u)%440u);
        gsKit_clear(gs,bg);
        gsKit_prim_sprite(gs,40,60,600,92,1,white);
        gsKit_prim_sprite(gs,x,180,x+96,310,1,yellow);
        gsKit_prim_sprite(gs,40,365,600,385,1,white);
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        ++field;
    }
}
