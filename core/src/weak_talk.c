/* weak_talk.c - weak defaults for Talk's platform and house hooks (owner:
 * TK, T0; docs/TALK.md). A platform with Talk (PS1 TP1, DC TD1, native T2)
 * overrides the plat_talk_* hooks with strong definitions in its own tree;
 * the house mode (T8) overrides home_talk_enter. plat_room_project's
 * default lives with the projection it repeats, in room_ref.c.
 *
 * Every default means "no Talk here" and is silent (no STUB log): the
 * house asks on normal paths (an A press on a housemate), where "no" is
 * the right answer on the GBA. With --gc-sections they cost nothing.
 */
#include "doomo/plat_ext.h"
#include "doomo/talk.h"

#define WEAK __attribute__((weak))

WEAK uint32_t plat_talk_load(const char *code)
{
    (void)code;
    return 0;
}

WEAK uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    (void)off; (void)dst; (void)len;
    return 0;
}

WEAK int home_talk_enter(int who)
{
    (void)who;
    return 0;
}
