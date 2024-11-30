/*
  CALICO

  Renderer phase 7 - Visplanes
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif

#define LIGHTCOEF (0x7ffffu << SLOPEBITS)
#define MIPSCALE (1<<24)
#define FLATSIZE 64

struct localplane_s;

typedef void (*mapplane_fn)(struct localplane_s* lpl, int, int, int);

typedef struct localplane_s
{
    visplane_t* pl;
    fixed_t height;
    angle_t angle;
    fixed_t x, y;
    int lightmin, lightmax, lightsub, lightcoef;
    fixed_t basexscale, baseyscale;

#ifdef MARS
    inpixel_t* ds_source[MIPLEVELS];
#else
    pixel_t* ds_source[MIPLEVELS];
#endif
#if MIPLEVELS > 1
    unsigned maxmip;
    int mipsize[MIPLEVELS];
#endif
    mapplane_fn mapplane;
    boolean lowres;
} localplane_t;

static void R_MapPlane(localplane_t* lpl, int y, int x, int x2) ATTR_DATA_CACHE_ALIGN;
static void R_MapPotatoPlane(localplane_t* lpl, int y, int x, int x2) ATTR_DATA_CACHE_ALIGN;
static void R_PlaneLoop(localplane_t* lpl) ATTR_DATA_CACHE_ALIGN;
static void R_DrawPlanes2(void) ATTR_DATA_CACHE_ALIGN;

static int  R_TryLockPln(void) ATTR_DATA_CACHE_ALIGN;
static void R_LockPln(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockPln(void) ATTR_DATA_CACHE_ALIGN;
static visplane_t* R_GetNextPlane(uint16_t *sortedvisplanes) ATTR_DATA_CACHE_ALIGN;

void R_DrawPlanes(void) __attribute__((noinline));

static char pl_lock = 0;
#ifdef MARS
#define pl_next MARS_SYS_COMM6
#else
static int pl_next = 0;
#endif

//
// Render the horizontal spans determined by R_PlaneLoop
//
static void R_MapPlane(localplane_t* lpl, int y, int x, int x2)
{
    int remaining;
    fixed_t distance;
    fixed_t ds;
    fixed_t length, xfrac, yfrac, xstep, ystep;
    angle_t angle;
    int light;
    unsigned scale;
    unsigned miplevel, mipsize;

    remaining = x2 - x + 1;

    if (remaining <= 0)
        return; // nothing to draw (shouldn't happen)

    distance = FixedMul(lpl->height, yslope[y]);

#ifdef MARS
    int32_t t, divunit;
    __asm volatile (
        "mov #-128, %1\n\t"
        "add %1, %1 /* r0 is now 0xFFFFFF00 */ \n\t"
        "mov.l %2, @(0, %1) /* set 32-bit divisor */ \n\t"
        "mov #0, %0\n\t"
        "mov.l %3, @(16, %1) /* set high bits of the 64-bit dividend */ \n\t"
        "mov.l %0, @(20, %1) /* set low  bits of the 64-bit dividend, start divide */\n\t"
        : "=&r" (t), "=&r" (divunit) : "r" (distance), "r"(lpl->lightcoef));
#endif

    ds = distscale[x];
    ds <<= 1;
    length = FixedMul(distance, ds);

    xstep = FixedMul(distance, lpl->basexscale);
    ystep = FixedMul(distance, lpl->baseyscale);

#if MIPLEVELS > 1
    miplevel = (unsigned)distance / MIPSCALE;
    if (miplevel > lpl->maxmip)
        miplevel = lpl->maxmip;
    mipsize = lpl->mipsize[miplevel];
#else
    miplevel = 0;
    mipsize = FLATSIZE;
#endif

    angle = (lpl->angle + (xtoviewangle[x]<<FRACBITS)) >> ANGLETOFINESHIFT;

    xfrac = FixedMul(finecosine(angle), length);
    xfrac = lpl->x + xfrac;
    yfrac = FixedMul(finesine(angle), length);
    yfrac = lpl->y - yfrac;
#ifdef MARS
    yfrac *= FLATSIZE;
#endif

