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
#if defined(PS2_CADENCE)
#include <timer.h>
#include "cadence.h"
#endif
#if defined(PS2_SCRIPTED_STORY_PROBE)
#include "scripted_story.h"
#include "doomo/scene.h"
#endif
int ps2_memcard_init(void);
int ps2_audio_init(void);
void ps2_audio_frame(void);
void ps2_talk_disc_init(void);

#if defined(PS2_CADENCE)
/* The INTC event is independent of gsKit's polling of the GS CSR. Read
 * only this aligned, single-writer counter from the gameplay thread. */
static volatile uint32_t cadence_vblanks;
static int cadence_handler = -1;
static Ps2CadenceWindow cadence_window;
static uint64_t cadence_start_ticks, cadence_marks[PS2_CADENCE_PHASES];
static uint32_t cadence_start_logic, cadence_start_uploads;
static unsigned cadence_reports;

static int count_vblank(int cause)
{
    (void)cause;
    cadence_vblanks++;
    ExitHandler();
    return 0;
}

static void cadence_reset_window(void)
{
    ps2_game2d_probe(&cadence_start_logic,&cadence_start_uploads,NULL);
    cadence_start_ticks=GetTimerSystemTime();
    cadence_window=(Ps2CadenceWindow){.last_vblank=cadence_vblanks};
}

static void cadence_start(void)
{
    cadence_handler=AddIntcHandler(INTC_VBLANK_S,count_vblank,0);
    if(cadence_handler<0){
        printf("PS2_CADENCE unavailable=vblank_handler code=%d\n",cadence_handler);
        return;
    }
    EnableIntc(INTC_VBLANK_S);
    cadence_reset_window();
}

static void cadence_stop(void)
{
    RemoveIntcHandler(INTC_VBLANK_S,cadence_handler);
    cadence_handler=-1;
}

static unsigned long long cadence_us(uint64_t ticks)
{
    return ticks*1000000u/kBUSCLK;
}

static void cadence_finish(unsigned field,int flipped)
{
    uint64_t phase_ticks[PS2_CADENCE_PHASES]={0};
    uint64_t now,elapsed,hold_ticks=0;
    uint32_t vblanks,logic,uploads;
    const Ps2CadenceWindow *w=&cadence_window;

    if(cadence_handler<0)return;
    now=GetTimerSystemTime();
    vblanks=cadence_vblanks;
    if(now<=cadence_marks[PS2_CADENCE_UPDATE]){
        printf("PS2_CADENCE unavailable=timer\n");
        cadence_stop();
        return;
    }
    if(flipped){
        if((field&1u)==0){
            phase_ticks[PS2_CADENCE_UPDATE]=cadence_marks[PS2_CADENCE_AUDIO]-cadence_marks[PS2_CADENCE_UPDATE];
            phase_ticks[PS2_CADENCE_AUDIO]=cadence_marks[PS2_CADENCE_PREPARE]-cadence_marks[PS2_CADENCE_AUDIO];
            phase_ticks[PS2_CADENCE_PREPARE]=cadence_marks[PS2_CADENCE_FLIP]-cadence_marks[PS2_CADENCE_PREPARE];
        }else{
            phase_ticks[PS2_CADENCE_PREPARE]=cadence_marks[PS2_CADENCE_FLIP]-cadence_marks[PS2_CADENCE_UPDATE];
        }
        phase_ticks[PS2_CADENCE_FLIP]=now-cadence_marks[PS2_CADENCE_FLIP];
    }else{
        hold_ticks=now-cadence_marks[PS2_CADENCE_UPDATE];
    }
    ps2_cadence_record(&cadence_window,vblanks,flipped,phase_ticks,hold_ticks);
    elapsed=now-cadence_start_ticks;
    if(elapsed<4ull*kBUSCLK)return;

    ps2_game2d_probe(&logic,&uploads,NULL);
    printf("PS2_CADENCE mode=%s window=%u elapsed_us=%llu vblank_irqs=%lu "
           "logic_ticks=%lu gs_uploads=%lu gs_flips=%lu refresh_holds=%lu "
           "skipped_refreshes=%lu max_vblank_gap=%lu zero_vblank_iterations=%lu "
           "update_draw_max_us=%llu audio_max_us=%llu frame_prepare_max_us=%llu "
           "gs_submit_wait_max_us=%llu hold_wait_max_us=%llu\n",
#if defined(PS2_PROGRESSIVE)
           "480p",
#else
           "480i",
#endif
           ++cadence_reports,cadence_us(elapsed),(unsigned long)w->vblanks,
           (unsigned long)(logic-cadence_start_logic),
           (unsigned long)(uploads-cadence_start_uploads),
           (unsigned long)w->flips,(unsigned long)w->holds,
           (unsigned long)w->skipped_refreshes,(unsigned long)w->max_vblank_gap,
           (unsigned long)w->zero_vblank_iterations,
           cadence_us(w->max_ticks[PS2_CADENCE_UPDATE]),
           cadence_us(w->max_ticks[PS2_CADENCE_AUDIO]),
           cadence_us(w->max_ticks[PS2_CADENCE_PREPARE]),
           cadence_us(w->max_ticks[PS2_CADENCE_FLIP]),cadence_us(w->max_hold_ticks));
    /* Keep console output out of the measurement window. */
    if(cadence_reports>=300)cadence_stop();
    else cadence_reset_window();
}

