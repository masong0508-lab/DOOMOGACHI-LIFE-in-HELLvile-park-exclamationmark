/* plat_native.c - the headless native platform: an 8bpp software
 * framebuffer of any size up to 640x480, 2D drawing, file-backed saves,
 * counted audio and logging (owner: foundation). The 3D views live in
 * raycast.c and overworld.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "plat_music_native.h"     /* --- music: musrender --- */

uint8_t nat_fb[NAT_MAX_W * NAT_MAX_H];
int nat_w = 240, nat_h = 160;
uint32_t nat_tone_calls, nat_sample_calls;
int nat_last_sample;

static uint32_t ticks;
static char save_dir[512] = ".";
static uint32_t entropy = 0x5EED1234u;
static int quiet, verbose;

#define MAX_STUBS 128
static char stub_names[MAX_STUBS][64];
static int stub_n;

void nat_init(const NatOptions *o)
{
    nat_w = DOOMO_CLAMP(o->w, 64, NAT_MAX_W);
    nat_h = DOOMO_CLAMP(o->h, 64, NAT_MAX_H);
    if (o->save_dir) {
        strncpy(save_dir, o->save_dir, sizeof(save_dir) - 1);
        save_dir[sizeof(save_dir) - 1] = 0;
    }
    entropy = o->seed;
    quiet = o->quiet;
    verbose = o->verbose;
    ticks = 0;
    memset(nat_fb, 0, sizeof(nat_fb));
}

int nat_stub_count(void) { return stub_n; }
const char *nat_stub_name(int i) { return (i >= 0 && i < stub_n) ? stub_names[i] : ""; }

/* ---- display ------------------------------------------------------------ */
int plat_screen_w(void) { return nat_w; }
int plat_screen_h(void) { return nat_h; }
void plat_begin_frame(void) { }
void plat_end_frame(void)
{
    ticks++;
    nat_wav_frame();                      /* --- music: musrender (MU1, CF-46) --- */
}

uint32_t nat_calls[NC_N];

void plat_clear(uint8_t color)
{
    nat_calls[NC_CLEAR]++;
    memset(nat_fb, color, (size_t)(nat_w * nat_h));
}

static int clip(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > nat_w) *w = nat_w - *x;
    if (*y + *h > nat_h) *h = nat_h - *y;
    return *w > 0 && *h > 0;
}

void plat_rect(int x, int y, int w, int h, uint8_t color)
{
    nat_calls[NC_RECT]++;
    int j;
    if (!clip(&x, &y, &w, &h)) return;
    for (j = 0; j < h; j++) memset(&nat_fb[(y + j) * nat_w + x], color, (size_t)w);
}

void plat_shade_rect(int x, int y, int w, int h, int level)
{
    nat_calls[NC_SHADE]++;
    int i, j;
    const uint8_t *map;
    level = DOOMO_CLAMP(level, 0, DOOMO_LIGHT_LEVELS - 1);
    map = doomo_colormap[level];
    if (!clip(&x, &y, &w, &h)) return;
    for (j = 0; j < h; j++) {
        uint8_t *p = &nat_fb[(y + j) * nat_w + x];
        for (i = 0; i < w; i++) p[i] = map[p[i]];
    }
}

void plat_checker(int x, int y, int w, int h, uint8_t color)
{
    nat_calls[NC_CHECKER]++;
    int i, j;
    for (j = 0; j < h; j++)
        for (i = j & 1; i < w; i += 2) nat_px(x + i, y + j, color);
}

