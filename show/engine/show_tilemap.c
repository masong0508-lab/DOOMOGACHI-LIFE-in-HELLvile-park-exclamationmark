/* show_tilemap.c - the TILEMAP plane (show_tilemap.h). Owner: ENGINE lane 2. */
#include <string.h>
#include "doomo/doomo.h"
#include "show/show_l2.h"
#include "show/show_cam.h"
#include "show/show_tilemap.h"

typedef struct {
    const ShowTilemap *hdr;
    const uint8_t     *atlas;              /* natlas x 128 x 128               */
    const uint16_t    *map;
    DoomoImage         img[TM_MAX_ATLAS];  /* the atlases as cache images      */
} TPlane;

static TPlane s_tm[SHOW_TILEMAP_PLANES];
static int    s_ntm;

void show_tilemap_reset(void)
{
    memset(s_tm, 0, sizeof(s_tm));
    s_ntm = 0;
}

int show_tilemap_bind(const void *payload, uint32_t bytes)
{
    const ShowTilemap *h = (const ShowTilemap *)payload;
    TPlane *p;
    uint32_t need;
    int i;
    if (!payload || bytes < sizeof(ShowTilemap) || s_ntm >= SHOW_TILEMAP_PLANES) return -1;
    if (h->natlas > TM_MAX_ATLAS || (h->tile && h->tile != TM_TILE)) return -1;
    need = sizeof(ShowTilemap) + (uint32_t)h->natlas * TM_ATLAS * TM_ATLAS +
           (uint32_t)h->cols * h->rows * 2u;
    if (need > bytes) return -1;
    p = &s_tm[s_ntm];
    memset(p, 0, sizeof(*p));
    p->hdr = h;
    p->atlas = (const uint8_t *)(h + 1);
    p->map = (const uint16_t *)(p->atlas + (size_t)h->natlas * TM_ATLAS * TM_ATLAS);
    for (i = 0; i < h->natlas; i++) {
        p->img[i].w = TM_ATLAS;
        p->img[i].h = TM_ATLAS;
        p->img[i].pix = p->atlas + (size_t)i * TM_ATLAS * TM_ATLAS;
        p->img[i].uid = doomo_image_new_uid();          /* fresh key every shot */
    }
    return s_ntm++;
}

int show_tilemap_cell(int h, int c, int r)
{
    const TPlane *p;
    if (h < 0 || h >= s_ntm) return TM_EMPTY;
    p = &s_tm[h];
    if (c < 0 || r < 0 || c >= p->hdr->cols || r >= p->hdr->rows) return TM_EMPTY;
    return p->map[r * p->hdr->cols + c];
}

static void tile_uv(int k, int *atlas, int *u, int *v)
{
    int c = k % TM_PER_ATLAS;
    *atlas = k / TM_PER_ATLAS;
    *u = 1 + TM_CELL * (c % TM_PER_ROW);
    *v = 1 + TM_CELL * (c / TM_PER_ROW);
}

#if defined(DOOMO_PLATFORM_PS1) && !defined(SHOW_L2_HOST)
#include <psxgpu.h>
#include "ps1.h"