#define CADENCE_MARK(phase) do { \
    if(cadence_handler>=0)cadence_marks[phase]=GetTimerSystemTime(); \
} while(0)
#define CADENCE_FINISH(field,flipped) cadence_finish(field,flipped)
#else
#define CADENCE_MARK(phase) ((void)0)
#define CADENCE_FINISH(field,flipped) ((void)0)
#endif

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
    unsigned len;
    uint32_t buttons,stick;
    static unsigned retry_tick;
    static int analog_checked;
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
    if(state==PAD_STATE_DISCONN)analog_checked=0;
    if(state!=PAD_STATE_STABLE && state!=PAD_STATE_FINDCTP1)goto absent;
    len=padRead(0,0,&st);
    if(len<4)goto absent;
    buttons=ps2_buttons_to_core((uint16_t)~st.btns);
    if(!ps2_pad_has_sticks(st.mode,len)){
        if(!analog_checked && state==PAD_STATE_STABLE && st.mode==0x41){
            int i,modes=padInfoMode(0,0,PAD_MODETABLE,-1);
            analog_checked=1;  /* do not keep reconfiguring a digital-only pad */
            for(i=0;i<modes && i<16;i++){
                if(padInfoMode(0,0,PAD_MODETABLE,i)!=PAD_TYPE_DUALSHOCK)continue;
                (void)padSetMainMode(0,0,PAD_MMODE_DUALSHOCK,PAD_MMODE_LOCK);
                break;         /* PADMAN applies the request asynchronously */
            }
        }
        input_set_analog(0,0,0,0,0,0);
        return buttons;
    }
    lx=(int)st.ljoy_h-128; ly=(int)st.ljoy_v-128;
    rx=(int)st.rjoy_h-128; ry=(int)st.rjoy_v-128;
    stick=input_stick_dpad(lx,ly);
    /* Report which digital directions are stick-generated so the core
     * can distinguish analog walking from D-pad presses. */
    input_set_analog(1,lx,ly,rx,ry,stick);
    return buttons | stick;
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
    u64 bg=GS_SETREG_RGBAQ(25,16,44,0x80,0);
    u64 shadow=GS_SETREG_RGBAQ(58,37,76,0x80,0);
    u64 rose=GS_SETREG_RGBAQ(221,87,153,0x80,0);
    u64 light=GS_SETREG_RGBAQ(255,177,214,0x80,0);
    u64 done=GS_SETREG_RGBAQ(140,237,195,0x80,0);
    const int mid=gs->Height/2;
    int i;
    gsKit_clear(gs,bg);
    gsKit_prim_sprite(gs,0,mid+82,640,gs->Height,1,shadow);
    gsKit_prim_sprite(gs,228,mid-89,412,mid+77,1,shadow);
    gsKit_prim_sprite(gs,294,mid-58,314,mid-46,2,light);
    gsKit_prim_sprite(gs,326,mid-58,346,mid-46,2,light);
    gsKit_prim_sprite(gs,287,mid-46,353,mid-24,2,rose);
    gsKit_prim_sprite(gs,294,mid-24,346,mid-13,2,rose);
    gsKit_prim_sprite(gs,303,mid-13,337,mid-2,2,rose);
    gsKit_prim_sprite(gs,312,mid-2,328,mid+9,2,rose);
    gsKit_prim_sprite(gs,252,mid+38,388,mid+54,2,bg);
    for(i=0;i<5;i++)
        gsKit_prim_sprite(gs,258+i*26,mid+42,278+i*26,mid+50,3,
                          i<=stage?done:shadow);
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
    gs->Width=640;
#if defined(PS2_PROGRESSIVE)
    gs->Mode=GS_MODE_DTV_480P;gs->Interlace=GS_NONINTERLACED;
    gs->Field=GS_FRAME;gs->Height=480;
#else
    gs->Mode=GS_MODE_NTSC;gs->Interlace=GS_INTERLACED;
    gs->Field=GS_FIELD;gs->Height=448;
#endif
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
#if defined(PS2_CADENCE)
    cadence_start();
#endif
    for(;;){
        CADENCE_MARK(PS2_CADENCE_UPDATE);
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
            CADENCE_MARK(PS2_CADENCE_AUDIO);
            ps2_audio_frame();
            CADENCE_MARK(PS2_CADENCE_PREPARE);
            ps2_game2d_present(gs,1);
#if !defined(PS2_CADENCE)
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
#endif
        }else{
#if defined(PS2_PROGRESSIVE)
            /* Progressive scanout keeps showing the completed front
             * buffer. Wait for this refresh without drawing a duplicate
             * into the back buffer or flipping it. */
            gsKit_vsync_wait();
            CADENCE_FINISH(field,0);
            field++;
            continue;
#else
            /* Draw the same completed image from GS VRAM: no simulation
             * tick or upload on the alternate interlaced field. */
            ps2_game2d_present(gs,0);
#endif
        }
        CADENCE_MARK(PS2_CADENCE_FLIP);
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        CADENCE_FINISH(field,1);
        field++;
    }
    return 0;
}
