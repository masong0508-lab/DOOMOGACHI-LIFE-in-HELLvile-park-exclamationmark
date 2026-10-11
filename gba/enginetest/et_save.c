/* et_save.c - flash save self-test for the ENGINE_TEST ROM (owner: gba).
 *
 * Run by holding SELECT at boot. Log lines (checked by the harness):
 *   SAVETEST: chip <name> <KB>
 *   SAVETEST: persisted ok gen=N     a previous run's data read back intact
 *   SAVETEST: no previous data       (first run with an empty save file)
 *   SAVETEST: write ok gen=N         both slots written, read back, verified
 *   SAVETEST: FAIL <what>
 * Slot 1 first gets a 12000-byte image through plat_save_write/_read;
 * then both slots get 30000 bytes STREAMED (plat_save_stream_*, read back
 * with plat_save_read_at, as the core's saves do): slot 1 from offset
 * 0x0A000 crosses the 64 KB bank boundary of the 128 KB chip. An abandoned
 * stream must leave the slot empty; erase is checked too.
 */
#include "gba_plat.h"
#include "doomo/doomo.h"
#include "enginetest.h"

#define TEST_N 30000u           /* streamed (platform.h), per slot      */
#define IMG_N  12000u           /* plat_save_write / plat_save_read      */
#define CHUNK  1000u
static uint8_t *buf;            /* the platform's save scratch (bigbuf.c) */

static uint8_t pat(uint32_t gen, int slot, uint32_t i)
{
    uint32_t x = (i + 1) * 2654435761u ^ (gen * 0x9E3779B9u) ^ ((uint32_t)slot << 29);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    if (i == 0) return 'E';
    if (i == 1) return 'T';
    if (i == 2) return (uint8_t)gen;
    if (i == 3) return (uint8_t)slot;
    return (uint8_t)x;
}

/* the pattern, streamed in CHUNK pieces */
static int stream_write(uint32_t gen, int slot, uint32_t n)
{
    uint32_t pos = 0, i;
    if (!plat_save_stream_begin(slot, n)) return 0;
    while (pos < n) {
        uint32_t k = n - pos < CHUNK ? n - pos : CHUNK;
        for (i = 0; i < k; i++) buf[i] = pat(gen, slot, pos + i);
        if (!plat_save_stream_write(buf, k)) { plat_save_stream_end(0); return 0; }
        pos += k;
    }
    return plat_save_stream_end(1);
}

/* read back piecewise; returns 1 if the slot holds exactly the pattern */
static int stream_check(uint32_t gen, int slot, uint32_t n)
{
    uint32_t pos = 0, i, size = 0;
    while (pos < n) {
        uint32_t k = n - pos < CHUNK ? n - pos : CHUNK;
        if (!plat_save_read_at(slot, pos, buf, k, &size) || size != n) return 0;
        for (i = 0; i < k; i++) if (buf[i] != pat(gen, slot, pos + i)) return 0;
        pos += k;
    }
    return 1;
}

void et_save_test(void)
{
    uint32_t size = 0, gen = 0, cap = 0, i;
    uint8_t head[4];
    int slot, ok = 1;
    buf = (uint8_t *)plat_save_scratch(&cap);
    if (cap < IMG_N) { gba_log("SAVETEST: FAIL scratch too small"); return; }
    gba_logf("SAVETEST: chip %s %dKB", flash_chip_name(), flash_detected());
    if (!flash_detected()) { gba_log("SAVETEST: FAIL no flash chip"); return; }
    /* previous run? */
    if (plat_save_read_at(0, 0, head, 4, &size) && size == TEST_N && head[0] == 'E' &&
        head[1] == 'T') {
        gen = head[2];
        if (stream_check(gen, 0, TEST_N) && stream_check(gen, 1, TEST_N))
            gba_logf("SAVETEST: persisted ok gen=%u", gen);
        else
            gba_log("SAVETEST: FAIL persisted data corrupt");
        gen++;
    } else {
        gba_log("SAVETEST: no previous data");
    }
    /* whole-image calls: slot 1 (from 0x0A000) */
    for (i = 0; i < IMG_N; i++) buf[i] = pat(gen, 1, i);
    if (!plat_save_write(1, buf, IMG_N)) { gba_log("SAVETEST: FAIL write image"); ok = 0; }
    if (ok) {
        memset(buf, 0, IMG_N);
        if (!plat_save_read(1, buf, IMG_N, &size) || size != IMG_N) ok = 0;
        for (i = 0; ok && i < IMG_N; i++) if (buf[i] != pat(gen, 1, i)) ok = 0;
        if (!ok) gba_log("SAVETEST: FAIL verify image");
    }
    /* streamed: both slots, slot 1 crosses the 64 KB bank boundary */
    for (slot = 0; slot < 2 && ok; slot++) {
        if (!stream_write(gen, slot, TEST_N)) { gba_logf("SAVETEST: FAIL write slot %d", slot); ok = 0; break; }
    }
    for (slot = 0; slot < 2 && ok; slot++) {
        if (!stream_check(gen, slot, TEST_N)) {
            gba_logf("SAVETEST: FAIL verify slot %d", slot);
            ok = 0;
        }
    }
    /* an abandoned stream leaves the slot empty; erase + rewrite slot 1 */
    if (ok) {
        if (!plat_save_stream_begin(1, TEST_N) || plat_save_stream_end(0) ||
            plat_save_read_at(1, 0, head, 4, &size)) {
            gba_log("SAVETEST: FAIL abandon");
            ok = 0;
        } else if (!stream_write(gen, 1, TEST_N) || !plat_save_erase(1) ||
                   plat_save_read(1, buf, 4, &size)) {
            gba_log("SAVETEST: FAIL erase");
            ok = 0;
        } else {
            ok = stream_write(gen, 1, TEST_N);
            if (!ok) gba_log("SAVETEST: FAIL rewrite slot 1");
        }
    }
    if (ok) gba_logf("SAVETEST: write ok gen=%u", gen);
}
