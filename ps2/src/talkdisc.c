/* Disc-backed Talk through the SDK filesystem, with two aligned blocks.
 * The raw CDVD read could fail after a successful lookup in Play!.
 * Both physical CD/DVD and emulator boots use the ISO file path here. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "doomo/plat_ext.h"
#include "talk_path.h"

#define BLOCK_BYTES 2048u
#define TALK_DISC_LIMIT (4u * 1024u * 1024u)
static uint8_t block_data[2][BLOCK_BYTES] __attribute__((aligned(64)));
static uint32_t bank_size;
static int bank_fd = -1, cached_block[2] = { -1, -1 }, english_present;
static unsigned recent_block;
static char active_code[8];

static void close_language(void)
{
    if (bank_fd >= 0) close(bank_fd);
    bank_fd = cached_block[0] = cached_block[1] = -1;
    recent_block = 1; /* the first fill goes to slot zero */
    bank_size = 0;
    active_code[0] = 0;
}

static const uint8_t *read_block(uint32_t block)
{
    uint32_t start = block * BLOCK_BYTES;
    uint32_t bytes = bank_size - start, at = 0;
    unsigned slot = recent_block;
    if (cached_block[slot] == (int)block) return block_data[slot];
    slot ^= 1u;
    if (cached_block[slot] == (int)block) {
        recent_block = slot;
        return block_data[slot];
    }
    if (bytes > BLOCK_BYTES) bytes = BLOCK_BYTES;
    cached_block[slot] = -1;
    if (lseek(bank_fd, (off_t)start, SEEK_SET) != (off_t)start) return NULL;
    while (at < bytes) {
        ssize_t got = read(bank_fd, block_data[slot] + at, bytes - at);
        if (got <= 0) return NULL;
        at += (uint32_t)got;
    }
    cached_block[slot] = (int)block;
    recent_block = slot;
    return block_data[slot];
}

static int open_language(const char *code)
{
    static const char prefix[] = "cdrom0:";
    char path[sizeof prefix - 1 + 24];
    off_t size;
    const uint8_t *header;
    close_language();
    memcpy(path, prefix, sizeof prefix - 1);
    if (!ps2_talk_path(path + sizeof prefix - 1, code)) return 0;
    bank_fd = open(path, O_RDONLY);
    if (bank_fd < 0) return 0;
    size = lseek(bank_fd, 0, SEEK_END);
    if (size < 20 || size > (off_t)TALK_DISC_LIMIT) {
        close_language();
        return 0;
    }
    bank_size = (uint32_t)size;
    header = read_block(0);
    if (!header || memcmp(header, "TALK", 4) != 0) {
        close_language();
        return 0;
    }
    strcpy(active_code, code); /* ps2_talk_path verified length <= 6 */
    return 1;
}

void ps2_talk_disc_init(void)
{
    english_present = open_language("en");
    if (!english_present)
        printf("PS2 Talk: no readable TALK_EN.BIN disc bank; conversations disabled\n");
    else
        printf("PS2 Talk: disc bank TALK_EN.BIN, %lu bytes\n", (unsigned long)bank_size);
}

uint32_t plat_talk_load(const char *code)
{
    if (!code) return 0;
    if (bank_size && strcmp(active_code, code) == 0) return bank_size;
    return open_language(code) ? bank_size : 0;
}

uint32_t plat_talk_read(uint32_t off, void *dst, uint32_t len)
{
    uint8_t *out = dst;
    uint32_t at = 0;
    if (!dst || !bank_size || off > bank_size || len > bank_size - off) return 0;
    while (at < len) {
        uint32_t pos = off + at;
        uint32_t block = pos / BLOCK_BYTES;
        uint32_t in = pos % BLOCK_BYTES;
        uint32_t n = BLOCK_BYTES - in;
        const uint8_t *data;
        if (n > len - at) n = len - at;
        data = read_block(block);
        if (!data) return 0;
        memcpy(out + at, data + in, n);
        at += n;
    }
    return len;
}

uint32_t plat_features(void)
{
    return (english_present ? PLATF_TALK : 0u) | PLATF_3D_QUALITY;
}
