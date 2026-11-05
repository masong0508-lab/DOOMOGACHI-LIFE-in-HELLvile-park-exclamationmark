/* packdisc_host.c - E10 host test: the real dc/src/packdisc.c and core/src/pack.c over a mock of
 * the KOS file calls (packdisc_mock.c), reading a pack built by mpack.py for port dc. */
#include <stdio.h>
#include <string.h>
#include "doomo/pack.h"
#include "doomo/txt.h"
extern long mock_reads;
static const uint8_t *g_user[16];
int txt_user_set(int id, const uint8_t *img) { g_user[id - TXT_PACK_USER_FIRST] = img; return 1; }
const uint8_t *plat_pack_blob(uint32_t *size) { if (size) *size = 0; return 0; }
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void)
{
    DoomoPackInfo in;
    uint32_t cap = 0, scap = 0, sz = 0;
    uint8_t *still, *sbuf;
    static uint8_t big[PACK_CG_W * PACK_CG_H];
    CHECK(pack_open());
    CHECK(pack_streamed());
    CHECK(pack_info(&in) && in.port == PK_PORT_DC && in.n_cg == 1 && in.n_script == 1);
    CHECK(mock_reads < 40);                          /* the window: not one disc read per 16-byte entry */
    still = plat_mslot_buf(&cap);
    sbuf = plat_mslot_script_buf(&scap);
    CHECK(still && cap == PACK_CG_W * PACK_CG_H && sbuf && scap == PACK_SCRIPT_CAP_DC);
    CHECK(pack_art_read(PK_KIND_CG, PACK_UID(PK_KIND_CG, 0), still, cap) == PACK_CG_W * PACK_CG_H);
    CHECK(still[0] == 0 && still[20] == 7 && still[320 * 20 + 20] == 14);   /* example_spec's test pattern */
    CHECK(pack_art(PK_KIND_CG, PACK_UID(PK_KIND_CG, 0), &sz) == NULL);      /* streamed: no in-place pointer */
    CHECK(pack_script_bind(0, sbuf, scap) == TXT_PACK_USER_FIRST);
    CHECK(g_user[0] == sbuf && pack_script(0) == TXT_PACK_USER_FIRST);
    pack_script_release();
    CHECK(g_user[0] == NULL);
    CHECK(plat_pack_read(1u << 30, big, 4) == 0);                            /* past the end */
    CHECK(plat_pack_read(0, big, 64) == 64 && memcmp(big, "DMPK", 4) == 0);
    printf("packdisc host: %s\n", fails ? "FAILED" : "all checks passed");
    return fails != 0;
}
