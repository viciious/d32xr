/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"
#include "32x.h"

typedef struct drawtex_s
{
#ifdef MARS
   inpixel_t *data;
#else
   pixel_t *data;
#endif
   int      width;
   int      height;
   int      topheight;
   int      bottomheight;
   int      texturemid;
   int      pixelcount;
} drawtex_t;

static drawtex_t toptex;
static drawtex_t bottomtex;

static int *clipbounds;
static unsigned lightmin, lightmax, lightsub, lightcoef;
static int floorclipx, ceilingclipx;
static unsigned texturecol, iscale, texturelight;

#define NUM_VISPLANES_BUCKETS 64
static visplane_t* visplanes_hash[NUM_VISPLANES_BUCKETS];

static short buf[SCREENWIDTH * 12] __attribute__((aligned(16)));
static viswall_t workwall __attribute__((aligned(16)));

//
// Check for a matching visplane in the visplanes array, or set up a new one
// if no compatible match can be found.
//
#ifdef MARS
static visplane_t *R_FindPlane(visplane_t *check, fixed_t height, unsigned flatnum,
#else
static visplane_t *R_FindPlane(visplane_t *check, fixed_t height, unsigned flatnum,
#endif
                               unsigned lightlevel, int start, int stop)
{
   int i;
   int* open;
   const int mark = (OPENMARK << 16) | OPENMARK;
   int hash = ((((unsigned)height>>8)+lightlevel)^flatnum) & (NUM_VISPLANES_BUCKETS-1);

   for (check = visplanes_hash[hash]; check; )
   {
      if(height == check->height && // same plane as before?
         flatnum == check->flatnum &&
         lightlevel == check->lightlevel)
      {
         if(check->open[start] == OPENMARK)
         {
            // found a plane, so adjust bounds and return it
            if(start < check->minx) // in range of the plane?
               check->minx = start; // mark the new edge
            if(stop > check->maxx)
               check->maxx = stop;  // mark the new edge

            return check; // use the same one as before
         }
      }
      check = check->next;
   }

   // make a new plane
   check = lastvisplane;
   ++lastvisplane;

   check->height = height;
   check->flatnum = flatnum;
   check->lightlevel = lightlevel;
   check->minx = start;
   check->maxx = stop;

   open = (int*)check->open;
   for(i = 0; i < SCREENWIDTH/4; i++)
   {
       *open++ = mark;
       *open++ = mark;
   }
   check->next = visplanes_hash[hash];
   visplanes_hash[hash] = check;

   return check;
}

//
// Render a wall texture as columns
//
static void R_DrawTexture(drawtex_t *tex, int x, int top, int bottom)
{
   int frac;
   unsigned colnum;
#ifdef MARS
   inpixel_t *src;
#else
   pixel_t *src;
#endif

   if(top <= ceilingclipx)
      top = ceilingclipx + 1;

   if(bottom >= floorclipx)
      bottom = floorclipx - 1;

   // column has no length?
   if(top > bottom)
      return;

   colnum = texturecol;
   frac = tex->texturemid - (CENTERY - top) * iscale;

   // DEBUG: fixes green pixels in MAP01...
   frac += (iscale + (iscale >> 5) + (iscale >> 6));

   while(frac < 0)
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
   I_DrawColumn(x, top, bottom, texturelight, frac, iscale, src, tex->height);

   // pixel counter
   tex->pixelcount += (bottom - top);
}

void R_ComputeSeg(void)
{
    int i, x, stop, scale;
    unsigned scalefrac, iscale;
    int low, high, top, bottom;
    int scalestep;
    unsigned texturecol;
    unsigned actionbits;
    unsigned distance;
    unsigned centerangle;
    unsigned offset;
    int t_topheight, t_bottomheight;
    int b_topheight, b_bottomheight;
    int floorheight, floornewheight;
    int ceilingheight, ceilingnewheight;
    viswall_t* segl = &workwall;
    const int numlines = (sizeof(viswall_t) + 15) / 16;
    intptr_t cacheaddr = (intptr_t)segl;
    short* p = buf;

    for (i = 0; i < numlines; i++)
    {
        CacheClearLine((void *)cacheaddr);
        cacheaddr += 16;
    }

    scalefrac = segl->scalefrac; // cache the first 8 words of segl: scalefrac, scale2, scalestep, centerangle
    scalestep = segl->scalestep;
    centerangle = segl->centerangle;

    offset = segl->offset; // cache the second 8 words of segl: offset, distance, seglightlevel, start, stop
    distance = segl->distance;
    x = segl->start;
    stop = segl->stop;

    actionbits = segl->actionbits;
    t_topheight = segl->t_topheight;
    t_bottomheight = segl->t_bottomheight;
    b_topheight = segl->b_topheight;
    b_bottomheight = segl->b_bottomheight;
    floorheight = segl->floorheight;
    floornewheight = segl->floornewheight;
    ceilingheight = segl->ceilingheight;
    ceilingnewheight = segl->ceilingnewheight;

    do
    {
        scale = scalefrac >> FIXEDTOSCALE;
        scalefrac += scalestep;

        if (scale >= 0x7fff)
            scale = 0x7fff; // fix the scale to maximum

        //
        // texture only stuff
        //
        if (actionbits & AC_CALCTEXTURE)
        {
            // calculate texture offset
            fixed_t r = FixedMul(distance,
                finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT));

            // other texture drawing info
            texturecol = (offset - r) >> FRACBITS;
            iscale = (1 << (FRACBITS + SCALEBITS)) / (unsigned)scale;

            *(int*)p = iscale, p += 2; // iscale
            *p = texturecol, p += 1; // colnum

            //
            // draw textures
            //
            if (actionbits & AC_TOPTEXTURE)
            {
                top = CENTERY - ((scale * t_topheight) >> (HEIGHTBITS + SCALEBITS));
                bottom = CENTERY - 1 - ((scale * t_bottomheight) >> (HEIGHTBITS + SCALEBITS));
                *p = top, p += 1; // t_top
                *p = bottom, p += 1; // t_bottom
            }
            if (actionbits & AC_BOTTOMTEXTURE)
            {
                top = CENTERY - ((scale * b_topheight) >> (HEIGHTBITS + SCALEBITS));
                bottom = CENTERY - 1 - ((scale * b_bottomheight) >> (HEIGHTBITS + SCALEBITS));
                *p = top, p += 1; // b_top
                *p = bottom, p += 1; // b_bottom
            }
        }

        //
        // floor
        //
        if (actionbits & AC_ADDFLOOR)
        {
            top = CENTERY - ((scale * floorheight) >> (HEIGHTBITS + SCALEBITS));
            *p = top, p += 1; // top
        }

        //
        // ceiling
        //
        if (actionbits & AC_ADDCEILING)
        {
            bottom = CENTERY - 1 - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS));
            *p = bottom, p += 1; // bottom
        }

        //
        // calc high and low
        //
        low = CENTERY - ((scale * floornewheight) >> (HEIGHTBITS + SCALEBITS));
        high = CENTERY - 1 - ((scale * ceilingnewheight) >> (HEIGHTBITS + SCALEBITS));

        *p = low, p += 1; // top
        *p = high, p += 1; // bottom

        // sky mapping
        if (actionbits & AC_ADDSKY)
        {
            bottom = (CENTERY - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS))) - 1;
            *p = bottom, p += 1; // bottom
        }

        p = (short*)(((intptr_t)p + 3) & ~3);

        MARS_SYS_COMM6 = ++x;
    } while (x <= stop);
}

