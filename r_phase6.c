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
   pixel_t *data;
#endif
   int      width;
   int      height;
   int      topheight;
   int      bottomheight;
   int      texturemid;
   int      pixelcount;
} drawtex_t;

typedef struct
{
    viswall_t* segl;
    drawtex_t toptex;
    drawtex_t bottomtex;

    unsigned short * clipbounds;
#ifdef GRADIENTLIGHT
    unsigned lightmin, lightmax, lightsub, lightcoef;
#else
    unsigned lightmax;
#endif
} seglocal_t;

typedef struct
{
    int x;
    int floorclipx, ceilingclipx;
    fixed_t scale;
    unsigned iscale;
    unsigned colnum;
    unsigned light;
} segdraw_t;

#ifdef MARS
static void R_DrawTexture(int x, segdraw_t* sdr, drawtex_t* tex) __attribute__((section(".data"), aligned(16)));
static void R_SegLoop(seglocal_t* lseg, int cpu0) __attribute__((section(".data"), aligned(16)));
#endif

//
// Render a wall texture as columns
//
static void R_DrawTexture(int x, segdraw_t *sdr, drawtex_t* tex)
{
   int top, bottom, frac;
   int colnum;
   unsigned iscale;
#ifdef MARS
   inpixel_t *src;
#else
   pixel_t *src;
#endif

   top = CENTERY - ((sdr->scale * tex->topheight) >> (HEIGHTBITS + SCALEBITS));

   if(top <= sdr->ceilingclipx)
      top = sdr->ceilingclipx + 1;

   bottom = CENTERY - 1 - ((sdr->scale * tex->bottomheight) >> (HEIGHTBITS + SCALEBITS));

   if(bottom >= sdr->floorclipx)
      bottom = sdr->floorclipx - 1;

   // column has no length?
   if(top > bottom)
      return;

   iscale = sdr->iscale;
   frac = tex->texturemid - (CENTERY - top) * iscale;

   // DEBUG: fixes green pixels in MAP01...
   frac += (iscale + (iscale >> 5) + (iscale >> 6));

   colnum = sdr->colnum;
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
   I_DrawColumn(x, top, bottom, sdr->light, frac, iscale, src, tex->height);

   // pixel counter
   tex->pixelcount += (bottom - top);
}

