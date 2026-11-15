/* talkdisc_host.c - TD1 host test: the real dc/src/talkdisc.c and core/src/talk_db.c over a mock of the
 * KOS file calls (packdisc_mock.c), reading a TALK_en.BIN built by talkc.py: the bank loads whole from
 * "the disc", reads are memcpys, the database opens and verifies, a missing language is "none". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/plat_ext.h"
#include "doomo/town.h"
#include "doomo/txt.h"
DoomoTown doomo_town;                                   /* option 0 = English */
const uint8_t *txt_plat_pack(int pack) { (void)pack; return NULL; }
int txt_plat_load(int pack) { (void)pack; return 0; }
extern long mock_reads, mock_opens;
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void)
{
    uint8_t head[16], again[16];
    uint32_t size, r0;
    int en = lang_find("en");
    CHECK(en >= 0);
    CHECK(plat_talk_read(0, head, 16) == 0);                  /* nothing loaded yet */
    CHECK(plat_talk_load("xx") == 0 && plat_talk_load("") == 0 && plat_talk_load(NULL) == 0);
    CHECK(plat_talk_load("waytoolongcode") == 0);
    CHECK(mock_opens == 0);                                   /* a bad code never reaches the disc */
    size = plat_talk_load("en");
    CHECK(size > 1024);
    CHECK(mock_opens == 1 && mock_reads == 1);                /* the whole file, one read */
    CHECK(plat_talk_load("en") == size && mock_opens == 1);   /* already loaded: no second disc read */
    CHECK(plat_talk_read(0, head, 16) == 16 && memcmp(head, "TALK", 4) == 0);
    CHECK(plat_talk_read(size - 16, again, 16) == 16);
    CHECK(plat_talk_read(size - 15, again, 16) == 0);         /* past the end */
    CHECK(plat_talk_read(size + 1, again, 0) == 0);
    CHECK(plat_talk_read(size, again, 0) == 0);
    CHECK(plat_talk_read(0xFFFFFFF0u, again, 32) == 0);       /* off + len must not wrap */
    r0 = mock_reads;
    CHECK(plat_talk_read(4, again, 8) == 8 && mock_reads == r0);   /* reads never touch the disc */
    CHECK(plat_talk_load("fr") == 0 && plat_talk_read(0, head, 16) == 0);   /* another language replaces it */
    CHECK(talk_db_open(en) == TALK_DB_OK && talk_db_is_open());
    CHECK(talk_db_nconcepts() > 100);
    CHECK(talk_db_verify() == TALK_DB_OK);                    /* every bank's CRC over the disc copy */
    printf("talkdisc host: %s (TALK_EN.BIN %u bytes)\n", fails ? "FAILED" : "all checks passed", (unsigned)size);
    return fails != 0;
}