//
// Main seg clipping loop
//
static void R_SegLoop(viswall_t *segl)
{
   int i, x, stop;
   int low, high, top, bottom;
   visplane_t *ceiling, *floor;
   unsigned actionbits;
   short* p = buf;
   int numwords;
   int numlines;
   intptr_t cacheaddr = (intptr_t)buf;

   D_memcpy(&workwall, segl, sizeof(viswall_t));

#ifdef MARS
   texturelight = lightmax;
   texturelight = (((255 - texturelight) >> 3) & 31) * 256;
#endif

   x = segl->start;
   stop = segl->stop;
   actionbits = segl->actionbits;

   MARS_SYS_COMM6 = x;
   MARS_SYS_COMM4 = 1;

   numwords = 0;
   if (actionbits & AC_CALCTEXTURE)
   {
       numwords = 2 + 1; // iscale + colnum
       if (actionbits & AC_TOPTEXTURE)
           numwords += 2; // top and bottom
       if (actionbits & AC_BOTTOMTEXTURE)
           numwords += 2; // top and bottom
   }
   if (actionbits & AC_ADDFLOOR)
       numwords += 1; // top
   if (actionbits & AC_ADDCEILING)
       numwords += 1; // bottom
   numwords = 2 + 1; // low and high
   if (actionbits & AC_ADDSKY)
       numwords += 1; // bottom
   numlines = (numwords * 2 * (stop - x + 1) + 15) / 16;

   for (i = 0; i < numlines; i++)
   {
       CacheClearLine((void *)cacheaddr);
       cacheaddr += 16;
   }

   // force R_FindPlane for both planes
   floor = ceiling = visplanes;

   do
   {
      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx   = ceilingclipx & 0x00ff;
      ceilingclipx = ((ceilingclipx & 0xff00) >> 8) - 1;

      // wait for slave to complete calculations for this coordinate
      while (MARS_SYS_COMM6 <= x);

      //
      // texture only stuff
      //
      if(actionbits & AC_CALCTEXTURE)
      {
         // other texture drawing info
         iscale = *(int*)p, p += 2; // iscale
         texturecol = *p, p += 1;

         //
         // draw textures
         //
         if (actionbits & AC_TOPTEXTURE)
         {
             top = *p++;
             bottom = *p++;
             R_DrawTexture(&toptex, x, top, bottom);
         }
         if (actionbits & AC_BOTTOMTEXTURE)
         {
             top = *p++;
             bottom = *p++;
             R_DrawTexture(&bottomtex, x, top, bottom);
         }
      }

      //
      // floor
      //
      if(actionbits & AC_ADDFLOOR)
      {
         top = *p++;
         if(top <= ceilingclipx)
            top = ceilingclipx + 1;
         
         bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            if(floor->open[x] != OPENMARK)
            {
               floor = R_FindPlane(floor + 1, segl->floorheight, segl->floorpicnum,
                                   segl->seglightlevel, x, segl->stop);
            }
            floor->open[x] = (unsigned short)((top << 8) + bottom);
         }
      }

      //
      // ceiling
      //
      if(actionbits & AC_ADDCEILING)
      {
         top = ceilingclipx + 1;

         bottom = *p++;
         if(bottom >= floorclipx)
            bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            if(ceiling->open[x] != OPENMARK)
            {
               ceiling = R_FindPlane(ceiling + 1, segl->ceilingheight, segl->ceilingpicnum,
                                     segl->seglightlevel, x, segl->stop);
            }
            ceiling->open[x] = (unsigned short)((top << 8) + bottom);
         }
      }

      //
      // calc high and low
      //
      low = *p++;
      if(low < 0)
         low = 0;
      if(low > floorclipx)
         low = floorclipx;

      high = *p++;
      if(high > SCREENHEIGHT - 1)
         high = SCREENHEIGHT - 1;
      if(high < ceilingclipx)
         high = ceilingclipx;

      // bottom sprite clip sil
      if(actionbits & AC_BOTTOMSIL)
         segl->bottomsil[x] = low;

      // top sprite clip sil
      if(actionbits & AC_TOPSIL)
         segl->topsil[x] = high + 1;

      // sky mapping
      if(actionbits & AC_ADDSKY)
      {
         top = ceilingclipx + 1;
         bottom = *p++;
         
         if(bottom >= floorclipx)
            bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            // CALICO: draw sky column
            int colnum = ((viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT) & 0xff;
#ifdef MARS
            inpixel_t * data = skytexturep->data + colnum * skytexturep->height;
#else
            pixel_t *data = skytexturep->data + colnum * skytexturep->height;
#endif
            I_DrawColumn(x, top, bottom, 0, (top * 18204) << 2,  FRACUNIT + 7281, data, 128);
         }
      }

      if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
      {
         // rewrite clipbounds
         if(actionbits & AC_NEWFLOOR)
            floorclipx = low;
         if(actionbits & AC_NEWCEILING)
            ceilingclipx = high;

         clipbounds[x] = ((ceilingclipx + 1) << 8) + floorclipx;
      }

      p = (short*)(((intptr_t)p + 3) & ~3);
   }
   while(++x <= stop);
}

