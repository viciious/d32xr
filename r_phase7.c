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
#ifdef GRADIENTLIGHT
    unsigned  lightmin, lightmax, lightsub;
#else
    int     lightmax;
#endif
    fixed_t basexscale, baseyscale;
    int	pixelcount;

#ifdef MARS
    inpixel_t* ds_source;
#else
    pixel_t* ds_source;
#endif
} localplane_t;

static unsigned short numplanes;
static short* sortedplanes;

static void R_MapPlane(localplane_t* lpl, int y, int x, int x2) ATTR_DATA_CACHE_ALIGN;
static void R_PlaneLoop(localplane_t* lpl, const int mask) __attribute__((always_inline));
static void R_DrawPlanes2(const int mask) __attribute__((always_inline));
void R_DrawPlanes(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

//
// Render the horizontal spans determined by R_PlaneLoop
//
static void R_MapPlane(localplane_t *lpl, int y, int x, int x2)
{
   int remaining;
   volatile fixed_t distance;
   fixed_t length, xfrac, yfrac, xstep, ystep;
   angle_t angle;
   volatile unsigned light;

   remaining = x2 - x + 1;

   if (!remaining)
       return; // nothing to draw (shouldn't happen)

   FixedMul2(distance, lpl->height, yslope[y]);

#if defined(MARS) && defined(GRADIENTLIGHT)
   SH2_DIVU_DVSR = distance;   // set 32-bit divisor
   SH2_DIVU_DVDNT = LIGHTCOEF; // set high bits of the 64-bit dividend, start divide
#endif

   FixedMul2(length, distance, distscale[x]);
   angle = (lpl->angle + xtoviewangle[x]) >> ANGLETOFINESHIFT;

   FixedMul2(xfrac, (finecosine(angle)), length);
   xfrac = lpl->x + xfrac;
   FixedMul2(yfrac, (finesine(angle)), length);
   yfrac = lpl->y - yfrac;

   FixedMul2(xstep, distance, lpl->basexscale);
   FixedMul2(ystep, lpl->baseyscale, distance);

#ifdef GRADIENTLIGHT
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
#else
   light = lpl->lightmax;
#endif

   I_DrawSpan(y, x, x2, light, xfrac, yfrac, xstep, ystep, lpl->ds_source);
}

//
// Determine the horizontal spans of a single visplane
//
static void R_PlaneLoop(localplane_t *lpl, const int mask)
{
   unsigned pl_x, pl_stopx;
   unsigned short *pl_openptr;
   unsigned short t1, t2, b1, b2, pl_oldtop, pl_oldbottom;
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
#ifdef MARS
            if ((t1 & 1) == mask)
#endif
                R_MapPlane(lpl, t1, x, x2);
#ifdef MARS
            if (mask == 0)
#endif
            lpl->pixelcount += x2 - x + 1;
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
#ifdef MARS
            if ((b1 & 1) == mask)
#endif
                R_MapPlane(lpl, b1, x, x2);
#ifdef MARS
            if (mask == 0)
#endif
                lpl->pixelcount += x2 - x + 1;
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

static void R_DrawPlanes2(const int cpu)
{
    unsigned i;
    angle_t angle;
    localplane_t lpl;

    lpl.x = vd.viewx;
    lpl.y = -vd.viewy;

    lpl.angle = vd.viewangle;
    angle = (lpl.angle - ANG90) >> ANGLETOFINESHIFT;

    lpl.basexscale = FixedDiv(finecosine(angle), centerXFrac);
    lpl.baseyscale = -FixedDiv(finesine(angle), centerXFrac);

    for (i = 0; i < numplanes; i++)
    {
        visplane_t* pl = visplanes + sortedplanes[(i<<1) + 1];

        if (pl->minx > pl->maxx)
            continue;
 
#ifdef MARS
        if (cpu == 1)
            Mars_ClearCacheLines((intptr_t)&flatpixels[pl->flatnum] & ~15, 1);
#endif

        lpl.pl = pl;
        lpl.pixelcount = 0;
        lpl.ds_source = flatpixels[pl->flatnum];
        lpl.height = (unsigned)D_abs(pl->height) << FIXEDTOHEIGHT;
        lpl.lightmax = pl->lightlevel + extralight;
        if (lpl.lightmax > 255)
            lpl.lightmax = 255;

#ifdef GRADIENTLIGHT
#ifdef MARS
        unsigned light = lpl.lightmax;
        if (light <= 160)
            light = light - (light >> 1);
#else
        int light = lpl.lightmax;
        light = light - ((255 - light) << 1);
#endif
        if (light < MINLIGHT)
            light = MINLIGHT;
        if (light > lpl.lightmax)
            light = lpl.lightmax;
        lpl.lightmin = (unsigned)light;
        lpl.lightsub = 160 * (lpl.lightmax - lpl.lightmin) / (800 - 160);
#endif

#ifndef GRADIENTLIGHT
        lpl.lightmax = HWLIGHT(lpl.lightmax);
#endif

        R_PlaneLoop(&lpl, cpu);

        if (cpu == 0)
        {
            R_AddPixelsToTexCache(&r_flatscache, pl->flatnum, lpl.pixelcount);
        }
    }
}

//
// Render all visplanes
//
#ifdef MARS
void Mars_Slave_R_DrawPlanes(void)
{
    Mars_ClearCacheLines((intptr_t)&numplanes & ~15, 1);
    Mars_ClearCacheLines((intptr_t)&visplanes & ~15, 1);

    // commented out because visplanes are in VRAM anyway
    //Mars_ClearCacheLines((intptr_t)visplanes, (numplanes * sizeof(visplane_t) + 15) / 16);

    Mars_ClearCacheLines((intptr_t)&sortedplanes & ~15, 1);
    Mars_ClearCacheLines((intptr_t)sortedplanes & ~15, (numplanes * sizeof(*sortedplanes) + 15) / 16);

    R_DrawPlanes2(1);
}
#endif

//
// Render all visplanes
//
void R_DrawPlanes(void)
{
    int j;
    visplane_t* pl;
    short sortbuf[MAXVISPLANES * 2] __attribute__((aligned(4)));

    for (pl = visplanes + 1; pl < lastvisplane; pl++)
    {
        // see if there is any open space
        if (pl->minx > pl->maxx)
            continue; // nothing to map
        pl->open[pl->maxx + 1] = OPENMARK;
        pl->open[pl->minx - 1] = OPENMARK;
    }

    j = 0;
    numplanes = 0;
    for (pl = visplanes + 1; pl < lastvisplane; pl++)
    {
        if (pl->minx <= pl->maxx)
        {
            sortbuf[j + 0] = pl->flatnum;
            sortbuf[j + 1] = pl - visplanes;
            j += 2;
            numplanes++;
        }
    }

    D_isort((int*)sortbuf, numplanes);
    sortedplanes = (short*)sortbuf;

#ifdef MARS
    Mars_R_BeginDrawPlanes();

    R_DrawPlanes2(0);

    Mars_R_EndDrawPlanes();

#else
    R_DrawPlanes2(0);
#endif
}

// EOF

