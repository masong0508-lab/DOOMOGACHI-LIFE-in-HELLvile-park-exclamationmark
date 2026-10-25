/* show_text.c - SUPERs, signs and cards (show_text.h). Owner: ENGINE lane 2. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/font.h"
#include "show/show_l2.h"
#include "show/show_text.h"

#define MAX_ITEMS   48
#define GLYPH_SLOTS 160
#define GLYPH_PIX   256                 /* (gw + 3) x (gh + 3) <= 16 x 16      */

static struct {
    const ShowTextSet  *set;
    const ShowTextItem *it;
    const char         *pool;
} s_tx;

/* ---- glyph images: the font bitmap with its outline and shadow baked in ---- */
typedef struct {
    uint32_t   key;                     /* font | ch << 8 | colours, 0 = free    */
    uint32_t   key2;
    DoomoImage img;
} GlyphSlot;
static GlyphSlot s_gs[GLYPH_SLOTS];
static uint8_t   s_gpix[GLYPH_SLOTS][GLYPH_PIX];
static int       s_gn;

/* one glyph's bitmap: width, height and the bit at (x, y) */
static int glyph_dims(int font, unsigned ch, int *w, int *h)
{
    if (font == TF_CHILD) {
        const ShowPxtFont *f = &show_font_child;
        if (ch < f->first || ch >= (unsigned)f->first + f->count || !f->w[ch - f->first]) return 0;
        *w = f->w[ch - f->first];
        *h = f->h;
        return 1;
    }
    if (ch < FONT_FIRST || ch > FONT_LAST) return 0;
    *w = 5;
    *h = 8;
    return 1;
}

static int glyph_bit(int font, unsigned ch, int x, int y)
{
    if (font == TF_CHILD) {
        const ShowPxtFont *f = &show_font_child;
        return (f->rows[(ch - f->first) * f->h + y] >> (7 - x)) & 1;
    }
    return (doomo_font[ch - FONT_FIRST][x] >> y) & 1;
}

/* the glyph image (cell origin = glyph origin - (1, 1)) or NULL */
static const DoomoImage *glyph_img(int font, unsigned ch, uint8_t col, uint8_t out, uint8_t sh)
{
    uint32_t k1 = (uint32_t)font | (ch << 8) | ((uint32_t)col << 16) | ((uint32_t)out << 24);
    uint32_t k2 = sh | 0x100u;
    int i, gw, gh, cw, chh, x, y;
    uint8_t *p;
    if (!glyph_dims(font, ch, &gw, &gh)) return 0;
    for (i = 0; i < s_gn; i++)
        if (s_gs[i].key == k1 && s_gs[i].key2 == k2) return &s_gs[i].img;
    if (s_gn >= GLYPH_SLOTS) return 0;
    cw = gw + 3;
    chh = gh + 3;
    if (cw * chh > GLYPH_PIX) return 0;
    p = s_gpix[s_gn];
    memset(p, 0, (size_t)(cw * chh));
    /* shadow: the outlined shape at (+1, +1); outline: the 8-neighbour dilation */
    for (y = 0; y < gh; y++)
        for (x = 0; x < gw; x++) {
            int dx, dy;
            if (!glyph_bit(font, ch, x, y)) continue;
            if (sh)
                for (dy = 0; dy <= 2; dy++)
                    for (dx = 0; dx <= 2; dx++)
                        p[(y + dy + 1) * cw + x + dx + 1] = sh;
        }
    if (out)
        for (y = 0; y < gh; y++)
            for (x = 0; x < gw; x++) {
                int dx, dy;
                if (!glyph_bit(font, ch, x, y)) continue;
                for (dy = 0; dy <= 2; dy++)
                    for (dx = 0; dx <= 2; dx++)
                        p[(y + dy) * cw + x + dx] = out;
            }
    for (y = 0; y < gh; y++)
        for (x = 0; x < gw; x++)
            if (glyph_bit(font, ch, x, y)) p[(y + 1) * cw + x + 1] = col;
    s_gs[s_gn].key = k1;
    s_gs[s_gn].key2 = k2;
    s_gs[s_gn].img.w = (uint16_t)cw;
    s_gs[s_gn].img.h = (uint16_t)chh;
    s_gs[s_gn].img.pix = p;
    s_gs[s_gn].img.uid = doomo_image_new_uid();
    return &s_gs[s_gn++].img;
}

