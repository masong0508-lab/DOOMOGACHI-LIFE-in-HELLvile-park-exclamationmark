/* reverb_test.c - host test of the PS1 reverb preset (ps1/src/reverb.h).
 *   cc -O2 -I ps1/src ps1/harness/reverb_test.c -o reverb_test && ./reverb_test */
#include <stdio.h>
#include "reverb.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main(void)
{
    const Ps1Reverb *p = &ps1_reverb_studio_small;
    uint32_t start = (uint32_t)p->mbase * 8u, end = start + p->size_bytes;
    CHECK(start >= PS1_REVERB_AREA_START);                  /* inside the free work area */
    CHECK(end <= PS1_REVERB_AREA_END);                      /* never past SPU RAM */
    CHECK(end == PS1_REVERB_AREA_END);                      /* BIOS presets end at the top */
    CHECK(PS1_REVERB_REG_BASE + 2u * PS1_REVERB_REGS == 0x1F801E00u);
    CHECK(ps1_reverb_level(REVERB_OFF) == 0);
    CHECK(ps1_reverb_level(REVERB_ROOM) > 0 && ps1_reverb_level(REVERB_HALL) > ps1_reverb_level(REVERB_ROOM));
    CHECK(ps1_reverb_level(REVERB_HALL) < 0x4000);          /* well below full scale */
    printf("reverb preset: work area %05x-%05x, %d registers: %s\n", (unsigned)start, (unsigned)end,
           PS1_REVERB_REGS, fails ? "FAIL" : "ok");
    return fails != 0;
}
