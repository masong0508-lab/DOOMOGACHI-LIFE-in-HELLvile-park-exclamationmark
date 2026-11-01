/* lang_native.c - the native platform's plat_text_is_const() (L1; lang.h):
 * the runner's and the tests' string literals are in the program's
 * read-only segments, so lang_tr() may remember its answers for them by
 * address, as on the consoles. Linux (ELF): the segments that are loaded
 * neither writable nor executable come from dl_iterate_phdr(). Elsewhere
 * (macOS): 0, no cache, which only costs speed. */
#if defined(__linux__)
#define _GNU_SOURCE
#include <link.h>
#endif
#include <stdint.h>
#include "doomo/lang.h"

#if defined(__linux__)
#define RO_MAX 8
static uintptr_t ro_lo[RO_MAX], ro_hi[RO_MAX];
static int ro_n = -1;

static int ro_scan(struct dl_phdr_info *info, size_t size, void *data)
{
    int i;
    (void)size;
    (void)data;
    for (i = 0; i < info->dlpi_phnum && ro_n < RO_MAX; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD || (ph->p_flags & (PF_W | PF_X))) continue;
        ro_lo[ro_n] = (uintptr_t)(info->dlpi_addr + ph->p_vaddr);
        ro_hi[ro_n] = ro_lo[ro_n] + (uintptr_t)ph->p_memsz;
        ro_n++;
    }
    return 1;                         /* the first object is the program itself */
}

int plat_text_is_const(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    int i;
    if (ro_n < 0) {
        ro_n = 0;
        dl_iterate_phdr(ro_scan, NULL);
    }
    for (i = 0; i < ro_n; i++)
        if (a >= ro_lo[i] && a < ro_hi[i]) return 1;
    return 0;
}
#endif