void plat_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    nat_calls[NC_LINE]++;
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, n = 0;
    for (;;) {
        nat_px(x0, y0, color);
        if ((x0 == x1 && y0 == y1) || ++n > 4096) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

void plat_text(int x, int y, const char *s, uint8_t color)
{
    nat_calls[NC_TEXT]++;
    int cx = x;
    if (!s) return;
    s = lang_tr(s);
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        int col, row;
        if (ch == '\n') { cx = x; y += FONT_LINE; continue; }
        if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
        if (cx > -FONT_ADVANCE && cx < nat_w && y > -8 && y < nat_h) {
            for (col = 0; col < 5; col++) {
                uint8_t bits = doomo_font[ch - FONT_FIRST][col];
                for (row = 0; row < 8; row++)
                    if (bits & (1 << row)) nat_px(cx + col, y + row, color);
            }
        }
        cx += FONT_ADVANCE;
    }
}

void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color)
{
    nat_calls[NC_IMAGE]++;
    int ix, iy, w, h;
    if (!img || !img->pix || !img->w || !img->h) return;
    scale = DOOMO_CLAMP(scale, 1, 4);
    w = img->w; h = img->h;
    if (flags & IMG_SHADOW) {
        /* soft 1 px drop shadow down-right of every opaque pixel */
        for (iy = 0; iy < h; iy++)
            for (ix = 0; ix < w; ix++) {
                int sx = (flags & IMG_FLIPX) ? w - 1 - ix : ix;
                if (!img->pix[iy * w + sx]) continue;
                {
                    int px = x + ix * scale + scale, py = y + iy * scale + scale, a, b;
                    for (b = 0; b < scale; b++)
                        for (a = 0; a < scale; a++) {
                            int qx = px + a, qy = py + b;
                            if ((unsigned)qx < (unsigned)nat_w && (unsigned)qy < (unsigned)nat_h)
                                nat_fb[qy * nat_w + qx] = doomo_colormap[5][nat_fb[qy * nat_w + qx]];
                        }
                }
            }
    }
    for (iy = 0; iy < h; iy++) {
        for (ix = 0; ix < w; ix++) {
            int sx = (flags & IMG_FLIPX) ? w - 1 - ix : ix;
            uint8_t c = img->pix[iy * w + sx];
            if (!c) continue;
            if (flags & IMG_SILHOUETTE) c = color;
            if (scale == 1) nat_px(x + ix, y + iy, c);
            else plat_rect(x + ix * scale, y + iy * scale, scale, scale, c);
        }
    }
}

/* ---- audio: counted, optionally logged ----------------------------------- */
void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    nat_tone_calls++;
    nat_wav_tone(channel, freq_hz, vol, wave);   /* --- music: musrender --- */
    if (verbose) fprintf(stderr, "[tone] ch%d %uHz vol%u wave%u\n", channel, freq_hz, vol, wave);
}

void plat_sample(int sfx_id, uint8_t vol)
{
    nat_sample_calls++;
    nat_wav_sample(sfx_id, vol);                 /* --- music: musrender --- */
    nat_last_sample = sfx_id;
    if (verbose) fprintf(stderr, "[sfx] %d vol%u\n", sfx_id, vol);
}

/* ---- saves: one file per slot ---------------------------------------------- */
static void slot_path(int slot, char *buf, size_t cap)
{
    snprintf(buf, cap, "%s/slot%d.sav", save_dir, slot);
}