/* ---- measurement ------------------------------------------------------------ */
static int advance(int font, unsigned ch)
{
    int w, h;
    if (font == TF_CHILD) {
        const ShowPxtFont *f = &show_font_child;
        if (ch == ' ') return f->w[' ' - f->first] ? f->w[' ' - f->first] + f->space : 4;
        if (!glyph_dims(font, ch, &w, &h)) return 0;
        return w + f->space;
    }
    return FONT_ADVANCE;
}

int show_text_line_w(int font, const char *s, int n)
{
    int i, w = 0;
    for (i = 0; i < n && s[i] && s[i] != '\n'; i++) w += advance(font, (uint8_t)s[i]);
    if (font == TF_CHILD && w > 0) w -= show_font_child.space;    /* no gap after the last */
    if (font == TF_GAME && w > 0) w -= 1;
    return w;
}

int show_text_line_h(int font)
{
    return font == TF_CHILD ? show_font_child.line : FONT_LINE;
}

/* ---- binding --------------------------------------------------------------- */
void show_text_reset(void)
{
    memset(&s_tx, 0, sizeof(s_tx));
    memset(s_gs, 0, sizeof(s_gs));
    s_gn = 0;
}

int show_text_bind(const void *payload, uint32_t bytes)
{
    const ShowTextSet *set = (const ShowTextSet *)payload;
    show_text_reset();
    if (!payload || bytes < sizeof(ShowTextSet)) return -1;
    if (set->nitems > MAX_ITEMS ||
        sizeof(ShowTextSet) + set->nitems * sizeof(ShowTextItem) + set->pool_bytes > bytes) return -1;
    s_tx.set = set;
    s_tx.it = (const ShowTextItem *)(set + 1);
    s_tx.pool = (const char *)(s_tx.it + set->nitems);
    return 0;
}

/* ---- schedules -------------------------------------------------------------- */
static int glyph_count(const ShowTextItem *it)
{
    int i, n = 0;
    for (i = 0; i < it->len; i++) if (s_tx.pool[it->str + i] != '\n') n++;
    return n;
}

/* the frame glyph k of n appears on (type) */
static int glyph_frame(const ShowTextItem *it, int k, int n)
{
    int span = (int)it->t1 - (int)it->t0;
    if (n <= 1 || span <= 0) return it->t0;
    return it->t0 + (2 * k * span + (n - 1)) / (2 * (n - 1));
}

int show_text_glyphs(int i, int t, int *n)
{
    const ShowTextItem *it;
    int k, g;
    if (!s_tx.set || i < 0 || i >= s_tx.set->nitems) { if (n) *n = 0; return 0; }
    it = &s_tx.it[i];
    g = glyph_count(it);
    if (n) *n = g;
    if (t < it->t_on || (it->t_off != TX_END && t >= it->t_off)) return 0;
    if (it->style != TS_TYPE) return g;
    for (k = 0; k < g && glyph_frame(it, k, g) <= t; k++) {}
    return k;
}

/* pop: scale x256 over the first frames */
static int pop_q8(int age)
{
    static const int16_t P[5] = { 128, 307, 282, 248, 256 };
    if (age < 0) return 0;
    return age < 5 ? P[age] : 256;
}

