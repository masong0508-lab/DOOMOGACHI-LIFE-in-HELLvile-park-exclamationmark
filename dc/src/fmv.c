/* fmv.c - the boot FMV on the Dreamcast (DC9C; owner: dreamcast).
 *
 * main() calls dc_fmv_play() after the sound stream is up and before
 * doomo_init(): it plays /cd/INTRO.DMV (the Dippinn Productions ident, built
 * from trailer/assets/dippinn_logo.mp4 by dc/tools/mkfmv.py; format and
 * decoder: dmv.h, dmv.c) and returns. START or A skips it, and so does a disc
 * without the file (the ELF run from a cable, a build without ffmpeg) or any
 * trouble (a short file, a bad frame): the game then boots as before. It is
 * not played in a scripted harness run (DC_TEST_NO_FMV, set by dcrun unless
 * --fmv), so the frame counts and sound hashes of every other script stay as
 * they were.
 *
 * The movie is 15 fps, 320x176, drawn at 2x (640x352, bars above and below)
 * on one textured PVR quad, and mono 22,050 Hz sound. The whole file is read
 * into RAM (about 1 MB) in 16 KB steps while it plays, one step a vblank, so
 * the first frame is up a few vblanks after the file is opened. Time is the
 * SH4 timer in 30 Hz ticks, like the game's own loop:
 *   sound   tick t's 735 samples are made into the mixer's 1,470 stereo
 *           frames and handed over as one PCM slot (dcsnd_push_pcm), kept at
 *           most 3 slots ahead;
 *   video   frame f is due at tick 2 f + VIDEO_LAG (the AICA's buffer delays
 *           the sound by about that much). A late frame is skipped, never
 *           decoded: every frame stands alone.
 * Two RGB565 textures of 512x256 (not twiddled, the movie in the top left
 * corner) alternate so the PVR never reads the one being written.
 */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <kos.h>
#include <dc/pvr.h>
#include <dc/video.h>
#include <arch/timer.h>
#include "dc_plat.h"
#include "dmv.h"
#include "audio_int.h"
#include "doomo/input.h"

#define PATH        "/cd/INTRO.DMV"
#define STEP        (16u * 1024u)       /* file bytes read per vblank */
#define TICK_US     33333u
#define VIDEO_LAG   3                   /* ticks the picture waits for the sound */
#define AHEAD       3                   /* PCM slots kept queued                  */
#define TEX_W       512
#define TEX_H       256
#define MAX_REC     600

/* The working memory comes from the heap for the length of the movie and goes
 * back after it: nothing of the movie is resident (the .bss is as it was). */
typedef struct FmvMem {
    uint8_t  planes[DMV_PLANES_MAX];
    uint16_t rgb[TEX_W * DMV_MAX_H];
    int16_t  pcm[DCSND_PER_TICK * 2];
    uint32_t rec_off[MAX_REC + 1];
} FmvMem;
static FmvMem *mem;

/* tick t's sound: its half of record t / 2, 22,050 Hz mono, to 44,100 Hz stereo */
static void make_slot(const DmvInfo *in, const uint8_t *aud, int half)
{
    int n = in->apr / 2, i;                         /* samples a tick */
    const uint8_t *s = aud + (size_t)half * n * 2;
    const uint8_t *last = aud + (size_t)in->apr * 2 - 2;
    for (i = 0; i < n && i * 2 + 1 < DCSND_PER_TICK; i++) {
        int a = (int16_t)(s[i * 2] | (s[i * 2 + 1] << 8));
        const uint8_t *q = s + i * 2 + 2;
        int b = a;
        if (q <= last) b = (int16_t)(q[0] | (q[1] << 8));
        mem->pcm[i * 4] = mem->pcm[i * 4 + 1] = (int16_t)a;
        mem->pcm[i * 4 + 2] = mem->pcm[i * 4 + 3] = (int16_t)((a + b) / 2);
    }
}