#if MIPLEVELS > 1
    if (miplevel > 0) {
        unsigned m = miplevel;
        do {
            xfrac >>= 1, xstep >>= 1;
            yfrac >>= 2, ystep >>= 2;
        } while (--m);
    }
#endif

    if (lpl->lightcoef != 0)
    {
#ifdef MARS
        __asm volatile (
            "mov #-128, %0\n\t"
            "add %0, %0 /* %0 is now 0xFFFFFF00 */ \n\t"
            "mov.l @(20,%0), %0 /* get 32-bit quotient */ \n\t"
            : "=r" (scale));
#else
        scale = lpl->lightcoef / distance;
#endif

        light = lpl->lightsub;
        light -= scale;

        if (light < lpl->lightmin)
            light = lpl->lightmin;
        if (light > lpl->lightmax)
            light = lpl->lightmax;
        light = (unsigned)light >> FRACBITS;
        light <<= 8;
    }
    else
    {
        light = lpl->lightmax;
    }

    if (lpl->lowres)
    {
        x >>= 1;
        x2 >>= 1;
    }

    drawspan(y, x, x2, light, xfrac, yfrac, xstep, ystep, lpl->ds_source[miplevel], mipsize);
}

static void R_MapPotatoPlane(localplane_t* lpl, int y, int x, int x2)
{
    if (x2 < x)
        return; // nothing to draw (shouldn't happen)
    drawspan(y, x, x2, lpl->lightmax, 0, 0, 0, 0, lpl->ds_source[0], 64);
}

//
// Determine the horizontal spans of a single visplane
//
static void R_PlaneLoop(localplane_t *lpl)
{
   int pl_x, pl_stopx;
   uint16_t *pl_openptr;
   int t1, t2, b1, b2, pl_oldtop, pl_oldbottom;
   int16_t spanstart[SCREENHEIGHT];
   visplane_t* pl = lpl->pl;
   const mapplane_fn mapplane = lpl->mapplane;

   pl_x       = pl->minx;
   pl_stopx   = pl->maxx;

   // see if there is any open space
   if(pl_x > pl_stopx)
      return; // nothing to map

   pl_openptr = &pl->open[pl_x];

   t1 = OPENMARK;
   b1 = t1 & 0xff;
   t1 = (unsigned)t1 >> 8;
   t2 = *pl_openptr;
  
   do
   {
      int x2;

      b2 = t2 & 0xff;
      t2 = (unsigned)t2 >> 8;

      pl_oldtop = t2;
      pl_oldbottom = b2;

      x2 = pl_x - 1;

      // top diffs
      while (t1 < t2 && t1 <= b1)
      {
          mapplane(lpl, t1, spanstart[t1], x2);
          ++t1;
      }

      // bottom diffs
      while (b1 > b2 && b1 >= t1)
      {
          mapplane(lpl, b1, spanstart[b1], x2);
          --b1;
      }

      if (pl_x == pl_stopx + 1)
          break;

      while (t2 < t1 && t2 <= b2)
      {
          // top dif spanstarts
          spanstart[t2] = pl_x;
          ++t2;
      }

      while (b2 > b1 && b2 >= t2)
      {
          // bottom dif spanstarts
          spanstart[b2] = pl_x;
          --b2;
      }

      b1 = pl_oldbottom;
      t1 = pl_oldtop;
      t2 = pl_x++ < pl_stopx ? *++pl_openptr : OPENMARK;
   }
   while(1);
}

static int R_TryLockPln(void)
{
    int res;

    __asm volatile (\
       "tas.b %1\n\t" \
        "movt %0\n\t" \
        : "=&r" (res) \
        : "m" (pl_lock) \
    );

    return res != 0;
}

static void R_LockPln(void)
{
    while (!R_TryLockPln());
}

static void R_UnlockPln(void)
{
    pl_lock = 0;
}

static visplane_t *R_GetNextPlane(uint16_t *sortedvisplanes)
{
    int p;

    R_LockPln();

    p = pl_next;
    pl_next = p + 1;

    R_UnlockPln();

#ifdef MARS
    if (p + vd->visplanes + 1 >= vd->lastvisplane)
        return NULL;
    return vd->visplanes + sortedvisplanes[p*2+1];
#else
    visplane_t *pl = vd->visplanes + p + 1;
    return pl == vd->lastvisplane ? NULL : pl;
#endif
}