static void draw_cells(TPlane *p, const ShowXform *xf, int c0, int c1, int r0, int r1)
{
    TexRef ref[TM_MAX_ATLAS];
    uint8_t got[TM_MAX_ATLAS];
    int c, r, whole = show_xf_whole(xf);
    memset(got, 0, sizeof(got));
    for (r = r0; r < r1; r++) {
        int Y0 = show_xf_y(xf, r * TM_TILE) + SHOW_CANVAS_Y, Y1 = show_xf_y(xf, (r + 1) * TM_TILE) + SHOW_CANVAS_Y;
        if (Y1 <= Y0) continue;
        for (c = c0; c < c1; c++) {
            int k = p->map[r * p->hdr->cols + c], a, u, v;
            int X0, X1;
            if (k == TM_EMPTY || k >= p->hdr->ntiles) continue;
            tile_uv(k, &a, &u, &v);
            if (a >= p->hdr->natlas) continue;
            if (!got[a]) {
                if (!vram_image(&p->img[a], 0, 0, &ref[a])) continue;
                got[a] = 1;
            }
            X0 = show_xf_x(xf, c * TM_TILE);
            X1 = show_xf_x(xf, (c + 1) * TM_TILE);
            if (X1 <= X0) continue;
            if (whole == 1) {
                SPRT *s;
                gfx_tpage(ref[a].tpage);
                s = (SPRT *)gfx_alloc(sizeof(SPRT));
                if (!s) return;
                setSprt(s);
                setShadeTex(s, 1);
                setXY0(s, X0, Y0);
                setUV0(s, ref[a].u + u, ref[a].v + v);
                setWH(s, TM_TILE, TM_TILE);
                s->clut = ref[a].clut;
                gfx_chain(s);
            } else {
                POLY_FT4 *q = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
                int U = ref[a].u + u, V = ref[a].v + v;
                if (!q) return;
                setPolyFT4(q);
                setShadeTex(q, 1);
                setXY4(q, X0, Y0, X1, Y0, X0, Y1, X1, Y1);
                setUV4(q, U, V, U + TM_TILE, V, U, V + TM_TILE, U + TM_TILE, V + TM_TILE);
                q->tpage = ref[a].tpage;
                q->clut = ref[a].clut;
                gfx_chain(q);
                g_tpage_cur = ref[a].tpage;
            }
        }
    }
}
#else
static void draw_cells(TPlane *p, const ShowXform *xf, int c0, int c1, int r0, int r1)
{
    int c, r, i, j;
    for (r = r0; r < r1; r++)
        for (c = c0; c < c1; c++) {
            int k = p->map[r * p->hdr->cols + c], a, u, v;
            if (k == TM_EMPTY || k >= p->hdr->ntiles) continue;
            tile_uv(k, &a, &u, &v);
            if (a >= p->hdr->natlas) continue;
            for (j = 0; j < TM_TILE; j++) {
                int y0 = show_xf_y(xf, r * TM_TILE + j), y1 = show_xf_y(xf, r * TM_TILE + j + 1);
                if (y1 <= y0) continue;
                for (i = 0; i < TM_TILE; i++) {
                    uint8_t px = p->atlas[(size_t)a * TM_ATLAS * TM_ATLAS + (size_t)(v + j) * TM_ATLAS + u + i];
                    int x0 = show_xf_x(xf, c * TM_TILE + i), x1 = show_xf_x(xf, c * TM_TILE + i + 1);
                    if (px && x1 > x0) plat_rect(x0, y0 + SHOW_CANVAS_Y, x1 - x0, y1 - y0, px);
                }
            }
        }
}
#endif

void show_tilemap_draw(int h, const ShowXform *xf)
{
    TPlane *p;
    int c0, c1, r0, r1, step;
    if (h < 0 || h >= s_ntm || show_preroll()) return;
    p = &s_tm[h];
    /* the visible cells: screen x in [0, 320), y in [-30, 210) (open matte) */
    step = (int)(((int64_t)TM_TILE * xf->s) >> 16);
    if (step < 1) step = 1;
    c0 = (int)show_floordiv(-xf->ox, step) - 1;
    c1 = (int)show_floordiv(SHOW_CANVAS_W - xf->ox, step) + 2;
    r0 = (int)show_floordiv(-SHOW_CANVAS_Y - xf->oy, step) - 1;
    r1 = (int)show_floordiv(SHOW_CANVAS_H + SHOW_CANVAS_Y - xf->oy, step) + 2;
    c0 = DOOMO_MAX(c0, 0); r0 = DOOMO_MAX(r0, 0);
    c1 = DOOMO_MIN(c1, p->hdr->cols); r1 = DOOMO_MIN(r1, p->hdr->rows);
    if (c0 >= c1 || r0 >= r1) return;
    draw_cells(p, xf, c0, c1, r0, r1);
}
