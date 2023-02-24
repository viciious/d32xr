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

typedef struct
{
    visplane_t* pl;
    fixed_t height;
    angle_t angle;
    fixed_t x, y;
    int lightmin, lightmax, lightsub;
    fixed_t basexscale, baseyscale;
    int	pixelcount;
    unsigned mipcount;

#ifdef MARS
    inpixel_t* ds_source[MIPLEVELS];
#else
    pixel_t* ds_source[MIPLEVELS];
#endif
    int mipsize[MIPLEVELS];
} localplane_t;

static void R_MapPlane(localplane_t* lpl, int y, int x, int x2) ATTR_DATA_CACHE_ALIGN;
static void R_PlaneLoop(localplane_t* lpl) ATTR_DATA_CACHE_ALIGN;
static void R_DrawPlanes2(uint16_t *sortedvisplanes) ATTR_DATA_CACHE_ALIGN;

static void R_LockPln(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockPln(void) ATTR_DATA_CACHE_ALIGN;
static visplane_t* R_GetNextPlane(uint16_t *sortedvisplanes) ATTR_DATA_CACHE_ALIGN;

void R_DrawPlanes(void) ATTR_DATA_CACHE_ALIGN;

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
    fixed_t length, xfrac, yfrac, xstep, ystep;
    angle_t angle;
    int light;
    unsigned scale;
    unsigned miplevel;

    remaining = x2 - x + 1;

    if (remaining <= 0)
        return; // nothing to draw (shouldn't happen)

    FixedMul2(distance, lpl->height, yslope[y]);

#ifdef MARS
    __asm volatile (
        "mov #-128, r0\n\t"
        "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
        "mov.l %0, @(0,r0) /* set 32-bit divisor */ \n\t"
        "mov.l %1, @(4,r0) /* start divide */\n\t"
        : : "r" (distance), "r" (LIGHTCOEF) : "r0");
#endif

    FixedMul2(length, distance, distscale[x]);
    angle = (lpl->angle + xtoviewangle[x]) >> ANGLETOFINESHIFT;

    FixedMul2(xfrac, (finecosine(angle)), length);
    xfrac = lpl->x + xfrac;
    FixedMul2(yfrac, (finesine(angle)), length);
    yfrac = lpl->y - yfrac;
#ifdef MARS
    yfrac *= 64;
#endif

    FixedMul2(xstep, distance, lpl->basexscale);
    FixedMul2(ystep, distance, lpl->baseyscale);

#ifdef MARS
    __asm volatile (
        "mov #-128, r0\n\t"
        "add r0, r0 /* r0 is now 0xFFFFFF00 */ \n\t"
        "mov.l @(20,r0), %0 /* get 32-bit quotient */ \n\t"
        : "=r" (scale) : : "r0");
#else
    scale = LIGHTCOEF / distance;
#endif

    miplevel = (unsigned)distance / MIPSCALE;
    if (miplevel > lpl->mipcount)
        miplevel = lpl->mipcount;

    if (lpl->lightmin != lpl->lightmax)
    {
        light = scale;
        light -= lpl->lightsub;
        if (light < lpl->lightmin)
            light = lpl->lightmin;
        else if (light > lpl->lightmax)
            light = lpl->lightmax;

        // transform to hardware value
        light = HWLIGHT(light);
    }
    else
    {
        light = lpl->lightmax;
    }

    if (miplevel > 0) {
        unsigned m = miplevel;
        do {
            xfrac >>= 1;
            xstep >>= 1;
            yfrac >>= 2;
            ystep >>= 2;
        } while (--m);
    } else {
        lpl->pixelcount += x2 - x + 1;
    }

    drawspan(y, x, x2, light, xfrac, yfrac, xstep, ystep, lpl->ds_source[miplevel], lpl->mipsize[miplevel]);
}

