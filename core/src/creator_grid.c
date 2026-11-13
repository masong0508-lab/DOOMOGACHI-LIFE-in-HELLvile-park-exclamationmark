/* creator_grid.c - the creator's picture grid for part rows (CR6; owner: creator).
 *
 * A on a part row (hats, eyes, tails ...: every row whose list is in the part catalog) opens a grid
 * of its choices as pictures (the prerendered icons of CR7, part_icons.h) in the list window:
 *
 *   - the d-pad moves the cursor; the citizen tries each part on at once (B puts the old one back,
 *     A keeps the new one);
 *   - L / R step through the filter chips of the slot: All, Unlocked, its shape chips (Caps,
 *     Tall ...) and its themed sets (Mayor Giggs' Tea Party ...);
 *   - START types a few letters to find a part by its name or its set's name; SELECT clears the
 *     search and the filter;
 *   - a locked part is shown as a lavender silhouette with a padlock; the footer names it and
 *     says how to unlock it, and the citizen wears it in lavender (creator_draw.c);
 *   - parts above the draft's age class are not listed at all (CR12).
 *
 * The lock bit of every choice is worked out once when the grid opens (CR9: no unlock lookups
 * per cell per frame). State lives in CrState (the scene scratch). docs/CREATOR.md "The grid". */
#include "creator_int.h"

/* A filter chip is one byte: the kind in the top 3 bits, a shape or set index in the low 5, and
 * cr_grid_open() gathers the sets of a slot in a 32-bit mask. More than 32 sets (CREATOR_PLAN 3.3
 * aims at about 120) needs a wider chip and mask first. */
DOOMO_STATIC_ASSERT(PART_SET_COUNT <= 32, grid_set_chips_fit_five_bits);

static int locked_at(int v) { return v >= 0 && v < CR_GRID_MAX && (cr.g_lock[v >> 3] >> (v & 7)) & 1; }

void cr_grid_layout(int *cols, int *rows, int *cell, int *gx, int *gy)
{
    int c = crl.big ? 24 : 21, foot = crl.ly + crl.lh - (crl.big ? 27 : 15);
    int y0 = crl.ly + (crl.big ? 32 : 29);
    int n = (crl.lw - 16) / c, r = (foot - y0) / c;
    if (n < 1) n = 1;
    if (r < 1) r = 1;
    if (cols) *cols = n;
    if (rows) *rows = r;
    if (cell) *cell = c;
    if (gx) *gx = crl.lx + 5 + (crl.lw - 16 - n * c) / 2;
    if (gy) *gy = y0;
}

int cr_grid_choice(int pos)
{
    return pos >= 0 && pos < cr.g_n ? cr.g_idx[pos] : -1;
}

const char *cr_grid_chip(int f, char *buf)
{
    int k, a;
    const PartSlotInfo *sl = &doomo_part_slots[cr.g_slot];
    if (f < 0 || f >= cr.g_nfilt) return "";
    k = cr.g_filt[f] >> 5;
    a = cr.g_filt[f] & 31;
    switch (k) {
    case GFK_OPEN:
        return "Unlocked";
    case GFK_SHAPE:
        if (a < sl->nshapes && sl->shapes) {
            doomo_snprintf(buf, 48, "%s", sl->shapes[a]);
            if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 32);
            return buf;
        }
        return "";
    case GFK_SET: {
        const PartSetInfo *si = part_set_info(a);
        return si ? si->name : "";
    }
    default:
        return "All";
    }
}

/* the choice the cursor should stay on across a rebuild */
static int keep_choice(void)
{
    const CrRow *row = cr_cur_row();
    int v = cr_grid_choice(cr.g_sel);
    if (v < 0) v = cr.browse >= 0 ? cr.browse : cr_row_value(&cr.draft, row);
    return v;
}

static void scroll_to_cursor(void)
{
    int cols, rows, r;
    cr_grid_layout(&cols, &rows, NULL, NULL, NULL);
    r = cr.g_sel / cols;
    if (r < cr.g_top) cr.g_top = (int16_t)r;
    if (r >= cr.g_top + rows) cr.g_top = (int16_t)(r - rows + 1);
    if (cr.g_top < 0) cr.g_top = 0;
}

