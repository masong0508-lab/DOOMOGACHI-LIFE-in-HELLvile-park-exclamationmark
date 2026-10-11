/* et_save.c - flash save self-test for the ENGINE_TEST ROM (owner: gba).
 *
 * Run by holding SELECT at boot. Log lines (checked by the harness):
 *   SAVETEST: chip <name> <KB>
 *   SAVETEST: persisted ok gen=N     a previous run's data read back intact
 *   SAVETEST: no previous data       (first run with an empty save file)
 *   SAVETEST: write ok gen=N         both slots written, read back, verified
 *   SAVETEST: FAIL <what>
 * Slot 0 gets 30000 bytes, slot 1 30000 bytes from offset 0x0A000, which
 * crosses the 64 KB bank boundary of the 128 KB chip.
 */
#include "gba_plat.h"
#include "doomo/doomo.h"
#include "enginetest.h"

#define TEST_N 30000u
static uint8_t *buf;            /* the platform's save scratch (bigbuf.c) */

static uint8_t pat(uint32_t gen, int slot, uint32_t i)
{
    uint32_t x = (i + 1) * 2654435761u ^ (gen * 0x9E3779B9u) ^ ((uint32_t)slot << 29);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return (uint8_t)x;
}

static void fill(uint32_t gen, int slot)
{
    uint32_t i;
    buf[0] = 'E'; buf[1] = 'T'; buf[2] = (uint8_t)gen; buf[3] = (uint8_t)slot;
    for (i = 4; i < TEST_N; i++) buf[i] = pat(gen, slot, i);
}

static int check(uint32_t gen, int slot)
{
    uint32_t i;
    if (buf[0] != 'E' || buf[1] != 'T' || buf[2] != (uint8_t)gen || buf[3] != (uint8_t)slot)
        return 0;
    for (i = 4; i < TEST_N; i++)
        if (buf[i] != pat(gen, slot, i)) return 0;
    return 1;
}

void et_save_test(void)
{
    uint32_t size = 0, gen = 0, cap = 0;
    int slot, ok = 1;
    buf = (uint8_t *)plat_save_scratch(&cap);
    if (cap < TEST_N) { gba_log("SAVETEST: FAIL scratch too small"); return; }
    gba_logf("SAVETEST: chip %s %dKB", flash_chip_name(), flash_detected());
    if (!flash_detected()) { gba_log("SAVETEST: FAIL no flash chip"); return; }
    /* previous run? */
    if (plat_save_read(0, buf, TEST_N, &size) && size == TEST_N && buf[0] == 'E' && buf[1] == 'T') {
        gen = buf[2];
        if (check(gen, 0) && plat_save_read(1, buf, TEST_N, &size) && size == TEST_N &&
            check(gen, 1))
            gba_logf("SAVETEST: persisted ok gen=%u", gen);
        else
            gba_log("SAVETEST: FAIL persisted data corrupt");
        gen++;
    } else {
        gba_log("SAVETEST: no previous data");
    }
    for (slot = 0; slot < 2 && ok; slot++) {
        fill(gen, slot);
        if (!plat_save_write(slot, buf, TEST_N)) { gba_logf("SAVETEST: FAIL write slot %d", slot); ok = 0; break; }
    }
    for (slot = 0; slot < 2 && ok; slot++) {
        memset(buf, 0, TEST_N);
        if (!plat_save_read(slot, buf, TEST_N, &size) || size != TEST_N || !check(gen, slot)) {
            gba_logf("SAVETEST: FAIL verify slot %d (size %u)", slot, size);
            ok = 0;
        }
    }
    /* erase + rewrite slot 1 to exercise plat_save_erase */
    if (ok) {
        if (!plat_save_erase(1) || plat_save_read(1, buf, TEST_N, &size)) {
            gba_log("SAVETEST: FAIL erase");
            ok = 0;
        } else {
            fill(gen, 1);
            ok = plat_save_write(1, buf, TEST_N);
            if (!ok) gba_log("SAVETEST: FAIL rewrite slot 1");
        }
    }
    if (ok) gba_logf("SAVETEST: write ok gen=%u", gen);
}
