/* lang_gba.c - the GBA's plat_text_is_const() (L1; core/include/doomo/lang.h).
 *
 * Every constant string (literals, the generated tables) is in the
 * cartridge ROM (0x08000000 and its wait-state mirrors up to 0x0DFFFFFF);
 * nothing there is ever written, so lang_tr() may remember its answers by
 * address (a 512-byte cache in EWRAM, docs/GBA.md "EWRAM ledger"). Text
 * built in EWRAM, IWRAM or on the stack is looked up every time.
 */
#include <stdint.h>
#include "doomo/lang.h"

int plat_text_is_const(const void *p)
{
    return (uintptr_t)p - 0x08000000u < 0x06000000u;
}
