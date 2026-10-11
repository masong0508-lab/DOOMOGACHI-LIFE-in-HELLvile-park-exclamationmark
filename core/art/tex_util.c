/* tex_util.c - small shared helpers for the texture generators. */
#include "art.h"

void tx_brick(int x, int y, int bw, int bh, int stagger, int mortar, TxCell *c)
{
    int row = y / bh, sx;
    c->row = row;
    sx = x + ((row & 1) ? stagger : 0);
    c->col = sx / bw;
    c->lx = sx % bw;
    c->ly = y % bh;
    c->w = bw;
    c->h = bh;
    c->mortar = (c->lx < mortar || c->ly < mortar);
    c->id = art_hash((uint32_t)c->row, (uint32_t)art_wrap(c->col, 64 / bw > 0 ? 64 / bw : 1), 0xB51Cu);
}

int tx_bevel(int lx, int ly, int w, int h, int e)
{
    if (ly < e || lx < e) {
        if (ly >= h - e || lx >= w - e) return 0;
        return 1;
    }
    if (ly >= h - e || lx >= w - e) return -1;
    return 0;
}

int tx_phase(int frame, int nframes) { return frame * ART_TURN / (nframes > 0 ? nframes : 1); }

void tx_ball(ArtImg *im, int cx, int cy, int rx, int ry, int ramp, int lo, int hi)
{
    ArtMat m = art_mat_ramp(ramp, lo, hi);
    img_ball(im, cx, cy, rx, ry, &m, 0);
}

void tx_shadow_px(ArtImg *im, int x, int y)
{
    img_shift(im, x, y, -1);
}

void tx_flower(ArtImg *im, int x, int y, uint8_t petal, uint8_t petal_d, uint8_t centre)
{
    /* plus-shaped bloom with a shaded lower-right petal and a soft shadow */
    tx_shadow_px(im, x + 1, y + 2);
    tx_shadow_px(im, x + 2, y + 1);
    img_put(im, x, y - 1, petal);
    img_put(im, x - 1, y, petal);
    img_put(im, x + 1, y, petal_d);
    img_put(im, x, y + 1, petal_d);
    img_put(im, x, y, centre);
}

int tx_scatter(int n, int w, int h, uint32_t seed, int *xs, int *ys)
{
    /* jittered grid with g x g cells, keeps the first n of a shuffled list */
    int g = 1, i, cw, ch, cnt = 0;
    while (g * g < n) g++;
    cw = w / g; ch = h / g;
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    for (i = 0; i < g * g && cnt < n; i++) {
        uint32_t hsh = art_hash((uint32_t)i, seed, 0x5CA7u);
        int gx = i % g, gy = i / g;
        /* skip a pseudo-random subset when g*g > n */
        if (g * g > n && (int)(art_hash((uint32_t)i, seed, 77u) % (uint32_t)(g * g)) < g * g - n) continue;
        xs[cnt] = gx * cw + (int)(hsh % (uint32_t)cw);
        ys[cnt] = gy * ch + (int)((hsh >> 8) % (uint32_t)ch);
        cnt++;
    }
    return cnt;
}
