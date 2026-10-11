/* save_flash.c - plat_save_*() on the cartridge's 128 KB FLASH
 * (owner: gba). Low-level chip routines: flash.iwram.c.
 *
 * Layout (platform.h / save.h):
 *   slot 0 (story)     0x00000 .. 0x09FFF   40 KB
 *   slot 1 (free play) 0x0A000 .. 0x1FFFF   88 KB (crosses the 64 KB bank)
 * The last 12 bytes of each slot are a trailer written LAST:
 *   "DMSV", size (LE32), ~size (LE32)
 * so a slot only reads back as valid once every data byte is programmed
 * and verified. Writing first erases the trailer sector (the old save is
 * gone from that moment - the core only writes after a successful
 * serialise), then erases and programs only the 4 KB sectors the data
 * needs, verifying every byte. Capacity per slot is therefore the slot
 * size minus 12 bytes. On a 64 KB chip slot 1 shrinks to what fits.
 */
#include "gba_plat.h"
#include "flash_int.h"
#include "doomo/save.h"

#define SECTOR      4096u
#define TRAILER     12u
#define SLOT0_OFF   0x00000u
#define SLOT0_LEN   0x0A000u
#define SLOT1_OFF   0x0A000u
#define SLOT1_LEN   0x16000u

static uint16_t chip_id;
static int chip_kb;            /* 0 = none */
static int chip_macronix, chip_atmel;
static int cur_bank = -1;
static const char *chip_name = "none";

void flash_init(void)
{
    /* A bank switch before the ID read: harmless on 64 KB chips (unknown
     * command, back to read mode), and it makes emulators that guess the
     * flash size on first use (mGBA) switch to the 128 KB chip. */
    fl_bank(1);
    fl_bank(0);
    chip_id = fl_read_id();
    chip_kb = 0;
    switch (chip_id) {
    case 0x09C2: chip_kb = 128; chip_macronix = 1; chip_name = "Macronix MX29L010"; break;
    case 0x1362: chip_kb = 128; chip_name = "Sanyo LE26FV10N1TS"; break;
    case 0x1CC2: chip_kb = 64; chip_macronix = 1; chip_name = "Macronix MX29L512"; break;
    case 0x1B32: chip_kb = 64; chip_name = "Panasonic MN63F805MNP"; break;
    case 0xD4BF: chip_kb = 64; chip_name = "SST 39VF512"; break;
    case 0x3D1F: chip_kb = 64; chip_atmel = 1; chip_name = "Atmel AT29LV512"; break;
    default:
        chip_name = "none";
        break;
    }
    cur_bank = -1;
    if (chip_kb == 128) { fl_bank(0); cur_bank = 0; }
}

int flash_detected(void) { return chip_kb; }
const char *flash_chip_name(void) { return chip_name; }

/* ---------------------------------------------------------------------- */
static void select_bank(uint32_t addr)
{
    int b = (int)(addr >> 16);
    if (chip_kb != 128) return;
    if (b != cur_bank) { fl_bank(b); cur_bank = b; }
}

static void read_bytes(uint32_t addr, uint8_t *dst, uint32_t n)
{
    while (n) {
        uint32_t room = 0x10000u - (addr & 0xFFFFu);
        uint32_t k = n < room ? n : room;
        select_bank(addr);
        fl_read(addr & 0xFFFFu, dst, k);
        addr += k; dst += k; n -= k;
    }
}

static int erase_sector(uint32_t addr)
{
    if (chip_atmel) {
        /* no erase command: write 0xFF pages */
        static const uint8_t ff[128] = {
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255 };
        uint32_t p;
        for (p = 0; p < SECTOR; p += 128)
            if (!fl_program_atmel_page((addr + p) & 0xFFFFu, ff)) return 0;
        return 1;
    }
    select_bank(addr);
    return fl_erase_sector(addr & 0xFFFFu, chip_macronix);
}

/* program n bytes at addr (the area must be erased; never crosses a bank
 * because callers write within one 4 KB sector) */
static int program(uint32_t addr, const uint8_t *src, uint32_t n)
{
    if (chip_atmel) {
        /* page-aligned read-modify-write */
        uint8_t page[128];
        while (n) {
            uint32_t base = addr & ~127u, o = addr - base;
            uint32_t k = 128 - o < n ? 128 - o : n;
            uint32_t i;
            read_bytes(base, page, 128);
            for (i = 0; i < k; i++) page[o + i] = src[i];
            if (!fl_program_atmel_page(base & 0xFFFFu, page)) return 0;
            addr += k; src += k; n -= k;
        }
        return 1;
    }
    select_bank(addr);
    return fl_program(addr & 0xFFFFu, src, n, chip_macronix);
}

