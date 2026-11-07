/* fmv.c - the boot-up FMV: the Dippinn Productions ident (owner: ps1).
 *
 * Plays INTRO.STR (tools/mkintro.py: trailer/assets/dippinn_logo.mp4 encoded
 * as an MDEC .STR with XA-ADPCM audio) once, before anything else is shown.
 * START or Cross skips it. If INTRO.STR is not on the disc, or anything goes
 * wrong (no frame for 3 s, a bad sector), it returns at once and the game
 * boots as before. Built only with DOOMO_INTRO_FMV (CMake option).
 *
 * The pipeline is the one of PSn00bSDK's examples/mdec/strvideo: the CD IRQ
 * demuxes the frame's sectors into a bitstream buffer, the CPU turns it into
 * MDEC codes (DecDCTvlc), the MDEC decodes 16-pixel slices that its DMA
 * callback uploads to the framebuffer that is not on screen. Differences:
 *
 *  - RAM. The EXE has no room for ~340 KB of stream buffers, so they live in
 *    the display lists' memory (plat_save_scratch(), 104 KB): at boot, before
 *    the first frame, nothing uses it. The MDEC input buffer is single
 *    (the CPU decodes the next frame only after the MDEC finished the last
 *    one; at 15 fps there is time for that), the bitstream buffers are double.
 *  - the frames are drawn into the game's two framebuffers (y 0 and 240), so
 *    nothing needs to be put back; the next gfx_begin() redraws everything.
 *  - the CD / MDEC callbacks are removed again, and the drive is paused.
 */
#ifdef DOOMO_INTRO_FMV
#include <stdint.h>
#include <string.h>
#include <psxapi.h>
#include <psxetc.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxspu.h>
#include <psxcd.h>
#include <psxpress.h>
#include "ps1.h"
#include "doomo/input.h"

#define BLOCK        16                     /* MDEC slice width (15 bpp)      */
#define SECT_WORDS   (2016 / 4)             /* bitstream words per sector     */
#define BS_SECTORS   10                     /* longest frame we accept        */
#define BS_WORDS     (SECT_WORDS * BS_SECTORS)
#define MD_WORDS     0x3000                 /* MDEC codes of one frame (48 KB) */
#define SLICE_WORDS  (BLOCK * SCR_H / 2)
#define TIMEOUT_VB   180                    /* no frame for 3 s: give up      */

#define NEED_BYTES   (2 * BS_WORDS * 4 + MD_WORDS * 4 + 2 * SLICE_WORDS * 4)
typedef char fmv_fits[NEED_BYTES <= 2 * PRIM_BYTES ? 1 : -1];

typedef struct {                            /* the 32-byte header of a .STR sector */
    uint16_t magic;                         /* 0x0160                          */
    uint16_t type;                          /* 0x8001 = MDEC                   */
    uint16_t sector_id, sector_count;
    uint32_t frame_id, bs_length;
    uint16_t width, height;
    uint8_t  bs_header[8];
    uint32_t reserved;
} StrHeader;

static uint32_t *s_bs[2];                   /* bitstream, written by the CD IRQ */
static uint32_t *s_md;                      /* MDEC codes                       */
static uint32_t *s_slice[2];
static uint16_t  s_w[2], s_h[2];
static StrHeader s_hdr;                     /* DMA target: not on the stack     */

static int  s_frame_id, s_sectors_left, s_cur_frame, s_cur_slice;
static int  s_frame_w;
static RECT s_slice_pos;
static volatile int8_t s_pending, s_ready;

static void sector_handler(void)
{
    int cur = s_cur_frame;
    CdGetSector(&s_hdr, sizeof(StrHeader) / 4);
    if (s_hdr.magic != 0x0160) { s_ready = -1; return; }     /* past the end */
    if (s_hdr.type != 0x8001) return;                        /* not video    */
    if ((int)s_hdr.frame_id < s_frame_id) { s_ready = -1; return; }
    if ((int)s_hdr.frame_id > s_frame_id) {
        if (s_sectors_left <= 0 && s_frame_id >= 0)
            s_ready = 1;                    /* the frame before was complete */
        s_frame_id = (int)s_hdr.frame_id;
        s_sectors_left = s_hdr.sector_count;
        s_cur_frame ^= 1;
        cur = s_cur_frame;
        s_w[cur] = (uint16_t)((s_hdr.width + 15) & 0xfff0);
        s_h[cur] = (uint16_t)((s_hdr.height + 15) & 0xfff0);
    }
    if (s_hdr.sector_id >= BS_SECTORS || s_hdr.sector_count > BS_SECTORS) {
        s_ready = -1;                       /* does not fit our buffers      */
        return;
    }
    s_sectors_left--;
    CdGetSector(&s_bs[cur][SECT_WORDS * s_hdr.sector_id], SECT_WORDS);
}

static void mdec_dma_handler(void)
{
    if (s_pending) {                        /* a sector arrived during the DMA */
        sector_handler();
        s_pending = 0;
    }
    LoadImage(&s_slice_pos, s_slice[s_cur_slice]);
    s_cur_slice ^= 1;
    s_slice_pos.x += BLOCK;
    if (s_slice_pos.x < s_frame_w)
        DecDCTout(s_slice[s_cur_slice], BLOCK * s_slice_pos.h / 2);
}