void cr_grid_rebuild(void)
{
    const CrRow *row = cr_cur_row();
    int keep = keep_choice(), n = cr_row_count(row), v, kind, arg, sel = 0;
    kind = cr.g_filter < cr.g_nfilt ? cr.g_filt[cr.g_filter] >> 5 : GFK_ALL;
    arg = cr.g_filter < cr.g_nfilt ? cr.g_filt[cr.g_filter] & 31 : 0;
    cr.g_n = 0;
    for (v = 0; v < n && v < CR_GRID_MAX; v++) {
        const PartMeta *m = cr_choice_meta(row, v);
        if (!m || cr_value_hidden(row, v)) continue;            /* CR12: not for this age */
        if (kind == GFK_OPEN && locked_at(v)) continue;
        if (kind == GFK_SHAPE && m->shape != arg) continue;
        if (kind == GFK_SET && m->set != arg) continue;
        if (cr.g_find[0] && !part_matches(m, cr.g_find)) continue;
        if (v == keep) sel = cr.g_n;
        cr.g_idx[cr.g_n] = (uint8_t)v;
        cr.g_meta[cr.g_n] = (uint16_t)(m - doomo_part_meta);
        cr.g_n++;
    }
    cr.g_sel = (int16_t)sel;
    cr.g_top = 0;
    scroll_to_cursor();
}

static void add_chip(int kind, int arg)
{
    if (cr.g_nfilt < CR_GRID_FILT) cr.g_filt[cr.g_nfilt++] = (uint8_t)(kind << 5 | (arg & 31));
}

void cr_grid_open(const CrRow *row)
{
    const PartSlotInfo *sl;
    int n = cr_row_count(row), v, k, any_locked = 0;
    uint32_t shapes = 0, sets = 0;
    cr.g_slot = (uint8_t)cr_row_slot(row);
    sl = &doomo_part_slots[cr.g_slot];
    cr.pick_orig = (int16_t)cr_row_value(&cr.draft, row);
    cr.pick_err = 0;
    memset(cr.g_lock, 0, sizeof(cr.g_lock));
    /* CR9: every lock once, here (unlock_find walks tables; the cells ask per frame) */
    for (v = 0; v < n && v < CR_GRID_MAX; v++) {
        const PartMeta *m = cr_choice_meta(row, v);
        if (cr_value_locked(row, v)) { cr.g_lock[v >> 3] |= (uint8_t)(1 << (v & 7)); any_locked = 1; }
        if (!m || cr_value_hidden(row, v)) continue;
        if (m->shape < 32) shapes |= 1u << m->shape;
        if (m->set < 32) sets |= 1u << m->set;
    }
    /* the chips: All, Unlocked, the slot's shapes, then its themed sets (in catalog order) */
    cr.g_nfilt = 0;
    add_chip(GFK_ALL, 0);
    if (any_locked) add_chip(GFK_OPEN, 0);
    for (k = 0; k < sl->nshapes && k < 32; k++)
        if (shapes & (1u << k)) add_chip(GFK_SHAPE, k);
    for (k = 0; k < PART_SET_COUNT && k < 32; k++)
        if ((sets & (1u << k)) && strcmp(doomo_part_sets[k].key, "basics") != 0) add_chip(GFK_SET, k);
    cr.g_filter = 0;
    cr.g_find[0] = 0;
    cr.g_sel = 0;
    cr.browse = cr_value_locked(row, cr.pick_orig) ? cr.pick_orig : -1;
    cr_grid_rebuild();
}

/* START: find by letters */
static void cb_find(int ok)
{
    int i, n;
    if (!ok) return;
    n = (int)strlen(cr.g_findbuf);
    while (n > 0 && cr.g_findbuf[n - 1] == ' ') n--;
    for (i = 0; i < n && i < (int)sizeof(cr.g_find) - 1; i++) cr.g_find[i] = cr.g_findbuf[i];
    cr.g_find[i] = 0;
    cr_grid_rebuild();
    if (cr.g_n == 0) {
        ui_toast("Nothing by that name - showing all");
        cr.g_find[0] = 0;
        cr.g_filter = 0;
        cr_grid_rebuild();
    }
    if (cr.g_n > 0) cr_pick_preview(cr_cur_row(), cr_grid_choice(cr.g_sel));   /* try on what it found */
}

