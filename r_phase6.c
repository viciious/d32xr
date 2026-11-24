/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"
#include "mars.h"

#define MIPSCALE 0x20000
#define LIGHTZSHIFT	10

typedef struct
{
#ifdef MARS
   inpixel_t *data;
#else
   pixel_t   *data;
#endif
   VINT      height;
   VINT      pitch;
   drawcol_t drawcol;

   // decals stuff
   VINT       numdecals;
   VINT       lastcol;
   texture_t *tex;
   int8_t    *colormaps;
   uint8_t   *columncache;
} drawmip_t;

typedef struct drawtex_s
{
   drawmip_t mip[MIPLEVELS];
   VINT      maxmip;
   VINT      widthmask;
   fixed_t   texturemid;
   fixed_t   topheight;
   fixed_t   bottomheight;
   uint8_t   *columncache;
} drawtex_t;

typedef struct
{
    viswall_t* segl;
    drawtex_t *first, *last;
    int lightmin, lightmax, lightsub, lightcoef;
    fixed_t scalestep, offset, distance;
    unsigned centerangle;
    int skypitch;
    drawcol_t drawskycol;
    unsigned skyangle;
#if MIPLEVELS > 1
    // keep within the mov.l @(disp,Rn) range (60 bytes)
    unsigned minmip, maxmip;
#endif

    drawtex_t tex[2];

    int8_t *skycolormaps;
} seglocal_t;

static atomic_flag seg_lock = ATOMIC_FLAG_INIT;

#define R_LockSeg() do { while (atomic_flag_test_and_set(&seg_lock)); } while (0)
#define R_UnlockSeg() do { atomic_flag_clear(&seg_lock); } while (0)

