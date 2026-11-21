/* PS2-1 experimental game entry, separate from PS2-0 hardware test.
 * Runs the actual shared doomo_init()/doomo_frame(), not a debug menu.
 * First implementation has real 2D UI + portraits and a GS upload;
 * geometry and UI come from the shared core; PS2-3/4 add SPU2 audio,
 * real disc-side Talk and journaled card saving. Hardware validation,
 * performance, and complete audio/visual parity remain outstanding.
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
#if defined(PS2_SCRIPTED_STORY_PROBE)
#include "scripted_story.h"
#include "doomo/scene.h"
#endif
int ps2_memcard_init(void);
int ps2_audio_init(void);
void ps2_audio_frame(void);
void ps2_talk_disc_init(void);

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
/* GS-only boot panels exist before libmc, CDVD, PADMAN, or SPU2 RPCs.
 * If an IOP service blocks (notably on experimental HLE emulators),
 * the user still gets a visible hardware-initialization stage instead
 * of an unexplained black screen. These are a few primitives at boot,
 * not a new texture or an every-frame rendering cost. */
static void show_boot_stage(GSGLOBAL *gs,int stage)
{
    u64 bg=GS_SETREG_RGBAQ(18,11,38,0x80,0);
    u64 bar=GS_SETREG_RGBAQ(189,72,138,0x80,0);
    u64 done=GS_SETREG_RGBAQ(96,217,178,0x80,0);
    gsKit_clear(gs,bg);
    gsKit_prim_sprite(gs,0,0,640,22,1,bar);
    gsKit_prim_sprite(gs,64,183,576,264,1,bar);
    gsKit_prim_sprite(gs,72,191,568,256,2,bg);
    gsKit_prim_sprite(gs,72,191,72+((stage+1)*496)/5,256,3,done);
    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
    printf("PS2_BOOT_STAGE stage=%d\n",stage);
}
int main(void)
{
    GSGLOBAL *gs;
    unsigned field=0;
    /* The GS must start FIRST. Previously card/CDVD/audio RPCs ran
     * before even allocating a graphics context and could hang boot
     * with no signal of which subsystem failed. */
    gs=gsKit_init_global();
    if(!gs)return 1;
    gs->Mode=GS_MODE_NTSC;gs->Interlace=GS_INTERLACED;
    gs->Field=GS_FIELD;gs->Width=640;gs->Height=448;
    /* Output is 16-bit RGB, matching our native RGB5551 frame texture.
     * Preserves gsKit VRAM for streaming sprites and avoids excessive
     * 32-bit display double buffers on a real 4 MiB GS. */
    gs->PSM=GS_PSM_CT16;
    gs->PSMZ=GS_PSMZ_16S;
    gs->ZBuffering=GS_SETTING_OFF;
    gs->DoubleBuffering=GS_SETTING_ON;
    /* This game presents an opaque full-screen indexed image. In Play!,
     * enabling global GS primitive alpha caused the isolated CT32 probe
     * to go completely black; CT16 with alpha off displayed correctly.
     * Keep blending disabled until an independently verified alpha path
     * is implemented. This changes no game buffer or upload allocation. */
    gs->PrimAlphaEnable=GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs,GS_ONESHOT);
    gsKit_set_test(gs,GS_ZTEST_OFF);
    show_boot_stage(gs,0);
    SifInitRpc(0);
    show_boot_stage(gs,1);
    /* Memory card initialization before entering the game ensures the
     * shared save UI sees real media errors, not a fake in-RAM success. */
    if(!ps2_memcard_init())
        printf("PS2: no ready formatted memory card in slot 1; saves unavailable\n");
    show_boot_stage(gs,2);
    ps2_talk_disc_init();
    (void)ps2_audio_init();
    pad_open=init_pad();
    show_boot_stage(gs,3);
    if(!ps2_game2d_init(gs)){printf("PS2 GS texture memory exhausted\n");return 2;}
    doomo_init();
    show_boot_stage(gs,4);
    /* PS2 follows the familiar Cross/Circle/Square/Triangle help
     * labels until a dedicated PS2 glyph-label profile is added. */
    input_set_pad(PAD_PS1);
    input_set_feel(1);
    printf("PS2 shared-core prototype: doomo_init complete, 30Hz logic\n");
    for(;;){
        /* Input is sampled each game tick; 60Hz GS fields never advance
         * the simulation twice. Present the last complete 8bpp image. */
        if((field&1u)==0){
            uint32_t buttons;
#if defined(PS2_SCRIPTED_STORY_PROBE)
            /* This variant exercises the REAL scene/input/update code
             * with deterministic synthetic button pulses. It is never
             * packaged as the game ISO; no controller is being tested. */
            buttons=ps2_scripted_story_buttons(field/2u);
#else
            buttons=read_pad();
#endif
            plat_begin_frame();
            doomo_frame(buttons);
#if defined(PS2_SCRIPTED_STORY_PROBE)
            if(buttons){
                const DoomoScene *s=scene_top();
                printf("PS2_SCRIPTED_STORY tick=%u button=%08lx scene=%s\n",
                       field/2u,(unsigned long)buttons,
                       s && s->name ? s->name : "none");
            }
#endif
            plat_end_frame();
            ps2_audio_frame();
            ps2_game2d_present(gs,1);
            /* Sparse EE-console heartbeat (~4s at 60 fields): useful to
             * diagnose a black/frozen emulator screen or runaway uploads.
             * Hash is not computed during normal frames. */
            if((field%240u)==0u){
                uint32_t ticks,uploads,hash;
                ps2_game2d_probe(&ticks,&uploads,&hash);
                printf("PS2_FRAME_HEARTBEAT tick=%lu gs_uploads=%lu indexed_fnv=%08lx\n",
                       (unsigned long)ticks,(unsigned long)uploads,
                       (unsigned long)hash);
            }
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
