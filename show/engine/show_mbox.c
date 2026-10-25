/* show_mbox.c - g_show_mbox (owner: ENGINE lane 1; spec 8.9; layout and
 * protocol in show/include/show/show_mbox.h). The Lua fork finds it by
 * name in show.map (nm -f posix). Nothing touches it on a console. */
#include <string.h>
#include "show_core.h"

ShowMbox g_show_mbox = {
    SHOW_MBOX_MAGIC, SHOW_MBOX_ABI, SHOW_CANVAS_W, SHOW_CANVAS_H, SHOW_CANVAS_Y,
    -1, -1,             /* req_shot, req_frame */
    -1, -1,             /* cur_shot, cur_frame */
    0, 0, 0, { 0 }, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, SHOW_ST_BOOT, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0, 0, { 0 }
};

/* take a seek request (Lua writes while the game is stopped); 1 = taken */
int show_mbox_take_seek(int *shot, int *frame);
int show_mbox_take_seek(int *shot, int *frame)
{
    int s = g_show_mbox.req_shot;
    if (s < 0) return 0;
    *shot = s;
    *frame = g_show_mbox.req_frame < 0 ? 0 : g_show_mbox.req_frame;
    g_show_mbox.req_shot = -1;
    g_show_mbox.req_frame = -1;
    return 1;
}

void show_mbox_publish(int shot, int frame, uint32_t ep_frame, const char *id, uint32_t p1);
void show_mbox_publish(int shot, int frame, uint32_t ep_frame, const char *id, uint32_t p1)
{
    ShowMbox *m = &g_show_mbox;
    m->cur_shot = shot;
    m->cur_frame = frame;
    m->ep_frame = ep_frame;
    m->p1_state_crc = p1;
    m->p2_draw_crc = 0;
    memset(m->shot_id, 0, sizeof m->shot_id);
    if (id) memcpy(m->shot_id, id, SHW_ID_LEN);
    show_stats_publish();
    m->seq++;
}
