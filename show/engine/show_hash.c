/* show_hash.c - P1 state CRC and P4 audio-event ids (owner: ENGINE lane 1;
 * spec 8.9). P2 (the draw-call CRC) is deferred: p2_draw_crc stays 0.
 *
 * P1 = CRC32 (zlib polynomial, so Python's zlib.crc32 checks it) of a
 * fixed-layout core record, then of every linked module's hash() output in
 * registry order. The core record: shot, frame, beat_now, the 32 label ages,
 * the 8 actor clocks, the camfx read-backs (fade, flash, iris radius, zoom,
 * palette lerp, shake offset), the RNG state, the plane visibility mask,
 * then the host-kind state of show_ext.c (actor channels, pops, tint, marks).
 */
#include <string.h>
#include "show_core.h"

static uint32_t s_crc_tab[256];

static void crc_init(void)
{
    uint32_t i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        s_crc_tab[i] = c;
    }
}

uint32_t show_crc32(uint32_t crc, const void *data, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    if (!s_crc_tab[1]) crc_init();
    crc = ~crc;
    while (n--) crc = s_crc_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void show_crc_add(ShowCrc *c, const void *data, uint32_t n)
{
    c->v = show_crc32(c->v, data, n);
}

typedef struct P1Core {                 /* fixed layout, 152 B */
    int32_t  shot, t, beat_now;
    int16_t  age[BEAT_LABEL_MAX];
    int16_t  clock[BEAT_ACTORS];
    int32_t  fade, flash, iris_r, zoom, pal_id, pal_level, off_x, off_y;
    uint32_t rng, vis;
} P1Core;

uint32_t show_p1_frame(void)
{
    P1Core r;
    ShowCrc c = { 0 };
    int i, n;
    memset(&r, 0, sizeof r);
    r.shot = g_show_shot.index;
    r.t = g_show_shot.t;
    r.beat_now = beat_now();
    for (i = 0; i < BEAT_LABEL_MAX; i++) r.age[i] = (int16_t)beat_age(i);
    for (i = 0; i < BEAT_ACTORS; i++) r.clock[i] = (int16_t)beat_clock(i);
    r.fade = camfx_fade_level();
    r.flash = camfx_flash_level();
    r.iris_r = camfx_iris_radius();
    r.zoom = camfx_zoom();
    r.pal_id = camfx_pal_state(&r.pal_level);
    camfx_offset(&r.off_x, &r.off_y);
    r.rng = show_rng_state();
    r.vis = show_plane_vis_mask();
    show_crc_add(&c, &r, sizeof r);
    show_ext_hash(&c);
    n = show_module_count();
    for (i = 0; i < n; i++) {
        const ShowModule *m = show_module(i);
        if (m->hash) m->hash(&c);
    }
    return c.v;
}

/* ---- P4 ----------------------------------------------------------------- */
void show_p4_reset_frame(void)
{
    g_show_mbox.p4_n = 0;
}

void show_p4_fire(uint32_t k)
{
    if (g_show_mbox.p4_n < SHOW_P4_MAX) g_show_mbox.p4_k[g_show_mbox.p4_n++] = (uint16_t)k;
    else g_show_mbox.p4_overflow++;
}

void show_p4_fire_due(int t)
{
    int n, i;
    const ShwAudioK *k = (const ShwAudioK *)show_chunk_find(SHWR_AUDIO_K, &n, NULL);
    for (i = 0; k && i < n; i++)
        if (k[i].frame == t) show_p4_fire(k[i].k);
}
