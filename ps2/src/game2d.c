/* PS2-1: first playable shared-core 2D framebuffer.
 * The core's authoritative 256-colour palette, 5x8 font, portraits and
 * user interface draw here at 320x240. A single 320x240 GS RGB5551
 * texture (150 KiB per changed frame) is refreshed at up to 30Hz;
 * NTSC fields display the most recent image at 60Hz. The reference 3D/overworld renderers share this indexed surface;
 * native GS polygon acceleration is still an unimplemented milestone.
 * No new per-resident portrait cache, heap allocation or save fields.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <gsKit.h>
#include "game2d.h"
#include "indexed_blit.h"
#include "indexed_present.h"
#include "doomo/doomo.h"
#include "doomo/font.h"
#include "doomo/palette.h"

enum { W=PS2_GAME_W, H=PS2_GAME_H };
uint8_t ps2_indexed_fb[W*H];
#define fb ps2_indexed_fb
static u16 upload[W*H] __attribute__((aligned(64)));
static u16 colors[256];
static GSTEXTURE picture;
static float picture_left, picture_right, picture_bottom;
static uint32_t frame_ticks;
static uint32_t gs_upload_count;
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
    if(w>W-x)w=W-x;
    if(h>H-y)h=H-y;
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
    if(scale<1)scale=1;
    if(scale>4)scale=4;
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

/* The staging image contains only visible pixels. gsKit rounds the
 * sampler exponents and the CT16 VRAM footprint independently. */
int ps2_game2d_init(GSGLOBAL *gs)
{
    unsigned i;
    /* Keep the source at 4:3. In 480p each texel covers 2x2 display
     * pixels with nearest sampling. */
    int width = ((gs->Height * W) / H) & ~1;
    picture_left = (float)(gs->Width - width) / 2.0f;
    picture_right = picture_left + (float)width;
    picture_bottom = (float)gs->Height;
    memset(&picture,0,sizeof picture);
    gs_upload_count=0;
    memset(upload,0,sizeof upload);
    picture.Width=W;picture.Height=H;
    picture.PSM=GS_PSM_CT16;
    picture.Filter=GS_FILTER_NEAREST;
    /* gsKit declares GSTEXTURE.Mem as u32* for all texture PSMs.
     * CT16 uploads remain packed u16 pixels; the upload byte size is
     * derived from picture.PSM, not the pointer's C element type. */
    picture.Mem=(u32 *)(void *)upload;
    picture.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(W,H,picture.PSM),
                                  GSKIT_ALLOC_USERBUFFER);
    if(picture.Vram==GSKIT_ALLOC_ERROR)return 0;
    for(i=0;i<256;i++){
        /* The source is RGB555, which is the GS CT16 bit layout:
         * R bits 0-4, G 5-9, B 10-14, 1-bit opaque alpha at 15. */
        colors[i]=(u16)(doomo_palette[i]|0x8000u);
    }
    gsKit_set_clamp(gs,GS_CMODE_CLAMP);
    return 1;
}
void ps2_game2d_present(GSGLOBAL *gs, int upload_frame)
{
    u64 tint=GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0);
    if(upload_frame && ps2_indexed_update(fb,W,upload,W,W,H,colors)){
        /* The full texture remains resident in GS VRAM during alternate
         * fields and unchanged logic frames. No duplicate backing image
         * or probabilistic hash; exact per-pixel comparison is the gate. */
        gsKit_texture_upload(gs,&picture);
        gs_upload_count++;
    }
    /* The opaque texture replaces every pixel in its viewport. Repaint
     * only the 480i side gutters so both back buffers keep the same matte. */
    if (picture_left > 0.0f) {
        const u64 matte = GS_SETREG_RGBAQ(12,9,20,0x80,0);
        gsKit_prim_sprite(gs,0.0f,0.0f,picture_left,picture_bottom,0,matte);
        gsKit_prim_sprite(gs,picture_right,0.0f,(float)gs->Width,picture_bottom,0,matte);
    }
    gsKit_prim_sprite_texture(gs,&picture,picture_left,0.0f,0.0f,0.0f,
                              picture_right,picture_bottom,(float)W,(float)H,1,tint);
}

/* Sparse emulator diagnostics: a framebuffer checksum plus the true
 * upload count distinguishes a progressing simulation from a static or
 * frozen rendering loop without a second 320x240 buffer. This is never
 * used to suppress uploads (exact comparisons do that). */
void ps2_game2d_probe(uint32_t *ticks,uint32_t *uploads,uint32_t *frame_hash)
{
    if(ticks)*ticks=frame_ticks;
    if(uploads)*uploads=gs_upload_count;
    if(frame_hash){
        uint32_t h=2166136261u;
        unsigned i;
        for(i=0;i<W*H;i++){
            h^=(uint32_t)fb[i];
            h*=16777619u;
        }
        *frame_hash=h;
    }
}

/* Real map/first-person geometry is rendered in ref_overworld.c and
 * ref_view3d.c, using the same pixel buffer. */
/* Real SPU2 plat_tone/plat_sample are supplied by audio.c. */
/* PS2-2: save transport is implemented in memcard.c. */
uint32_t plat_entropy(void){return 0xCE11A17u;}
uint32_t plat_ticks(void){return frame_ticks;}
void plat_log(const char *s){if(s)printf("[PS2 core] %s\n",s);}
const char *plat_name(void){return "PS2";}
