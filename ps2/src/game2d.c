/* PS2-1: first playable shared-core 2D framebuffer.
 * The core's authoritative 256-colour palette, 5x8 font, portraits and
 * user interface draw here at 320x240. A single 512x256 RGBA GS texture
 * is refreshed at 30Hz; NTSC fields display the most recent image at
 * 60Hz. The reference 3D/overworld renderers share this indexed surface;
 * native GS polygon acceleration is still an unimplemented milestone.
 * No new per-resident portrait cache, heap allocation or save fields.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <gsKit.h>
#include "game2d.h"
#include "indexed_blit.h"
#include "doomo/doomo.h"
#include "doomo/font.h"
#include "doomo/palette.h"

enum { W=320, H=240, TEX_W=512, TEX_H=256 };
uint8_t ps2_indexed_fb[W*H];
#define fb ps2_indexed_fb
static u32 upload[TEX_W*TEX_H] __attribute__((aligned(64)));
static u32 colors[256];
static GSTEXTURE picture;
static uint32_t frame_ticks;
static uint8_t drawing;

int plat_screen_w(void){return W;}
int plat_screen_h(void){return H;}
void plat_begin_frame(void){ drawing=1; }
void plat_end_frame(void){ drawing=0; frame_ticks++; }
void plat_clear(uint8_t c){memset(fb,c,sizeof fb);}
static void pix(int x,int y,uint8_t c)
{
    if((unsigned)x<(unsigned)W && (unsigned)y<(unsigned)H) fb[y*W+x]=c;
}
void plat_rect(int x,int y,int w,int h,uint8_t c)
{
    int row;
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(w>W-x) w=W-x;
    if(h>H-y) h=H-y;
    if(w<=0 || h<=0) return;
    for(row=0;row<h;row++)memset(fb+(y+row)*W+x,c,(size_t)w);
}
void plat_line(int x0,int y0,int x1,int y1,uint8_t c)
{
    int dx=x1>=x0?x1-x0:x0-x1;
    int dy=y1>=y0?y1-y0:y0-y1;
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
    int i;
    for(i=0;i<8192;i++){
        int twice;
        pix(x0,y0,c);
        if(x0==x1 && y0==y1)break;
        twice=err*2;
        if(twice>-dy){err-=dy;x0+=sx;}
        if(twice<dx){err+=dx;y0+=sy;}
    }
}
void plat_shade_rect(int x,int y,int w,int h,int level)
{
    int row,col;
    const uint8_t *map;
    if(level<0)level=0;
    if(level>=DOOMO_LIGHT_LEVELS)level=DOOMO_LIGHT_LEVELS-1;
    map=doomo_colormap[level];
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(w>W-x)w=W-x;if(h>H-y)h=H-y;
    if(w<=0||h<=0)return;
    for(row=0;row<h;row++){
        uint8_t *p=fb+(y+row)*W+x;
        for(col=0;col<w;col++)p[col]=map[p[col]];
    }
}
void plat_text(int x,int y,const char *s,uint8_t c)
{
    int bx=x;
    if(!s)return;
    while(*s){
        unsigned ch=(unsigned char)*s++;
        int col,row;
        if(ch=='\n'){x=bx;y+=FONT_LINE;continue;}
        if(ch<FONT_FIRST||ch>FONT_LAST)ch='?';
        for(col=0;col<5;col++){
            uint8_t bits=doomo_font[ch-FONT_FIRST][col];
            for(row=0;row<8;row++)if(bits&(1u<<row))pix(x+col,y+row,c);
        }
        x+=FONT_ADVANCE;
    }
}
void plat_image(int x,int y,const DoomoImage *img,int scale,int flags,uint8_t color)
{
    int iy,ix,dx,dy;
    if(!img||!img->pix||!img->w||!img->h)return;
    if(scale<1)scale=1;if(scale>4)scale=4;
    /* A fully visible 1x image is the common UI/portrait path.
     * Composition is clipped-safe but avoids one screen-bound check per
     * opaque texel, leaving clipped/shadowed/scaled cases unchanged. */
    if(scale==1 && !(flags&IMG_SHADOW) &&
       ps2_blit_visible_1x(fb,W,H,x,y,img->pix,img->w,img->h,
                            (flags&IMG_FLIPX)!=0,(flags&IMG_SILHOUETTE)!=0,color))
        return;
    if(flags&IMG_SHADOW){
        for(iy=0;iy<img->h;iy++)for(ix=0;ix<img->w;ix++){
            int sx=(flags&IMG_FLIPX)?img->w-1-ix:ix;
            if(img->pix[iy*img->w+sx])
                plat_rect(x+ix*scale+scale,y+iy*scale+scale,scale,scale,PAL_GRAY_D);
        }
    }
    for(iy=0;iy<img->h;iy++)for(ix=0;ix<img->w;ix++){
        int sx=(flags&IMG_FLIPX)?img->w-1-ix:ix;
        uint8_t c=img->pix[iy*img->w+sx];
        if(!c)continue;
        if(flags&IMG_SILHOUETTE)c=color;
        if(scale==1)pix(x+ix,y+iy,c);
        else for(dy=0;dy<scale;dy++)for(dx=0;dx<scale;dx++)
            pix(x+ix*scale+dx,y+iy*scale+dy,c);
    }
}
void plat_checker(int x,int y,int w,int h,uint8_t c)
{
    int i,j;
    if(w<=0||h<=0)return;
    for(j=0;j<h;j++)for(i=0;i<w;i++)if(((x+i+y+j)&1)==0)pix(x+i,y+j,c);
}

