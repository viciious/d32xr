/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "p_camera.h"
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
   drawcol_t drawcol;

#ifdef USE_DECALS
   // decals stuff
   VINT       numdecals;
   VINT       lastcol;
   texdecal_t *decals;
   uint8_t   *columncache;
#endif
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

    drawtex_t tex[2];
    drawtex_t *first, *last;

#ifndef SIMPLELIGHT
    int lightmin;
#endif
    int lightmax;
#ifndef SIMPLELIGHT
    int lightsub, lightcoef;
#endif
#if MIPLEVELS > 1   
    unsigned minmip, maxmip;
#endif
} seglocal_t;

extern boolean sky_md_layer;

static char seg_lock = 0;

static void R_DrawTexture(int x, unsigned iscale, int colnum, fixed_t scale2, int floorclipx, int ceilingclipx, unsigned light, drawtex_t *tex, int miplevel) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
static void R_SetupDrawTexture(drawtex_t *drawtex, texture_t *tex, fixed_t texturemid, fixed_t topheight, fixed_t bottomheight) ATTR_DATA_CACHE_ALIGN;

static void R_LockSeg(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockSeg(void) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Render a wall texture as columns
//
static void R_DrawTexture(int x, unsigned iscale, int colnum, fixed_t scale2, int floorclipx, int ceilingclipx, unsigned light, drawtex_t *tex, int miplevel)
{
    fixed_t top, bottom;

    top = FixedMul(scale2, tex->topheight)>>FRACBITS;
    bottom = FixedMul(scale2, tex->bottomheight)>>FRACBITS;

    top = centerY - top;
    if (top < ceilingclipx)
        top = ceilingclipx;

    bottom = centerY - bottom;
    if (bottom > floorclipx)
        bottom = floorclipx;

#if MIPLEVELS <= 1
    miplevel = 0;
#endif

    // column has no length?
    if (top < bottom)
    {
        VINT mipcolnum;
        drawmip_t *mip;
        fixed_t frac;
#ifdef MARS
        inpixel_t* src;
#else
        pixel_t* src;
#endif

        colnum &= tex->widthmask;
        mipcolnum = colnum;
        mipcolnum &= tex->widthmask;
        frac = tex->texturemid - (centerY - top) * iscale;

#if MIPLEVELS > 1
        if (miplevel > tex->maxmip)
            miplevel = tex->maxmip;
        if (miplevel > 0) {
            unsigned m = miplevel;
            do {
                frac >>= 1;
                mipcolnum >>= 1;
                iscale >>= 1;
            } while (--m);
        }
#endif

        // CALICO: Jaguar-specific GPU blitter input calculation starts here.
        // We invoke a software column drawer instead.
        mip = &tex->mip[miplevel];
        src = mip->data + mipcolnum * mip->height;

#ifdef USE_DECALS
        if (mip->numdecals > 0)
        {
            // decals/composite textures
            if (mip->lastcol == colnum)
            {
                src = mip->columncache;
            }
            else
            {
                boolean decaled;

                decaled = R_CompositeColumn(colnum, mip->numdecals, mip->decals,
                    src, mip->columncache, mip->height, miplevel);
                if (decaled)
                {
                    src = mip->columncache;
                    mip->lastcol = colnum;
                }
            }
        }
#endif

        mip->drawcol(x, top, bottom-1, light, frac, iscale, src, mip->height);
    }
}

//
// Main seg drawing loop
//
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds)
{
    viswall_t* segl = lseg->segl;

    #ifdef MDSKY
    drawskycol_t drawmdsky;
    #endif
    drawcol_t draw32xsky;

    #ifdef MDSKY
    if (sky_32x_layer) {
        draw32xsky = (segl->actionbits & AC_ADDSKY) != 0 ? draw32xskycol : NULL;
        drawmdsky = NULL;
    }
    else {
        drawmdsky = (segl->actionbits & AC_ADDSKY) != 0 ? drawskycol : NULL;
        draw32xsky = NULL;
    }
    #else
    draw32xsky = (segl->actionbits & AC_ADDSKY) != 0 ? draw32xskycol : NULL;
    #endif

    fixed_t scalefrac = segl->scalefrac;
    const fixed_t scalestep = segl->scalestep;

    const unsigned centerangle = segl->centerangle;
#ifdef WALLDRAW2X
    unsigned offset = segl->offset >> 1;
    fixed_t distance = segl->distance >> 1;
#else
    unsigned offset = segl->offset;
    fixed_t distance = segl->distance;
#endif

    const int ceilingheight = segl->ceilingheight;

#ifdef SIMPLELIGHT
    const int texturelight = lseg->lightmax;
#else
    int texturelight = lseg->lightmax;
    int lightmax = lseg->lightmax, lightmin = lseg->lightmin,
        lightcoef = lseg->lightcoef, lightsub = lseg->lightsub;
#endif

    const int start = segl->start;
    const int stop = segl->stop;
    int x;
    unsigned miplevel = 0;

    drawtex_t *tex;

    uint16_t *segcolmask = (segl->actionbits & AC_MIDTEXTURE) ? segl->clipbounds + (stop - start + 1) : NULL;

    for (x = start; x <= stop; x++)
    {
       fixed_t r;
       int floorclipx, ceilingclipx;
       fixed_t scale2;
       unsigned colnum, iscale;

#ifdef MARS
        volatile int32_t t;
        __asm volatile (
           "mov #-128, r0\n\t"
           "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
           "mov #0, %0\n\t"
           "mov.l %0, @(16, r0) /* set high bits of the 64-bit dividend */ \n\t"
           "mov.l %1, @(0, r0) /* set 32-bit divisor */ \n\t"
           "mov #-1, %0\n\t"
           "mov.l %0, @(20, r0) /* set low  bits of the 64-bit dividend, start divide */\n\t"
           : "=&r" (t) : "r" (scalefrac) : "r0");
#else
        fixed_t scale = scalefrac;
#endif

        scale2 = scalefrac;
        scalefrac += scalestep;

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = (unsigned)ceilingclipx >> 8;

        //
        // sky mapping
        //
        #ifdef MDSKY
        if (draw32xsky || drawmdsky)
        #else
        if (draw32xsky)
        #endif
        {
            int top, bottom;

            top = ceilingclipx;
            bottom = FixedMul(scale2, ceilingheight)>>FRACBITS;
            bottom = centerY - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            if (top < bottom)
            {
                if (draw32xsky) {
                    int colnum = ((vd.viewangle + (xtoviewangle[x]<<FRACBITS)) >> ANGLETOSKYSHIFT) & (skytexturep->width-1);
                    inpixel_t* data = skytexturep->data[0] + colnum * skytexturep->height;
                    
                    draw32xsky(
                        x,
                        -gamemapinfo.skyOffsetY
                                - gamemapinfo.skyBitmapOffsetY
                                - (((signed int)vd.aimingangle) >> 22)
                                - ((vd.viewz >> 16) >> (16-gamemapinfo.skyBitmapScrollRate)),
                        top,
                        bottom,
                        gamemapinfo.skyTopColor,
                        gamemapinfo.skyBottomColor,
                        data,
                        skytexturep->height
                        );
                }
#ifdef MDSKY
                else {
                    drawmdsky(x, top, bottom);
                }
#endif
            }
        }

        //
        // texture only stuff
        //
#ifndef SIMPLELIGHT
        if (lightcoef != 0)
        {
            // calc light level
            texturelight = FixedMul(scale2, lightcoef) - lightsub;
            if (texturelight < lightmin)
                texturelight = lightmin;
            else if (texturelight > lightmax)
                texturelight = lightmax;
            // convert to a hardware value
            texturelight = HWLIGHT((unsigned)texturelight>>FRACBITS);
        }
#endif

        // calculate texture offset
        r = finetangent((centerangle + (xtoviewangle[x]<<FRACBITS)) >> ANGLETOFINESHIFT);
        r = FixedMul(distance, r);

        colnum = (offset - r) >> FRACBITS;

        if (segcolmask)
            segcolmask[x] = texturelight | (colnum & 0xff);

#ifdef MARS
#ifdef WALLDRAW2X
        __asm volatile (
            "mov #-128, r0\n\t"
            "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
            "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
            "shar %0\n\t"
            : "=r" (iscale) : : "r0");
#else
        __asm volatile (
            "mov #-128, r0\n\t"
            "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
            "mov.l @(20, r0), %0 /* get 32-bit quotient */ \n\t"
            : "=r" (iscale) : : "r0");
#endif
#else
        iscale = 0xffffffffu / scale;
#endif

#if MIPLEVELS > 1
        // other texture drawing info
        miplevel = iscale / MIPSCALE;
        if (miplevel > lseg->maxmip)
            lseg->maxmip = miplevel;
        if (miplevel < lseg->minmip)
            lseg->minmip = miplevel;
#endif

        for (tex = lseg->first; tex < lseg->last; tex++)
            R_DrawTexture(x, iscale, colnum, scale2, floorclipx, ceilingclipx, texturelight, tex, miplevel);
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

static void R_SetupDrawTexture(drawtex_t *drawtex, texture_t *tex, 
    fixed_t texturemid, fixed_t topheight, fixed_t bottomheight)
{
    int j;
    int width = tex->width, height = tex->height;
#ifdef USE_DECALS
    uint8_t *columncache = drawtex->columncache;
#endif
    int mipwidth, mipheight;

    drawtex->widthmask = width-1;
    drawtex->topheight = topheight;
    drawtex->bottomheight = bottomheight;
    drawtex->texturemid = texturemid;
#if MIPLEVELS > 1
    drawtex->maxmip = !texmips ? 0 : tex->mipcount-1;
#else
    drawtex->maxmip = 0;
#endif

    mipwidth = width;
    mipheight = height;
    for (j = 0; j <= drawtex->maxmip; j++)
    {
        drawmip_t *mip = &drawtex->mip[j];

        mip->height = mipheight;
        mip->data = tex->data[j];
        mip->drawcol = (mipheight & (mipheight - 1)) ? drawcolnpo2 : drawcol;
        mipwidth >>= 1, mipheight >>= 1;
        if (mipwidth < 1)
            mipwidth = 1;
        if (mipheight < 1)
            mipheight = 1;

#ifdef USE_DECALS
        // decals stuff
        mip->lastcol = -1;
        mip->columncache = columncache;
        columncache += mipheight;

        mip->numdecals = tex->decals & 0x3;
        if (mip->numdecals && R_InTexCache(&r_texcache, mip->data)) {
            mip->numdecals = 0;
            continue;
        }
        mip->decals = &decals[tex->decals >> 2];
#endif
    }
}

void R_SegCommands(void)
{
    int i, segcount;
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

    D_memset(toptex, 0, sizeof(*toptex));
    D_memset(bottomtex, 0, sizeof(*bottomtex));

    I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

    I_GetThreadLocalVar(DOOMTLS_COLUMNCACHE, toptex->columncache);
    bottomtex->columncache = toptex->columncache + COLUMN_CACHE_SIZE;

    extralight = vd.extralight;

    segcount = vd.lastwallcmd - vd.viswalls;
    for (i = 0; i < segcount; i++)
    {
        int seglight;
        unsigned actionbits;
        viswall_t* segl = vd.viswalls + i;

#ifdef MARS
        if (*addedsegs == -1)
            return; // the other cpu is done drawing segments, so should we
        while (*readysegs <= i)
            continue;
#endif
        if (segl->start > segl->stop)
            continue;

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

#ifdef SIMPLELIGHT
        if (vd.fixedcolormap)
        {
#ifndef SIMPLELIGHT
            lseg.lightmin = 
#endif
            lseg.lightmax = vd.fixedcolormap;
        }
        else
        {
            seglight = (segl->seglightlevel + extralight) & 0xff;
#ifndef SIMPLELIGHT
            lseg.lightmin = 
#endif
            lseg.lightmax = HWLIGHT((unsigned)seglight);
        }
#else
        if (vd.fixedcolormap)
        {
            lseg.lightmin = lseg.lightmax = vd.fixedcolormap;
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
                lseg.lightcoef = ((unsigned)(lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);

            if (lseg.lightcoef != 0)
            {
                lseg.lightsub = 160 * lseg.lightcoef;
                lseg.lightcoef <<= LIGHTZSHIFT;
                lseg.lightmin <<= FRACBITS;
                lseg.lightmax <<= FRACBITS;
            }
            else
            {
                lseg.lightcoef = 0;
                lseg.lightmin = lseg.lightmax = HWLIGHT((unsigned)lseg.lightmax);
            }
        }
#endif

        if (actionbits & AC_TOPTEXTURE)
        {
            R_SetupDrawTexture(toptex, &textures[UPPER8(segl->tb_texturenum)],
                segl->t_texturemid, segl->t_topheight, segl->t_bottomheight);
            lseg.first--;
        }

        if (actionbits & AC_BOTTOMTEXTURE)
        {
            R_SetupDrawTexture(bottomtex, &textures[LOWER8(segl->tb_texturenum)],
                segl->b_texturemid, segl->b_topheight, segl->b_bottomheight);
            lseg.last++;
        }

        R_DrawSeg(&lseg, clipbounds);

#if MIPLEVELS > 1
        if (1)
        {
            if (lseg.maxmip >= MIPLEVELS)
                lseg.maxmip = MIPLEVELS-1;
            SETLOWER8(segl->newmiplevels, lseg.minmip);
            SETUPPER8(segl->newmiplevels, lseg.maxmip);
        }
        else
#endif
        {
#ifndef FLOOR_OVER_FLOOR
            segl->newmiplevels = 0;
#endif
        }

post_draw:
        if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
        {
            unsigned w = segl->stop - segl->start + 1;
            unsigned short *src = segl->clipbounds + segl->start, *dst = clipbounds + segl->start;

            if ((actionbits & (AC_NEWFLOOR|AC_NEWCEILING)) == (AC_NEWFLOOR|AC_NEWCEILING)) {
                --src;
                do {
                    *dst++ = *++src;
                } while (--w > 0);
            }
            else {
                int8_t *psrc = (int8_t *)src;
                int8_t *pdst = (int8_t *)dst;
                if (actionbits & AC_NEWFLOOR) {
                    psrc++, pdst++;
                }                 
                do {
                    *pdst = *psrc, psrc += 2, pdst += 2;
                } while (--w > 0);
            }
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
    viswall_t *segl;

    for (segl = vd.viswalls; segl < vd.lastwallcmd; segl++)
    {
        if (segl->actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[UPPER8(segl->tb_texturenum)];
            Mars_ClearCacheLines(tex->data, (sizeof(tex->data)+31)/16);
        }
        if (segl->actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[LOWER8(segl->tb_texturenum)];
            Mars_ClearCacheLines(tex->data, (sizeof(tex->data)+31)/16);
        }
        if (segl->actionbits & AC_MIDTEXTURE)
        {
            texture_t* tex = &textures[segl->m_texturenum];
            Mars_ClearCacheLines(tex->data, (sizeof(tex->data)+31)/16);
        }

        if (segl->actionbits & AC_ADDFLOOR)
        {
            flattex_t *flat = &flatpixels[LOWER8(segl->floorceilpicnum)];
            Mars_ClearCacheLines(flat->data, (sizeof(flat->data)+31)/16);
        }
        if (segl->actionbits & AC_ADDCEILING)
        {
            flattex_t *flat = &flatpixels[UPPER8(segl->floorceilpicnum)];
            Mars_ClearCacheLines(flat->data, (sizeof(flat->data)+31)/16);
        }
    }

    R_SegCommands();
}

#endif