//
// Determine the horizontal spans of a single visplane
//
static void R_PlaneLoop(localplane_t *lpl)
{
   int pl_x, pl_stopx;
   uint16_t *pl_openptr;
   unsigned t1, t2, b1, b2, pl_oldtop, pl_oldbottom;
   int16_t spanstart[SCREENHEIGHT];
   visplane_t* pl = lpl->pl;

   pl_x       = pl->minx;
   pl_stopx   = pl->maxx;

   // see if there is any open space
   if(pl_x > pl_stopx)
      return; // nothing to map

   pl_openptr = &pl->open[pl_x];

   t1 = OPENMARK;
   b1 = t1 & 0xff;
   t1 >>= 8;
   t2 = *pl_openptr;
  
   do
   {
      int x2;

      b2 = t2 & 0xff;
      t2 >>= 8;

      pl_oldtop = t2;
      pl_oldbottom = b2;

      x2 = pl_x - 1;

      // top diffs
      while (t1 < t2 && t1 <= b1)
      {
          R_MapPlane(lpl, t1, spanstart[t1], x2);
          ++t1;
      }

      // bottom diffs
      while (b1 > b2 && b1 >= t1)
      {
          R_MapPlane(lpl, b1, spanstart[b1], x2);
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

static void R_LockPln(void)
{
    int res;
    do {
        __asm volatile (\
        "tas.b %1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "m" (pl_lock) \
            );
    } while (res == 0);
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
    if (p + visplanes + 1 >= lastvisplane)
        return NULL;
    return visplanes + sortedvisplanes[p*2+1];
#else
    visplane_t *pl = visplanes + p + 1;
    return pl == lastvisplane ? NULL : pl;
#endif
}

static void R_DrawPlanes2(uint16_t *sortedvisplanes)
{
    angle_t angle;
    localplane_t lpl;
    visplane_t* pl;
    int extralight;

    lpl.x = vd.viewx;
    lpl.y = -vd.viewy;

    lpl.angle = vd.viewangle;
    angle = (lpl.angle - ANG90) >> ANGLETOFINESHIFT;

    lpl.basexscale = FixedDiv(finecosine(angle), centerXFrac);
    lpl.baseyscale = -FixedDiv(finesine(angle), centerXFrac);
#ifdef MARS
    lpl.baseyscale *= 64;
#endif
    extralight = vd.extralight;

    while ((pl = R_GetNextPlane(sortedvisplanes)) != NULL)
    {
        int j;
        int light;

        if (pl->minx > pl->maxx)
            continue;

        lpl.pl = pl;
        lpl.pixelcount = 0;
        if (debugmode == DEBUGMODE_NOTEXCACHE)
        {
            lpl.ds_source[0] = flatpixels[pl->flatnum].data[0];
            lpl.mipsize[0] = 64;
            lpl.mipcount = 0;
        }
        else
        {
            int mipsize = 64;
            for (j = 0; j < MIPLEVELS; j++)
            {
                lpl.ds_source[j] = flatpixels[pl->flatnum].data[j];
                lpl.mipsize[j] = mipsize;
                mipsize >>= 1;
            }
            lpl.mipcount = MIPLEVELS-1;
        }
        lpl.height = (unsigned)D_abs(pl->height) << FIXEDTOHEIGHT;

        if (vd.fixedcolormap)
        {
            lpl.lightmin = lpl.lightmax = vd.fixedcolormap;
        }
        else
        {
            light = pl->lightlevel + extralight;
            if (light < 0)
                light = 0;
            if (light > 255)
                light = 255;
            lpl.lightmax = light;
            lpl.lightmin = lpl.lightmax;

            if (detailmode == detmode_high)
            {
#ifdef MARS
                if (light <= 160 + extralight)
                    light = (light >> 1);
#else
                light = light - ((255 - light) << 1);
#endif
                if (light < MINLIGHT)
                    light = MINLIGHT;
                lpl.lightmin = (unsigned)light;
            }

            if (lpl.lightmin != lpl.lightmax)
                lpl.lightsub = 160 * (lpl.lightmax - lpl.lightmin) / (800 - 160);
            else
                lpl.lightmin = lpl.lightmax = HWLIGHT((unsigned)lpl.lightmax);
        }

        R_PlaneLoop(&lpl);

        pl->pixelcount = lpl.pixelcount;
    }
}

#ifdef MARS

static void Mars_R_SplitPlanes(void) ATTR_DATA_CACHE_ALIGN;
static void Mars_R_SortPlanes(uint16_t *sortedvisplanes) ATTR_DATA_CACHE_ALIGN;

void Mars_Sec_R_DrawPlanes(uint16_t *sortedvisplanes)
{
    Mars_ClearCacheLine(&lastvisplane);
    Mars_ClearCacheLines(visplanes, ((lastvisplane - visplanes) * sizeof(visplane_t) + 31) / 16);
    Mars_ClearCacheLines(sortedvisplanes, ((lastvisplane - visplanes - 1) * sizeof(int) + 31) / 16);
    R_DrawPlanes2(sortedvisplanes);
}

static void Mars_R_SplitPlanes(void)
{
    const int minlen = centerX;
    const int maxlen = centerX * 2;
    visplane_t *pl, *last = lastvisplane;

    for (pl = visplanes + 1; pl < last; pl++)
    {
        int start, stop;
        visplane_t* newpl;
        int numplanes;
        int newstop;

        numplanes = lastvisplane - visplanes;
        if (numplanes >= MAXVISPLANES)
            return;

        // see if there is any open space
        start = pl->minx, stop = pl->maxx;
        if (start > stop)
            continue; // nothing to map

        // split long visplane into two

        int span = stop - start + 1;
        if (span < maxlen)
            continue;

        pl->maxx = start;
        newstop = start + minlen;

        do {
            newstop = start + minlen;
            if (newstop > stop || numplanes == MAXWALLCMDS - 1)
                newstop = stop;

            newpl = lastvisplane++;
            newpl->open = pl->open;
            newpl->height = pl->height;
            newpl->flatnum = pl->flatnum;
            newpl->lightlevel = pl->lightlevel;
            newpl->minx = start + 1;
            newpl->maxx = newstop;

            numplanes++;
            start = newstop;
        } while (start < stop);
    }
}

// sort visplanes by flatnum so that texture data 
// has a better chance to stay in the CPU cache

static void Mars_R_SortPlanes(uint16_t *sortedvisplanes)
{
    int i, numplanes;
    visplane_t* pl;
    uint16_t *sortbuf = sortedvisplanes;

    i = 0;
    numplanes = 0;
    for (pl = visplanes + 1; pl < lastvisplane; pl++)
    {
        sortbuf[i + 0] = pl->flatnum;
        sortbuf[i + 1] = pl - visplanes;
        i += 2;
        numplanes++;
    }

    D_isort((int*)sortbuf, numplanes);
}

#endif

//
// Render all visplanes
//
void R_DrawPlanes(void)
{
#ifdef MARS
    int numplanes;
    __attribute__((aligned(16)))
        uint16_t sortedvisplanes[MAXVISPLANES*2];

    Mars_ClearCacheLine(&lastvisplane);
    Mars_ClearCacheLines(visplanes, ((lastvisplane - visplanes) * sizeof(visplane_t) + 31) / 16);

    numplanes = lastvisplane - visplanes;
    if (numplanes <= 1)
        return;

    Mars_R_SplitPlanes();

    Mars_R_SortPlanes(sortedvisplanes);

    Mars_R_BeginDrawPlanes(sortedvisplanes);

    R_DrawPlanes2(sortedvisplanes);

    Mars_R_EndDrawPlanes();
#else
    R_DrawPlanes2(NULL);
#endif
}

// EOF

