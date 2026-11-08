/* talk_safe.c - Talk T4: the safety gate (owner: TK; docs/TALK.md "Safety";
 * design section 7). NON-NEGOTIABLE: nothing Talk generates is reachable
 * in play before this gate and its tests are green.
 *
 *   minor / ctx  who is in the room (design 7.3). Talk keeps NO list of
 *                minors of its own: it asks sim_life_stage() (LS0), the
 *                roster's KID / TEEN traits, ending_kid_cast() (E1, the
 *                compiled core/data/story/kid_cast.txt) and, for a family
 *                pool member, its saved stage. An unknown id is a minor.
 *   S1 scan      typed input (Dreamcast, native): the TB_PARSE word flags
 *                (explicit / flirting sexual words, hate, distress, minor
 *                words, swears), the hashed hate list (exact and edit
 *                distance 1), "HATEV ... a group of real people", "12 year
 *                old" / "I'm 12" (a minor), a person named in the line who
 *                is not in the room and talk_safe_minor() (a minor).
 *   S2 gate      rewrites the utterance (design 7.2): sexual + any child
 *                (in the room, the target, the avatar, a minor word or a
 *                child named) ends the conversation (K-7); explicit sexual
 *                input is refused and never echoed; romance needs two
 *                adults who pass sim_romance_ok and playas_romance_ok BOTH
 *                ways, no child and no pet in the room (K-4), the target's
 *                consent (K-5), and no child in the line (a romance act
 *                with TS_MINOR is REFUSE_MINOR: "I'm 12, go out with me?",
 *                "kiss my little sister"); hate is refused (K-8); distress
 *                stops the comedy (K-11). A friendly hug stays a hug
 *                (DECISIONS "TALK HUG").
 *   S3 lint      every generated line, every platform (TB_SAFETY): the
 *                register words of core/data/story/register.tsv with the
 *                story lint's own matching rules, the never-generated
 *                words, the hate hashes, the swear cap (K-9) and the bubble.
 *
 * Every build links the S2 / S3 half (the PS1 runs the output lint); S1
 * needs TB_PARSE (Dreamcast, native). RAM: one u32 counter.
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/sim.h"
#include "doomo/roster.h"
#include "doomo/ending.h"
#include "doomo/family.h"
#include "doomo/playas.h"

#define SAF_MAGIC 0x31464153u                /* "SAF1" */
#define SR_HEAD    1u
#define SR_TAIL    2u
#define SR_FORBID  4u
#define SR_SUGGEST 8u
#define SR_ALLOW   16u                       /* a phrase that is fine although a forbidden word is in it ("tug of war") */
/* talkc_parse.py: the rules file declares these classes first, in order */
enum { TCL_EVERYONE = 0, TCL_HATEV, TCL_YEARS, TCL_YOU, TCL_I, TCL_IM, TCL_GREETW, TCL_HATEADJ, TCL_ATTRACT };

static uint32_t lint_hits;

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- who is who (design 7.3) ------------------------------------------- */
int talk_safe_is_pet(int id)
{
    if (id == TALK_PET_ID) return 1;
    return id > DOOMO_PLAYER_ID && family_role(id) == FAMROLE_PET;
}

int talk_safe_minor(int id)
{
    if (id >= 0 && id < DOOMO_MAX_RESIDENTS)
        return LIFE_IS_MINOR(sim_life_stage(id)) || (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) ||
               ending_kid_cast(id);
    if (id == DOOMO_PLAYER_ID) return LIFE_IS_MINOR(sim_life_stage(id));
    if (id == TALK_PET_ID) return 0;                   /* the pet: K-3, and never a romance target */
    if (id > DOOMO_PLAYER_ID && id <= PLAYAS_ID_FAMILY_LAST) {
        const FamilyMember *fm = family_find(id);
        if (!fm) return 1;
        if (fm->role == FAMROLE_PET) return 0;
        return LIFE_IS_MINOR(fm->stage);
    }
    return 1;                                          /* unknown: fail safe */
}

