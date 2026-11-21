/* PS2 memory-card save journal: portable header/CRC helpers.
 * No allocations, no new core save bytes. The 16-byte envelope wraps
 * the existing DMOO v1/v2 checksummed image, never replacing its codec. */
#ifndef DOOMO_PS2_SAVE_JOURNAL_H
#define DOOMO_PS2_SAVE_JOURNAL_H
#include <stdint.h>

#define PS2_SJ_HEADER 16u
#define PS2_SJ_MAGIC 0x32534D44u /* little-endian DMS2 */

static inline uint32_t ps2_sj_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void ps2_sj_put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static inline uint32_t ps2_sj_crc_step(uint32_t c, const uint8_t *data, uint32_t n)
{
    uint32_t i;
    for(i=0;i<n;i++){
        int k;
        c^=data[i];
        for(k=0;k<8;k++) c=(c>>1) ^ ((c&1u)?0xEDB88320u:0u);
    }
    return c;
}
static inline uint32_t ps2_sj_crc(const uint8_t *p, uint32_t n)
{
    return ps2_sj_crc_step(0xFFFFFFFFu,p,n)^0xFFFFFFFFu;
}
static inline void ps2_sj_header(uint8_t h[PS2_SJ_HEADER],
                                 uint32_t gen,uint32_t size,uint32_t crc)
{
    ps2_sj_put32(h,PS2_SJ_MAGIC);
    ps2_sj_put32(h+4,gen);
    ps2_sj_put32(h+8,size);
    ps2_sj_put32(h+12,crc);
}
static inline int ps2_sj_decode(const uint8_t *h,uint32_t max,
                                 uint32_t *gen,uint32_t *size,uint32_t *crc)
{
    uint32_t n;
    if(ps2_sj_le32(h)!=PS2_SJ_MAGIC)return 0;
    n=ps2_sj_le32(h+8);
    if(n<48u || n>max)return 0;
    if(gen)*gen=ps2_sj_le32(h+4);
    if(size)*size=n;
    if(crc)*crc=ps2_sj_le32(h+12);
    return 1;
}
/* Compare counters across wrap; newest is at most 2^31 saves ahead. */
static inline int ps2_sj_newer(uint32_t a,uint32_t b)
{
    return (int32_t)(a-b)>0;
}
#endif