static void R_DrawPlanes2(void)
{
    angle_t angle;
    localplane_t lpl;
    visplane_t* pl;
    int extralight;
    fixed_t basexscale, baseyscale;
    fixed_t basexscale2, baseyscale2;
    fixed_t cXf = centerXFrac;

#ifdef MARS
    Mars_ClearCacheLine(&vd->lastvisplane);
    Mars_ClearCacheLine(&vd->gsortedvisplanes);
    Mars_ClearCacheLines(vd->gsortedvisplanes, ((vd->lastvisplane - vd->visplanes - 1) * sizeof(*vd->gsortedvisplanes) + 31) / 16);
#endif

    if (vd->gsortedvisplanes == NULL)
        return;

    cXf = centerXFrac;
    lpl.lowres = false;
    if (!lowres && detailmode == detmode_lowres)
    {
        cXf >>= 1;
        lpl.lowres = true;
    }

    angle = (vd->viewangle - ANG90) >> ANGLETOFINESHIFT;
    basexscale = FixedDiv(finecosine(angle), cXf);
    baseyscale = FixedDiv(finesine(angle), cXf);

    angle = (vd->viewangle        ) >> ANGLETOFINESHIFT;
    basexscale2 = FixedDiv(finecosine(angle), cXf);
    baseyscale2 = FixedDiv(finesine(angle), cXf);

    lpl.mapplane = detailmode == detmode_potato ? R_MapPotatoPlane : R_MapPlane;
    extralight = vd->extralight;

    while ((pl = R_GetNextPlane((uint16_t *)vd->gsortedvisplanes)) != NULL)
    {
        int light;
        int flatnum;
        boolean rotated = false;

#ifdef MARS
        Mars_ClearCacheLines(pl, (sizeof(visplane_t) + 31) / 16);
#endif

        if (pl->minx > pl->maxx)
            continue;

        flatnum = pl->flatandlight&0xffff;

        if (flatnum >= col2flat)
            I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps2);
        else
            I_SetThreadLocalVar(DOOMTLS_COLORMAP, dc_colormaps);

        lpl.pl = pl;
        lpl.ds_source[0] = flatpixels[flatnum].data[0];
        if (rotated)
        {
            lpl.x = -vd->viewy;
            lpl.y = -vd->viewx;
            lpl.angle = vd->viewangle + ANG90;
            lpl.basexscale =  basexscale2;
            lpl.baseyscale = -baseyscale2;
        }
        else
        {
            lpl.x =  vd->viewx;
            lpl.y = -vd->viewy;
            lpl.angle = vd->viewangle;
            lpl.basexscale =  basexscale;
            lpl.baseyscale = -baseyscale;
        }
#ifdef MARS
        lpl.baseyscale *= FLATSIZE;
#endif

#if MIPLEVELS > 1
        lpl.mipsize[0] = FLATSIZE;
        lpl.maxmip = texmips ? MIPLEVELS-1 : 0;
        if (lpl.maxmip > 0)
        {
            int j;
            int mipsize = FLATSIZE>>1;
            for (j = 1; j <= (int)lpl.maxmip; j++)
            {
                lpl.ds_source[j] = flatpixels[flatnum].data[j];
                lpl.mipsize[j] = mipsize;
                mipsize >>= 1;
            }
        }
#endif
        lpl.height = (unsigned)D_abs(pl->height);

        if (vd->fixedcolormap)
        {
            lpl.lightmin = lpl.lightmax = vd->fixedcolormap;
            lpl.lightcoef = 0;
        }
        else
        {
            light = ((unsigned)pl->flatandlight>>16);
            lpl.lightmax = light;
            lpl.lightmax += extralight;
            if (lpl.lightmax > 255)
                lpl.lightmax = 255;

#ifdef MARS
            light = light - ((255 - light - light/2) << 1);
#else
            light = light - ((255 - light) << 1);
#endif
            light += extralight;
            if (light < MINLIGHT)
                light = MINLIGHT;
            if (light > lpl.lightmax)
                light = lpl.lightmax;
            lpl.lightmin = light;

            lpl.lightsub = 0;
            if (lpl.lightmin != lpl.lightmax)
            {
                lpl.lightcoef = (unsigned)(lpl.lightmax - lpl.lightmin) << 8;
                lpl.lightsub = (((lpl.lightmax - lpl.lightmin) * 160) << FRACBITS) / (800 - 160);
            }

            if (detailmode != detmode_potato && lpl.lightsub != 0)
            {
                int t;

                lpl.lightmin <<= FRACBITS;
                lpl.lightmax <<= FRACBITS;

                // perform HWLIGHT calculations on the coefficients
                // to reduce the number of calculations we will do
                // later per column
                t = lpl.lightmax;
                lpl.lightmax = -lpl.lightmin;
                lpl.lightmin = -t;

                lpl.lightsub += 255 * FRACUNIT;
                lpl.lightmax += 255 * FRACUNIT;
                lpl.lightmin += 255 * FRACUNIT;

                lpl.lightcoef >>= 3;
                lpl.lightsub >>= 3;
                lpl.lightmax >>= 3;
                lpl.lightmin >>= 3;

                if (lpl.lightmin < 0)
                    lpl.lightmin = 0;
                if (lpl.lightmax > 31 * FRACUNIT)
                    lpl.lightmax = 31 * FRACUNIT;
            }
            else
            {
                lpl.lightcoef = 0;
                lpl.lightmin = lpl.lightmax = HWLIGHT((unsigned)lpl.lightmax);
            }
        }

        R_PlaneLoop(&lpl);
    }
}