//
// Main seg clipping loop
//
static void R_SegLoop(seglocal_t* lseg, const int cpu)
{
   unsigned i, x, stop;
   unsigned pixcount;
   fixed_t scale;
   unsigned scalefrac;
   unsigned scalestep;
   unsigned texturelight;
   int low, high, top, bottom;
   visplane_t *ceiling, *floor;
   segdraw_t sdr;
   int floorclipx, ceilingclipx;
   int floorplhash, ceilingplhash;
   fixed_t floorheight, ceilingheight;
   unsigned floorpicnum, ceilingpicnum;
   unsigned lightlevel;
   unsigned actionbits;
   unsigned	centerangle;
   unsigned	offset;
   unsigned	distance;
   viswall_t* segl;
   unsigned short* clipbounds;

   segl = lseg->segl;
   clipbounds = lseg->clipbounds;

   actionbits = segl->actionbits;
   lightlevel = segl->seglightlevel;

   scalefrac = segl->scalefrac;
   scalestep = segl->scalestep;

   centerangle = segl->centerangle;
   offset = segl->offset;
   distance = segl->distance;

   x = segl->start;
   stop = segl->stop;
#ifdef MARS
   unsigned draw = 0;
#else
   const unsigned draw = 1;
#endif
   pixcount = stop - x + 1;

   floorheight = segl->floorheight;
   ceilingheight = segl->ceilingheight;

   floorpicnum = segl->floorpicnum;
   ceilingpicnum = segl->ceilingpicnum;

   // force R_FindPlane for both planes
   floor = ceiling = visplanes;

#ifndef GRADIENTLIGHT
   texturelight = lseg->lightmax;
   texturelight = HWLIGHT(texturelight);
#endif
   
   floorplhash = 0;
   ceilingplhash = 0;

   if (actionbits & AC_ADDFLOOR)
       floorplhash = R_PlaneHash(floorheight, floorpicnum, lightlevel);
   if (actionbits & AC_ADDCEILING)
       ceilingplhash = R_PlaneHash(ceilingheight, ceilingpicnum, lightlevel);

   for (i = 0; i < pixcount; i++, x++)
   {
      scale = scalefrac >> FIXEDTOSCALE;
      scalefrac += scalestep;
#ifdef MARS
      draw = cpu ? (x & 3) != 0 : (x & 3) == 0;
#endif

      if(scale >= 0x7fff)
         scale = 0x7fff; // fix the scale to maximum

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx   = ceilingclipx & 0x00ff;
      ceilingclipx = ((ceilingclipx & 0xff00) >> 8) - 1;

      //
      // texture only stuff
      //
      if (draw)
      {
          if (actionbits & AC_CALCTEXTURE)
          {
              // calculate texture offset
              fixed_t r = FixedMul(distance,
                  finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT));

              // other texture drawing info
              sdr.colnum = (offset - r) >> FRACBITS;
              sdr.iscale = (1 << (FRACBITS + SCALEBITS)) / (unsigned)scale;

#ifdef GRADIENTLIGHT
              // calc light level
              texturelight = ((scale * lseg->lightcoef) / FRACUNIT);
              if (texturelight <= lseg->lightsub)
              {
                  texturelight = lseg->lightmin;
              }
              else
              {
                  texturelight -= lseg->lightsub;
                  if (texturelight < lseg->lightmin)
                      texturelight = lseg->lightmin;
                  if (texturelight > lseg->lightmax)
                      texturelight = lseg->lightmax;
              }

              // convert to a hardware value
              texturelight = HWLIGHT(texturelight);
#endif

              sdr.light = texturelight;
              sdr.scale = scale;
              sdr.floorclipx = floorclipx;
              sdr.ceilingclipx = ceilingclipx;

              //
              // draw textures
              //
              if (actionbits & AC_TOPTEXTURE)
                  R_DrawTexture(x, &sdr, &lseg->toptex);
              if (actionbits & AC_BOTTOMTEXTURE)
                  R_DrawTexture(x, &sdr, &lseg->bottomtex);
          }
      }

      if (cpu == 0)
      {
          //
          // floor
          //
          if (actionbits & AC_ADDFLOOR)
          {
              top = CENTERY - ((scale * floorheight) >> (HEIGHTBITS + SCALEBITS));
              if (top <= ceilingclipx)
                  top = ceilingclipx + 1;
              bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (floor->open[x] != OPENMARK)
                  {
                      floor = R_FindPlane(floor, floorplhash, floorheight, floorpicnum,
                          lightlevel, x, stop);
                  }
                  floor->open[x] = (unsigned short)((top << 8) + bottom);
              }

          }

          //
          // ceiling
          //
          if (actionbits & AC_ADDCEILING)
          {
              top = ceilingclipx + 1;
              bottom = CENTERY - 1 - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS));
              if (bottom >= floorclipx)
                  bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (ceiling->open[x] != OPENMARK)
                  {
                      ceiling = R_FindPlane(ceiling, ceilingplhash, ceilingheight, ceilingpicnum,
                          lightlevel, x, stop);
                  }
                  ceiling->open[x] = (unsigned short)((top << 8) + bottom);
              }

          }
      }

      //
      // calc high and low
      //
      low = CENTERY - ((scale * segl->floornewheight) >> (HEIGHTBITS + SCALEBITS));
      if(low < 0)
         low = 0;
      if(low > floorclipx)
         low = floorclipx;

      high = CENTERY - 1 - ((scale * segl->ceilingnewheight) >> (HEIGHTBITS + SCALEBITS));
      if(high > SCREENHEIGHT - 1)
         high = SCREENHEIGHT - 1;
      if(high < ceilingclipx)
         high = ceilingclipx;

      if (cpu == 0)
      {
          // bottom sprite clip sil
          if (actionbits & AC_BOTTOMSIL)
              segl->bottomsil[x] = low;

          // top sprite clip sil
          if (actionbits & AC_TOPSIL)
              segl->topsil[x] = high + 1;
      }

      // sky mapping
      if (draw)
      {
          if (actionbits & AC_ADDSKY)
          {
              top = ceilingclipx + 1;
              bottom = (CENTERY - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS))) - 1;

              if (bottom >= floorclipx)
                  bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  // CALICO: draw sky column
                  int colnum = ((vd.viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT) & 0xff;
#ifdef MARS
                  inpixel_t* data = skytexturep->data + colnum * skytexturep->height;
#else
                  pixel_t* data = skytexturep->data + colnum * skytexturep->height;
#endif
                  I_DrawColumn(x, top, bottom, 0, (top * 18204) << 2, FRACUNIT + 7281, data, 128);
              }
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
   }
}

