/* PS2 GS indexed-to-RGB5551 change-detect upload.
 * Compare against the *same* persistent 512x256 DMA staging texture.
 * Identical frames avoid a 256KiB GIF upload (and matching VRAM traffic)
 * without storing a second framebuffer or hashing away correctness.
 * Only the used 320x240 pixels are touched; padded edges remain clear.
 */
#ifndef PS2_INDEXED_PRESENT_H
#define PS2_INDEXED_PRESENT_H
#include <stdint.h>
static inline int ps2_indexed_update(const uint8_t *indexed, int src_pitch,
                                     uint16_t *staging, int dest_pitch,
                                     int width,int height,
                                     const uint16_t palette[256])
{
    int x,y,changed=0;
    if(!indexed || !staging || !palette || width<=0 || height<=0 ||
       src_pitch<width || dest_pitch<width)return 0;
    for(y=0;y<height;y++){
        const uint8_t *s=indexed+y*src_pitch;
        uint16_t *d=staging+y*dest_pitch;
        for(x=0;x<width;x++){
            const uint16_t c=palette[s[x]];
            if(d[x]!=c){
                d[x]=c;
                changed=1;
            }
        }
    }
    return changed;
}
#endif
