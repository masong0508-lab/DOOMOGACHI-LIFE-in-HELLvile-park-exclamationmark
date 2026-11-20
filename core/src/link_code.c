/* link_code.c - lightweight, deterministic GBA/PS1 progress exchange.
 * Only bit shifts and a 16-bit CRC, no heap, global caches or I/O. */
#include "doomo/link_code.h"

static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

/* CCITT-FALSE over the first 13 payload bytes, initial 0xffff. */
static uint16_t crc16(const uint8_t *p, int n)
{
    uint16_t crc = 0xffffu;
    int i, j;
    for (i = 0; i < n; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (j = 0; j < 8; j++)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u));
    }
    return crc;
}

int doomo_link_encode(const DoomoLinkProgress *data, char *code, int capacity)
{
    uint8_t b[15];
    uint16_t crc;
    uint32_t bits = 0;
    unsigned count = 0;
    int i, j = 0;
    if (!data || !code || capacity < DOOMO_LINK_CODE_SIZE) return 0;
    b[0] = 0xd1; /* version / signature: do not accept unknown revisions */
    for (i = 0; i < 4; ++i) b[1 + i] = (uint8_t)(data->breach >> (24 - i * 8));
    b[5] = (uint8_t)(data->gold & 0x7fu);
    for (i = 0; i < 4; ++i) b[6 + i] = (uint8_t)(data->money >> (24 - i * 8));
    b[10] = (uint8_t)(data->exorcisms >> 8);
    b[11] = (uint8_t)data->exorcisms;
    b[12] = 0; /* reserved, kept zero for forward compatibility */
    crc = crc16(b, 13);
    b[13] = (uint8_t)(crc >> 8);
    b[14] = (uint8_t)crc;
    for (i = 0; i < 15; i++) {
        bits = (bits << 8) | b[i];
        count += 8;
        while (count >= 5) {
            count -= 5;
            if (j && j % 5 == 4) code[j++] = '-'; /* every four letters + separator */
            code[j++] = alphabet[(bits >> count) & 31u];
        }
    }
    code[j] = '\0';
    return j == DOOMO_LINK_CODE_SIZE - 1;
}

static int digit(int c)
{
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

int doomo_link_decode(DoomoLinkProgress *out, const char *code)
{
    uint8_t b[15];
    uint32_t bits = 0;
    unsigned count = 0;
    int n = 0, nsym = 0, d, i;
    if (!out || !code) return 0;
    for (; *code; code++) {
        int c = (unsigned char)*code;
        if (c == '-' || c == ' ') continue;
        d = digit(c);
        if (d < 0 || ++nsym > 24) return 0;
        bits = (bits << 5) | (unsigned)d;
        count += 5;
        if (count >= 8) {
            count -= 8;
            if (n >= 15) return 0;
            b[n++] = (uint8_t)(bits >> count);
        }
    }
    if (nsym != 24 || n != 15 || count || b[0] != 0xd1 || b[12] != 0)
        return 0;
    if (crc16(b, 13) != (uint16_t)(((uint16_t)b[13] << 8) | b[14])) return 0;
    /* All seven level ranks are 0..4; reject future/invalid code values. */
    for (i = 0; i < 7; i++) {
        uint32_t r = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) |
                     ((uint32_t)b[3] << 8) | b[4];
        if (((r >> (i * 3)) & 7u) > 4u) return 0;
    }
    out->breach = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) |
                  ((uint32_t)b[3] << 8) | b[4];
    out->gold = b[5];
    if (out->gold & 0x80u) return 0;
    out->money = ((uint32_t)b[6] << 24) | ((uint32_t)b[7] << 16) |
                 ((uint32_t)b[8] << 8) | b[9];
    out->exorcisms = (uint16_t)(((uint16_t)b[10] << 8) | b[11]);
    return 1;
}
