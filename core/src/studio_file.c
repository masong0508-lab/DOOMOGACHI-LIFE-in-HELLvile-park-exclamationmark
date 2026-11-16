/* studio_file.c - the Music Studio's FILE tab (owner: studio): the song
 * book (6 pages, saved with the game in the 'SONG' chunk), new songs (the
 * "Start a song" card), templates, "Inspire me!", undo / redo with their
 * step counters, the Studio level (Easy / Advanced) and the way out.
 * The list scrolls (ftop) when the screen is too short for all 13 rows.
 */
#include "studio_int.h"

enum { FR_SLOT0 = 0, FR_NEW = SONG_BOOK_SIZE, FR_TEMPLATE, FR_INSPIRE, FR_UNDO, FR_REDO, FR_LEVEL,
       FR_LEAVE, FR_COUNT };

static int pending_slot, pending_tpl;

/* ---- book ------------------------------------------------------------------ */
static void save_to(int slot)
{
    DoomoSong *dst = &doomo_song_book[slot];
    songx_book_drop(slot);                      /* the studio edits the classic song: an old arrangement would not match it */
    *dst = studio.song;
    dst->flags |= SONGF_VALID | SONGF_CUSTOM;
    song_sanitize(dst);
    studio.slot = (int8_t)slot;
    studio.dirty = 0;
    if (studio_stats.saved < 0xFFFF) studio_stats.saved++;
    studio_stats.genres |= (uint16_t)(1u << (studio.song.genre % MUSIC_COUNT));
    studio_check_unlocks();
    st_msg("Saved on page %d " STR_ICON_HEART, slot + 1);
    audio_sfx(SFX_SPARKLE);
}

static void load_from(int slot)
{
    song_stop();
    studio.song = doomo_song_book[slot];
    song_sanitize(SS);
    studio.slot = (int8_t)slot;
    studio.dirty = 0;
    ed_reset();
    st_msg("Opened \"%s\"", SS->title);
    audio_sfx(SFX_SELECT);
}

static void confirm_overwrite(int c) { if (c == 1) save_to(pending_slot); }
static void confirm_load(int c) { if (c == 1) load_from(pending_slot); }
static void confirm_delete(int c)
{
    if (c != 1) return;
    memset(&doomo_song_book[pending_slot], 0, sizeof(DoomoSong));
    songx_book_drop(pending_slot);
    if (studio.slot == pending_slot) studio.slot = -1;
    st_msg("Page %d is blank again", pending_slot + 1);
    audio_sfx(SFX_CANCEL);
}

static void slot_menu(int c)
{
    static const char *const sure_save[2] = { "No", "Yes, overwrite it" };
    static const char *const sure_load[2] = { "No", "Yes, lose my changes" };
    static const char *const sure_del[2] = { "No", "Yes, delete it" };
    int slot = pending_slot, full = (doomo_song_book[slot].flags & SONGF_VALID) != 0;
    if (c < 0) return;
    if (!full) { save_to(slot); return; }
    switch (c) {
    case 0:
        if (studio.slot == slot) save_to(slot);
        else st_popup("Overwrite this song?", sure_save, 2, 0, confirm_overwrite);
        break;
    case 1:
        if (studio.dirty) st_popup("Open without saving?", sure_load, 2, 0, confirm_load);
        else load_from(slot);
        break;
    case 2:
        doomo_song_book[slot].flags ^= SONGF_FAVORITE;
        audio_sfx(SFX_LOVE);
        break;
    case 3: st_popup("Delete this song?", sure_del, 2, 0, confirm_delete); break;
    default: break;
    }
}

/* ---- new songs ----------------------------------------------------------------- */
static void do_new(int c)
{
    if (c != 1 && c != -2) return;
    song_stop();
    ed_touch(SS, (int)sizeof(DoomoSong));
    song_clear(SS);
    studio_choir_fill(SS, FILL_CLEAR, -1, 0);
    SS->singer[0] = DOOMO_PLAYER_ID;
    ed_commit();
    studio.slot = -1;
    st_set_tab(STAB_SONG);
    st_card_open();                          /* ST-EASY: a new song starts on the "Start a song" card */
    audio_sfx(SFX_SELECT);
}