void R_SegCommands_Mars(void)
{
   int i;
   int *clip;
   viswall_t *segl;

   // initialize the clipbounds array
   clipbounds = (int *)&r_workbuf[0];
   clip = clipbounds;
   for(i = 0; i < SCREENWIDTH / 4; i++)
   {
      *clip++ = SCREENHEIGHT;
      *clip++ = SCREENHEIGHT;
      *clip++ = SCREENHEIGHT;
      *clip++ = SCREENHEIGHT;
   }

   for (i = 0; i < NUM_VISPLANES_BUCKETS; i++)
       visplanes_hash[i] = NULL;

   /*
   ; setup blitter
   movei #15737348,r0   r0 = 15737348; // 0xf02204 - A1_FLAGS
   movei #145440,r1     r1 = 145440;   // X add ctrl = Add zero; Width = 128; Pixel size = 16
   store r1,(r0)        *r0 = r1;
                       
   movei #15737384,r0   r0 = 15737384; // 0xf02228 - A2_FLAGS
   movei #145952,r1     r1 = 145952;   // X add ctrl = Add zero; Width = 160; Pixel size = 16
   store r1,(r0)        *r0 = r1;
   */
  
   segl = viswalls;
   while(segl < lastwallcmd)
   {
      lightmin = segl->seglightlevel - (255 - segl->seglightlevel) * 2;
      if(lightmin < 0)
         lightmin = 0;

      lightmax = segl->seglightlevel;
      
      lightsub  = 160 * (lightmax - lightmin) / (800 - 160);
      lightcoef = ((lightmax - lightmin) << FRACBITS) / (800 - 160);

      if(segl->actionbits & AC_TOPTEXTURE)
      {
         texture_t *tex = &textures[segl->t_texturenum];

         toptex.topheight    = segl->t_topheight;
         toptex.bottomheight = segl->t_bottomheight;
         toptex.texturemid   = segl->t_texturemid;
         toptex.width        = tex->width;
         toptex.height       = tex->height;
         toptex.data         = tex->data;
	 toptex.pixelcount   = 0;
      }

      if(segl->actionbits & AC_BOTTOMTEXTURE)
      {
         texture_t *tex = &textures[segl->b_texturenum];

         bottomtex.topheight    = segl->b_topheight;
         bottomtex.bottomheight = segl->b_bottomheight;
         bottomtex.texturemid   = segl->b_texturemid;
         bottomtex.width        = tex->width;
         bottomtex.height       = tex->height;
         bottomtex.data         = tex->data;
	 bottomtex.pixelcount   = 0;
      }

      R_SegLoop(segl);

      if(segl->actionbits & AC_TOPTEXTURE)
      {
         R_AddPixelsToTexCache(&r_wallscache, segl->t_texturenum, toptex.pixelcount);
      }

      if(segl->actionbits & AC_BOTTOMTEXTURE)
      {
         R_AddPixelsToTexCache(&r_wallscache, segl->b_texturenum, bottomtex.pixelcount);
      }

      ++segl;
   }
}