/* No frame-time allocation. Palette BGR555 -> GS RGBA32 once at boot;
 * the 512x256 DMA surface reserves 512KiB and 512KiB of GS eDRAM. */
int ps2_game2d_init(GSGLOBAL *gs)
{
    unsigned i;
    memset(&picture,0,sizeof picture);
    /* GS sampling covers 320x240; the larger power-of-two texture edge
     * is always transparent. The edge is never touched by game drawing,
     * so clear it once at boot instead of 212KiB every 30Hz upload. */
    memset(upload,0,sizeof upload);
    picture.Width=TEX_W;picture.Height=TEX_H;
    picture.PSM=GS_PSM_CT32;
    picture.Filter=GS_FILTER_NEAREST;
    picture.Mem=upload;
    picture.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(TEX_W,TEX_H,picture.PSM),
                                  GSKIT_ALLOC_USERBUFFER);
    if(picture.Vram==GSKIT_ALLOC_ERROR)return 0;
    for(i=0;i<256;i++){
        uint16_t v=doomo_palette[i];
        uint32_t r=(v&31u)*255u/31u;
        uint32_t g=((v>>5)&31u)*255u/31u;
        uint32_t b=((v>>10)&31u)*255u/31u;
        colors[i]=r|(g<<8)|(b<<16)|0x80000000u;
    }
    gsKit_set_clamp(gs,GS_CMODE_CLAMP);
    return 1;
}
void ps2_game2d_present(GSGLOBAL *gs, int upload_frame)
{
    unsigned y,x;
    u64 tint=GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0);
    if(upload_frame){
        for(y=0;y<H;y++){
            u32 *dst=upload+y*TEX_W;
            const uint8_t *src=fb+y*W;
            for(x=0;x<W;x++)dst[x]=colors[src[x]];
        }
        gsKit_texture_upload(gs,&picture);
    }
    gsKit_clear(gs,GS_SETREG_RGBAQ(12,9,20,0x80,0));
    gsKit_prim_sprite_texture(gs,&picture,22.0f,0.0f,0.0f,0.0f,
                              618.0f,448.0f,(float)W,(float)H,1,tint);
}

/* Real map/first-person geometry is rendered in ref_overworld.c and
 * ref_view3d.c, using the same pixel buffer. */
void plat_tone(int ch,uint16_t hz,uint8_t vol,uint8_t wave)
{(void)ch;(void)hz;(void)vol;(void)wave;}
void plat_sample(int sfx,uint8_t vol){(void)sfx;(void)vol;}
/* PS2-2: save transport is implemented in memcard.c. */
uint32_t plat_entropy(void){return 0xCE11A17u;}
uint32_t plat_ticks(void){return frame_ticks;}
void plat_log(const char *s){if(s)printf("[PS2 core] %s\n",s);}
const char *plat_name(void){return "PS2";}