static void R_DrawSeg(seglocal_t *lseg, unsigned short *restrict clipbounds) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static void R_DrawSegSky(seglocal_t *lseg, unsigned short *restrict clipbounds) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static void R_SetupDrawTexture(drawtex_t *drawtex, texture_t *tex, fixed_t texturemid, fixed_t topheight, fixed_t bottomheight) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Main seg drawing loop
//
static void R_DrawSeg(seglocal_t *lseg, unsigned short *restrict clipbounds)
{
    viswall_t* segl = lseg->segl;

    fixed_t scalefrac = segl->scalefrac;
    int texturelight = lseg->lightmax;

    const int start = segl->start;
    const int stop = segl->stop;
    int x;
    unsigned miplevel = 0;

    drawtex_t *tex;

    uint16_t *restrict segcolmask = (segl->actionbits & AC_MIDTEXTURE) ? (segl->clipbounds + 1) + stop - start : NULL;

#if MIPLEVELS > 1
    // scale is linear in x, so miplevel is monotonic and its extremes are
    // at the first and the last column: maxmip holds the first one until
    // the loop is done
    lseg->maxmip = -1;
#endif

    x = start;
    do {
       fixed_t r;
       unsigned clip;
       fixed_t scale2;
       unsigned colnum;
       unsigned iscale;

#ifdef MARS
        int32_t t, divunit;
        __asm volatile (
           "mov #-128, %1\n\t"
           "add %1, %1 /* %1 is now 0xFFFFFF00 */ \n\t"
           "mov.l %2, @(0, %1) /* set 32-bit divisor */ \n\t"
           "mov #0, %0\n\t"
           "mov.l %0, @(16, %1) /* set high bits of the 64-bit dividend */ \n\t"
           "mov #-1, %0\n\t"
           "mov.l %0, @(20, %1) /* set low  bits of the 64-bit dividend, start divide */\n\t"
           : "=&r" (t), "=&r" (divunit) : "r" (scalefrac));
#else
        fixed_t scale = scalefrac;
#endif

        scale2 = scalefrac;
        scalefrac += lseg->scalestep;

        // calculate texture offset
        r = finetangent((uint16_t)(lseg->centerangle + xtoviewangle[x]) >> (ANGLETOFINESHIFT-FRACBITS));
        r = FixedMul(lseg->distance, r);
        colnum = ((unsigned)(lseg->offset - r) >> FRACBITS) & 0xff;

        // ceilingclipx and floorclipx, unpacked per texture
        clip = clipbounds[x];

        //
        // texture only stuff
        //
        if (lseg->lightcoef != 0)
        {
            // calc light level
            texturelight = lseg->lightsub - FixedMul(scale2, lseg->lightcoef);
            if (texturelight < lseg->lightmin)
                texturelight = lseg->lightmin;
            if (texturelight > lseg->lightmax)
                texturelight = lseg->lightmax;
            texturelight = texturelight >> FRACBITS;
            texturelight <<= 8;
        }

        if (segcolmask)
            segcolmask[x] = texturelight | colnum;

#ifdef MARS
        __asm volatile (
            "mov #-128, %0\n\t"
            "add %0, %0 /* %0 is now 0xFFFFFF00 */ \n\t"
            "mov.l @(20, %0), %0 /* get 32-bit quotient */ \n\t"
            : "=r" (iscale));
#else
        iscale = 0xffffffffu / scale;
#endif

#if MIPLEVELS > 1
        // other texture drawing info
        miplevel = iscale / MIPSCALE;
        if ((int)lseg->maxmip < 0)
            lseg->maxmip = miplevel;
#endif

        //
        // draw the textures
        //
        tex = lseg->first;
        if (tex < lseg->last)
        do {
            fixed_t top, bottom;
            int floorclipx = clip & 0x00ff, ceilingclipx = clip >> 8;

            top = FixedMul(scale2, tex->topheight)>>FRACBITS;
            bottom = FixedMul(scale2, tex->bottomheight)>>FRACBITS;

            top = centerY - top;
            if (top < ceilingclipx)
                top = ceilingclipx;

            bottom = centerY - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            // column has no length?
            if (top < bottom)
            {
                VINT texcolnum, mipcolnum;
                unsigned m = miplevel;
                unsigned texiscale = iscale;
                drawmip_t *mip;
                fixed_t frac;
#ifdef MARS
                inpixel_t* src;
#else
                pixel_t* src;
#endif

                texcolnum = colnum & tex->widthmask;
                mipcolnum = texcolnum;
                frac = tex->texturemid - (centerY - top) * iscale;
                mip = tex->mip;

#if MIPLEVELS > 1
                if (m > (unsigned)tex->maxmip)
                    m = tex->maxmip;
                if (m > 0) {
                    unsigned i = m;
                    do {
                        frac >>= 1;
                        mipcolnum >>= 1;
                        texiscale >>= 1;
                        mip++;
#ifdef MARS
                        // keep GCC from folding the increments into mip + m*28 (mul.l)
                        // and from spilling frac to the stack on every iteration
                        __asm volatile ("" : "+r" (mip), "+r" (frac), "+r" (texiscale), "+r" (mipcolnum));
#endif
                    } while (--i);
                }
#else
                m = 0;
#endif

                // CALICO: Jaguar-specific GPU blitter input calculation starts here.
                // We invoke a software column drawer instead.
#ifdef MARS
                // keep GCC from turning mip back into &tex->mip[m] (mul.l)
                __asm volatile ("" : "+r" (mip));
#endif
                src = mip->data + mipcolnum * mip->pitch;

                if (mip->numdecals > 0)
                {
                    // decals/composite textures
                    if (mip->lastcol == texcolnum)
                    {
                        src = mip->columncache;
                    }
                    else
                    {
                        boolean decaled;

                        decaled = R_CompositeColumn(texcolnum, mip->tex,
                            src, mip->columncache, mip->height, m);
                        if (decaled)
                        {
                            src = mip->columncache;
                            mip->lastcol = texcolnum;
                        }
                    }
                }

                I_SetThreadLocalVar(DOOMTLS_COLORMAP, mip->colormaps);

                mip->drawcol(x, top, bottom-1, texturelight, frac, texiscale, src, mip->height);
            }
        } while (++tex < lseg->last);
    } while (++x <= stop);

#if MIPLEVELS > 1
    {
        // miplevel is now the last column's
        unsigned lo = lseg->maxmip, hi = miplevel;
        if (lo > hi)
            lo = miplevel, hi = lseg->maxmip;
        if (lo > MIPLEVELS)
            lo = MIPLEVELS;
        lseg->minmip = lo;
        lseg->maxmip = hi;
    }
#endif
}

//
// Main seg drawing loop
//
static void R_DrawSegSky(seglocal_t *lseg, unsigned short *restrict clipbounds)
{
    viswall_t* segl = lseg->segl;

    fixed_t scalefrac = segl->scalefrac;
    fixed_t scalestep;

    const fixed_t ceilingheight = FROMHEIGHT16(segl->ceilingheight);
    const int start = segl->start;
    const int stop = segl->stop;
    int x;

    //
    // sky mapping
    //
    I_SetThreadLocalVar(DOOMTLS_COLORMAP, lseg->skycolormaps);

    scalefrac = FixedMul(scalefrac, ceilingheight);
    scalestep = FixedMul(segl->scalestep, ceilingheight);

    x = start;
    do {
        fixed_t scale2;
        int floorclipx, ceilingclipx;
        int top, bottom;

        scale2 = scalefrac;
        scalefrac += scalestep;

        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = (unsigned)ceilingclipx >> 8;

        top = ceilingclipx;
        bottom = scale2>>FRACBITS;
        bottom = centerY - bottom;
        if (bottom > floorclipx)
            bottom = floorclipx;

        if (top < bottom)
        {
            unsigned colnum;

            colnum = ((uint16_t)(lseg->skyangle + xtoviewangle[x]) >> (ANGLETOSKYSHIFT-FRACBITS)) & 0xff;

#ifdef MARS
            inpixel_t* data = skytexturep + colnum * lseg->skypitch;
#else
            pixel_t* data = skytexturep + colnum * lseg->skypitch;
#endif
            lseg->drawskycol(x, top, --bottom, 0, (top * 18204) << 2, FRACUNIT + 7281, data, 128);
        }
    } while (++x <= stop);

    I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);
}

