/* Test the PS2 Talk reader against a file at its real disc path. */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "doomo/plat_ext.h"

#define BLOCK 2048u
#define BANK_BYTES (3u * BLOCK + 17u)
static const char path[] = "cdrom0:\\TALK_EN.BIN;1";
static uint8_t bank[BANK_BYTES];
static int failures, checks;

#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

void ps2_talk_disc_init(void);

static void write_bank(void)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    CHECK(fd >= 0);
    if (fd < 0) return;
    CHECK(write(fd, bank, sizeof bank) == (ssize_t)sizeof bank);
    CHECK(close(fd) == 0);
}

static void verify_read(uint32_t offset, uint32_t n)
{
    uint8_t dst[97];
    memset(dst, 0xA5, sizeof dst);
    CHECK(n < sizeof dst);
    if (n >= sizeof dst) return;
    CHECK(plat_talk_read(offset, dst, n) == n);
    CHECK(memcmp(dst, bank + offset, n) == 0);
    CHECK(dst[n] == 0xA5);
}

int main(void)
{
    char dir[] = "/tmp/doomo-ps2-talk-XXXXXX";
    uint32_t i;
    uint8_t dst[96];
    int fd;
    if (!mkdtemp(dir) || chdir(dir) < 0) return 1;
    for (i = 0; i < sizeof bank; i++) bank[i] = (uint8_t)(i * 73u + (i >> 3) + i / BLOCK);

    ps2_talk_disc_init();
    CHECK((plat_features() & PLATF_TALK) == 0);
    CHECK(plat_talk_load("en") == 0);
    write_bank();
    ps2_talk_disc_init();
    CHECK((plat_features() & PLATF_TALK) == 0); /* invalid bank signature */

    memcpy(bank, "TALK", 4);
    write_bank();
    ps2_talk_disc_init();
    CHECK((plat_features() & PLATF_TALK) != 0);
    CHECK(plat_talk_load("en") == BANK_BYTES);
    verify_read(10, 30);
    verify_read(17, 40);
    verify_read(BLOCK - 8, 48);
    verify_read(3 * BLOCK, 17);
    CHECK(plat_talk_read(BANK_BYTES - 1, dst, 2) == 0);
    CHECK(plat_talk_read(BANK_BYTES, dst, 1) == 0);
    CHECK(plat_talk_read(0, NULL, 2) == 0);

    /* Lookups alternate between table records and text in different
     * sectors. Confirm that both hot sectors remain readable without
     * another transfer, even for sectors with the same parity. */
    verify_read(12, 16);
    verify_read(2 * BLOCK + 12, 16);
    fd = open(path, O_WRONLY);
    CHECK(fd >= 0);
    if (fd >= 0) {
        CHECK(ftruncate(fd, 0) == 0);
        CHECK(close(fd) == 0);
    }
    verify_read(12, 16);
    verify_read(2 * BLOCK + 12, 16);
    write_bank();
    ps2_talk_disc_init();

    /* A short read must never re-use bytes from a different cached block. */
    fd = open(path, O_WRONLY);
    CHECK(fd >= 0);
    if (fd >= 0) {
        CHECK(ftruncate(fd, 2 * BLOCK + 8) == 0);
        CHECK(close(fd) == 0);
    }
    memset(dst, 0x5C, sizeof dst);
    CHECK(plat_talk_read(2 * BLOCK, dst, 16) == 0);
    CHECK(dst[0] == 0x5C);
    write_bank();
    verify_read(2 * BLOCK, 16);

    CHECK(plat_talk_load("../en") == 0);
    CHECK(plat_talk_read(0, dst, 16) == 0);
    CHECK(plat_talk_load("en") == BANK_BYTES);
    verify_read(0, 16);

    CHECK(unlink(path) == 0);
    ps2_talk_disc_init();
    CHECK((plat_features() & PLATF_TALK) == 0);
    CHECK(plat_talk_read(0, dst, 16) == 0);
    CHECK(chdir("/tmp") == 0);
    CHECK(rmdir(dir) == 0);
    printf("PS2 Talk disc transport: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
