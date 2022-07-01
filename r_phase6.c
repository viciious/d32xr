/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"
#include "mars.h"

typedef struct drawtex_s
{
#ifdef MARS
   inpixel_t *data;
#else
   pixel_t   *data;
#endif
   int       width;
   int       height;
   int       topheight;
   int       bottomheight;
   int       texturemid;
   int       pixelcount;
   drawcol_t drawcol;
} drawtex_t;

typedef struct
{
    viswall_t* segl;

    drawtex_t tex[2];
    drawtex_t *first, *last;

    int lightmin, lightmax, lightsub, lightcoef;
} seglocal_t;

static char seg_lock = 0;

static void R_DrawTextures(int x, unsigned iscale, int colnum, fixed_t scale2, int floorclipx, int ceilingclipx, unsigned light, seglocal_t* lsegl) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

static void R_LockSeg(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockSeg(void) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;

//
// Render a wall texture as columns
//
static void R_DrawTextures(int x, unsigned iscale, int colnum_, fixed_t scale2, int floorclipx, int ceilingclipx, unsigned light, seglocal_t* lsegl)
{
   drawtex_t *tex = lsegl->first;

   for (; tex < lsegl->last; tex++) {
       int top, bottom;

       FixedMul2(top, scale2, tex->topheight);
       top = centerY - top;
       if (top < ceilingclipx)
           top = ceilingclipx;

       FixedMul2(bottom, scale2, tex->bottomheight);
       bottom = centerY - bottom;
       if (bottom >= floorclipx)
           bottom = floorclipx;

       // column has no length?
       if (top < bottom)
       {
           fixed_t frac = tex->texturemid - (centerY - top) * iscale;
#ifdef MARS
           inpixel_t* src;
#else
           pixel_t* src;
#endif
           int colnum = colnum_;

           // pixel counter
           tex->pixelcount += (bottom - top);

           // DEBUG: fixes green pixels in MAP01...
           frac += (iscale + (iscale >> 5) + (iscale >> 6));

           while (frac < 0)
           {
               colnum--;
               frac += tex->height << FRACBITS;
           }

           // CALICO: comment says this, but the code does otherwise...
           // colnum = colnum - tex->width * (colnum / tex->width)
           colnum &= (tex->width - 1);

           // CALICO: Jaguar-specific GPU blitter input calculation starts here.
           // We invoke a software column drawer instead.
           src = tex->data + colnum * tex->height;
           tex->drawcol(x, top, bottom-1, light, frac, iscale, src, tex->height, NULL);
       }
   }
}

//
// Main seg drawing loop
//
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds)
{
    viswall_t* segl = lseg->segl;

    drawcol_t drawsky = (segl->actionbits & AC_ADDSKY) != 0 ? drawcol : NULL;

    unsigned scalefrac = segl->scalefrac;
    const unsigned scalestep = segl->scalestep;

    const unsigned centerangle = segl->centerangle;
    const unsigned offset = segl->offset;
    const unsigned distance = segl->distance;

    const int ceilingheight = segl->ceilingheight;

    int texturelight = lseg->lightmax;
    int lightmax = lseg->lightmax, lightmin = lseg->lightmin,
        lightcoef = lseg->lightcoef, lightsub = lseg->lightsub;

    const int stop = segl->stop;
    int x;

    // set GBR to division unit address
    __asm volatile (
        "mov #-128, r0\n\t"
        "add r0, r0\n\t"
        "ldc r0, gbr\n\t"
    );

    for (x = segl->start; x <= stop; x++)
    {
       fixed_t r;
       int floorclipx, ceilingclipx;
       unsigned scale2, colnum, iscale;

#ifdef MARS
        __asm volatile (
           "mov #0, r0\n\t"
           "mov.l r0, @(16, gbr) /* set high bits of the 64-bit dividend */ \n\t"
           "mov %0, r0\n\t"
           "mov.l r0, @(0, gbr) /* set 32-bit divisor */ \n\t"
           "mov #-1, r0\n\t"
           "mov.l r0, @(20, gbr) /* set low  bits of the 64-bit dividend, start divide */\n\t"
           : : "r" (scalefrac) : "r0", "gbr");
#else
        fixed_t scale = scalefrac;
#endif

        scale2 = scalefrac >> HEIGHTBITS;
        scalefrac += scalestep;

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = (unsigned)ceilingclipx >> 8;

        //
        // texture only stuff
        //
        if (lightcoef != 0)
        {
            // calc light level
            texturelight = scale2 * lightcoef - lightsub;
            if (texturelight < lightmin)
                texturelight = lightmin;
            else if (texturelight > lightmax)
                texturelight = lightmax;
            // convert to a hardware value
            texturelight = HWLIGHT((unsigned)(texturelight>>FRACBITS));
        }

        // calculate texture offset
        r = finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT);
        FixedMul2(r, distance, r);

        // other texture drawing info
        colnum = (offset - r) >> FRACBITS;

#ifdef MARS
        __asm volatile (
            "mov.l @(20, gbr), r0 /* get 32-bit quotient */ \n\t"
            "mov r0, %0"
            : "=r" (iscale) : : "r0", "gbr");
#else
        iscale = 0xffffffffu / scale;
#endif

        R_DrawTextures(x, iscale, colnum, scale2, floorclipx, ceilingclipx, texturelight, lseg);

        //
        // sky mapping
        //
        if (drawsky)
        {
            int top, bottom;

            top = ceilingclipx;
            FixedMul2(bottom, scale2, ceilingheight);
            bottom = centerY - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            if (top < bottom)
            {
                // CALICO: draw sky column
                int colnum = ((vd.viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT) & 0xff;
#ifdef MARS
                inpixel_t* data = skytexturep->data + colnum * skytexturep->height;
#else
                pixel_t* data = skytexturep->data + colnum * skytexturep->height;
#endif
                drawsky(x, top, --bottom, 0, (top * 18204) << 2, FRACUNIT + 7281, data, 128, NULL);
            }
        }
    }
}