static void cd_event_handler(CdlIntrResult event, uint8_t *payload)
{
    (void)payload;
    if (event != CdlDataReady)
        return;
    if (DecDCTinSync(1))                    /* MDEC busy: its DMA and the CD's clash */
        s_pending = 1;
    else
        sector_handler();
}

static int wait_frame(void)
{
    int t0 = VSync(-1);
    while (!s_ready) {
        if (VSync(-1) - t0 > TIMEOUT_VB)
            return 0;
    }
    if (s_ready < 0)
        return 0;
    s_ready = 0;
    return 1;
}

/* both framebuffers to black: 16-pixel strips of zeros (the slice buffer) */
static void clear_vram(void)
{
    RECT r;
    int x, y;
    memset(s_slice[0], 0, SLICE_WORDS * 4);
    for (y = 0; y < 2 * SCR_H; y += SCR_H)
        for (x = 0; x < SCR_W; x += BLOCK) {
            r.x = x; r.y = y; r.w = BLOCK; r.h = SCR_H;
            LoadImage(&r, s_slice[0]);
            DrawSync(0);
        }
}

static void show(DISPENV *d)
{
    VSync(0);
    PutDispEnv(d);
    SetDispMask(1);
}

void fmv_intro_play(void)
{
    CdlFILE file;
    DISPENV disp[2];
    uint32_t cap;
    uint8_t *mem;
    int i, shown = -1, frames = 0, drawn = 0;
    uint8_t mode = CdlModeRT | CdlModeSpeed;

    mem = (uint8_t *)plat_save_scratch(&cap);
    if (!mem || cap < NEED_BYTES)
        return;
    SpuInit();                              /* CD audio on, volume up        */
    if (!CdInit())
        return;
    if (!CdSearchFile(&file, "\\INTRO.STR;1")) {
        dbg("fmv: no INTRO.STR\n");
        return;
    }
    s_bs[0] = (uint32_t *)mem;
    s_bs[1] = s_bs[0] + BS_WORDS;
    s_md = s_bs[1] + BS_WORDS;
    s_slice[0] = s_md + MD_WORDS;
    s_slice[1] = s_slice[0] + SLICE_WORDS;

    for (i = 0; i < 2; i++) {
        SetDefDispEnv(&disp[i], 0, i * SCR_H, SCR_W, SCR_H);
#ifdef DOOMO_PAL
        disp[i].screen.y = 8;
        disp[i].screen.h = SCR_H;
#endif
    }
    DrawSync(0);
    clear_vram();                           /* black bars around the picture */

    DecDCTReset(0);
    EnterCriticalSection();
    DMACallback(1, &mdec_dma_handler);
    CdReadyCallback(&cd_event_handler);
    ExitCriticalSection();

    s_frame_id = -1; s_sectors_left = 0; s_pending = 0; s_ready = 0;
    s_cur_frame = 0; s_cur_slice = 0;
    CdSync(0, 0);
    CdControl(CdlSetmode, &mode, 0);        /* 2x, real time, XA audio on    */
    CdControl(CdlReadS, &file.pos, 0);

    while (wait_frame()) {
        VLC_Context vlc;
        int src = s_cur_frame ^ 1;          /* the frame the IRQ completed   */
        int dst = drawn & 1;                /* framebuffer to decode into    */

        if (pad_read() & (BTN_START | BTN_A))
            break;
        DecDCTinSync(0);                    /* previous frame fully decoded  */
        DecDCToutSync(0);
        if (drawn > 0) {                    /* ... so show it                */
            shown = dst ^ 1;
            show(&disp[shown]);
        }
        if (DecDCTvlcStart(&vlc, s_md, MD_WORDS, s_bs[src]))
            continue;                       /* undecodable: skip this frame  */
        DecDCTin(s_md, DECDCT_MODE_16BPP);
        s_frame_w = s_w[src];
        s_slice_pos.x = 0 + (SCR_W - s_w[src]) / 2;
        s_slice_pos.y = dst * SCR_H + (SCR_H - s_h[src]) / 2;
        s_slice_pos.w = BLOCK;
        s_slice_pos.h = s_h[src];
        s_frame_w = s_slice_pos.x + s_w[src];
        DecDCTout(s_slice[s_cur_slice], BLOCK * s_slice_pos.h / 2);
        drawn++;
        frames++;
    }
    if (drawn > 0) {                        /* the last decoded frame        */
        DecDCTinSync(0);
        DecDCToutSync(0);
        show(&disp[(drawn - 1) & 1]);
        for (i = 0; i < 20 && !(pad_read() & (BTN_START | BTN_A)); i++)
            VSync(0);                       /* hold the wordmark a moment    */
    }
    (void)shown;

    CdControlB(CdlPause, 0, 0);
    EnterCriticalSection();
    CdReadyCallback((CdlCB)0);
    DMACallback(1, (void (*)(void))0);
    ExitCriticalSection();
    DecDCTReset(0);
    clear_vram();                           /* hand over a black VRAM        */
    dbg("fmv: intro played, %d frames\n", frames);
}
#endif /* DOOMO_INTRO_FMV */
