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

    unsigned lightmin, lightmax, lightsub, lightcoef;
    unsigned actionbits;
} seglocal_t;

static char seg_lock = 0;

static void R_DrawTextures(int x, int floorclipx, int ceilingclipx, fixed_t scale2, int colnum, unsigned light, seglocal_t* lsegl) ATTR_DATA_CACHE_ALIGN;
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds) ATTR_DATA_CACHE_ALIGN;

static void R_LockSeg(void) ATTR_DATA_CACHE_ALIGN;
static void R_UnlockSeg(void) ATTR_DATA_CACHE_ALIGN;
static void R_SegCommands2(void) ATTR_DATA_CACHE_ALIGN;

void R_SegCommands(void) ATTR_DATA_CACHE_ALIGN;

//
// Render a wall texture as columns
//
static void R_DrawTextures(int x, int floorclipx, int ceilingclipx, fixed_t scale2, int colnum_, unsigned light, seglocal_t* lsegl)
{
   unsigned actionbits = lsegl->actionbits;
   drawtex_t *tex, *last;

   tex = lsegl->tex;
   last = lsegl->tex + 2;

   if (!(actionbits & AC_TOPTEXTURE))
       tex++;
   if (!(actionbits & AC_BOTTOMTEXTURE))
       --last;

#ifdef MARS
   unsigned iscale = SH2_DIVU_DVDNTL; // get 32-bit quotient
#endif

   for (; tex < last; tex++) {
       int top, bottom;

       FixedMul2(top, scale2, tex->topheight);
       top = centerY - top;
       if (top < ceilingclipx)
           top = ceilingclipx;

       FixedMul2(bottom, scale2, tex->bottomheight);
       bottom = centerY - 1 - bottom;
       if (bottom >= floorclipx)
           bottom = floorclipx - 1;

       // column has no length?
       if (top <= bottom)
       {
           fixed_t frac = tex->texturemid - (centerY - top) * iscale;
#ifdef MARS
           inpixel_t* src;
#else
           pixel_t* src;
#endif
           int colnum = colnum_;

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
   }
}

//
// Main seg drawing loop
//
static void R_DrawSeg(seglocal_t* lseg, unsigned short *clipbounds)
{
   viswall_t* segl = lseg->segl;

   const unsigned actionbits = lseg->actionbits;

   unsigned scalefrac = segl->scalefrac;
   unsigned scalestep = segl->scalestep;

   const unsigned centerangle = segl->centerangle;
   const unsigned offset = segl->offset;
   const unsigned distance = segl->distance;

   const int ceilingheight = segl->ceilingheight;

   unsigned texturelight = lseg->lightmax;
   const boolean gradientlight = lseg->lightmin != lseg->lightmax;

   int x, stop = segl->stop;

   for (x = segl->start; x <= stop; x++)
   {
      fixed_t scale;
      int floorclipx, ceilingclipx;
      int top, bottom;
      unsigned scale2;

      scale = scalefrac;
      scale2 = (unsigned)scalefrac >> HEIGHTBITS;
      scalefrac += scalestep;

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx = ceilingclipx & 0x00ff;
      ceilingclipx = (((unsigned)ceilingclipx & 0xff00) >> 8);

      //
      // texture only stuff
      //
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

static void R_LockSeg(void)
{
    int res;
    do {
        __asm volatile (\
        "tas.b %1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "m" (seg_lock) \
            );
    } while (res == 0);
}

static void R_UnlockSeg(void)
{
    seg_lock = 0;
}

void R_SegCommands2(void)
{
    viswall_t* segl;
    seglocal_t lseg;
    drawtex_t* toptex, * bottomtex;
    int extralight;
    unsigned clipbounds_[SCREENWIDTH / 2 + 1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;

    // initialize the clipbounds array
    R_InitClipBounds(clipbounds_);

    extralight = vd.extralight;
    toptex = &lseg.tex[0];
    bottomtex = &lseg.tex[1];

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
        int seglight;
#ifdef MARS
        int state;

        if (segl->start > segl->stop)
            continue;

        while ((state = *(volatile VINT*)&segl->state) == 0);

        if (state == RW_DRAWN || !(segl->actionbits & (AC_CALCTEXTURE | AC_ADDSKY)))
        {
            goto skip_draw;
        }

        R_LockSeg();
        state = *(volatile VINT*)&segl->state;
        switch (state) {
            case RW_READY:
                segl->state = RW_DRAWN;
                break;
            case RW_DRAWN:
                R_UnlockSeg();
                goto skip_draw;
        }
        R_UnlockSeg();
#endif

        lseg.segl = segl;
        
        seglight = segl->seglightlevel + extralight;
#ifdef MARS
        if (seglight < 0)
            seglight = 0;
        if (seglight > 255)
            seglight = 255;
#endif
        lseg.lightmax = seglight;
        lseg.lightmin = lseg.lightmax;

        if (detailmode == detmode_high)
        {
#ifdef MARS
            if (seglight <= 160 + extralight)
                seglight = (seglight >> 1);
#else
            seglight = light - (255 - light) * 2;
            if (seglight < 0)
                seglight = 0;
#endif
            lseg.lightmin = seglight;
        }
        lseg.actionbits = segl->actionbits;

        if (lseg.actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[segl->t_texturenum];
            toptex->topheight = segl->t_topheight;
            toptex->bottomheight = segl->t_bottomheight;
            toptex->texturemid = segl->t_texturemid;
            toptex->width = tex->width;
            toptex->height = tex->height;
            toptex->data = tex->data;
            toptex->pixelcount = 0;
            toptex->drawcol = (tex->height & (tex->height - 1)) ? drawcolnpo2 : drawcol;
        }

        if (lseg.actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[segl->b_texturenum];
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
        }
        else
        {
            lseg.lightmin = HWLIGHT(lseg.lightmax);
            lseg.lightmax = lseg.lightmin;
        }

        R_DrawSeg(&lseg, clipbounds);

        if (lseg.actionbits & AC_TOPTEXTURE)
        {
            segl->t_pixcount = toptex->pixelcount;
        }
        if (lseg.actionbits & AC_BOTTOMTEXTURE)
        {
            segl->b_pixcount = bottomtex->pixelcount;
        }

skip_draw:
        if(segl->actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
        {
            int x, stop = segl->stop;
            unsigned short *newclipbounds = segl->newclipbounds;
            for (x = segl->start; x <= stop; x++)
                clipbounds[x] = newclipbounds[x];
        }
    }
}

#ifdef MARS

void Mars_Sec_R_SegCommands(void)
{
    R_SegCommands2();
}

void R_SegCommands(void)
{
    R_SegCommands2();

    Mars_R_SecWait();
}

#else

void R_SegCommands(void)
{
    R_SegCommands2();
}

#endif

