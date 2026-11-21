/* talkdisc_host.c - TD1 host test: the real dc/src/talkdisc.c and core/src/talk_db.c over a mock of the
 * KOS file calls (packdisc_mock.c), reading a TALK_en.BIN built by talkc.py: the bank loads whole from
 * "the disc", reads are memcpys, the database opens and verifies, a missing language is "none". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/plat_ext.h"
#include "dc_plat.h"
#include "doomo/town.h"
#include "doomo/txt.h"
DoomoTown doomo_town;                                   /* option 0 = English */
const uint8_t *txt_plat_pack(int pack) { (void)pack; return NULL; }
int txt_plat_load(int pack) { (void)pack; return 0; }
extern long mock_reads, mock_opens, mock_maxread;
static int fails;
static uint32_t clk;                                      /* a clock that normally advances 5 ms per read */
static uint32_t clk_step = 5000;
uint32_t dc_us(void) { return clk += clk_step; }
void dc_logf(const char *fmt, ...) { (void)fmt; }
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void)
{
    uint8_t head[16], again[16];
    uint32_t size, r0;
    int ticks;
    int en = lang_find("en");
    CHECK(en >= 0);
    CHECK(plat_talk_read(0, head, 16) == 0);                  /* nothing loaded yet */
    CHECK(plat_talk_load("xx") == 0 && plat_talk_load("") == 0 && plat_talk_load(NULL) == 0);
    CHECK(plat_talk_load("waytoolongcode") == 0);
    CHECK(mock_opens == 0);                                   /* a bad code never reaches the disc */
    /* the load is sliced: the first call opens the file and answers LOADING, a read stays out of the
     * way until the pump (one call per tick) has brought every slice in */
    CHECK(plat_talk_load("en") == PLAT_TALK_LOADING);
    CHECK(mock_opens == 1 && mock_reads == 0);                /* opening does not read: no tick pays the file */
    CHECK(plat_talk_read(0, head, 16) == 0);                  /* not readable while loading */
    dc_talk_pump(0);                                          /* a full tick (slot 0): even the first slice waits */
    CHECK(mock_reads == 0);
    dc_talk_pump(19999);                                      /* no slice time known yet: it is assumed 20000 us */
    CHECK(mock_reads == 0);
    dc_talk_pump(20000);                                      /* the guess fits; each slice then takes 5000: 4 slices */
    CHECK(mock_reads == 4);
    dc_talk_pump(4999);                                       /* the last slice took 5000 us: this slot is too short */
    CHECK(mock_reads == 4);
    dc_talk_pump(5000);
    CHECK(mock_reads == 5);
    /* An emulated zero-duration read must not cause a single idle tick
     * to pull an entire bank. The floor allows four sectors in 20 ms,
     * then the normal 5 ms host clock is restored for the rest. */
    clk_step = 0;
    dc_talk_pump(20000);
    CHECK(mock_reads == 9);
    clk_step = 5000;
    dc_talk_pump(12000);                                      /* a roomy slot takes two slices (5000 each), the third does not fit */
    CHECK(mock_reads == 11);
    for (ticks = 0; plat_talk_load("en") == PLAT_TALK_LOADING && ticks < 100000; ticks++) {
        long before = mock_reads;
        dc_talk_pump(5000);
        CHECK(mock_reads == before + 1);                      /* one slice per tick */
    }
    size = plat_talk_load("en");
    CHECK(size > 2048 * 4 && size != PLAT_TALK_LOADING);
    CHECK(mock_reads == (long)((size + 2047u) / 2048u) && mock_maxread == 2048);   /* no read bigger than a slice */
    CHECK(mock_opens == 1);
    dc_talk_pump(0xFFFFFFFFu);                                /* idle: nothing more to read */
    CHECK(mock_reads == (long)((size + 2047u) / 2048u));
    CHECK(plat_talk_read(0, head, 16) == 16 && memcmp(head, "TALK", 4) == 0);
    CHECK(plat_talk_read(size - 16, again, 16) == 16);
    CHECK(plat_talk_read(size - 15, again, 16) == 0);         /* past the end */
    CHECK(plat_talk_read(size + 1, again, 0) == 0);
    CHECK(plat_talk_read(size, again, 0) == 0);
    CHECK(plat_talk_read(0xFFFFFFF0u, again, 32) == 0);       /* off + len must not wrap */
    r0 = mock_reads;
    CHECK(plat_talk_read(4, again, 8) == 8 && mock_reads == r0);   /* reads never touch the disc */
    CHECK(plat_talk_load("fr") == 0 && plat_talk_read(0, head, 16) == 0);   /* another language replaces it */
    /* the prefetch: a pump with no load asked for starts the English bank by itself after TALK_PREFETCH_TICKS */
    ticks = 0;
    while (plat_talk_read(0, head, 16) == 0 && ticks < 100000) { dc_talk_pump(5000); ticks++; }
    CHECK(ticks == 90 + (int)((size + 2047u) / 2048u));       /* the wait, the open, then one slice a tick */
    CHECK(plat_talk_load("en") == size);
    CHECK(talk_db_open(en) == TALK_DB_OK && talk_db_is_open());
    CHECK(talk_db_nconcepts() > 100);
    CHECK(talk_db_verify() == TALK_DB_OK);                    /* every bank's CRC over the disc copy */
    printf("talkdisc host: %s (TALK_EN.BIN %u bytes)\n", fails ? "FAILED" : "all checks passed", (unsigned)size);
    return fails != 0;
}