static void R_SetupDrawTexture(drawtex_t *drawtex, texture_t *tex, 
    fixed_t texturemid, fixed_t topheight, fixed_t bottomheight)
{
    int j;
    int width = tex->width, height = tex->height;
    uint8_t *columncache = drawtex->columncache;
    int mipwidth, mipheight;

    drawtex->widthmask = width-1;
    drawtex->topheight = FROMHEIGHT16(topheight);
    drawtex->bottomheight = FROMHEIGHT16(bottomheight);
    drawtex->texturemid = FROMHEIGHT16(texturemid);
#if MIPLEVELS > 1
    drawtex->maxmip = !texmips ? 0 : tex->mipcount-1;
#else
    drawtex->maxmip = 0;
#endif

    mipwidth = width;
    mipheight = height;
    j = 0;
    do {
        drawmip_t *mip = &drawtex->mip[j];

        mip->tex = tex;
        mip->height = mipheight;
        mip->pitch = mipheight;
        mip->data = tex->data[j];
        mip->drawcol = (mipheight & (mipheight - 1)) ? drawcolnpo2 : drawcol;
        mip->colormaps = dc_colormaps;
        mipwidth >>= 1, mipheight >>= 1;
        if (mipwidth < 1)
            mipwidth = 1;
        if (mipheight < 1)
            mipheight = 1;

        // decals stuff
        mip->lastcol = -1;
        mip->columncache = columncache;
        columncache += mip->height;

        mip->numdecals = tex->decals & 0x3;
        if (R_InTexCache(&r_texcache, mip->data) == 1) {
            mip->numdecals = 0; 
            mip->colormaps = dc_colormaps;
        } else if (!mip->numdecals && tex->colormaps != NULL) {
            mip->drawcol = (mipheight & (mipheight - 1)) ? draw4bcolnpo2 : draw4bcol;
            mip->colormaps = tex->colormaps;
            mip->pitch /= 2;
            if (mip->pitch < 1)
                mip->pitch = 1;
        }
    } while (++j <= drawtex->maxmip);
}