void cr_grid_update(void)
{
    const CrRow *row = cr_cur_row();
    int cols, old = cr.g_sel, sel = cr.g_sel, n = cr.g_n;
    if (!row) return;
    cr_grid_layout(&cols, NULL, NULL, NULL, NULL);
    if (cr.pick_err) cr.pick_err--;
    if (n > 0) {
        if (btn_repeat(BTN_LEFT)) sel = (sel + n - 1) % n;
        if (btn_repeat(BTN_RIGHT)) sel = (sel + 1) % n;
        if (btn_repeat(BTN_UP)) {
            if (sel >= cols) sel -= cols;
            else {                                  /* wrap to the same column of the last row */
                sel += (n - 1) / cols * cols;
                if (sel > n - 1) sel -= cols;
                if (sel < 0) sel = n - 1;
            }
        }
        if (btn_repeat(BTN_DOWN)) {
            if (sel + cols < n) sel += cols;
            else if (sel / cols < (n - 1) / cols) sel = n - 1;    /* a short last row */
            else sel %= cols;                                     /* wrap to the top */
        }
    }
    if (btn_pressed(BTN_L | BTN_R) && cr.g_nfilt > 1) {
        int d = btn_pressed(BTN_L) ? cr.g_nfilt - 1 : 1;
        cr.g_filter = (uint8_t)((cr.g_filter + d) % cr.g_nfilt);
        cr_grid_rebuild();
        audio_sfx(SFX_CURSOR);
        sel = cr.g_sel;
        old = -1;                                   /* try on what the cursor lands on */
    }
    if (btn_pressed(BTN_SELECT) && (cr.g_filter || cr.g_find[0])) {
        cr.g_filter = 0;
        cr.g_find[0] = 0;
        cr_grid_rebuild();
        sel = cr.g_sel;
        audio_sfx(SFX_BLIP);
    }
    cr.g_sel = (int16_t)sel;
    scroll_to_cursor();
    if (cr.g_sel != old && cr.g_n > 0) {
        cr_pick_preview(row, cr_grid_choice(cr.g_sel));
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_START)) {
        memcpy(cr.g_findbuf, cr.g_find, sizeof(cr.g_findbuf));
        ui_text_entry("Find a part: type a few letters", cr.g_findbuf, (int)sizeof(cr.g_findbuf), cb_find);
        audio_sfx(SFX_SELECT);
        return;
    }
    if (btn_pressed(BTN_A)) {
        int v = cr_grid_choice(cr.g_sel);
        if (v < 0) { audio_sfx(SFX_ERROR); return; }
        if (locked_at(v)) { audio_sfx(SFX_ERROR); cr.pick_err = 14; return; }
        cr_row_set(&cr.draft, row, v);
        cr_pick_close(row, 1);
    } else if (btn_pressed(BTN_B)) {
        cr_pick_close(row, 0);
    }
}

/* ====================================================================== */
/* drawing                                                                  */
/* CR9: will the footer scroll (a marquee on small screens)? */
static int grid_footer_moves(const CrRow *row)
{
    char buf[96];
    int v = cr_grid_choice(cr.g_sel);
    const PartMeta *m = v >= 0 ? cr_choice_meta(row, v) : NULL;
    if (!m) return 0;
    if (crl.big) return locked_at(v) && ui_text_width(cr_lock_hint(row, v)) > crl.lw - 16;
    if (locked_at(v)) {
        doomo_snprintf(buf, sizeof(buf), "%s: %s", part_meta_name(m), cr_lock_hint(row, v));
        return ui_text_width(buf) > crl.lw - 24;
    }
    doomo_snprintf(buf, sizeof(buf), "%s - %s", part_meta_name(m), doomo_part_sets[m->set].name);
    return ui_text_width(buf) > crl.lw - 16;
}

/* the end of the grid window: its panel (S3-3: one drawn before it is taken now) */
static void grid_end(int panel, int pre, uint32_t key)
{
    if (!panel) return;
    if (pre) cr_region_pre_end(CRR_LIST, PANEL_CR_LIST, crl.lx, crl.ly, crl.lw + 3,
                               DOOMO_MIN(crl.lh + 3, crl.sh - 13 - crl.ly), key, CR_CALM, 0, 0);
    else plat_panel_end();
}

