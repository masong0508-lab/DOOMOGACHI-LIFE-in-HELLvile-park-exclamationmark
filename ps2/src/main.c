/* PlayStation 2 Deluxe - PS2-0 hardware bring-up.
 *
 * Native PS2 homebrew ELF, EE + GS + PADMAN (not PS1 backwards mode).
 * This phase intentionally DOES NOT pretend to run the shared game:
 * PS2-1 will replace the diagnostic views with doomo_init/doomo_frame and
 * a pixel-accurate software/GS backend. A real boot, pad, video and
 * predictable 30 Hz logic divider are exercised here.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "padmap.h"
#include "doomo/input.h"

#define VIEW_W 640
#define VIEW_H 448
#define FONT_SCALE 2
#define PAD_PORT 0
#define PAD_SLOT 0
static unsigned char s_pad_area[256] __attribute__((aligned(64)));
static int s_pad_open;
static unsigned s_vblanks, s_logic_ticks;
static uint32_t s_held, s_edges;
static int s_selected, s_show_details;

/* Tiny fixed 5x7 ROM-free diagnostic lettering; one entry per glyph,
 * no font asset, dynamic malloc or framebuffer. Only PS2-0 uses this;
 * normal game text will come from the shared font and GS palette cache. */
typedef struct Glyph { char c; uint8_t row[7]; } Glyph;
static const Glyph glyphs[] = {
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{14,4,4,4,4,4,14}},      {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}},   {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}},     {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}},     {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}},    {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {':',{0,4,4,0,4,4,0}}, {'-',{0,0,0,31,0,0,0}},
    {'/',{1,1,2,4,8,16,16}}, {'.',{0,0,0,0,0,12,12}},
    {'>',{16,8,4,2,4,8,16}}, {' ',{0,0,0,0,0,0,0}}
};