void R_SegCommands(void)
{
    int i;
    viswall_t *segl, *last;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;
    int extralight;
    uint32_t clipbounds_[SCREENWIDTH / 2 + 1];
    uint16_t *clipbounds = (uint16_t *)clipbounds_;
#ifdef MARS
    volatile int8_t *addedsegs = (volatile int8_t *)&MARS_SYS_COMM6;
    volatile uint8_t *readysegs = (volatile uint8_t *)addedsegs + 1;
#endif

    // initialize the clipbounds array
    R_InitClipBounds(clipbounds_);

    D_memset(&lseg, 0, sizeof(lseg));

    toptex = &lseg.tex[0];
    bottomtex = &lseg.tex[1];

    // sky setup
    lseg.skycolormaps = skycolormaps ? skycolormaps : dc_colormaps;
    lseg.skypitch = 128;
    lseg.drawskycol = drawcol;
    if (skydepth == 2) {
        lseg.skypitch = 64;
        lseg.drawskycol = draw4bcol;
    }
    lseg.skyangle = vd->viewangle >> FRACBITS;

    I_GetThreadLocalVar(DOOMTLS_COLUMNCACHE, toptex->columncache);
    bottomtex->columncache = toptex->columncache + COLUMN_CACHE_SIZE;

    extralight = vd->extralight;
    last = vd->lastwallcmd;
    i = 0;

    for (segl = vd->viswalls; segl < last; segl++)
    {
        int seglight;
        int actionbits;

#ifdef MARS
        if (*addedsegs == -1)
            return; // the other cpu is done drawing segments, so should we
        while (*readysegs <= i) {
            SH2_Delay64();
            continue;
        }
#endif
        i++;

#ifdef MARS
        R_LockSeg();
        actionbits = *(volatile short *)&segl->actionbits;
        if (actionbits & AC_DRAWN || !(actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_MIDTEXTURE | AC_ADDSKY))) {
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
#if MIPLEVELS > 1
        lseg.minmip = MIPLEVELS;
        lseg.maxmip = 0;
#endif

        if (vd->fixedcolormap)
        {
            lseg.lightmin = lseg.lightmax = vd->fixedcolormap;
            lseg.lightcoef = 0;
        }
        else
        {
            seglight = segl->seglightlevel & 0xff;
            seglight += (int8_t)((unsigned)segl->seglightlevel >> 8);
            if (seglight < 0)
                seglight = 0;
#ifdef MARS
            lseg.lightmax = seglight;
            lseg.lightmax += extralight;
#endif
            if (lseg.lightmax > 255)
                lseg.lightmax = 255;

#ifdef MARS
            seglight = seglight - (255 - seglight - seglight/2) * 2;
#else
            seglight = seglight - (255 - seglight) * 2;
#endif
            seglight += extralight;
            if (seglight < MINLIGHT)
                seglight = MINLIGHT;
            if (seglight > lseg.lightmax)
                seglight = lseg.lightmax;
            lseg.lightmin = seglight;
            lseg.lightcoef = 0;

            if (lseg.lightmin != lseg.lightmax)
            {
                int light1, light2;

                lseg.lightmin <<= FRACBITS;
                lseg.lightmax <<= FRACBITS;
                lseg.lightcoef = (unsigned)(lseg.lightmax - lseg.lightmin) / (800 - 160);
                lseg.lightsub = 160 * lseg.lightcoef;
                lseg.lightcoef <<= LIGHTZSHIFT;

                // calculate lighting values at both end points
                // if they are the same, disable gradient lighting

                light1 = FixedMul(segl->scalefrac, lseg.lightcoef) - lseg.lightsub;
                if (light1 < lseg.lightmin)
                    light1 = lseg.lightmin;
                else if (light1 > lseg.lightmax)
                    light1 = lseg.lightmax;

                light2 = FixedMul(segl->scale2, lseg.lightcoef) - lseg.lightsub;
                if (light2 < lseg.lightmin)
                    light2 = lseg.lightmin;
                else if (light2 > lseg.lightmax)
                    light2 = lseg.lightmax;

                if (light1 == light2)
                {
                    lseg.lightcoef = 0;
                    lseg.lightmax = HWLIGHT((unsigned)light1>>FRACBITS);
                }
                else
                {
                    int t;

                    // perform HWLIGHT calculations on the coefficients
                    // to reduce the number of calculations we will do
                    // later per column
                    t = lseg.lightmax;
                    lseg.lightmax = -lseg.lightmin;
                    lseg.lightmin = -t;

                    if (lseg.lightmax > 0)
                        lseg.lightmax = 0;

                    lseg.lightsub += 255 * FRACUNIT;
                    lseg.lightmax += 255 * FRACUNIT;
                    lseg.lightmin += 255 * FRACUNIT;

                    lseg.lightcoef = (unsigned)lseg.lightcoef >> 3;
                    lseg.lightsub = (unsigned)lseg.lightsub >> 3;
                    lseg.lightmax = (unsigned)lseg.lightmax >> 3;
                    lseg.lightmin = (unsigned)lseg.lightmin >> 3;
                }
            }
            else
            {
                lseg.lightmax = HWLIGHT((unsigned)lseg.lightmax);
            }
        }

        if (actionbits & AC_TOPTEXTURE)
        {
            R_SetupDrawTexture(toptex, &textures[segl->t_texturenum],
                segl->t_texturemid, segl->t_topheight, segl->t_bottomheight);
            lseg.first--;
        }

        if (actionbits & AC_BOTTOMTEXTURE)
        {
            R_SetupDrawTexture(bottomtex, &textures[segl->b_texturenum],
                segl->b_texturemid, segl->b_topheight, segl->b_bottomheight);
            lseg.last++;
        }

        if ((actionbits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE|AC_MIDTEXTURE)) != 0)
        {
            lseg.scalestep = segl->scalestep;
            lseg.centerangle = segl->centerangle >> FRACBITS;
            lseg.offset = segl->offset;
            lseg.distance = segl->distance;
            R_DrawSeg(&lseg, clipbounds);
        }

        if ((actionbits & AC_ADDSKY) != 0)
            R_DrawSegSky(&lseg, clipbounds);

        *(int16_t *)&segl->miplevels[0] = 0;
#if MIPLEVELS > 1
        if (lseg.maxmip >= MIPLEVELS)
            lseg.maxmip = MIPLEVELS-1;
        segl->miplevels[0] = lseg.minmip;
        segl->miplevels[1] = lseg.maxmip;
#endif

post_draw:
        if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING) && segl + 1 < last)
        {
            unsigned w = segl->stop - segl->start + 1;
            short *restrict src = (short *)segl->clipbounds + segl->start;
            short *restrict dst = (short *)clipbounds + segl->start;

            do {
                *dst = *src, src++, dst++;
            } while (--w);
        }
    }

#ifdef MARS
    // mark all segments as rendered
    *addedsegs = -1;
#endif
}

#ifdef MARS

void Mars_Sec_R_SegCommands(void)
{
    Mars_ClearCache();

    R_SegCommands();
}

#endif