#ifdef MARS

void Mars_Sec_R_DrawPlanes(void)
{
    R_DrawPlanes2();
}

// sort visplanes by flatnum so that texture data 
// has a better chance to stay in the CPU cache

static void Mars_R_SortPlanes(void)
{
    int i, numplanes;
    visplane_t* pl;
    uint16_t *sortbuf = (uint16_t *)vd->gsortedvisplanes;

    i = 0;
    numplanes = 0;
    for (pl = vd->visplanes + 1; pl < vd->lastvisplane; pl++)
    {
        // composite sort key: 1b - sign bit, 7b - negated span length, 8b - flat
        unsigned key = (unsigned)(pl->maxx - pl->minx - 1) >> 4;
        if (key > 127) {
            key = 127;
        }
        // to minimize pipeline stalls, the larger planes must be drawn first, hence length negation
        key = (127 - key) << 8;
        key |= (pl->flatandlight & 0xFF);
        sortbuf[i + 0] = key;
        sortbuf[i + 1] = ++numplanes;
        i += 2;
    }

    D_isort(vd->gsortedvisplanes, numplanes);
}

static void R_PreDrawPlanes(void)
{
    int numplanes;

    Mars_ClearCacheLine(&vd->lastvisplane);
    Mars_ClearCacheLine(&vd->gsortedvisplanes);

    // check to see if we still need to fill the sorted planes list
    numplanes = vd->lastvisplane - vd->visplanes; // visplane 0 is a dummy plane
    if (numplanes > 1) 
    {
        Mars_ClearCacheLines(vd->visplanes, (numplanes * sizeof(visplane_t) + 31) / 16);

        if (vd->gsortedvisplanes == NULL)
        {
            vd->gsortedvisplanes = (int *)vd->viswallextras;

            Mars_R_SortPlanes();
        }
    }
}

void Mars_Sec_R_PreDrawPlanes(void)
{
    if (R_TryLockPln())
    {
        R_PreDrawPlanes();
        R_UnlockPln();
    }
}

#endif

//
// Render all visplanes
//
void R_DrawPlanes(void)
{
#ifdef MARS
    if (R_TryLockPln())
    {
        R_PreDrawPlanes();
        R_UnlockPln();
    }
    else
    {
        // the secondary CPU is already on it, take our hands off the bus
        Mars_R_SecWait();
    }

    Mars_R_BeginDrawPlanes();

    R_DrawPlanes2();

    Mars_R_EndDrawPlanes();
#else
    R_DrawPlanes2();
#endif
}

// EOF

