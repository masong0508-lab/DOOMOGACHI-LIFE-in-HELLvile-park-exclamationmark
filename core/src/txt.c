/* txt.c - the story text packs: the canonical-Huffman decoder, the pack
 * reader (scripts, beat scripts) and the caption stash (owner: S2;
 * docs/design/story/SPEC.md sec.5, R2, R6, C3, C5; story_final.md
 * sec.16.2-16.3; the format is in core/tools/textpack.py).
 *
 * Resident on the PS1 (the file name does not match an overlay group's
 * regex, SPEC R4): the VM, the dialogue and every host scene read text
 * through it. It never calls overlay code. Where a pack's bytes are is the
 * platform's business (txt_plat_pack): ROM arrays on the GBA and native
 * (core/gen/text_dir.c), the resident window on the PS1 (ps1/src/txtload.c).
 *
 * Decoding is per string into the caller's buffer (a dialogue queue entry,
 * a 200-char line): nothing resident points into a pack. Cost: about 15
 * cycles per bit, ~15k cycles for a 200-character line, once, when the
 * line opens (SF sec.16.6).
 *
 * RAM: the stash (4 x 64 B) only.
 */
#include "doomo/doomo.h"
#include "doomo/txt.h"
#include "doomo/story_vm.h"
#include "doomo/beat.h"

/* core/gen/txt_dict.c */
extern const uint8_t doomo_txt_huff_count[17];
extern const uint16_t doomo_txt_huff_nsym;
extern const uint16_t doomo_txt_huff_sym[];
extern const uint16_t doomo_txt_dict_n;
extern const uint16_t doomo_txt_dict_off[];
extern const char doomo_txt_dict_chars[];

/* the pack header (textpack.py): u16 words 0..7, then u32 section offsets */
enum { H_MAGIC, H_VER_ID, H_NSTR, H_NSCR, H_NOPS, H_NBEAT, H_NEV, H_FLAGS };
enum { O_STR, O_SCR, O_OPS, O_BEAT, O_EV, O_STREAM };
#define PACK_MAGIC 0x5854u

static char stash[TXT_STASH_LINES][TXT_STASH_LEN];

static const uint8_t *live_base(int pack)
{
    const uint8_t *b;
    if (pack < 0 || pack >= (int)doomo_txt_npacks) return NULL;
    b = txt_plat_pack(pack);
    if (!b || ((const uint16_t *)b)[H_MAGIC] != PACK_MAGIC) return NULL;
    return b;
}

/* a pack's bytes, loading it if it is not live (PS1: a CD read) */
static const uint8_t *pack_base(int pack)
{
    const uint8_t *b = live_base(pack);
    if (b || pack < 0 || pack >= (int)doomo_txt_npacks || !txt_plat_load(pack)) return b;
    b = txt_plat_pack(pack);
    if (!b || ((const uint16_t *)b)[H_MAGIC] != PACK_MAGIC) return NULL;
    return b;
}

static unsigned hdr16(const uint8_t *b, int i) { return ((const uint16_t *)b)[i]; }
static uint32_t off32(const uint8_t *b, int i) { return ((const uint32_t *)(b + 16))[i]; }

int txt_pack_count(void) { return (int)doomo_txt_npacks; }
int txt_pack_is_live(int pack) { return live_base(pack) != NULL; }

int txt_pack_load(int pack) { return pack_base(pack) != NULL; }

static int hdr_or(int pack, int i)
{
    const uint8_t *b = pack_base(pack);
    return b ? (int)hdr16(b, i) : -1;
}
int txt_pack_strings(int pack) { return hdr_or(pack, H_NSTR); }
int txt_pack_scripts(int pack) { return hdr_or(pack, H_NSCR); }
int txt_pack_beats(int pack) { return hdr_or(pack, H_NBEAT); }
int txt_pack_flags(int pack) { return hdr_or(pack, H_FLAGS); }
int txt_pack_kind(int pack)
{
    return (pack >= 0 && pack < (int)doomo_txt_npacks) ? doomo_txt_pack_kinds[pack] : -1;
}

/* one canonical-Huffman symbol from the stream at bit *pos */
static int next_symbol(const uint8_t *s, uint32_t *pos)
{
    unsigned code = 0, first = 0, index = 0, l;
    for (l = 1; l <= 16; l++) {
        unsigned n = doomo_txt_huff_count[l];
        code |= (s[*pos >> 3] >> (7 - (*pos & 7))) & 1u;
        (*pos)++;
        if (code - first < n) return doomo_txt_huff_sym[index + code - first];
        index += n;
        first = (first + n) << 1;
        code <<= 1;
    }
    return 0;                                   /* corrupt: end the string */
}

int txt_get(uint16_t id, char *out, int cap)
{
    const uint8_t *b, *s;
    unsigned idx = (unsigned)TXT_INDEX(id);
    uint32_t pos = 0;
    int n = 0;
    if (!out || cap <= 0) return -1;
    out[0] = 0;
    if (id == TXT_NONE || !(b = pack_base(TXT_PACK(id))) || idx >= hdr16(b, H_NSTR)) return -1;
    s = b + off32(b, O_STREAM) + ((const uint16_t *)(b + off32(b, O_STR)))[idx];
    for (;;) {
        int sym = next_symbol(s, &pos);
        if (sym == 0) break;
        if (sym < 256) {
            if (n < cap - 1) out[n++] = (char)sym;
        } else if ((unsigned)(sym - 256) < doomo_txt_dict_n) {
            unsigned k = doomo_txt_dict_off[sym - 256], e = doomo_txt_dict_off[sym - 255];
            while (k < e && n < cap - 1) out[n++] = doomo_txt_dict_chars[k++];
        }
    }
    out[n] = 0;
    return n;
}

const StoryOp *txt_script(uint16_t script_id, int *n_ops)
{
    const uint8_t *b = pack_base(TXT_PACK(script_id)), *row;
    unsigned idx = (unsigned)TXT_INDEX(script_id);
    if (n_ops) *n_ops = 0;
    if (!b || idx >= hdr16(b, H_NSCR)) return NULL;
    row = b + off32(b, O_SCR) + idx * 6u;
    if (n_ops) *n_ops = ((const uint16_t *)row)[1];
    return (const StoryOp *)(b + off32(b, O_OPS)) + ((const uint16_t *)row)[0];
}

int txt_beat(uint16_t beat_id, BeatScript *out)
{
    const uint8_t *b = pack_base(TXT_PACK(beat_id));
    const uint16_t *row;
    unsigned idx = (unsigned)TXT_INDEX(beat_id);
    if (!out || !b || idx >= hdr16(b, H_NBEAT)) return 0;
    row = (const uint16_t *)(b + off32(b, O_BEAT)) + idx * 2u;
    out->ev = (const BeatEv *)(b + off32(b, O_EV)) + row[0];
    out->n = row[1];
    out->nlabels = BEAT_LABEL_MAX;
    return 1;
}

void txt_stash(int slot, uint16_t id)
{
    if (slot < 0 || slot >= TXT_STASH_LINES) return;
    if (txt_get(id, stash[slot], TXT_STASH_LEN) < 0) stash[slot][0] = 0;
}

const char *txt_stash_get(int slot)
{
    return (slot >= 0 && slot < TXT_STASH_LINES) ? stash[slot] : "";
}