void talk_safe_ctx(int avatar, const int *ids, int n, int target, TalkAudience *out)
{
    int i, adults_ok = 1;
    memset(out, 0, sizeof *out);
    out->avatar_minor = (uint8_t)talk_safe_minor(avatar);
    out->any_minor = out->avatar_minor;
    if (n > TALK_PARTS_MAX) n = TALK_PARTS_MAX;
    for (i = 0; i < n; i++) {
        int id = ids[i], minor = talk_safe_minor(id);
        if (talk_safe_is_pet(id)) out->pet = 1;
        if (minor) out->any_minor = 1;
        if (id >= 0 && id < DOOMO_MAX_RESIDENTS && ending_kid_cast(id)) out->cast_kid = 1;
        if (!talk_safe_is_pet(id) && !minor && !mature_adult_ok(id)) adults_ok = 0;
        if ((target < 0 || i == target) && minor) out->target_minor = 1;   /* "everyone" includes the kids */
    }
    if (target >= n) out->target_minor = 1;              /* no such participant: fail safe */
    if (!mature_adult_ok(avatar)) adults_ok = 0;
    out->mature_ok = (uint8_t)(opt_mature() >= MATURE_ON && adults_ok && !out->any_minor && !out->pet);
}

/* ---- the hate list (TB_SAFETY) ------------------------------------------- */
typedef struct SafHdr {
    uint32_t nreg, reg_off, salt_lo, salt_hi, nexact, exact_off, ndel, del_off, str_off, str_len;
} SafHdr;

static int saf_hdr(SafHdr *h)
{
    uint8_t b[48];
    if (!talk_db_read(TB_SAFETY, 0, b, 48) || le32(b) != SAF_MAGIC) return 0;
    h->nreg = le32(b + 4); h->reg_off = le32(b + 8); h->salt_lo = le32(b + 12); h->salt_hi = le32(b + 16);
    h->nexact = le32(b + 20); h->exact_off = le32(b + 24); h->ndel = le32(b + 28); h->del_off = le32(b + 32);
    h->str_off = le32(b + 36); h->str_len = le32(b + 40);
    return 1;
}

static uint64_t fnv64(const SafHdr *h, const char *s, int len)
{
    uint64_t x = 0xCBF29CE484222325ull;
    int i;
    for (i = 0; i < 8; i++) {
        uint32_t w = i < 4 ? h->salt_lo : h->salt_hi;
        x ^= (uint8_t)(w >> (8 * (i & 3)));
        x *= 0x100000001B3ull;
    }
    for (i = 0; i < len; i++) {
        x ^= (uint8_t)s[i];
        x *= 0x100000001B3ull;
    }
    return x;
}

