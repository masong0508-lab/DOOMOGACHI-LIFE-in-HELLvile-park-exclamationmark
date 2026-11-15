/* test_pslice_policy.c - S3-2: when a portrait slice may run inside a frame (doomo/pslice_policy.h, shared by
 * gba/src/pslice.c and ps1/src/pslice.c). Units: 1000 a vblank, margins 20 and 100, patience 4 / 15, cheap 300. */
#include "test.h"
#include "doomo/pslice_policy.h"

static int ok(uint32_t base, uint32_t spent, uint32_t est, int waited)
{
    return pslice_ok(base, spent, est, 1000, 20, 100, waited, 4, 15, 300);
}

TEST(s32_a_slice_that_fits_the_frames_room_runs)
{
    CHECK(ok(900, 0, 100, 0) == 1);          /* ends at 1000 + ... under the 2 vblanks (1980) */
    CHECK(ok(900, 500, 500, 0) == 1);        /* 1900 <= 1980 */
    CHECK(ok(900, 500, 600, 0) == 0);        /* 2000 is over the room: waits */
}

TEST(s32_a_cheap_slice_over_the_room_is_forced_after_the_patience_once_a_frame)
{
    CHECK(ok(1900, 0, 200, 3) == 0);         /* waited less than 4 */
    CHECK(ok(1900, 0, 200, 4) == 1);         /* ends at 2100: before the 3rd vblank (2980) */
    CHECK(ok(1900, 150, 200, 9) == 0);       /* one forced slice a frame at most */
}

TEST(s32_a_forced_slice_that_would_pass_the_next_vblank_waits_for_idle_time)
{
    /* dear slice (over cheap = 300), the frame already at 1900: it would end at 2900 + 400 = 3300 > 2980 */
    CHECK(ok(1900, 0, 1100, 15) == 0);
    CHECK(ok(1900, 0, 1100, 30) == 0);
    CHECK(ok(1900, 0, 1100, 59) == 0);
    CHECK(ok(1900, 0, 1100, 60) == 1);       /* nothing starves */
    CHECK(ok(1900, 0, 500, 14) == 0);        /* dear: the big patience (15) first */
    CHECK(ok(1900, 0, 500, 15) == 1);        /* 2400 < 2980 */
}