static void show(pvr_ptr_t tex, const DmvInfo *in)
{
    pvr_poly_cxt_t cx;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t v;
    float x0 = 0.0f, x1 = (float)in->w * 2.0f;
    float y0 = (480.0f - (float)in->h * 2.0f) / 2.0f, y1 = y0 + (float)in->h * 2.0f;
    float u0 = 0.5f / TEX_W, u1 = ((float)in->w - 0.5f) / TEX_W;
    float v0 = 0.5f / TEX_H, v1 = ((float)in->h - 0.5f) / TEX_H;
    pvr_wait_ready();
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_poly_cxt_txr(&cx, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, TEX_W, TEX_H,
                     tex, PVR_FILTER_BILINEAR);
    cx.gen.culling = PVR_CULLING_NONE;
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    cx.depth.write = false;
    pvr_poly_compile(&hdr, &cx);
    pvr_prim(&hdr, sizeof(hdr));
    memset(&v, 0, sizeof(v));
    v.argb = 0xFFFFFFFFu;
    v.z = 1.0f;
    v.flags = PVR_CMD_VERTEX;
    v.x = x0; v.y = y0; v.u = u0; v.v = v0; pvr_prim(&v, sizeof(v));
    v.x = x1; v.y = y0; v.u = u1; v.v = v0; pvr_prim(&v, sizeof(v));
    v.x = x0; v.y = y1; v.u = u0; v.v = v1; pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x1; v.y = y1; v.u = u1; v.v = v1; pvr_prim(&v, sizeof(v));
    pvr_list_finish();
    pvr_scene_finish();
}

static void black(void)
{
    pvr_wait_ready();
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_list_finish();
    pvr_scene_finish();
}