int plat_save_write(int slot, const void *data, uint32_t size)
{
    char path[600], tmp[620];
    FILE *f;
    if (slot < 0 || slot > 1) return 0;
    slot_path(slot, path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (!f) return 0;
    if (fwrite(data, 1, size, f) != size) { fclose(f); remove(tmp); return 0; }
    if (fclose(f) != 0) { remove(tmp); return 0; }
    if (rename(tmp, path) != 0) { remove(tmp); return 0; }
    return 1;
}

int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    char path[600];
    FILE *f;
    size_t n;
    if (size) *size = 0;
    if (slot < 0 || slot > 1) return 0;
    slot_path(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(data, 1, cap, f);
    fclose(f);
    if (size) *size = (uint32_t)n;
    return n > 0;
}

/* Scratch buffer (platform.h). nat_save_stream > 0 shrinks it to a
 * stream window of that many bytes (plus the codec's work area, save.h),
 * so save_game()/load_game() take the streaming path below, like the GBA
 * (tests, runner --save-stream N). */
int nat_save_stream;

void *plat_save_scratch(uint32_t *cap)
{
    static uint8_t buf[DOOMO_SAVE_SCRATCH_MIN] __attribute__((aligned(8)));
    if (cap) *cap = nat_save_stream > 0
                        ? (uint32_t)DOOMO_MIN(nat_save_stream + (int)DOOMO_SAVE_WORK_SIZE,
                                              (int)sizeof(buf))
                        : (uint32_t)sizeof(buf);
    return buf;
}

/* streaming saves: written to slotN.sav.tmp, renamed over the slot on commit */
static FILE *ws_f;
static char ws_path[600], ws_tmp[620];
static uint32_t ws_size, ws_n;
uint32_t nat_stream_writes, nat_stream_reads;

int plat_save_stream_begin(int slot, uint32_t size)
{
    if (slot < 0 || slot > 1 || ws_f || size == 0) return 0;
    slot_path(slot, ws_path, sizeof(ws_path));
    snprintf(ws_tmp, sizeof(ws_tmp), "%s.tmp", ws_path);
    ws_f = fopen(ws_tmp, "wb");
    ws_size = size;
    ws_n = 0;
    return ws_f != NULL;
}

int nat_stream_fail_at;                 /* tests: the Nth write fails (1-based) */

int plat_save_stream_write(const void *data, uint32_t n)
{
    if (!ws_f || ws_n + n > ws_size) return 0;
    if (nat_stream_fail_at > 0 && --nat_stream_fail_at == 0) return 0;
    if (fwrite(data, 1, n, ws_f) != n) return 0;
    ws_n += n;
    nat_stream_writes++;
    return 1;
}

int plat_save_stream_end(int commit)
{
    int ok;
    if (!ws_f) return 0;
    ok = fclose(ws_f) == 0;
    ws_f = NULL;
    if (commit && ok && ws_n == ws_size && rename(ws_tmp, ws_path) == 0) return 1;
    remove(ws_tmp);
    return 0;
}

int plat_save_read_at(int slot, uint32_t offset, void *data, uint32_t n, uint32_t *size)
{
    char path[600];
    FILE *f;
    long sz;
    int ok;
    if (size) *size = 0;
    if (slot < 0 || slot > 1) return 0;
    slot_path(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 0) { fclose(f); return 0; }
    if (size) *size = (uint32_t)sz;
    ok = (uint64_t)offset + n <= (uint64_t)sz && fseek(f, (long)offset, SEEK_SET) == 0 &&
         fread(data, 1, n, f) == n;
    fclose(f);
    nat_stream_reads++;
    return ok;
}

int plat_save_erase(int slot)
{
    char path[600];
    if (slot < 0 || slot > 1) return 0;
    slot_path(slot, path, sizeof(path));
    remove(path);
    return 1;
}

/* ---- misc ----------------------------------------------------------------- */
uint32_t plat_entropy(void) { return entropy; }
/* --- S2: the runner's `seed N` (SPEC C18) overrides plat_entropy() --- */
void nat_set_entropy(uint32_t v) { entropy = v; }
/* --- end S2 --- */
uint32_t plat_ticks(void) { return ticks; }
const char *plat_name(void) { return "NATIVE"; }

void plat_log(const char *msg)
{
    if (!msg) return;
    if (strncmp(msg, "STUB:", 5) == 0) {
        int i;
        for (i = 0; i < stub_n; i++)
            if (strcmp(stub_names[i], msg + 6) == 0) break;
        if (i == stub_n && stub_n < MAX_STUBS) {
            strncpy(stub_names[stub_n], msg + (msg[5] == ' ' ? 6 : 5), 63);
            stub_names[stub_n][63] = 0;
            stub_n++;
        }
    }
    if (!quiet) fprintf(stderr, "[%5u] %s\n", ticks, msg);
}
