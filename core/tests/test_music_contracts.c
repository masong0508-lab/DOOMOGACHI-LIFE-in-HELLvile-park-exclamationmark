/* test_music_contracts.c - the music contracts (owner: MU0;
 * docs/design/music/SPEC.md sec.5, 9, 21.2 MU0 acceptance). */
#include "test.h"
#include "doomo/music.h"

/* D12: the engine tick is music_update(); `music_tick` must not exist
 * (static music_tick lives in overworld.c and levels.c). */
extern void music_tick(void) __attribute__((weak));

TEST(music_contract_record_sizes)
{
    CHECK_EQ(sizeof(MusBank), 52);
    CHECK_EQ(sizeof(MusDirEnt), 8);
    CHECK_EQ(sizeof(MusCue), 24);
    CHECK_EQ(sizeof(MusStingHdr), 8);
    CHECK_EQ(sizeof(MusRole), 8);
    CHECK_EQ(sizeof(MusNode), 12);
    CHECK_EQ(sizeof(MusEdge), 4);
    CHECK_EQ(sizeof(MusBind), 4);
    CHECK_EQ(sizeof(MusInst), 20);
    CHECK_EQ(sizeof(MusGroove), 64);
    CHECK_EQ(sizeof(MusTrans), 8);
    CHECK_EQ(sizeof(MusParamDef), 4);
    CHECK_EQ(sizeof(MusGenSpec), 12);
    CHECK_EQ(sizeof(DoomoMusEv), 8);
    CHECK_EQ(sizeof(DoomoVoice), 12);
    CHECK_EQ(sizeof(DoomoVoicePlan), 4);
    CHECK_EQ(sizeof(DoomoAudioCaps), 12);
    CHECK_EQ(sizeof(DoomoVoiceChange), 20);
    CHECK_EQ(sizeof(MusPos), 20);
    CHECK_EQ(sizeof(MusBeatEv), 12);
    CHECK_EQ(sizeof(MusVoiceRt), 28);
    CHECK_EQ(sizeof(MusDeck), 80);
    CHECK_EQ(sizeof(DrvVoice), 16);
    CHECK_EQ(sizeof(MusCueReq), 8);
}

TEST(music_contract_enums)
{
    CHECK_EQ(MP_COUNT, 32);
    CHECK_EQ(MQ_COUNT, 7);
    CHECK_EQ(MEV_KINDS, 12);
    CHECK(MEV_KINDS <= 16);                       /* 4 bits in DoomoMusEv.pos */
    CHECK_EQ(MTC_COUNT, 24);                      /* C0..D7 */
    CHECK_EQ(MT_CMD + MTC_COUNT, MT_GEN);
    CHECK_EQ(MUS_MT_PER_TICK, 8);
    CHECK_EQ(MUS_MT_HZ, DOOMO_TICK_HZ * MUS_MT_PER_TICK);
    CHECK(KIT_COUNT <= 8 && KH_COUNT <= 8);       /* 3-bit DRUM fields */
    CHECK_EQ(MC_NONE, 0);
    CHECK_EQ(VINYL_NEEDLE, 12);                   /* the Studio scratch nibble (sec.17) */
    CHECK(music_tick == NULL);
}

TEST(music_contract_event_packing_round_trips)
{
    uint32_t a = MEV_NOTE_ARG(11, 127, 63, 2047, 7);
    DoomoMusEv e;
    CHECK_EQ(MEV_VOICE(a), 11);
    CHECK_EQ(MEV_NOTE_PITCH(a), 127);
    CHECK_EQ(MEV_NOTE_VEL(a), 63);
    CHECK_EQ(MEV_NOTE_DUR(a), 2047);
    CHECK_EQ(MEV_NOTE_FLAGS(a), 7);
    a = MEV_DRUM_ARG(3, KH_CLAP, KIT_CLASSIC, 44, 0, 0);
    CHECK_EQ(MEV_DRUM_HIT(a), KH_CLAP);
    CHECK_EQ(MEV_DRUM_KIT(a), KIT_CLASSIC);
    CHECK_EQ(MEV_DRUM_VEL(a), 44);
    a = MEV_INST_ARG(2, 200, 16, 52);
    CHECK_EQ(MEV_INST_INST(a), 200);
    CHECK_EQ(MEV_INST_TP(a), 16);
    CHECK_EQ(MEV_INST_VEL(a), 52);
    a = MEV_MUTE_ARG(0xABC, 1, 0x7FFF);
    CHECK_EQ(MEV_MUTE_MASK(a), 0xABC);
    CHECK_EQ(MEV_MUTE_DECK(a), 1);
    CHECK_EQ(MEV_MUTE_LEN(a), 0x7FFF);
    e.pos = MEV_MAKE_POS(MEV_MARK, 0x1234567u);
    CHECK_EQ(MEV_KIND(&e), MEV_MARK);
    CHECK_EQ(MEV_POS(&e), 0x1234567u);
}

TEST(music_contract_bank_header)
{
    const MusBank *b = (const MusBank *)(const void *)doomo_mus_bank;
    uint32_t h = 0x811C9DC5u, i;
    CHECK_EQ(((uintptr_t)doomo_mus_bank) & 3u, 0);   /* read in place */
    CHECK_EQ(b->magic, MUS_BANK_MAGIC);
    CHECK_EQ(b->version, MUS_BANK_VERSION);
    CHECK_EQ(b->total, doomo_mus_bank_size);
    for (i = sizeof(MusBank); i < b->total; i++) { h ^= doomo_mus_bank[i]; h *= 0x01000193u; }
    CHECK_EQ_U(h, b->fnv);
}

TEST(music_contract_weak_bridge_defaults)
{
    DoomoAudioCaps c;
    DoomoVoicePlan p[3] = { { 0, 0, 0, 0 }, { 3, 0, 0, 0 }, { 12, 0, 0, 0 } };
    plat_audio_caps(&c);
    CHECK_EQ(c.tier, AT_BRIDGE);                  /* native renders through the bridge at M1 */
    CHECK(!(c.flags & CAP_DRV_IRQ));
    CHECK_EQ(plat_music_plan(p, 3), 0x9u);        /* voices 0..3 only */
}
