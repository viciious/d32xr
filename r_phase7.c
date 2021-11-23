/*
  CALICO

  Renderer phase 7 - Visplanes
*/

#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif

#define LIGHTCOEF (0x7ffff << SLOPEBITS)

typedef struct
{
    visplane_t* pl;
    fixed_t height;
    angle_t angle;
    fixed_t x, y;
    unsigned  lightmin, lightmax, lightsub;
    fixed_t basexscale, baseyscale;
    int	pixelcount;

#ifdef MARS
    inpixel_t* ds_source;
#else
    pixel_t* ds_source;
#endif
} localplane_t;

static void R_MapPlane(localplane_t* lpl, int y, int x, int x2) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_PlaneLoop(localplane_t* lpl) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_DrawPlanes2(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

static void R_LockPln(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_UnlockPln(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static visplane_t* R_GetNextPlane(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

void R_DrawPlanes(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

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
    unsigned light;
    boolean gradientlight = lpl->lightmin != lpl->lightmax;

    remaining = x2 - x + 1;

    if (!remaining)
        return; // nothing to draw (shouldn't happen)

    FixedMul2(distance, lpl->height, yslope[y]);

    if (gradientlight)
    {
#if defined(MARS)
        SH2_DIVU_DVSR = distance;   // set 32-bit divisor
        SH2_DIVU_DVDNT = LIGHTCOEF; // set high bits of the 64-bit dividend, start divide
#endif
    }

    FixedMul2(length, distance, distscale[x]);
    angle = (lpl->angle + xtoviewangle[x]) >> ANGLETOFINESHIFT;

    FixedMul2(xfrac, (finecosine(angle)), length);
    xfrac = lpl->x + xfrac;
    FixedMul2(yfrac, (finesine(angle)), length);
    yfrac = lpl->y - yfrac;

    FixedMul2(xstep, distance, lpl->basexscale);
    FixedMul2(ystep, lpl->baseyscale, distance);

    if (gradientlight)
    {
#ifdef MARS
        light = SH2_DIVU_DVDNT;
#else
        light = LIGHTCOEF / distance;
#endif

        if (light <= lpl->lightsub)
            light = lpl->lightmin;
        else
        {
            light -= lpl->lightsub;
            if (light < lpl->lightmin)
                light = lpl->lightmin;
            else if (light > lpl->lightmax)
                light = lpl->lightmax;
        }

        // transform to hardware value
        light = HWLIGHT(light);
    }
    else
    {
        light = lpl->lightmax;
    }

    drawspan(y, x, x2, light, xfrac, yfrac, xstep, ystep, lpl->ds_source);
    lpl->pixelcount += x2 - x + 1;
}

//
// Determine the horizontal spans of a single visplane
//
static void R_PlaneLoop(localplane_t *lpl)
{
   unsigned pl_x, pl_stopx;
   unsigned short *pl_openptr;
   unsigned t1, t2, b1, b2, pl_oldtop, pl_oldbottom;
   unsigned short spanstart[SCREENHEIGHT];
   visplane_t* pl = lpl->pl;

   pl_x       = pl->minx;
   pl_stopx   = pl->maxx;

   // see if there is any open space
   if(pl_x > pl_stopx)
      return; // nothing to map

   pl_stopx += 2;
   pl_openptr = &pl->open[pl_x - 1];

   t1 = *pl_openptr++;
   b1 = t1 & 0xff;
   t1 >>= 8;
   t2 = *pl_openptr;
  
   do
   {
      int x, x2;

      b2 = t2 & 0xff;
      t2 >>= 8;

      pl_oldtop = t2;
      pl_oldbottom = b2;

      ++pl_openptr;

      x2 = pl_x - 1;

      // top diffs
      if(t1 != pl_oldtop)
      {
         while(t1 < t2 && t1 <= b1)
         {
            x = spanstart[t1];
            R_MapPlane(lpl, t1, x, x2);
            ++t1;
         }

         while(t2 < t1 && t2 <= b2)
         {
            // top dif spanstarts
            spanstart[t2] = pl_x;
            ++t2;
         }
      }

      // bottom diffs
      if(b1 != b2)
      {
         while(b1 > b2 && b1 >= t1)
         {
            x = spanstart[b1];
            R_MapPlane(lpl, b1, x, x2);
            --b1;
         }

         while(b2 > b1 && b2 >= t2)
         {
            // bottom dif spanstarts
            spanstart[b2] = pl_x;
            --b2;
         }
      }

      ++pl_x;
      b1 = pl_oldbottom;
      t1 = pl_oldtop;
      t2 = *pl_openptr;
   }
   while(pl_x != pl_stopx);
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

static visplane_t *R_GetNextPlane(void)
{
    int p;
    visplane_t *pl;

    R_LockPln();

    p = pl_next;
    pl_next = p + 1;

    R_UnlockPln();

    pl = visplanes + p;
    if (pl >= lastvisplane)
        return NULL;
    return pl;
}

static void R_DrawPlanes2(void)
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

    extralight = vd.extralight;

    while ((pl = R_GetNextPlane()) != NULL)
    {
        if (pl->minx > pl->maxx)
            continue;

        lpl.pl = pl;
        lpl.pixelcount = 0;
        lpl.ds_source = flatpixels[pl->flatnum];
        lpl.height = (unsigned)D_abs(pl->height) << FIXEDTOHEIGHT;

        lpl.lightmax = pl->lightlevel + extralight;
        if (lpl.lightmax > 255)
            lpl.lightmax = 255;
        lpl.lightmin = lpl.lightmax;

        if (detailmode == detmode_high)
        {
#ifdef MARS
            int light = lpl.lightmax;
            if (light <= 160 + extralight)
                light = light - (light >> 1);
#else
            int light = lpl.lightmax;
            light = light - ((255 - light) << 1);
#endif
            if (light < MINLIGHT)
                light = MINLIGHT;
            lpl.lightmin = (unsigned)light;
        }

        if (lpl.lightmin != lpl.lightmax)
        {
            lpl.lightsub = 160 * (lpl.lightmax - lpl.lightmin) / (800 - 160);
        }
        else
        {
            lpl.lightmin = HWLIGHT(lpl.lightmax);
            lpl.lightmax = lpl.lightmin;
        }

        R_PlaneLoop(&lpl);

        pl->pixelcount = lpl.pixelcount;
    }
}

#ifdef MARS
void Mars_R_PrepPlanes(void)
{
    Mars_ClearCacheLines((intptr_t)&lastvisplane & ~15, 1);
    Mars_ClearCacheLines((intptr_t)&visplanes & ~15, 1);

    visplane_t* pl;
    for (pl = visplanes + 1; pl < lastvisplane; pl++)
    {
        pl->pixelcount = 0;
        // see if there is any open space
        if (pl->minx > pl->maxx)
            continue; // nothing to map
        pl->open[pl->maxx + 1] = OPENMARK;
        pl->open[pl->minx - 1] = OPENMARK;
    }

    pl_next = 1;
}

void Mars_Sec_R_DrawPlanes(void)
{
    R_DrawPlanes2();
}
#endif

//
// Render all visplanes
//
void R_DrawPlanes(void)
{
#ifdef MARS
    Mars_R_PrepPlanes();

    Mars_R_BeginDrawPlanes();

    R_DrawPlanes2();

    Mars_R_EndDrawPlanes();
#else
    R_DrawPlanes2();
#endif
}

// EOF

