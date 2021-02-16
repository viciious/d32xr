/*
  CALICO

  Renderer phase 7 - Visplanes
*/

#include "r_local.h"

typedef struct
{
	fixed_t height;
	angle_t angle;
	fixed_t x, y;
	int     lightcoef;
#ifndef MARS
	int     lightmin, lightmax, lightsub;
#endif
	fixed_t basexscale, baseyscale;
	int	pixelcount;

#ifdef MARS
	inpixel_t *ds_source;
#else
	pixel_t *ds_source;
#endif
} localplane_t;

static localplane_t lpl;
static unsigned *spanstart;

//
// Render the horizontal spans determined by R_PlaneLoop
//
static void R_MapPlane(int y, int x, int x2)
{
   int remaining;
   fixed_t distance, length, xfrac, yfrac, xstep, ystep;
   angle_t angle;
   int light;

   remaining = x2 - x + 1;

   if (!remaining)
       return; // nothing to draw (shouldn't happen)

   distance = (lpl.height * yslope[y]) >> 12;
   length = (distance * distscale[x]) >> 14;
   angle = (lpl.angle + xtoviewangle[x]) >> ANGLETOFINESHIFT;

   xfrac = lpl.x + (((finecosine(angle) >> 1) * length) >> 4);
   yfrac = lpl.y - (((finesine(angle) >> 1) * length) >> 4);

   xstep = (distance * lpl.basexscale) >> 4;
   ystep = (lpl.baseyscale * distance) >> 4;

#ifdef MARS
   light = lpl.lightcoef;
#else
   light = lpl.lightcoef / distance;

   // finish light calculations
   light -= lpl.lightsub;

   if (light > lpl.lightmax)
       light = lpl.lightmax;
   if (light < lpl.lightmin)
       light = lpl.lightmin;

   // transform to hardware value
   light = -((255 - light) << 14) & 0xffffff;
#endif

   // CALICO: invoke I_DrawSpan here.
   I_DrawSpan(y, x, x2, light, xfrac, yfrac, xstep, ystep, lpl.ds_source);

   lpl.pixelcount += x2 - x + 1;
}

//
// Determine the horizontal spans of a single visplane
//
static void R_PlaneLoop(visplane_t *pl)
{
   unsigned pl_x, pl_stopx;
   unsigned short *pl_openptr;
   unsigned short t1, t2, b1, b2, pl_oldtop, pl_oldbottom;

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
      b2 = t2 & 0xff;
      t2 >>= 8;

      pl_oldtop = t2;
      pl_oldbottom = b2;

      ++pl_openptr;
      
      // top diffs
      if(t1 != pl_oldtop)
      {
         while(t1 < t2 && t1 <= b1)
         {
            R_MapPlane(t1, spanstart[t1], pl_x - 1);
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
            R_MapPlane(b1, spanstart[b1], pl_x - 1);
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

//
// Render all visplanes
//
void R_DrawPlanes(void)
{
   angle_t angle;
   visplane_t *pl;

   lpl.x =  vd.viewx;
   lpl.y = -vd.viewy;

   lpl.angle = vd.viewangle;
   angle = (lpl.angle - ANG90) >> ANGLETOFINESHIFT;

   lpl.basexscale =  (finecosine(angle) / (SCREENWIDTH / 2));
   lpl.baseyscale = -(  finesine(angle) / (SCREENWIDTH / 2));

   spanstart = (unsigned *)&r_workbuf[0];

   pl = visplanes + 1;
   while(pl < lastvisplane)
   {
      if(pl->minx <= pl->maxx)
      {
         int light;

         lpl.pixelcount = 0;

         lpl.ds_source = flatpixels[pl->flatnum];

         lpl.height = D_abs(pl->height);

         light = pl->lightlevel;

#ifdef MARS
         lpl.lightcoef = (((255 - light) >> 3) & 31) * 256;
#else
         lpl.lightmin = light - ((255 - light) << 1);
         if (lpl.lightmin < 0)
             lpl.lightmin = 0;
         lpl.lightmax = light;
         lpl.lightsub = ((light - lpl.lightmin) * 160) / 640;
         lpl.lightcoef = (light - lpl.lightmin) << SLOPEBITS;

         if (lpl.lightcoef > lpl.lightmax)
             lpl.lightcoef = lpl.lightmax;
         if (lpl.lightcoef < lpl.lightmin)
             lpl.lightcoef = lpl.lightmin;
#endif

         pl->open[pl->maxx + 1] = OPENMARK;
         pl->open[pl->minx - 1] = OPENMARK;

         R_PlaneLoop(pl);

         R_AddPixelsToTexCache(&r_flatscache, pl->flatnum, lpl.pixelcount);
      }

      ++pl;
   }
}

// EOF