/* ---- drawing ---------------------------------------------------------------- */
static void draw_item(const ShowTextItem *it, int t)
{
    const char *s = s_tx.pool + it->str;
    int sc = it->scale ? it->scale : 1, n = glyph_count(it), shown = n, k = 0, i;
    int lh = show_text_line_h(it->font) * sc, line = 0, x0, ax = it->x, ay = it->y;
    int pq = 256, cx, cy, lines = 1, maxw = 0, x;
    uint8_t out = (it->flags & TX_NOOUTLINE) ? 0 : it->outline;
    if (it->style == TS_BOX) {
        if (it->outline) plat_rect(ax - 1, ay - 1 + SHOW_CANVAS_Y, it->w + 2, it->h + 2, it->outline);
        if (it->color) plat_rect(ax, ay + SHOW_CANVAS_Y, it->w, it->h, it->color);
        return;
    }
    if ((it->flags & TX_SLOT) && !show_stage_slot_xy(it->w, &ax, &ay)) return;
    if (it->flags & TX_SLOT) { ax += it->x; ay += it->y; }
    if (it->style == TS_TYPE) shown = show_text_glyphs((int)(it - s_tx.it), t, 0);
    if (it->style == TS_POP) pq = pop_q8(t - it->t_on);
    /* the block's centre (the pop scales about it) */
    for (i = 0; i < it->len; i++) if (s[i] == '\n') lines++;
    {
        const char *p = s;
        int left = it->len;
        while (left > 0) {
            int ln = 0, w;
            while (ln < left && p[ln] != '\n') ln++;
            w = show_text_line_w(it->font, p, ln) * sc;
            if (w > maxw) maxw = w;
            p += ln + 1;
            left -= ln + 1;
        }
    }
    cx = it->align == TA_CENTER ? ax : it->align == TA_RIGHT ? ax - maxw / 2 : ax + maxw / 2;
    cy = ay + lines * lh / 2;
    /* line by line */
    x0 = 0;
    for (i = 0; i < it->len; ) {
        int ln = 0, w;
        while (i + ln < it->len && s[i + ln] != '\n') ln++;
        w = show_text_line_w(it->font, s + i, ln) * sc;
        x0 = it->align == TA_CENTER ? ax - w / 2 : it->align == TA_RIGHT ? ax - w : ax;
        x = x0;
        for (; ln > 0; ln--, i++) {
            unsigned ch = (uint8_t)s[i];
            int gy = ay + line * lh, hop = 0, a;
            const DoomoImage *g;
            if (k >= shown) return;
            if (it->style == TS_TYPE && (it->flags & TX_HOP)) {
                a = t - glyph_frame(it, k, n);
                hop = a == 0 ? -2 : a == 1 ? -1 : 0;
            }
            if (it->font == TF_CHILD)          /* a child's baseline: -1..+1 by position */
                hop += (int)(show_hash3(0xC417u, (uint32_t)k, ch) % 3u) - 1;
            k++;
            g = ch == ' ' ? 0 : glyph_img(it->font, ch, it->color, out, it->shadow);
            if (g) {
                if (pq == 256) {
                    plat_image(x - sc, gy - sc + hop * sc + SHOW_CANVAS_Y, g, sc, 0, 0);
                } else if (pq > 0) {
                    /* bottom-centre of the glyph cell, scaled about the block centre */
                    int bx = x - sc + g->w * sc / 2, by = gy - sc + g->h * sc;
                    int sx = cx + (bx - cx) * pq / 256, sy = cy + (by - cy) * pq / 256;
                    plat_image_xform(sx, sy + SHOW_CANVAS_Y, g, sc * pq, sc * pq, 0, 0);
                }
            }
            x += advance(it->font, ch) * sc;
        }
        i++;                                  /* the '\n' */
        line++;
    }
}

void show_text_draw(int t)
{
    int i;
    if (!s_tx.set || show_preroll()) return;
    for (i = 0; i < s_tx.set->nitems; i++) {
        const ShowTextItem *it = &s_tx.it[i];
        if (t < it->t_on || (it->t_off != TX_END && t >= it->t_off)) continue;
        draw_item(it, t);
    }
}