static void do_template(int c)
{
    int t = pending_tpl;
    if (c != 1 && c != -2) return;
    song_stop();
    ed_touch(SS, (int)sizeof(DoomoSong));
    studio_template_load(SS, t);
    ed_commit();
    studio.slot = -1;
    studio.fmode = 0;
    studio.card = 0;
    st_msg("Template: %s", studio_template_name(t));
    audio_sfx(SFX_SPARKLE);
    st_set_tab(STAB_SONG);
}

static void muse_done(int id)
{
    const DoomoResident *r;
    if (id < 0) return;
    song_stop();
    r = town_res(id);
    ed_touch(SS, (int)sizeof(DoomoSong));
    studio_inspire(SS, id, r ? RES_LIKE_MUSIC(r) % MUSIC_COUNT : -1, doomo_hash32(studio.t * 977u + (uint32_t)id));
    ed_commit();
    studio.slot = -1;
    if (studio_stats.inspired < 0xFFFF) studio_stats.inspired++;
    studio.card = 0;
    studio_check_unlocks();
    st_msg("A %s song about %s!", doomo_music_names[SS->genre % MUSIC_COUNT], st_name(id));
    audio_sfx(SFX_SPARKLE);
    st_set_tab(STAB_SONG);
}

static void ask_dirty(const char *q, StudioPopDone go)
{
    static const char *const sure[2] = { "No", "Yes, start over" };
    if (studio.dirty) st_popup(q, sure, 2, 0, go);
    else go(-2);
}

/* the Start card's two shortcuts */
void st_file_inspire(void)
{
    int muse = song_singer_id(SS, 0);
    st_pick_citizen("Who inspires you?", muse >= 0 ? muse : DOOMO_PLAYER_ID, muse_done);
}

void st_file_templates(void)
{
    studio.fmode = 1;
    st_set_tab(STAB_FILE);
}

/* rows that fit under the list's top edge (the book's divider adds 3 px below the pages) */
static int file_vis(int rowh)
{
    return DOOMO_MAX(1, (st_content().h - 8 - 3) / rowh);
}