void cr_grid_draw(void)
{
    const UiStyle *st;
    const CrRow *row = cr_cur_row();
    const PartSlotInfo *sl;
    int x = crl.lx, y = crl.ly, w = crl.lw, h = crl.lh, t = (int)cr.phase_t;
    int cols, rows, cell, gx, gy, i, total;
    char buf[64], b2[48], b3[48];
    int panel, pre = 0;
    uint32_t grid_k = 0;
    if (!row) return;
    sl = &doomo_part_slots[cr.g_slot];
    cr_grid_layout(&cols, &rows, &cell, &gx, &gy);
    /* CR9: once popped open the grid is a retained panel (GBA): put back while the cursor rests */
    panel = UI_PANELS && t >= 12;
    if (panel) {
        uint32_t k = ui_key_int(ui_panel_key(), cr.g_slot | cr.g_n << 8 | cr.g_filter << 16 | crl.big << 24);
        k = ui_key_int(k, cr.g_sel | cr.g_top << 8 | cr.pick_err << 16);
        k = ui_key_int(k, cr.pick_orig | (int)((unsigned)ui_window_glint(x, y, w, h) << 8));
        k = ui_key_str(k, cr.g_find);
        k = ui_key_int(k, (int)(cr.lmemo_sig & 0x7FFFFFFF));          /* the lock bits were made from it */
        if (grid_footer_moves(row)) k = ui_key_int(k, (int)(cr.t / 7));
        grid_k = k;
        pre = cr_region_pre(CRR_LIST, k);           /* S3-3: a new page is drawn before the panel */
        if (!pre) {
            switch (cr_region(CRR_LIST, PANEL_CR_LIST, x, y, w + 3, DOOMO_MIN(h + 3, crl.sh - 13 - y), k)) {
            case 2: return;                         /* G2-CR: on both held pages already */
            case 0:
                ui_window_shadow(x, y, w, h);
                plat_panel_end();
                return;
            default: break;
            }
            if (CR_CALM) plat_panel_solid();        /* a still backdrop */
        }
    } else {
        cr.hd_n[CRR_LIST] = 0;
    }
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, t)) { grid_end(panel, pre, grid_k); return; }
    st = ui_style(WIN_CREAM);
    /* header: the row's name and where the cursor is */
    cr_draw_tab_icon(x + 6, y + 4, cr.tab);
    ui_text_shadow(x + 20, y + 5, cr_row_label(row), st->title_bg, st->outline);
    total = cr_visible_count(row);
    if (cr.g_n < total) doomo_snprintf(buf, sizeof(buf), "%d/%d of %d", cr.g_n ? cr.g_sel + 1 : 0, cr.g_n, total);
    else doomo_snprintf(buf, sizeof(buf), "%d/%d", cr.g_n ? cr.g_sel + 1 : 0, cr.g_n);
    ui_text(x + w - 8 - ui_text_width(buf), y + 5, buf, st->text_dim);
    /* the filter chip: L < name > R, and the search */
    {
        int cy = y + (crl.big ? 17 : 16), cw = crl.big ? 112 : 92;
        const char *chip = cr_grid_chip(cr.g_filter, b2);
        uint8_t on = cr.g_filter ? st->accent : st->fill_lo;
        cr_pill(x + 6, cy, cw, 11, on, st->outline, ui_lighter(on));
        ui_text(x + 9, cy + 2, "L", cr.g_filter ? PAL_WHITE : st->text_dim);
        ui_text(x + 6 + cw - 9, cy + 2, "R", cr.g_filter ? PAL_WHITE : st->text_dim);
        ui_text_center(x + 6 + cw / 2, cy + 2, cr_fit(chip, cw - 24, b3), cr.g_filter ? PAL_WHITE : st->text);
        /* the search, if any (the button bar says START finds) */
        if (cr.g_find[0]) {
            doomo_snprintf(buf, sizeof(buf), "\"%s\"", cr.g_find);
            ui_text(x + w - 8 - ui_text_width(cr_fit(buf, w - cw - 20, b3)), cy + 2, cr_fit(buf, w - cw - 20, b3),
                    st->accent);
        }
    }
    /* the cells */
    for (i = 0; i < rows * cols; i++) {
        int pos = (cr.g_top + i / cols) * cols + i % cols, v = cr_grid_choice(pos);
        int cx = gx + (i % cols) * cell, cy = gy + (i / cols) * cell, sel = pos == cr.g_sel;
        DoomoImage im;
        if (v < 0) break;
        /* a locked part: its picture as it is (so you can see what it is), on a lavender cell with a
         * padlock; the footer says how to unlock it */
        if (sel) {
            int shake = cr.pick_err ? ((cr.pick_err & 2) ? 1 : -1) : 0;
            ui_round_rect(cx - 1 + shake, cy - 1, cell + 1, cell + 1, 3, st->accent);
            ui_round_rect(cx + 1 + shake, cy + 1, cell - 3, cell - 3, 2, locked_at(v) ? PAL_RAMP(RAMP_VAPE, 6) : PAL_WHITE);
        } else if (locked_at(v)) {
            ui_round_rect(cx, cy, cell - 1, cell - 1, 2, PAL_RAMP(RAMP_VAPE, 4));
            ui_round_rect(cx + 1, cy + 1, cell - 3, cell - 3, 2, PAL_RAMP(RAMP_VAPE, 6));
        } else {
            ui_round_rect(cx, cy, cell - 1, cell - 1, 2, st->fill_lo);
            ui_round_rect(cx + 1, cy + 1, cell - 3, cell - 3, 2, st->fill_hi);
        }
        if (part_icon_image(cr.g_meta[pos], &im, cr.icon_buf)) {
            int ix = cx + (cell - 1 - PART_ICON_W) / 2, iy = cy + (cell - 1 - PART_ICON_W) / 2;
            plat_image(ix, iy, &im, 1, 0, 0);
        } else {
            ui_text_center(cx + cell / 2, cy + cell / 2 - 4, "?", st->text_dim);
        }
        if (locked_at(v)) cr_padlock(cx + cell - 8, cy + cell - 9, PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 6));
        if (v == cr.pick_orig) ui_star(cx + cell - 5, cy + 3, 2, PAL_UI_GOLD);
    }
    if (cr.g_n == 0)
        ui_text_wrap(x + 10, gy + 4, w - 20, "Nothing here. L / R for other chips, SELECT shows all.", st->text_dim, 1);
    /* scroll bar */
    if ((cr.g_n + cols - 1) / cols > rows) {
        int nr = (cr.g_n + cols - 1) / cols, bx = x + w - 8, th = rows * cell - 2;
        int thumb = DOOMO_MAX(5, th * rows / nr), ty = gy + (th - thumb) * cr.g_top / DOOMO_MAX(1, nr - rows);
        ui_round_rect(bx, gy, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, ty, 3, thumb, 1, st->accent);
    }
    /* footer: name and set (or how to unlock it); a second line on big screens */
    {
        int v = cr_grid_choice(cr.g_sel), fy = y + h - (crl.big ? 25 : 13);
        const PartMeta *m = v >= 0 ? cr_choice_meta(row, v) : NULL;
        plat_rect(x + 5, fy - 2, w - 10, 1, st->fill_lo);
        if (!m) { grid_end(panel, pre, grid_k); return; }
        if (locked_at(v)) {
            doomo_snprintf(buf, sizeof(buf), "%s: %s", part_meta_name(m), cr_lock_hint(row, v));
            cr_padlock(x + 8, fy + 1, PAL_PAINT(PAINT_CHERRY, 1), st->fill);
            if (crl.big) {
                ui_text(x + 16, fy, cr_fit(part_meta_name(m), w - 24, b2), PAL_PAINT(PAINT_CHERRY, 0));
                /* the whole hint, never cut: it scrolls when it is longer than the line */
                ui_text(x + 8, fy + 11, cr_marquee(cr_lock_hint(row, v), w - 16, b3), PAL_PAINT(PAINT_CHERRY, 0));
            } else {
                ui_text(x + 16, fy, cr_marquee(buf, w - 24, b2), PAL_PAINT(PAINT_CHERRY, 0));
            }
            grid_end(panel, pre, grid_k);
            return;
        }
        if (crl.big) {
            ui_text(x + 8, fy, cr_fit(part_meta_name(m), w - 16, b2), st->text);
            ui_text(x + 8, fy + 11, cr_fit(doomo_part_sets[m->set].name, w - 16, b3), st->text_dim);
        } else {
            doomo_snprintf(buf, sizeof(buf), "%s - %s", part_meta_name(m), doomo_part_sets[m->set].name);
            ui_text(x + 8, fy, cr_marquee(buf, w - 16, b2), st->text);
        }
    }
    grid_end(panel, pre, grid_k);
    (void)sl;
}
