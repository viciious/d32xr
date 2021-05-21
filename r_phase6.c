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

static void R_DrawTexture(int x, segdraw_t* sdr, drawtex_t* tex) ATTR_DATA_CACHE_ALIGN;
static void R_SegLoop(seglocal_t* lseg, const int cpu) __attribute__((always_inline));
static void R_SegCommandsMask(const int mask) __attribute__((always_inline));

void Mars_Slave_R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;

//
// Render a wall texture as columns
//
static void R_DrawTexture(int x, segdraw_t *sdr, drawtex_t* tex)
{
   int top, bottom;
   fixed_t scale, frac;
   int colnum;
   unsigned iscale;
#ifdef MARS
   inpixel_t *src;
#else
   pixel_t *src;
#endif

   scale = sdr->scale;
   colnum = sdr->colnum;
   iscale = sdr->iscale;

   top = centerY - ((scale * tex->topheight) >> (HEIGHTBITS + SCALEBITS));
   if(top <= sdr->ceilingclipx)
      top = sdr->ceilingclipx + 1;

   bottom = centerY - 1 - ((scale * tex->bottomheight) >> (HEIGHTBITS + SCALEBITS));
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
   I_DrawColumn(x, top, bottom, sdr->light, frac, iscale, src, tex->height);

   // pixel counter
   tex->pixelcount += (bottom - top);
}

//
// Main seg clipping loop
//
static void R_SegLoop(seglocal_t* lseg, const int cpu)
{
   unsigned i;
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

   int x = segl->start;
   const int stop = segl->stop;
#ifdef MARS
   unsigned draw = 0;
#else
   const unsigned draw = 1;
#endif
   const unsigned pixcount = stop - x + 1;

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

   for (i = 0; i < pixcount; i++, x++)
   {
      fixed_t scale;
      int floorclipx, ceilingclipx;
      int low, high, top, bottom;

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
      // calc high and low
      //
      low = centerY0 - ((scale * floornewheight) >> (HEIGHTBITS + SCALEBITS));
      if (low < 0)
          low = 0;
      else if (low > floorclipx)
          low = floorclipx;

      high = centerY1 - ((scale * ceilingnewheight) >> (HEIGHTBITS + SCALEBITS));
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
              top = centerY0 - ((scale * floorheight) >> (HEIGHTBITS + SCALEBITS));
              if (top <= ceilingclipx)
                  top = ceilingclipx + 1;
              bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (flooropen[x] != OPENMARK)
                  {
                      floor = R_FindPlane(floor, floorplhash, floorheight, floorpicnum,
                          lightlevel, x, stop);
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
              bottom = centerY1 - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS));
              if (bottom >= floorclipx)
                  bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (ceilopen[x] != OPENMARK)
                  {
                      ceiling = R_FindPlane(ceiling, ceilingplhash, ceilingheight, ceilingpicnum,
                          lightlevel, x, stop);
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
         clipbounds[x] = ((newceilingclipx + 1) << 8) + newfloorclipx;
      }

      //
      // texture only stuff
      //
      if (!draw)
          continue;

      if (actionbits & AC_CALCTEXTURE)
      {
          segdraw_t sdr;

          // calculate texture offset
          fixed_t r = FixedMul(distance,
              finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT));

          // other texture drawing info
          sdr.colnum = (offset - r) >> FRACBITS;
          sdr.iscale = (1 << (FRACBITS + SCALEBITS)) / (unsigned)scale;

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

      // sky mapping
      if (actionbits & AC_ADDSKY)
      {
          top = ceilingclipx + 1;
          bottom = (centerY - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS))) - 1;

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

static void R_SegCommandsMask(const int mask)
{
    int i;
    unsigned short *clip;
    viswall_t* segl, *first, *last;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;

    // initialize the clipbounds array
    unsigned short clipbounds[SCREENWIDTH];
    clip = clipbounds;
    for (i = 0; i < screenWidth / 4; i++)
    {
        *clip++ = screenHeight, *clip++ = screenHeight;
        *clip++ = screenHeight, *clip++ = screenHeight;
    }

    toptex = &lseg.toptex;
    bottomtex = &lseg.bottomtex;
    lseg.clipbounds = clipbounds;

    first = viswalls;
    last = *(viswall_t **)((intptr_t)&lastwallcmd | 0x20000000);

    for (segl = first; segl < last; segl++)
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