static void R_LockSeg(void)
{
    int res;
    do {
        __asm volatile (\
        "tas.b %1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "m" (seg_lock) \
            );
    } while (res == 0);
}

static void R_UnlockSeg(void)
{
    seg_lock = 0;
}

void R_SegCommands(void)
{
    viswall_t* segl;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;
    int extralight;
    unsigned clipbounds_[SCREENWIDTH / 2 + 1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;
    unsigned short *newclipbounds = segclip;

    // initialize the clipbounds array
    R_InitClipBounds(clipbounds_);

    extralight = vd.extralight;
    toptex = &lseg.tex[0];
    bottomtex = &lseg.tex[1];

    lseg.lightmin = 0;
    lseg.lightmax = 0;
    lseg.lightcoef = 0;
    lseg.lightsub = 0;

    // workaround annoying compilation warnings
    toptex->height = 0;
    toptex->pixelcount = 0;
    bottomtex->height = 0;
    bottomtex->pixelcount = 0;

    for (segl = viswalls; segl < lastwallcmd; segl++)
    {
        int seglight;
        unsigned actionbits;

        if (segl->start > segl->stop)
            continue;

#ifdef MARS
        do
        {
            actionbits = *(volatile short *)&segl->actionbits;
        } while ((actionbits & AC_READY) == 0);

        if (actionbits & AC_DRAWN || !(actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_ADDSKY)))
        {
            goto post_draw;
        }

        R_LockSeg();
        actionbits = *(volatile short *)&segl->actionbits;
        if (actionbits & AC_DRAWN) {
            R_UnlockSeg();
            goto post_draw;
        } else {
            segl->actionbits = actionbits|AC_DRAWN;
            R_UnlockSeg();
        }
#endif

        lseg.segl = segl;
        lseg.first = lseg.tex + 1;
        lseg.last = lseg.tex + 1;

        if (vd.fixedcolormap)
        {
            lseg.lightmin = lseg.lightmax = vd.fixedcolormap;
        }
        else
        {
            seglight = segl->seglightlevel + extralight;
#ifdef MARS
            if (seglight > 255)
                seglight = 255;
#endif
            lseg.lightmax = seglight;
            lseg.lightmin = lseg.lightmax;

            if (detailmode == detmode_high)
            {
#ifdef MARS
                if (seglight <= 160 + extralight)
                    seglight = (seglight >> 1);
#else
                seglight = seglight - (255 - seglight) * 2;
                if (seglight < 0)
                    seglight = 0;
#endif
                lseg.lightmin = seglight;
            }

            if (lseg.lightmin != lseg.lightmax)
            {
                lseg.lightcoef = ((unsigned)(lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);
                lseg.lightsub = 160 * lseg.lightcoef;
                lseg.lightmin <<= FRACBITS;
                lseg.lightmax <<= FRACBITS;
            }
            else
            {
                lseg.lightcoef = 0;
                lseg.lightmin = lseg.lightmax = HWLIGHT((unsigned)lseg.lightmax);
            }
        }

        if (actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[segl->t_texturenum];
            toptex->topheight = segl->t_topheight;
            toptex->bottomheight = segl->t_bottomheight;
            toptex->texturemid = segl->t_texturemid;
            toptex->width = tex->width;
            toptex->height = tex->height;
            toptex->data = tex->data;
            toptex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
            lseg.first--;
        }

        if (actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[segl->b_texturenum];
            bottomtex->topheight = segl->b_topheight;
            bottomtex->bottomheight = segl->b_bottomheight;
            bottomtex->texturemid = segl->b_texturemid;
            bottomtex->width = tex->width;
            bottomtex->height = tex->height;
            bottomtex->data = tex->data;
            bottomtex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
            lseg.last++;
        }

        R_DrawSeg(&lseg, clipbounds);

        segl->t_topheight = toptex->pixelcount;
        segl->b_topheight = bottomtex->pixelcount;

post_draw:
        if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
        {
            int i;
            int x = segl->start;
            int width = segl->stop - segl->start + 1;
            for (i = 0; i < width; i++, x++)
                clipbounds[x] = newclipbounds[i];
            newclipbounds += width;
        }
    }
}

#ifdef MARS

void Mars_Sec_R_SegCommands(void)
{
    R_SegCommands();
}

#endif

