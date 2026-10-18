/* music_golden.h - T2's goldens (owner: MU6). RECORDED by
 * `core/build/tests --update-golden music_t2` (test_music_hash.c); do
 * not edit. Per classic cue: the FNV of its bank bytes, music_hash(cue, 2)
 * and the bridge's channel-state hash over 600 ticks. */
#define MUSIC_GOLDEN_N 28
static const uint32_t music_golden[MUSIC_GOLDEN_N][3] = {
    { 0x8F160C30u, 0x4FE502D7u, 0x43136ED1u },   /* Doomogachi Life! */
    { 0xADA9D997u, 0xDA6A0F01u, 0x45FEDE2Eu },   /* HaHaland Day */
    { 0x11DF19C0u, 0x41F4D59Eu, 0x2F2909CCu },   /* HaHaland Night */
    { 0x1DFCF7BDu, 0xECBC2CEAu, 0x3D7B3121u },   /* Make a Friend! */
    { 0xC820077Bu, 0x25A74990u, 0x042D4EB3u },   /* Shop Hop */
    { 0x3E80DA0Cu, 0x253853FFu, 0x21063DC8u },   /* St. Cuddles Hymn */
    { 0xD5F9ACC7u, 0x4FF6C653u, 0xBF6129CAu },   /* Doomogachi Beeps */
    { 0xE323E939u, 0xB7E3B673u, 0x61D4010Eu },   /* Hellvile After Dark */
    { 0x170B70D5u, 0xFC8C33C3u, 0xB8CB60D0u },   /* The Ritual */
    { 0xB5D00CE7u, 0xD24D7E65u, 0xE7006AA9u },   /* Rivet's Workshop */
    { 0x8E0584ECu, 0x4779BCE8u, 0x411D1B12u },   /* Hell Invades */
    { 0xF387918Eu, 0x0D0456D9u, 0x6D6E3CA4u },   /* Hell Breach */
    { 0x17199D5Eu, 0xF31D91A3u, 0xE933F09Du },   /* Lord Vapula */
    { 0xC672ABA9u, 0x797C9305u, 0x328BA654u },   /* Victory! */
    { 0x01B97B60u, 0x7E4589CBu, 0xBE4FAAFCu },   /* Goodbye, Friend */
    { 0x77280E8Du, 0xCD3DAE57u, 0x80861573u },   /* Hellvile Park Festival */
    { 0x1728E718u, 0xED933017u, 0x1C20BAF2u },   /* Everyone Together */
    { 0xB92232ECu, 0x275307F2u, 0x9FA13755u },   /* Cloud Nine Lounge */
    { 0x004B93C7u, 0x19EFC381u, 0x12441C08u },   /* The Wall */
    { 0x3FE604D2u, 0xAAFE1EC5u, 0xABF2678Fu },   /* Mingle */
    { 0x214911ACu, 0x66D7A7AFu, 0xD243E0C5u },   /* Lullaby for the Wall */
    { 0x9BB0C09Bu, 0x66D7A7AFu, 0xD243E0C5u },   /* Two Choirs */
    { 0x86FCB9A1u, 0x66D7A7AFu, 0xD243E0C5u },   /* Boat Floats */
    { 0x9F7F61ACu, 0x66D7A7AFu, 0xD243E0C5u },   /* Thirteen Bells */
    { 0x8F5ADEF3u, 0x66D7A7AFu, 0xD243E0C5u },   /* Hellvile Lullaby */
    { 0x4674CCCCu, 0x66D7A7AFu, 0xD243E0C5u },   /* Double Act */
    { 0xCF9EA38Eu, 0x8F7DF729u, 0x0D727BCAu },   /* Hee. Hee. */
    { 0xACB0B8FFu, 0x66D7A7AFu, 0xD243E0C5u },   /* Number Ten */
};