static u64 rgb(unsigned r, unsigned g, unsigned b)
{
    return GS_SETREG_RGBAQ(r,g,b,0x80,0);
}
static void rect(GSGLOBAL *gs, int x, int y, int w, int h, u64 color)
{
    if (w <= 0 || h <= 0) return;
    gsKit_prim_sprite(gs, (float)x, (float)y, (float)(x+w), (float)(y+h), 1, color);
}
static const uint8_t *letter(char c)
{
    unsigned i;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (i=0; i<sizeof(glyphs)/sizeof(glyphs[0]); ++i)
        if (glyphs[i].c==c) return glyphs[i].row;
    return glyphs[sizeof(glyphs)/sizeof(glyphs[0])-1].row;
}
static void label(GSGLOBAL *gs,int x,int y,const char *s,u64 color,int scale)
{
    while (*s) {
        int r,c;
        const uint8_t *g=letter(*s++);
        for(r=0;r<7;r++) for(c=0;c<5;c++)
            if(g[r] & (1u<<(4-c)))
                rect(gs,x+c*scale,y+r*scale,scale,scale,color);
        x+=6*scale;
    }
}
static int ps2_pad_init(void)
{
    int a=SifLoadModule("rom0:SIO2MAN",0,NULL);
    int b=SifLoadModule("rom0:PADMAN",0,NULL);
    if(a<0 || b<0) printf("[PS2] pad modules: SIO2=%d PAD=%d\n",a,b);
    if(padInit(0)<0) return 0;
    return padPortOpen(PAD_PORT,PAD_SLOT,s_pad_area) != 0;
}
static uint32_t ps2_pad_poll(void)
{
    struct padButtonStatus st;
    int state;
    if(!s_pad_open) return 0;
    state=padGetState(PAD_PORT,PAD_SLOT);
    if(state!=PAD_STATE_STABLE && state!=PAD_STATE_FINDCTP1) return 0;
    if(!padRead(PAD_PORT,PAD_SLOT,&st)) return 0;
    return ps2_buttons_to_core((uint16_t)~st.btns);
}
static void draw_boot(GSGLOBAL *gs)
{
    const char *const rows[]={
        "VIDEO OUTPUT",
        "DUALSHOCK 2 INPUT",
        "30 HZ LOGIC CLOCK",
        "SHARED CORE HANDOFF"
    };
    int i;
    char msg[64];
    u64 paper=rgb(245,235,216), dim=rgb(146,159,186), violet=rgb(108,55,133),
        dark=rgb(27,21,40), rose=rgb(239,111,158), mint=rgb(128,229,199);
    gsKit_clear(gs,dark);
    rect(gs,0,0,VIEW_W,20,violet);
    rect(gs,0,20,VIEW_W,3,rose);
    label(gs,48,45,"DOOMOGACHI",paper,4);
    label(gs,48,94,"PLAYSTATION 2 ENGINE",mint,FONT_SCALE);
    label(gs,48,121,"PS2-0 HARDWARE BOOT",dim,FONT_SCALE);
    rect(gs,44,156,552,204,rgb(45,35,58));
    for(i=0;i<4;i++){
        int y=170+i*46;
        if(i==s_selected){
            rect(gs,52,y-5,536,37,violet);
            rect(gs,52,y-5,5,37,rose);
        }
        label(gs,72,y,rows[i],i==s_selected?paper:dim,FONT_SCALE);
    }
    snprintf(msg,sizeof msg,"PAD: %s",s_pad_open?"CONNECTED":"NOT OPEN");
    label(gs,48,374,msg,s_pad_open?mint:rose,FONT_SCALE);
    snprintf(msg,sizeof msg,"FRAME: %u  TICK: %u",s_vblanks,s_logic_ticks);
    label(gs,48,399,msg,paper,FONT_SCALE);
    if(s_show_details) {
        snprintf(msg,sizeof msg,"BUTTONS: %04X",(unsigned)(s_held & 0xffffu));
        label(gs,48,423,msg,mint,FONT_SCALE);
    }else{
        label(gs,48,423,"CROSS: DETAILS  START: TOGGLE",dim,FONT_SCALE);
    }
}
int main(int argc,char **argv)
{
    GSGLOBAL *gs;
    (void)argc; (void)argv;
    /* Bring up the video hardware FIRST. In a BIOS-free emulator, an
     * unsupported SIF/PADMAN RPC might otherwise stall the application
     * before the GS has ever displayed a test frame. */
    gs=gsKit_init_global();
    if(!gs) { printf("[PS2] GS init allocation failed\n"); return 1; }
    gs->Mode=GS_MODE_NTSC;
    gs->Interlace=GS_INTERLACED;
    gs->Field=GS_FIELD;
    gs->Width=VIEW_W;
    gs->Height=VIEW_H;
    gs->ZBuffering=GS_SETTING_OFF;
    gs->DoubleBuffering=GS_SETTING_ON;
    gs->PrimAlphaEnable=GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_set_test(gs,GS_ZTEST_OFF);
    gsKit_mode_switch(gs,GS_ONESHOT);
    /* Paint a complete real GS frame before any IOP module RPC.
     * This separates graphics bring-up from controller/SIF startup in
     * Play! captures. It is the existing PS2-0 diagnostic view. */
    draw_boot(gs);
    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
    gsKit_queue_reset(gs->Os_Queue);
    SifInitRpc(0);
    s_pad_open=ps2_pad_init();
    printf("[PS2] DOOMOGACHI PS2-0 boot - GS 640x448 NTSC, PAD=%d\n",s_pad_open);
    for(;;){
        uint32_t b=ps2_pad_poll();
        s_edges=b & ~s_held;
        s_held=b;
        if(s_edges & BTN_UP) s_selected=(s_selected+3)%4;
        if(s_edges & BTN_DOWN) s_selected=(s_selected+1)%4;
        if(s_edges & BTN_A) s_show_details^=1;
        if(s_edges & BTN_B) s_show_details=0;
        if(s_edges & BTN_START) s_show_details^=1;
        /* The video path runs at field cadence; simulation advances at
         * exactly one 30Hz tick per two 60Hz NTSC fields, never per draw. */
        if((s_vblanks & 1u)==0) ++s_logic_ticks;
        draw_boot(gs);
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        gsKit_queue_reset(gs->Os_Queue);
        ++s_vblanks;
        if((s_vblanks % 600u)==0)
            printf("[PS2] video fields=%u logic ticks=%u pad=%04x\n",
                   s_vblanks,s_logic_ticks,(unsigned)(s_held&0xffff));
        if((s_held & (BTN_START|BTN_SELECT))==(BTN_START|BTN_SELECT)) break;
    }
    if(s_pad_open) padPortClose(PAD_PORT,PAD_SLOT);
    padEnd();
    printf("[PS2] clean exit\n");
    return 0;
}
