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
   void     (*drawcol)(int, int, int, int, fixed_t, fixed_t, inpixel_t*, int);
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
    fixed_t scale2;
    volatile unsigned iscale;
    unsigned colnum;
    unsigned light;
} segdraw_t;

static void R_DrawTexture(int x, segdraw_t* sdr, drawtex_t* tex) __attribute__((always_inline));
static void R_SegLoop(seglocal_t* lseg, const int cpu) __attribute__((always_inline));
static void R_SegCommands2(const int mask) __attribute__((always_inline));

void Mars_Slave_R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;

//
// Render a wall texture as columns
//
static void R_DrawTexture(int x, segdraw_t *sdr, drawtex_t* tex)
{
   int top, bottom;
   fixed_t scale2, frac;
   int colnum;
   unsigned iscale;
#ifdef MARS
   inpixel_t *src;
#else
   pixel_t *src;
#endif

   scale2 = sdr->scale2;
   colnum = sdr->colnum;
   iscale = sdr->iscale;

   FixedMul2(top, scale2, tex->topheight);
   top = centerY - top;
   if(top <= sdr->ceilingclipx)
      top = sdr->ceilingclipx + 1;

   FixedMul2(bottom, scale2, tex->bottomheight);
   bottom = centerY - 1 - bottom;
   if(bottom >= sdr->floorclipx)
      bottom = sdr->floorclipx - 1;

   // column has no length?
   if(top > bottom)
      return;

   frac = tex->texturemid - (centerY - top) * iscale;
 
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
   tex->drawcol(x, top, bottom, sdr->light, frac, iscale, src, tex->height);

   // pixel counter
   tex->pixelcount += (bottom - top);
}

//
// Main seg clipping loop
//
static void R_SegLoop(seglocal_t* lseg, const int cpu)
{
   visplane_t *ceiling, *floor;
   unsigned short* ceilopen, * flooropen;

   viswall_t* segl = lseg->segl;
   unsigned short *clipbounds = lseg->clipbounds;

   // for future reference: the top hoggers are
   // (AC_TOPTEXTURE | AC_ADDFLOOR | AC_NEWFLOOR)
   // (AC_TOPTEXTURE | AC_ADDCEILING | AC_NEWCEILING | AC_SOLIDSIL)

   const unsigned actionbits = segl->actionbits;
   const unsigned lightlevel = segl->seglightlevel;

   unsigned scalefrac = segl->scalefrac;
   unsigned scalestep = segl->scalestep;

   const unsigned centerangle = segl->centerangle;
   const unsigned offset = segl->offset;
   const unsigned distance = segl->distance;

   int x;
   const int stop = segl->stop;
#ifdef MARS
   unsigned draw = 0;
#else
   const unsigned draw = 1;
#endif

   const fixed_t floorheight = segl->floorheight;
   const fixed_t floornewheight = segl->floornewheight;

   const int ceilingheight = segl->ceilingheight;
   const int ceilingnewheight = segl->ceilingnewheight;

   const int floorpicnum = segl->floorpicnum;
   const int ceilingpicnum = segl->ceilingpicnum;

   // force R_FindPlane for both planes
   floor = ceiling = visplanes;
   flooropen = ceilopen = visplanes[0].open;

#ifndef GRADIENTLIGHT
   const unsigned texturelight = lseg->lightmax;
#endif
   
   const int centerY0 = centerY;
   const int centerY1 = centerY - 1;

   int floorplhash = 0;
   int ceilingplhash = 0;

   if (actionbits & AC_ADDFLOOR)
       floorplhash = R_PlaneHash(floorheight, floorpicnum, lightlevel);
   if (actionbits & AC_ADDCEILING)
       ceilingplhash = R_PlaneHash(ceilingheight, ceilingpicnum, lightlevel);

   for (x = segl->start; x <= stop; x++)
   {
      fixed_t scale;
      int floorclipx, ceilingclipx;
      int low, high, top, bottom;
      unsigned scale2;

      scale = scalefrac >> FIXEDTOSCALE;
      scalefrac += scalestep;

      if (scale >= 0x7fff)
          scale = 0x7fff; // fix the scale to maximum

#ifdef MARS
      SH2_DIVU_DVSR = scale; // set 32-bit divisor
      draw = cpu ? (x & 1) != 0 : (x & 1) == 0;
#endif

      scale2 = (unsigned)scale << (FRACBITS - (HEIGHTBITS + SCALEBITS));

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx   = ceilingclipx & 0x00ff;
      ceilingclipx = (((unsigned)ceilingclipx & 0xff00) >> 8) - 1;

      //
      // calc high and low
      //
      FixedMul2(low, scale2, floornewheight);
      low = centerY0 - low;
      if (low < 0)
          low = 0;
      else if (low > floorclipx)
          low = floorclipx;

      FixedMul2(high, scale2, ceilingnewheight);
      high = centerY1 - high;
      if (high > screenHeight - 1)
          high = screenHeight - 1;
      else if (high < ceilingclipx)
          high = ceilingclipx;

      if (cpu == 0)
      {
          //
          // floor
          //
          if (actionbits & AC_ADDFLOOR)
          {
              FixedMul2(top, scale2, floorheight);
              top = centerY0 - top;
              if (top <= ceilingclipx)
                  top = ceilingclipx + 1;
              bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (flooropen[x] != OPENMARK)
                  {
                      floor = R_FindPlane(floor, floorplhash, 
                          floorheight, floorpicnum, lightlevel, x, stop);
                      flooropen = floor->open;
                  }
                  flooropen[x] = (unsigned short)((top << 8) + bottom);
              }
          }

          //
          // ceiling
          //
          if (actionbits & AC_ADDCEILING)
          {
              top = ceilingclipx + 1;
              FixedMul2(bottom, scale2, ceilingheight);
              bottom = centerY1 - bottom;
              if (bottom >= floorclipx)
                  bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (ceilopen[x] != OPENMARK)
                  {
                      ceiling = R_FindPlane(ceiling, ceilingplhash, 
                          ceilingheight, ceilingpicnum, lightlevel, x, stop);
                      ceilopen = ceiling->open;
                  }
                  ceilopen[x] = (unsigned short)((top << 8) + bottom);
              }
          }

          // bottom sprite clip sil
          if (actionbits & AC_BOTTOMSIL)
              segl->bottomsil[x] = low;

          // top sprite clip sil
          if (actionbits & AC_TOPSIL)
              segl->topsil[x] = high + 1;
      }

      if(actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
      {
          int newfloorclipx = floorclipx;
          int newceilingclipx = ceilingclipx;

         // rewrite clipbounds
         if(actionbits & AC_NEWFLOOR)
            newfloorclipx = low;
         if(actionbits & AC_NEWCEILING)
            newceilingclipx = high;
         clipbounds[x] = ((unsigned)(newceilingclipx + 1) << 8) + newfloorclipx;
      }

      //
      // texture only stuff
      //
      if (!draw)
          continue;

      if (actionbits & AC_CALCTEXTURE)
      {
          segdraw_t sdr;
          fixed_t r;

#ifdef MARS
          SH2_DIVU_DVDNT = 1 << (FRACBITS + SCALEBITS); // set 32-bit dividend, start divide
#endif

          // calculate texture offset
          r = finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT);
          FixedMul2(r, distance, r);

          // other texture drawing info
          sdr.colnum = (offset - r) >> FRACBITS;

#ifdef GRADIENTLIGHT
          // calc light level
          unsigned texturelight = ((scale * lseg->lightcoef) / FRACUNIT);
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
          sdr.scale2 = scale2;
          sdr.floorclipx = floorclipx;
          sdr.ceilingclipx = ceilingclipx;
#ifdef MARS
          sdr.iscale = SH2_DIVU_DVDNT; // get 32-bit quotient
#else
          sdr.iscale = (1 << (FRACBITS + SCALEBITS)) / scale;
#endif

          //
          // draw textures
          //
          if (actionbits & AC_TOPTEXTURE)
              R_DrawTexture(x, &sdr, &lseg->toptex);
          if (actionbits & AC_BOTTOMTEXTURE)
              R_DrawTexture(x, &sdr, &lseg->bottomtex);
      }

      // sky mapping
      if (actionbits & AC_ADDSKY)
      {
          top = ceilingclipx + 1;
          FixedMul2(bottom, scale2, ceilingheight);
          bottom = (centerY - bottom) - 1;

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
}