void R_SegCommandsMask(int mask)
{
    int i;
    unsigned short *clip;
    viswall_t* segl;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;

    // initialize the clipbounds array
#ifdef MARS1
    unsigned short clipbounds[SCREENWIDTH/2];
    clip = clipbounds;
    for (i = 0; i < SCREENWIDTH / 8; i++)
    {
        *clip++ = SCREENHEIGHT;
        *clip++ = SCREENHEIGHT;
        *clip++ = SCREENHEIGHT;
        *clip++ = SCREENHEIGHT;
    }
#else
    unsigned short clipbounds[SCREENWIDTH];
    clip = clipbounds;
    for (i = 0; i < SCREENWIDTH / 4; i++)
    {
        *clip++ = SCREENHEIGHT, *clip++ = SCREENHEIGHT;
        *clip++ = SCREENHEIGHT, *clip++ = SCREENHEIGHT;
    }
#endif

    toptex = &lseg.toptex;
    bottomtex = &lseg.bottomtex;
    lseg.clipbounds = clipbounds;

    for (segl = viswalls; segl < lastwallcmd; segl++)
    {
        if (segl->start > segl->stop)
            continue;

        lseg.segl = segl;

#ifdef GRADIENTLIGHT
#ifdef MARS
        unsigned seglight = segl->seglightlevel;
        if (seglight <= 160)
            seglight = seglight - (seglight >> 1);
#else
        int seglight = segl->seglightlevel - (255 - segl->seglightlevel) * 2;
        if (seglight < 0)
            seglight = 0;
#endif

        lseg.lightmin = seglight;
        lseg.lightmax = segl->seglightlevel;
        lseg.lightsub = 160 * (lseg.lightmax - lseg.lightmin) / (800 - 160);
        lseg.lightcoef = ((lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);
#else
        lseg.lightmax = segl->seglightlevel;
#endif

        if (segl->actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[segl->t_texturenum];

            toptex->topheight = segl->t_topheight;
            toptex->bottomheight = segl->t_bottomheight;
            toptex->texturemid = segl->t_texturemid;
            toptex->width = tex->width;
            toptex->height = tex->height;
            toptex->data = tex->data;
            toptex->pixelcount = 0;
        }

        if (segl->actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[segl->b_texturenum];

            bottomtex->topheight = segl->b_topheight;
            bottomtex->bottomheight = segl->b_bottomheight;
            bottomtex->texturemid = segl->b_texturemid;
            bottomtex->width = tex->width;
            bottomtex->height = tex->height;
            bottomtex->data = tex->data;
            bottomtex->pixelcount = 0;
        }

        R_SegLoop(&lseg, mask);

        if (mask == 0)
        {
            if (segl->actionbits & AC_TOPTEXTURE)
            {
                R_AddPixelsToTexCache(&r_wallscache, segl->t_texturenum, toptex->pixelcount);
            }

            if (segl->actionbits & AC_BOTTOMTEXTURE)
            {
                R_AddPixelsToTexCache(&r_wallscache, segl->b_texturenum, bottomtex->pixelcount);
            }
        }
    }
}

#ifdef MARS

void Mars_Slave_R_SegCommands(void)
{
    R_SegCommandsMask(1);
}

void R_SegCommands(void)
{
    Mars_R_BeginComputeSeg();

    R_SegCommandsMask(0);

    Mars_R_EndComputeSeg();
}

#else

void R_SegCommands(void)
{
    R_SegCommandsMask(0);
}

#endif

