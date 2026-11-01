/* lang_ps1.c - the PS1's language hooks (L1; core/include/doomo/lang.h).
 *
 * The UI tables: the EXE links no core/gen/lang_tab_*.c (all of them are
 * far bigger than the RAM the game can spare; CMakeLists.txt), only each
 * language's resident table core/gen/lang_res_<id>.c, its short strings of
 * core/lang/<code>/ui_main.tsv. L6 replaces this with tables read from the
 * disc per overlay group (it must call lang_flush() when it swaps one).
 *
 * Constant strings: the resident .text and .rodata, [__text_start,
 * __CTOR_LIST__) in both linker scripts (ps1/tools/ovl_plan.py, the
 * PSn00bSDK's exe.ld). The scene overlays' const data lives in the slot
 * after the .bss, which another group overwrites, so it is never cached:
 * lang_tr() looks those strings up every time and needs no flush when the
 * group changes.
 */
#include <stdint.h>
#include "doomo/lang.h"

extern const char __text_start[], __CTOR_LIST__[];

const uint32_t *plat_lang_table(int lang)
{
    return lang >= 0 && lang < LANG_COUNT ? doomo_lang_res_tables[lang] : 0;
}

int plat_text_is_const(const void *p)
{
    return (uintptr_t)p - (uintptr_t)__text_start < (uintptr_t)(__CTOR_LIST__ - __text_start);
}
