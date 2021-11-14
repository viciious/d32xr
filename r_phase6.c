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
   pixel_t   *data;
#endif
   int       width;
   int       height;
   int       topheight;
   int       bottomheight;
   int       texturemid;
   int       pixelcount;
   drawcol_t drawcol;
} drawtex_t;

typedef struct
{
    viswall_t* segl;

    drawtex_t tex[2];

    unsigned short *clipbounds;
    unsigned lightmin, lightmax, lightsub, lightcoef;
    unsigned actionbits;
} seglocal_t;

static void R_DrawTextures(int x, int floorclipx, int ceilingclipx, fixed_t scale2, int colnum, unsigned light, seglocal_t* lsegl) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_SegLoop(seglocal_t* lseg, const int cpu, boolean gradientlight) __attribute__((always_inline));
void R_SegLoopFlat(seglocal_t* lseg, const int cpu) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void R_SegLoopGradient(seglocal_t* lseg, const int cpu) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_SegCommands2(const int mask) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

//
// Render a wall texture as columns
//
static void R_DrawTextures(int x, int floorclipx, int ceilingclipx, fixed_t scale2, int colnum, unsigned light, seglocal_t* lsegl)
{
   unsigned actionbits = lsegl->actionbits;
   drawtex_t *tex = lsegl->tex, *last;

   last = tex + 1;
   if ((actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE)) == AC_TOPTEXTURE)
       last = tex;
   if ((actionbits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE)) == AC_BOTTOMTEXTURE)
       tex++;

   do {
       int top, bottom;

       FixedMul2(top, scale2, tex->topheight);
       top = centerY - top;
       if (top < ceilingclipx)
           top = ceilingclipx;

       FixedMul2(bottom, scale2, tex->bottomheight);
       bottom = centerY - 1 - bottom;
       if (bottom >= floorclipx)
           bottom = floorclipx - 1;

#ifdef MARS
       unsigned iscale = SH2_DIVU_DVDNTL; // get 32-bit quotient
#endif

       // column has no length?
       if (top <= bottom)
       {
           fixed_t frac = tex->texturemid - (centerY - top) * iscale;
#ifdef MARS
           inpixel_t* src;
#else
           pixel_t* src;
#endif

           // DEBUG: fixes green pixels in MAP01...
           frac += (iscale + (iscale >> 5) + (iscale >> 6));

           while (frac < 0)
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
           tex->drawcol(x, top, bottom, light, frac, iscale, src, tex->height, NULL);

           // pixel counter
           tex->pixelcount += (bottom - top);
       }
   } while (++tex <= last);
}

//
// Main seg clipping loop
//
static void R_SegLoop(seglocal_t* lseg, const int cpu, boolean gradientlight)
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

   unsigned texturelight = lseg->lightmax;

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

      scale = scalefrac;
      scale2 = (unsigned)scalefrac >> HEIGHTBITS;
      scalefrac += scalestep;
#ifdef MARS
      draw = cpu ? (x & 1) != 0 : (x & 1) == 0;
#endif

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx   = ceilingclipx & 0x00ff;
      ceilingclipx = (((unsigned)ceilingclipx & 0xff00) >> 8);

      //
      // calc high and low
      //
      FixedMul2(low, scale2, floornewheight);
      low = centerY - low;
      if (low < 0)
          low = 0;
      else if (low > floorclipx)
          low = floorclipx;

      FixedMul2(high, scale2, ceilingnewheight);
      high = centerY - high;
      if (high > viewportHeight)
          high = viewportHeight;
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
              top = centerY - top;
              if (top < ceilingclipx)
                  top = ceilingclipx;
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
              top = ceilingclipx;
              FixedMul2(bottom, scale2, ceilingheight);
              bottom = centerY - 1 - bottom;
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
              segl->topsil[x] = high;
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
         clipbounds[x] = ((unsigned)(newceilingclipx) << 8) + newfloorclipx;
      }

      //
      // texture only stuff
      //
      if (!draw)
          continue;

      if (actionbits & AC_CALCTEXTURE)
      {
          unsigned colnum;
          fixed_t r;

#ifdef MARS
          SH2_DIVU_DVSR = scale;         // set 32-bit divisor
          SH2_DIVU_DVDNTH = 0;           // set high bits of the 64-bit dividend
          SH2_DIVU_DVDNTL = 0xffffffffu; // set low  bits of the 64-bit dividend, start divide
#else
          unsigned iscale = 0xffffffffu / scale;
#endif

          // calculate texture offset
          r = finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT);
          FixedMul2(r, distance, r);

          // other texture drawing info
          colnum = (offset - r) >> FRACBITS;

          if (gradientlight)
          {
              // calc light level
              texturelight = scale2 * lseg->lightcoef;
              if (texturelight <= lseg->lightsub)
              {
                  texturelight = lseg->lightmin;
              }
              else
              {
                  texturelight -= lseg->lightsub;
                  texturelight /= FRACUNIT;
                  if (texturelight < lseg->lightmin)
                      texturelight = lseg->lightmin;
                  if (texturelight > lseg->lightmax)
                      texturelight = lseg->lightmax;
              }

              // convert to a hardware value
              texturelight = HWLIGHT(texturelight);
          }

          //
          // draw textures
          //
          R_DrawTextures(x, floorclipx, ceilingclipx, scale2, colnum, texturelight, lseg);
      }

      // sky mapping
      if (actionbits & AC_ADDSKY)
      {
          top = ceilingclipx;
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
              drawcol(x, top, bottom, 0, (top * 18204) << 2, FRACUNIT + 7281, data, 128, NULL);
          }
      }
   }
}