static int in_set(uint32_t off, uint32_t n, uint64_t v)
{
    int lo = 0, hi = (int)n - 1;
    uint8_t b[8];
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        uint64_t x;
        if (!talk_db_read(TB_SAFETY, off + 8u * (uint32_t)mid, b, 8)) return 0;
        x = (uint64_t)le32(b) | ((uint64_t)le32(b + 4) << 32);
        if (x == v) return 1;
        if (x < v) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

/* letters only, leet folded, runs of one letter made one (talkc_safe.py squeeze) */
static int squeeze(const char *w, char *out, int cap)
{
    int o = 0;
    for (; *w && o < cap - 1; w++) {
        char c = *w;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        switch (c) {
        case '0': c = 'o'; break; case '1': c = 'i'; break; case '3': c = 'e'; break;
        case '4': c = 'a'; break; case '5': c = 's'; break; case '7': c = 't'; break;
        case '8': c = 'b'; break; case '9': c = 'g'; break; case '@': c = 'a'; break; case '$': c = 's'; break;
        }
        if (c < 'a' || c > 'z') continue;
        if (o > 0 && out[o - 1] == c) continue;
        out[o++] = c;
    }
    out[o] = 0;
    return o;
}

uint64_t talk_safe_hash(const char *word)
{
    SafHdr h;
    char s[TALK_TOK_LEN * 2];
    int n;
    if (!word || !saf_hdr(&h)) return 0;
    n = squeeze(word, s, (int)sizeof s);
    return fnv64(&h, s, n);
}

int talk_safe_hate_word(const char *word, int fuzzy)
{
    SafHdr h;
    char s[TALK_TOK_LEN * 2], d[TALK_TOK_LEN * 2];
    int n, i;
    if (!word || !saf_hdr(&h)) return 0;
    n = squeeze(word, s, (int)sizeof s);
    if (n < 3) return 0;
    if (in_set(h.exact_off, h.nexact, fnv64(&h, s, n))) return 1;
    /* edit distance 1 after the first letter (only unknown words reach here):
     * a letter added to a listed word of 4+ letters (a typo that breaks a
     * double letter squeezes to this); a letter left out, changed or two
     * swapped in a listed word of 5+ letters (the deletion hashes hold every
     * one-letter deletion of those, after the first) */
    if (!fuzzy || n < 4) return 0;
    if (in_set(h.del_off, h.ndel, fnv64(&h, s, n))) return 1;                 /* a letter left out (4+ letters) */
    for (i = 1; i < n; i++) {
        memcpy(d, s, (size_t)i);
        memcpy(d + i, s + i + 1, (size_t)(n - i));
        if (n - 1 >= 4 && in_set(h.exact_off, h.nexact, fnv64(&h, d, n - 1))) return 1;  /* one added */
        if (n >= 5 && in_set(h.del_off, h.ndel, fnv64(&h, d, n - 1))) return 1;          /* one changed, or two swapped */
    }
    return 0;
}

/* ---- S1: typed input --------------------------------------------------- */
static int has_class(const TalkTok *t, int bit) { return (t->tags[bit >> 5] >> (bit & 31)) & 1u; }

int talk_safe_explicit(const TalkTok *tok, int n)
{
    int i;
    for (i = 0; i < n; i++) if (tok[i].wflags & TW_SEXUAL) return 1;
    return 0;
}

uint8_t talk_safe_scan(TalkTok *tok, int n)
{
    uint8_t ts = 0;
    int i, j;
    for (i = 0; i < n; i++) {
        TalkTok *t = &tok[i];
        if ((t->flags & TTF_UNK) && talk_safe_hate_word(t->text, 1)) t->wflags |= TW_HATE;
        if (t->kind == TTK_WORD && !(t->flags & TTF_UNK) && !t->concept && talk_safe_hate_word(t->text, 0))
            t->wflags |= TW_HATE;
        if (t->wflags & (TW_SEXUAL | TW_SUGGEST)) ts |= TS_SEXUAL;
        if (t->wflags & TW_MINOR) ts |= TS_MINOR;
        /* a person named who is not in the room ("pip is sexy" with Pip
         * elsewhere): the game's lookup gave the id, talk_safe_minor() says
         * if it is a child (one source; an unknown id is a child) */
        if (t->person >= 0 && (t->kind == TTK_WORD || (t->kind == TTK_PERSON && (t->flags & TTF_ABSENT))) &&
            talk_safe_minor(t->person)) ts |= TS_MINOR;
        if (t->wflags & TW_HATE) ts |= TS_HATE;
        if (t->wflags & TW_DISTRESS) ts |= TS_DISTRESS;
        if (t->wflags & TW_REALWORLD) ts |= TS_REALWORLD;
        if (t->wflags & TW_STRONG) ts |= TS_STRONG;
        if (t->wflags & TW_CRUDE) ts |= TS_CRUDE;
        if (t->wflags & (TW_SEXUAL | TW_HATE | TW_STRONG)) t->flags |= TTF_MASK;
        /* "I hate <a group of real people>", "<a group> are vermin" */
        if (has_class(t, TCL_HATEV)) {
            for (j = i + 1; j < n && j <= i + 4; j++)
                if (tok[j].wflags & TW_GROUP) { ts |= TS_HATE; tok[j].wflags |= TW_HATE; }
        }
        if (t->wflags & TW_GROUP) {
            for (j = i + 1; j < n && j <= i + 4; j++)
                if (has_class(&tok[j], TCL_HATEADJ) || has_class(&tok[j], TCL_HATEV)) { ts |= TS_HATE; t->wflags |= TW_HATE; }
        }
        /* "12 years old", "a 9yo", "I'm 12", "she's 14": a child */
        if (t->kind == TTK_NUM && t->value < 18) {
            static const char *const age_words[] = { "she's", "shes", "he's", "hes", "they're", "is", "am", "are",
                                                     "aged", "age", "only", "just", "turned", "turning" };
            unsigned k;
            for (j = i + 1; j < n && j <= i + 2; j++) if (has_class(&tok[j], TCL_YEARS)) ts |= TS_MINOR;
            if (i > 0 && has_class(&tok[i - 1], TCL_IM)) ts |= TS_MINOR;
            for (k = 0; i > 0 && k < sizeof age_words / sizeof age_words[0]; k++)
                if (strcmp(tok[i - 1].text, age_words[k]) == 0) ts |= TS_MINOR;
        }
        /* a word that only ever means a child, with a liking word near it
         * ("I like underage girls"): sexual (K-7) */
        if (t->wflags & TW_MINORX) {
            int lo = i - 4 < 0 ? 0 : i - 4;
            for (j = lo; j < n && j <= i + 4; j++)
                if (j != i && has_class(&tok[j], TCL_ATTRACT)) { ts |= TS_SEXUAL | TS_MINOR; t->wflags |= TW_SEXUAL; }
        }
    }
    return ts;
}

/* ---- S2: the audience gate -------------------------------------------- */
int talk_safe_romance_ok(const TalkAudience *aud, int avatar, int target_id)
{
    if (!aud || target_id < 0 || target_id == avatar) return 0;
    if (aud->any_minor || aud->avatar_minor || aud->target_minor || aud->pet) return 0;
    if (talk_safe_minor(avatar) || talk_safe_minor(target_id)) return 0;
    if (talk_safe_is_pet(target_id) || talk_safe_is_pet(avatar)) return 0;
    if (!playas_romance_ok(avatar, target_id) || !playas_romance_ok(target_id, avatar)) return 0;
    if (avatar <= DOOMO_PLAYER_ID && target_id <= DOOMO_PLAYER_ID &&
        (!sim_romance_ok(avatar, target_id) || !sim_romance_ok(target_id, avatar))) return 0;
    return 1;
}

int talk_safe_jmat_ok(const int *ids, int n)
{
    int i;
    if (opt_mature() < MATURE_ON || n <= 0) return 0;
    for (i = 0; i < n; i++) if (!mature_adult_ok(ids[i])) return 0;
    return 1;
}

static void refuse(TalkUtter *u, int act, int act2, int hide)
{
    u->act[0] = (uint8_t)act;
    u->act[1] = (uint8_t)act2;
    u->topic[0] = u->topic[1] = TC_NONE;
    u->tone = TONE_PLAIN;
    if (hide) memset(u->shown, 0, sizeof u->shown);
}

int talk_safe_gate(TalkUtter *u, const TalkAudience *aud, int avatar, int target_id, int refused_today)
{
    int romance, child;
    if (!u || !aud) return TG_END_K7;
    child = aud->any_minor || aud->target_minor || aud->avatar_minor || (u->safety & TS_MINOR);
    /* K-7: sexual words with, at, about, by or near a child end it, unseen */
    if ((u->safety & TS_SEXUAL) || u->act[0] == TA_REFUSE_SEXUAL) {
        if (child) { refuse(u, TA_REFUSE_SEXUAL, TA_REFUSE_MINOR, 1); return TG_END_K7; }
    }
    if (u->act[0] == TA_REFUSE_SEXUAL || u->act[1] == TA_REFUSE_SEXUAL) {
        refuse(u, TA_REFUSE_SEXUAL, TA_NONE, 1);
        return TG_REFUSE;
    }
    /* K-11: distress, the comedy stops (before hate: someone in trouble
     * gets the kind answer even when the line is ugly; the bubble is hidden
     * if it was hateful too) */
    if ((u->safety & TS_DISTRESS) || u->act[0] == TA_DISTRESS || u->act[1] == TA_DISTRESS) {
        refuse(u, TA_DISTRESS, TA_NONE, (u->safety & TS_HATE) != 0);
        return TG_DISTRESS;
    }
    /* K-8: hate, unseen */
    if ((u->safety & TS_HATE) || u->act[0] == TA_REFUSE_HATE || u->act[1] == TA_REFUSE_HATE) {
        refuse(u, TA_REFUSE_HATE, TA_NONE, 1);
        return TG_HATE;
    }
    if (u->act[0] == TA_REFUSE_MINOR || u->act[0] == TA_DEFLECT_ROMANCE) {
        /* a safety act from elsewhere: keep it, never let it through as romance */
        u->tone = TONE_PLAIN;
        return u->act[0] == TA_REFUSE_MINOR ? TG_REFUSE_MINOR : TG_DEFLECT;
    }
    /* K-4 / K-5: romance. A romance act that is only the second act of a
     * line ("love you guys": a compliment) is dropped where the gate would
     * refuse it, and the first act stands. */
    if (TA_IS_ROMANCE(u->act[1]) && !TA_IS_ROMANCE(u->act[0]) && u->tone != TONE_FLIRTY && !(u->safety & TS_SEXUAL)) {
        if (refused_today || (u->safety & TS_MINOR) || !talk_safe_romance_ok(aud, avatar, target_id))
            u->act[1] = TA_NONE;
    }
    romance = TA_IS_ROMANCE(u->act[0]) || TA_IS_ROMANCE(u->act[1]) || u->tone == TONE_FLIRTY ||
              (u->safety & TS_SEXUAL);
    if (romance) {
        /* K-1 both ways: a child in the line ("I'm 12", "my daughter", a
         * child named) makes romance REFUSE_MINOR even between two adults */
        if ((u->safety & TS_MINOR) || aud->avatar_minor || aud->target_minor || talk_safe_minor(avatar) ||
            (target_id >= 0 && talk_safe_minor(target_id)) || (target_id < 0 && aud->any_minor)) {
            refuse(u, TA_REFUSE_MINOR, TA_NONE, 0);
            return TG_REFUSE_MINOR;
        }
        if (refused_today || !talk_safe_romance_ok(aud, avatar, target_id)) {
            refuse(u, TA_DEFLECT_ROMANCE, TA_NONE, 0);
            return TG_DEFLECT;
        }
    }
    /* a hug, a high five, a wave: as they are (a hug toward a child stays a
     * hug: wholesome only, and the child may decline; DECISIONS "TALK HUG") */
    return TG_PASS;
}

/* ---- S3: the output lint ----------------------------------------------- */
static int wordc(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '\''; }
static int wordch_at(const char *t, int i) { return wordc(t[i]) || (t[i] == '-' && wordc(t[i + 1])); }

/* the register word w ("what the hell", spaces match runs of spaces) at t[j]: its end or -1 */
static int match_body(const char *w, const char *t, int j)
{
    while (*w) {
        if (*w == ' ') {
            if (t[j] != ' ') return -1;
            while (t[j] == ' ') j++;
            w++;
            continue;
        }
        if (t[j] != *w) return -1;
        j++;
        w++;
    }
    return j;
}

/* story_register.py's rule: a leading * lets the word start inside another
 * one, a trailing * lets it run on; a hyphen between letters joins words */
static int count_word(const char *w, int head, int tail, const char *v)
{
    int i, n = 0;
    for (i = 0; v[i]; i++) {
        int j, e;
        if (i > 0 && wordch_at(v, i - 1)) continue;
        for (j = i; v[j] && (j == i || wordch_at(v, j - 1)); j++) {
            e = match_body(w, v, j);
            if (e >= 0) {
                if (tail) while (wordch_at(v, e)) e++;
                if (!wordch_at(v, e)) { n++; break; }
            }
            if (!head) break;
        }
    }
    return n;
}

int talk_safe_bubble_lines(const char *line, int cols)
{
    int lines = 1, col = 0;
    const char *p = line;
    if (cols <= 0) return 99;
    while (*p) {
        int wl = 0;
        while (*p == ' ') p++;
        if (!*p) break;
        while (p[wl] && p[wl] != ' ') wl++;
        while (wl > cols) {                              /* a word longer than the bubble breaks */
            if (col) { lines++; col = 0; }
            lines++;
            wl -= cols;
            p += cols;
        }
        if (col && col + 1 + wl > cols) { lines++; col = 0; }
        col += (col ? 1 : 0) + wl;
        p += wl;
    }
    return lines;
}

uint32_t talk_safe_lint_hits(void) { return lint_hits; }

static int lint_fail(int r) { lint_hits++; return r; }

/* strict (the probe): any register word is a problem, nothing is counted */
static int lint_core(const char *line, const TalkAudience *aud, int *swears, int strict)
{
    SafHdr h;
    char v[TALK_SHOWN_MAX * 3], word[TALK_TOK_LEN * 2];
    int i, o = 0, nsw = 0, k;
    if (!line || !aud) return TL_NO_DB;
    if (!saf_hdr(&h)) return TL_NO_DB;
    /* the visible text: line codes ([R4], {.}) out, '/' a space, lower case */
    for (i = 0; line[i] && o < (int)sizeof v - 1; i++) {
        char c = line[i];
        if (c == '[') { while (line[i] && line[i] != ']') i++; if (!line[i]) break; continue; }
        if (c == '{') { while (line[i] && line[i] != '}') i++; if (!line[i]) break; continue; }
        if (c == '/') c = ' ';
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        v[o++] = c;
    }
    v[o] = 0;
    for (k = 0; k < (int)h.nreg; k++) {
        uint8_t r[8];
        char w[48];
        uint32_t so;
        int n, len = 0;
        if (!talk_db_read(TB_SAFETY, h.reg_off + 8u * (uint32_t)k, r, 8)) return TL_NO_DB;
        so = le32(r);
        if (so >= h.str_len) return TL_NO_DB;
        len = (int)(h.str_len - so < sizeof w - 1 ? h.str_len - so : sizeof w - 1);
        if (!talk_db_read(TB_SAFETY, h.str_off + so, w, (uint32_t)len)) return TL_NO_DB;
        w[len] = 0;
        if (r[5] & SR_ALLOW) {
            /* blank the allowed phrase out of the text before the checks */
            int p, l = (int)strlen(w);
            for (p = 0; v[p]; p++)
                if ((p == 0 || !wordch_at(v, p - 1)) && strncmp(v + p, w, (size_t)l) == 0 && !wordch_at(v, p + l))
                    memset(v + p, ' ', (size_t)l);
            continue;
        }
        n = count_word(w, r[5] & SR_HEAD, r[5] & SR_TAIL, v);
        if (!n) continue;
        if (r[5] & SR_FORBID) return TL_FORBIDDEN;
        if (strict) return TL_REGISTER_KID;
        if (r[4] == TR_HATE || r[4] == TR_REALWORLD || r[4] == TR_DISTRESS) return TL_FORBIDDEN;
        if (r[4] == TR_SEXUAL || (r[5] & SR_SUGGEST)) {
            if (aud->any_minor || aud->target_minor || aud->avatar_minor) return TL_REGISTER_KID;
            if (aud->pet) return TL_SEXUAL_PET;
            continue;
        }
        if (aud->any_minor || aud->target_minor || aud->avatar_minor) return TL_REGISTER_KID;
        if (r[4] == TR_STRONG) nsw += n;
    }
    /* the hate list on every word */
    for (i = 0; v[i]; ) {
        int l = 0;
        while (v[i] && !wordc(v[i])) i++;
        while (v[i] && (wordc(v[i]) || v[i] == '-') && l < (int)sizeof word - 1) word[l++] = v[i++];
        while (v[i] && (wordc(v[i]) || v[i] == '-')) i++;
        word[l] = 0;
        if (l && talk_safe_hate_word(word, 0)) return TL_HATE;
    }
    if (talk_safe_bubble_lines(v, TALK_BUBBLE_COLS) > TALK_BUBBLE_LINES) return TL_LENGTH;
    if (nsw) {
        if (!swears || *swears + nsw > 1) return TL_SWEAR_CAP;
        *swears += nsw;
    }
    return TL_OK;
}

int talk_safe_lint(const char *line, const TalkAudience *aud, int *swears)
{
    int r = lint_core(line, aud, swears, 0);
    return r == TL_OK ? TL_OK : lint_fail(r);
}

int talk_safe_lint_probe(const char *text, const TalkAudience *aud, int strict)
{
    return lint_core(text, aud, NULL, strict);
}