static void R_SegCommands2(const int cpu)
{
    int i;
    viswall_t* segl;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;

    // initialize the clipbounds array
    unsigned short clipbounds[SCREENWIDTH];
    unsigned short *clip = clipbounds;
    for (i = 0; i < screenWidth / 4; i++)
    {
        *clip++ = screenHeight, *clip++ = screenHeight;
        *clip++ = screenHeight, *clip++ = screenHeight;
    }

    toptex = &lseg.toptex;
    bottomtex = &lseg.bottomtex;
    lseg.clipbounds = clipbounds;

    // workaround annoying compilation warnings
    toptex->height = 0;
    toptex->pixelcount = 0;
    bottomtex->height = 0;
    bottomtex->pixelcount = 0;

    for (segl = viswalls; segl < lastwallcmd; segl++)
    {
        if (segl->start > segl->stop)
            continue;

        lseg.segl = segl;
        lseg.lightmax = segl->seglightlevel + extralight;
#ifdef MARS
        if (lseg.lightmax > 255)
            lseg.lightmax = 255;
#endif

#ifdef GRADIENTLIGHT
#ifdef MARS
        unsigned seglight = segl->seglightlevel + extralight;
        if (seglight > 255)
            seglight = 255;
        else if (seglight <= 160)
            seglight = seglight - (seglight >> 1);
#else
        int seglight = segl->seglightlevel - (255 - segl->seglightlevel) * 2;
        if (seglight < 0)
            seglight = 0;
#endif

        lseg.lightmin = seglight;
        lseg.lightsub = 160 * (lseg.lightmax - lseg.lightmin) / (800 - 160);
        lseg.lightcoef = ((lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);
#endif

#ifndef GRADIENTLIGHT
        lseg.lightmax = HWLIGHT(lseg.lightmax);
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
            toptex->drawcol = (tex->height & (tex->height - 1)) ? I_DrawColumnNPo2 : I_DrawColumn;
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
            bottomtex->drawcol = (tex->height & (tex->height - 1)) ? I_DrawColumnNPo2 : I_DrawColumn;
        }

        R_SegLoop(&lseg, cpu);

        if (cpu == 0)
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
    R_SegCommands2(1);
}

void R_SegCommands(void)
{
    Mars_R_BeginComputeSeg();

    R_SegCommands2(0);

    Mars_R_EndComputeSeg();
}

#else

void R_SegCommands(void)
{
    R_SegCommands2(0);
}

#endif