void R_SegLoopFlat(seglocal_t* lseg, const int cpu)
{
    R_SegLoop(lseg, cpu, false);
}

void R_SegLoopGradient(seglocal_t* lseg, const int cpu)
{
    R_SegLoop(lseg, cpu, true);
}

static void R_SegCommands2(const int cpu)
{
    int i;
    viswall_t* segl;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;

    // initialize the clipbounds array
    unsigned short clipbounds[SCREENWIDTH];
    unsigned *clip = (unsigned *)clipbounds;
    unsigned clipval = (unsigned)viewportHeight << 16 | viewportHeight;
    for (i = 0; i < viewportWidth / 4; i++)
    {
        *clip++ = clipval;
        *clip++ = clipval;
    }

    toptex = &lseg.tex[0];
    bottomtex = &lseg.tex[1];
    lseg.clipbounds = clipbounds;

    lseg.lightmin = 0;
    lseg.lightmax = 0;
    lseg.lightcoef = 0;
    lseg.lightsub = 0;

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
        lseg.lightmin = lseg.lightmax;

        if (detailmode == detmode_high)
        {
#ifdef MARS
            int seglight = segl->seglightlevel + extralight;
            if (seglight > 255)
                seglight = 255;
            else if (seglight <= 160 + extralight)
                seglight = seglight - (seglight >> 1);
#else
            int seglight = segl->seglightlevel - (255 - segl->seglightlevel) * 2;
            if (seglight < 0)
                seglight = 0;
#endif
            lseg.lightmin = seglight;
        }
        lseg.actionbits = segl->actionbits;

        if (segl->actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[segl->t_texturenum];

#ifdef MARS
            if (cpu == 1)
                Mars_ClearCacheLines((intptr_t)&tex->data & ~15, 1);
#endif

            toptex->topheight = segl->t_topheight;
            toptex->bottomheight = segl->t_bottomheight;
            toptex->texturemid = segl->t_texturemid;
            toptex->width = tex->width;
            toptex->height = tex->height;
            toptex->data = tex->data;
            toptex->pixelcount = 0;
            toptex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
        }

        if (segl->actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[segl->b_texturenum];

#ifdef MARS
            if (cpu == 1)
                Mars_ClearCacheLines((intptr_t)&tex->data & ~15, 1);
#endif

            bottomtex->topheight = segl->b_topheight;
            bottomtex->bottomheight = segl->b_bottomheight;
            bottomtex->texturemid = segl->b_texturemid;
            bottomtex->width = tex->width;
            bottomtex->height = tex->height;
            bottomtex->data = tex->data;
            bottomtex->pixelcount = 0;
            bottomtex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
        }

        if (lseg.lightmin != lseg.lightmax)
        {
            lseg.lightcoef = ((lseg.lightmax - lseg.lightmin) << FRACBITS) / (800 - 160);
            lseg.lightsub = 160 * lseg.lightcoef;
            R_SegLoopGradient(&lseg, cpu);
        }
        else
        {
            lseg.lightmin = HWLIGHT(lseg.lightmax);
            lseg.lightmax = lseg.lightmin;
            R_SegLoopFlat(&lseg, cpu);
        }

        if (cpu == 0)
        {
            if (segl->actionbits & AC_TOPTEXTURE)
            {
                R_AddPixelsToTexCache(&r_texcache, segl->t_texturenum, toptex->pixelcount);
            }

            if (segl->actionbits & AC_BOTTOMTEXTURE)
            {
                R_AddPixelsToTexCache(&r_texcache, segl->b_texturenum, bottomtex->pixelcount);
            }
        }
    }
}

#ifdef MARS

void Mars_Sec_R_SegCommands(void)
{
    Mars_ClearCacheLines((intptr_t)&viswalls & ~15, 1);
    Mars_ClearCacheLines((intptr_t)&lastwallcmd & ~15, 1);
    //Mars_ClearCacheLines((intptr_t)viswalls & ~15, ((lastwallcmd - viswalls) * sizeof(viswall_t) + 15) / 16);

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