void dc_fmv_play(void)
{
    file_t fd;
    ssize_t total;
    uint8_t *file = NULL;
    uint32_t loaded = 0, size, nrec = 0;
    DmvInfo in;
    pvr_ptr_t tex[2] = { NULL, NULL };
    uint64_t base, t_start, dec_sum = 0, dec_max = 0;
    uint32_t dec_n = 0, shown = 0, skipped = 0, apushed = 0, ticks_total, last_f = 0xFFFFFFFFu;
    int ok = 0, pad_free = 0, skipped_by_pad = 0, cur = 0;

    if (dc_mailbox.test_flags & DC_TEST_NO_FMV) {
        dc_log("DC: fmv: off (scripted run)");
        return;
    }
    fd = fs_open(PATH, O_RDONLY);
    if (fd < 0) {
        dc_log("DC: fmv: no " PATH " on the disc, no intro");
        return;
    }
    total = fs_total(fd);
    if (total < DMV_HDR_SIZE + 8 || total > 4 * 1024 * 1024) {
        dc_log("DC: fmv: bad file size, no intro");
        fs_close(fd);
        return;
    }
    size = (uint32_t)total;
    file = (uint8_t *)memalign(32, size + 32);
    mem = (FmvMem *)memalign(32, sizeof(FmvMem));
    if (!file || !mem) {
        dc_log("DC: fmv: no memory, no intro");
        free(file);
        free(mem);
        mem = NULL;
        fs_close(fd);
        return;
    }
    {
        ssize_t r = fs_read(fd, file, STEP < size ? STEP : size);
        if (r <= 0) { dc_log("DC: fmv: read error, no intro"); goto out; }
        loaded = (uint32_t)r;
    }
    if (!dmv_parse_header(file, loaded, &in) || in.frames > MAX_REC || in.fps != 15) {
        dc_log("DC: fmv: not a playable INTRO.DMV, no intro");
        goto out;
    }
    tex[0] = pvr_mem_malloc(TEX_W * TEX_H * 2);
    tex[1] = pvr_mem_malloc(TEX_W * TEX_H * 2);
    if (!tex[0] || !tex[1]) {
        dc_log("DC: fmv: no video memory, no intro");
        goto out;
    }
    memset(mem->rgb, 0, sizeof(mem->rgb));
    ticks_total = (uint32_t)in.frames * 2u + VIDEO_LAG + 1u;
    mem->rec_off[0] = DMV_HDR_SIZE;
    dc_audio_stall(1);                  /* the first reads may take a moment */
    t_start = base = timer_us_gettime64();
    dc_logf("DC: fmv: %s, %u frames %ux%u at %u fps, %u bytes", PATH, (unsigned)in.frames, (unsigned)in.w,
            (unsigned)in.h, (unsigned)in.fps, (unsigned)size);

    for (;;) {
        uint32_t tick = (uint32_t)((timer_us_gettime64() - base) / TICK_US);
        const uint8_t *v, *a;
        uint32_t vlen, next;

        /* skip: START or A, once the pad was seen free (a button held from power-on does not) */
        {
            uint32_t b = dc_read_pad();
            if (!(b & (BTN_START | BTN_A))) pad_free = 1;
            else if (pad_free) { skipped_by_pad = 1; break; }
        }
        /* the file, a step a vblank */
        if (loaded < size) {
            uint32_t want = size - loaded < STEP ? size - loaded : STEP;
            ssize_t r = fs_read(fd, file + loaded, want);
            if (r <= 0) { dc_log("DC: fmv: read error, intro stopped"); break; }
            loaded += (uint32_t)r;
            /* the reads may have taken ticks of their own: nothing to catch up on for a picture */
        }
        while (nrec < in.frames &&
               dmv_record(&in, file, loaded, mem->rec_off[nrec], &v, &vlen, &a, &next)) {
            mem->rec_off[++nrec] = next;
        }
        /* sound: tick t is half (t & 1) of record t / 2 */
        while (apushed < tick + AHEAD && (apushed >> 1) < nrec && (uint32_t)dcsnd_queued() < AHEAD) {
            if (!dmv_record(&in, file, loaded, mem->rec_off[apushed >> 1], &v, &vlen, &a, &next)) break;
            make_slot(&in, a, (int)(apushed & 1));
            if (!dcsnd_push_pcm(mem->pcm)) break;
            apushed++;
        }
        /* picture */
        if (tick >= (uint32_t)VIDEO_LAG) {
            uint32_t f = (tick - VIDEO_LAG) / 2;
            if (f < in.frames && f != last_f && f < nrec) {
                uint64_t t0 = timer_us_gettime64();
                uint32_t dt;
                if (last_f != 0xFFFFFFFFu && f > last_f + 1) skipped += f - last_f - 1;
                if (dmv_record(&in, file, loaded, mem->rec_off[f], &v, &vlen, &a, &next) &&
                    dmv_decode(&in, v, vlen, mem->planes)) {
                    dmv_to_rgb565(&in, mem->planes, mem->rgb, TEX_W);
                    cur ^= 1;
                    pvr_txr_load(mem->rgb, tex[cur], (uint32_t)TEX_W * in.h * 2);
                    show(tex[cur], &in);
                    shown++;
                    dt = (uint32_t)(timer_us_gettime64() - t0);
                    dec_sum += dt;
                    dec_n++;
                    if (dt > dec_max) dec_max = dt;
                } else {
                    dc_logf("DC: fmv: frame %u is damaged, intro stopped", (unsigned)f);
                    break;
                }
                last_f = f;
            }
        }
        if (tick >= ticks_total) { ok = 1; break; }
        vid_waitvbl();
    }
    black();
    {
        uint32_t ms = (uint32_t)((timer_us_gettime64() - t_start) / 1000u);
        dc_logf("DC: fmv: %s after %u ms: %u frames shown, %u skipped, decode %u us mean / %u us max, "
                "%u sound slots", ok ? "done" : skipped_by_pad ? "skipped by the pad" : "stopped", (unsigned)ms,
                (unsigned)shown, (unsigned)skipped, (unsigned)(dec_n ? dec_sum / dec_n : 0), (unsigned)dec_max,
                (unsigned)apushed);
    }
    /* the PCM left in the ring plays out (a skip leaves up to 3 slots, 0.1 s); a stream that is
     * not running (no sound at all) never drains it: give up after 20 vblanks */
    {
        int w;
        for (w = 0; w < 20 && dcsnd_queued() > 0; w++) vid_waitvbl();
    }
out:
    if (tex[0]) pvr_mem_free(tex[0]);
    if (tex[1]) pvr_mem_free(tex[1]);
    free(file);
    free(mem);
    mem = NULL;
    fs_close(fd);
}