void tab_file_update(void)
{
    if (studio.fmode == 1) {
        /* template list */
        if (btn_repeat(BTN_UP)) { studio.tsel = (uint8_t)((studio.tsel + STUDIO_TEMPLATES - 1) % STUDIO_TEMPLATES); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { studio.tsel = (uint8_t)((studio.tsel + 1) % STUDIO_TEMPLATES); audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A)) {
            if (!studio_template_ok(studio.tsel)) {
                const DoomoUnlock *u = unlock_by_id(studio_template_unlock(studio.tsel));
                st_msg("Locked: %s", u ? u->how : "keep making music");
                audio_sfx(SFX_ERROR);
            } else {
                pending_tpl = studio.tsel;
                ask_dirty("Replace your song?", do_template);
            }
        }
        if (btn_pressed(BTN_B)) { studio.fmode = 0; audio_sfx(SFX_CANCEL); input_eat(); }
        return;
    }
    if (btn_repeat(BTN_UP)) { studio.fsel = (uint8_t)((studio.fsel + FR_COUNT - 1) % FR_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { studio.fsel = (uint8_t)((studio.fsel + 1) % FR_COUNT); audio_sfx(SFX_CURSOR); }
    {
        int vis = file_vis(st_wide() ? 14 : 10);
        if (studio.fsel < studio.ftop) studio.ftop = studio.fsel;
        if (studio.fsel >= studio.ftop + vis) studio.ftop = (uint8_t)(studio.fsel - vis + 1);
        if (FR_COUNT <= vis) studio.ftop = 0;
    }
    if (studio.fsel == FR_LEVEL && (btn_pressed(BTN_LEFT) || btn_pressed(BTN_RIGHT))) {
        st_level_set(!st_level());
        return;
    }
    if (!btn_pressed(BTN_A)) return;
    if (studio.fsel < SONG_BOOK_SIZE) {
        static const char *const full_items[4] = { "Save here", "Open", ("Favorite " STR_ICON_HEART), "Delete" };
        static const char *const empty_items[1] = { "Save here" };
        static const char *const titles[SONG_BOOK_SIZE] = { "Page 1", "Page 2", "Page 3",
                                                           "Page 4", "Page 5", "Page 6" };
        pending_slot = studio.fsel;
        if (doomo_song_book[studio.fsel].flags & SONGF_VALID)
            st_popup(titles[studio.fsel], full_items, 4, 0, slot_menu);
        else
            st_popup(titles[studio.fsel], empty_items, 1, 0, slot_menu);
        return;
    }
    switch (studio.fsel) {
    case FR_NEW: ask_dirty("Start a new song?", do_new); break;
    case FR_TEMPLATE: studio.fmode = 1; audio_sfx(SFX_SELECT); break;
    case FR_INSPIRE: {
        int muse = song_singer_id(SS, 0);
        st_pick_citizen("Who inspires you?", muse >= 0 ? muse : DOOMO_PLAYER_ID, muse_done);
        break; }
    case FR_UNDO:
        if (ed_undo()) { st_msg("Undone! (%d left)", ed_undo_steps()); audio_sfx(SFX_CANCEL); }
        else { st_msg("Nothing to undo"); audio_sfx(SFX_ERROR); }
        break;
    case FR_REDO:
        if (ed_redo()) { st_msg("Redone! (%d more)", ed_redo_steps()); audio_sfx(SFX_SELECT); }
        else { st_msg(st_easy() && st_easy()->redo_lost ? "Too big to redo" : "Nothing to redo"); audio_sfx(SFX_ERROR); }
        break;
    case FR_LEVEL: st_level_set(!st_level()); break;
    default: studio_try_leave(); break;
    }
}

/* ---- drawing ------------------------------------------------------------------ */
static void draw_templates(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int wide = st_wide(), rowh = wide ? 13 : 10, vis = (r.h - 8) / rowh, top, i, lw = wide ? 170 : 128;
    top = studio.tsel - vis / 2;
    top = DOOMO_CLAMP(top, 0, DOOMO_MAX(0, STUDIO_TEMPLATES - vis));
    st_panel(r.x, r.y, lw, r.h, WIN_CREAM);
    for (i = 0; i < vis && top + i < STUDIO_TEMPLATES; i++) {
        int t = top + i, y = r.y + 4 + i * rowh, sel = t == studio.tsel, ok = studio_template_ok(t);
        if (sel) {
            ui_round_rect(r.x + 3, y - 1, lw - 6, rowh, 3, st->accent);
            ui_cursor(r.x + 4, y + (rowh - 8) / 2);
        }
        ui_text(r.x + 14, y + (rowh - 9) / 2, ok ? studio_template_name(t) : "? ? ?",
                sel ? PAL_WHITE : ok ? st->text : st->text_dim);
        if (!ok) ui_text(r.x + lw - 12, y + (rowh - 9) / 2, STR_ICON_X, st->text_dim);
    }
    ui_scroll_arrows(r.x + lw / 2, r.y + 1, r.y + r.h - 3, top > 0, top + vis < STUDIO_TEMPLATES);
    /* the card */
    {
        int cx = r.x + lw + 4, cw = r.w - lw - 4, t = studio.tsel, ok = studio_template_ok(t);
        int g = studio_template_genre(t);
        st_panel(cx, r.y, cw, r.h, WIN_MINT);
        ui_text(cx + 6, r.y + 5, ok ? studio_template_name(t) : "Locked", ui_style_text(WIN_MINT));
        ui_text(cx + 6, r.y + 16, doomo_music_names[g], st_pattern_color(g, 0));
        if (ok) {
            ui_text_wrap(cx + 6, r.y + 28, cw - 12, studio_template_blurb(t), st->text_dim, 1);
        } else {
            const DoomoUnlock *u = unlock_by_id(studio_template_unlock(t));
            ui_text_wrap(cx + 6, r.y + 28, cw - 12, u ? u->how : "Keep making music!", st->text_dim, 1);
        }
        /* dancing notes */
        for (i = 0; i < 3; i++)
            st_note_glyph(cx + cw / 2 - 12 + i * 10, r.y + r.h - 20 - (ui_wave((int)studio.t + i * 10, 24, 3) + 3),
                          st_pattern_color(i, 1));
        ui_text(cx + 6, r.y + r.h - 11, STR_ICON_A "Use " STR_ICON_B "Back", st->text_dim);
    }
}

void tab_file_draw(StRect r)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int wide = st_wide(), rowh = wide ? 14 : 10, i, lw = r.w, vis = file_vis(rowh), top = studio.ftop;
    char buf[48];
    if (studio.fmode == 1) { draw_templates(r); return; }
    if (top + vis > FR_COUNT) top = DOOMO_MAX(0, FR_COUNT - vis);
    st_panel(r.x, r.y, lw, r.h, WIN_CREAM);
    for (i = top; i < FR_COUNT; i++) {
        int y = r.y + 4 + (i - top) * rowh + (i >= FR_NEW ? 3 : 0), sel = i == studio.fsel;
        uint8_t tc = sel ? PAL_WHITE : st->text;
        if (y + rowh > r.y + r.h) break;
        if (sel) {
            ui_round_rect(r.x + 3, y - 1, lw - 6, rowh, 3, st->accent);
            ui_cursor(r.x + 4, y + (rowh - 8) / 2);
        }
        if (i < SONG_BOOK_SIZE) {
            const DoomoSong *s = &doomo_song_book[i];
            int ty = y + (rowh - 9) / 2;
            /* a little book page */
            ui_round_rect(r.x + 14, y, 9, rowh - 2, 2, (s->flags & SONGF_VALID) ? st_pattern_color(i, 1) : st->fill_hi);
            doomo_snprintf(buf, sizeof(buf), "%d", i + 1);
            ui_text(r.x + 16, ty, buf, (s->flags & SONGF_VALID) ? PAL_WHITE : st->text_dim);
            if (s->flags & SONGF_VALID) {
                ui_text(r.x + 28, ty, s->title, tc);
                doomo_snprintf(buf, sizeof(buf), "%s " STR_ICON_NOTE "%d",
                               wide ? doomo_music_names[s->genre % MUSIC_COUNT] : "", song_singer_count(s));
                ui_text(r.x + lw - 10 - ui_text_width(buf), ty, buf, sel ? PAL_WHITE : st->text_dim);
                if (s->flags & SONGF_FAVORITE) ui_heart(r.x + 28 + ui_text_width(s->title) + 7, y + rowh / 2 - 1, 3, PAL_PAINT(PAINT_ROSE, 1));
                if (studio.slot == i) ui_text(r.x + 28 + ui_text_width(s->title) + 13, ty, studio.dirty ? "*" : "", tc);
            } else {
                ui_text(r.x + 28, ty, "- empty page -", sel ? PAL_WHITE : st->text_dim);
            }
        } else {
            static const char *const labels[] = { "New song...", "New from a template...",
                                                  "Inspire me!", "Undo", "Redo", "Studio level", "Leave the studio" };
            int ty = y + (rowh - 9) / 2;
            if (i == FR_UNDO) doomo_snprintf(buf, sizeof(buf), "Undo (%d)", ed_undo_steps());
            else if (i == FR_REDO) doomo_snprintf(buf, sizeof(buf), "Redo (%d)", ed_redo_steps());
            else if (i == FR_LEVEL) doomo_snprintf(buf, sizeof(buf), "Studio level: %s", st_level() ? "Advanced" : "Easy");
            else doomo_snprintf(buf, sizeof(buf), "%s", labels[i - FR_NEW]);
            if (i == FR_INSPIRE) ui_sparkle(r.x + 19, y + rowh / 2 - 1, (int)(studio.t >> 1) & 15, PAL_PAINT(PAINT_LEMON, 1));
            else if (i == FR_NEW) st_note_glyph(r.x + 15, ty, tc);
            else if (i == FR_TEMPLATE) ui_star(r.x + 18, y + rowh / 2 - 1, 3, PAL_PAINT(PAINT_HONEY, 2));
            else if (i == FR_UNDO) ui_text(r.x + 15, ty, STR_ICON_ARROW_L, tc);
            else if (i == FR_REDO) ui_text(r.x + 15, ty, STR_ICON_ARROW_R, tc);
            else if (i == FR_LEVEL) ui_text(r.x + 15, ty, "*", tc);
            else ui_text(r.x + 15, ty, STR_ICON_HOUSE, tc);
            ui_text(r.x + 28, ty, buf, tc);
        }
        if (i == SONG_BOOK_SIZE - 1) plat_rect(r.x + 10, y + rowh + 1, lw - 20, 1, st->fill_hi);
    }
    if (FR_COUNT > vis) ui_scroll_arrows(r.x + lw - 8, r.y + 3, r.y + r.h - 3, top > 0, top + vis < FR_COUNT);
    if (wide) {
        doomo_snprintf(buf, sizeof(buf), "Saved %d  Shows %d  Inspired %d", studio_stats.saved,
                       studio_stats.performed, studio_stats.inspired);
        ui_text(r.x + 8, r.y + r.h - 11, buf, st->text_dim);
    }
}