static int slot_region(int slot, uint32_t *off, uint32_t *len)
{
    uint32_t cap = (uint32_t)chip_kb * 1024u;
    if (!chip_kb) return 0;
    if (slot == 0) { *off = SLOT0_OFF; *len = SLOT0_LEN; }
    else if (slot == 1) { *off = SLOT1_OFF; *len = SLOT1_LEN; }
    else return 0;
    if (*off >= cap) return 0;
    if (*off + *len > cap) *len = cap - *off;
    *len &= ~(SECTOR - 1);
    return *len >= SECTOR;
}

static int read_trailer(uint32_t off, uint32_t len, uint32_t *size)
{
    uint8_t t[TRAILER];
    uint32_t s, ns;
    read_bytes(off + len - TRAILER, t, TRAILER);
    if (t[0] != 'D' || t[1] != 'M' || t[2] != 'S' || t[3] != 'V') return 0;
    s = (uint32_t)t[4] | ((uint32_t)t[5] << 8) | ((uint32_t)t[6] << 16) | ((uint32_t)t[7] << 24);
    ns = (uint32_t)t[8] | ((uint32_t)t[9] << 8) | ((uint32_t)t[10] << 16) | ((uint32_t)t[11] << 24);
    if (s != ~ns || s == 0 || s > len - TRAILER) return 0;
    *size = s;
    return 1;
}

/* ---------------------------------------------------------------------- */
int plat_save_write(int slot, const void *data, uint32_t size)
{
    uint32_t off, len, last, s, done;
    const uint8_t *src = (const uint8_t *)data;
    uint8_t t[TRAILER];
    if (!data || size == 0 || !slot_region(slot, &off, &len)) return 0;
    if (size > len - TRAILER) {
        gba_logf("save: slot %d: %u bytes do not fit (%u max)", slot, size, len - TRAILER);
        return 0;
    }
    last = off + len - SECTOR;
    /* 1. invalidate: erase the trailer sector */
    if (!erase_sector(last)) { gba_log("save: erase failed"); return 0; }
    /* 2. data sectors */
    for (done = 0, s = off; done < size; s += SECTOR) {
        uint32_t k = size - done < SECTOR ? size - done : SECTOR;
        if (s != last && !erase_sector(s)) { gba_log("save: erase failed"); return 0; }
        if (!program(s, src + done, k)) { gba_logf("save: program/verify failed at 0x%x", s); return 0; }
        done += k;
    }
    /* 3. commit: the trailer */
    t[0] = 'D'; t[1] = 'M'; t[2] = 'S'; t[3] = 'V';
    t[4] = (uint8_t)size; t[5] = (uint8_t)(size >> 8);
    t[6] = (uint8_t)(size >> 16); t[7] = (uint8_t)(size >> 24);
    t[8] = (uint8_t)~size; t[9] = (uint8_t)(~size >> 8);
    t[10] = (uint8_t)(~size >> 16); t[11] = (uint8_t)(~size >> 24);
    if (!program(off + len - TRAILER, t, TRAILER)) { gba_log("save: trailer failed"); return 0; }
    {
        uint32_t chk;
        if (!read_trailer(off, len, &chk) || chk != size) return 0;
    }
    return 1;
}

int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    uint32_t off, len, s, n;
    if (size) *size = 0;
    if (!data || !slot_region(slot, &off, &len)) return 0;
    if (!read_trailer(off, len, &s)) return 0;
    n = s < cap ? s : cap;
    read_bytes(off, (uint8_t *)data, n);
    if (size) *size = n;
    return 1;
}

int plat_save_erase(int slot)
{
    uint32_t off, len, s, sz = 0, end;
    if (!slot_region(slot, &off, &len)) return 0;
    if (read_trailer(off, len, &sz)) end = off + ((sz + SECTOR - 1) & ~(SECTOR - 1));
    else end = off;
    if (!erase_sector(off + len - SECTOR)) return 0;
    for (s = off; s < end && s < off + len - SECTOR; s += SECTOR)
        if (!erase_sector(s)) return 0;
    return 1;
}
