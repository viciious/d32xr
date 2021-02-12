/*
  CALICO

  Renderer phase 7 - Visplanes
*/

#include "r_local.h"

static fixed_t planeheight;
static angle_t planeangle;
static fixed_t planex, planey;
static int     plane_lightcoef;
#ifndef MARS
static int     plane_lightmin, plane_lightmax, plane_lightsub;
#endif

static fixed_t basexscale, baseyscale;

static unsigned *spanstart;
#ifdef MARS
static inpixel_t *ds_source;
#else
static pixel_t *ds_source;
#endif
static int pl_pixelcount;

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

   distance = (planeheight * yslope[y]) >> 12;
   length = (distance * distscale[x]) >> 14;
   angle = (planeangle + xtoviewangle[x]) >> ANGLETOFINESHIFT;

   xfrac = planex + (((finecosine(angle) >> 1) * length) >> 4);
   yfrac = planey - (((finesine(angle) >> 1) * length) >> 4);

   xstep = (distance * basexscale) >> 4;
   ystep = (baseyscale * distance) >> 4;

#ifdef MARS
   light = plane_lightcoef;
#else
   light = plane_lightcoef / distance;

   // finish light calculations
   light -= plane_lightsub;

   if (light > plane_lightmax)
       light = plane_lightmax;
   if (light < plane_lightmin)
       light = plane_lightmin;

   // transform to hardware value
   light = -((255 - light) << 14) & 0xffffff;
#endif

   // CALICO: invoke I_DrawSpan here.
   I_DrawSpan(y, x, x2, light, xfrac, yfrac, xstep, ystep, ds_source);

   pl_pixelcount += x2 - x + 1;
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
   int pl_flatnum;
 
   planex =  vd.viewx;
   planey = -vd.viewy;

   planeangle = vd.viewangle;
   angle = (planeangle - ANG90) >> ANGLETOFINESHIFT;

   basexscale =  (finecosine(angle) / (SCREENWIDTH / 2));
   baseyscale = -(  finesine(angle) / (SCREENWIDTH / 2));

   spanstart = (unsigned *)&r_workbuf[0];

   pl = visplanes + 1;
   while(pl < lastvisplane)
   {
      if(pl->minx <= pl->maxx)
      {
         int light;

         pl_flatnum = pl->flatnum;
         pl_pixelcount = 0;

         ds_source = flatpixels[pl_flatnum];

         planeheight = D_abs(pl->height);

         light = pl->lightlevel;

#ifdef MARS
         plane_lightcoef = (((255 - light) >> 3) & 31) * 256;
#else
         plane_lightmin = light - ((255 - light) << 1);
         if (plane_lightmin < 0)
             plane_lightmin = 0;
         plane_lightmax = light;
         plane_lightsub = ((light - plane_lightmin) * 160) / 640;
         plane_lightcoef = (light - plane_lightmin) << SLOPEBITS;

         if (plane_lightcoef > plane_lightmax)
             plane_lightcoef = plane_lightmax;
         if (plane_lightcoef < plane_lightmin)
             plane_lightcoef = plane_lightmin;
#endif

         pl->open[pl->maxx + 1] = OPENMARK;
         pl->open[pl->minx - 1] = OPENMARK;

         R_PlaneLoop(pl);

         R_AddPixelsToTexCache(&r_flatscache, pl_flatnum, pl_pixelcount);
      }

      ++pl;
   }
}

// EOF

