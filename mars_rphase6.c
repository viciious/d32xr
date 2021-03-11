/*
  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"
#include "mars.h"
#include "mars_ringbuf.h"

typedef struct
{
   VINT     width;
   VINT     height;
   int      topheight;
   int      bottomheight;
   int      texturemid;
   int      pixelcount;
   inpixel_t *data;
} drawtex_t;

static void Mars_Slave_R_ComputeSeg(viswall_t* segl) __attribute__((section(".data"), aligned(16)));
static void Mars_R_SegLoop(viswall_t *segl, int *clipbounds, drawtex_t *toptex, drawtex_t *bottomtex) __attribute__((section(".data"), aligned(16)));

//
// Render a wall texture as columns
//
static inline void Mars_R_DrawTexture(drawtex_t *tex, int x, int top, int bottom,
          unsigned colnum, unsigned light, unsigned iscale, unsigned iscale2)
{
   int frac;
   inpixel_t *src;

   // column has no length?
   if(top > bottom)
      return;

   frac = tex->texturemid - (CENTERY - top) * iscale;

   // DEBUG: fixes green pixels in MAP01...
   frac += iscale2;

   while(frac < 0)
   {
      colnum--;
      frac += tex->height << FRACBITS;
   }

   // CALICO: comment says this, but the code does otherwise...
   // colnum = colnum - tex->width * (colnum / tex->width)
   colnum &= (tex->width - 1);

   src = tex->data + colnum * tex->height;
   I_DrawColumn(x, top, bottom, light, frac, iscale, src, tex->height);

   // pixel counter
   tex->pixelcount += (bottom - top);
}

static inline int Mars_R_WordsForActionbits(unsigned actionbits)
{
    int numwords;

    numwords = 0;
    if (actionbits & AC_CALCTEXTURE)
    {
        numwords = 2 + 1; // iscale + colnum
#ifdef GRADIENTLIGHT
        numwords += 1; // light
#endif
        if (actionbits & AC_TOPTEXTURE)
            numwords += 1; // top and bottom
        if (actionbits & AC_BOTTOMTEXTURE)
            numwords += 1; // top and bottom
    }
    if (actionbits & (AC_ADDFLOOR| AC_ADDCEILING))
        numwords += 1; // top and bottom
    if (actionbits & (AC_NEWFLOOR|AC_NEWCEILING|AC_TOPSIL|AC_BOTTOMSIL))
        numwords += 1; // low and high
    if (actionbits & AC_ADDSKY)
        numwords += 1; // bottom
    return numwords;
}

static void Mars_Slave_R_ComputeSeg(viswall_t* segl)
{
    int x, stop, scale;
    unsigned scalefrac, iscale;
    int low, high, top, bottom;
    int scalestep;
    unsigned texturecol;
    unsigned actionbits;
    unsigned distance;
    unsigned centerangle;
    unsigned offset;
#ifdef GRADIENTLIGHT
    unsigned lightmin, lightmax, lightsub, lightcoef;
    unsigned texturelight;
#endif
    int t_topheight, t_bottomheight;
    int b_topheight, b_bottomheight;
    int floorheight, floornewheight;
    int ceilingheight, ceilingnewheight;
    short *p, numwords;

    actionbits = segl->actionbits;  // cache the first 8 words of segl: actionbits, light, scalefrac, scale2
#ifdef GRADIENTLIGHT
    lightmax = segl->seglightlevel;
#endif
    scalefrac = segl->scalefrac;

    scalestep = segl->scalestep; // cache the second 8 words of segl: scalestep, centerangle, offset, distance
    centerangle = segl->centerangle;
    offset = segl->offset; 
    distance = segl->distance;

    x = segl->start;
    stop = segl->stop;

    t_topheight = segl->t_topheight;
    t_bottomheight = segl->t_bottomheight;
    b_topheight = segl->b_topheight;
    b_bottomheight = segl->b_bottomheight;
    floorheight = segl->floorheight;
    floornewheight = segl->floornewheight;
    ceilingheight = segl->ceilingheight;
    ceilingnewheight = segl->ceilingnewheight;

#ifdef GRADIENTLIGHT
#ifdef MARS
    unsigned seglight = lightmax;
    seglight = seglight - (seglight >> 2) - (seglight >> 4);
#else
    int seglight = lightmax;
    seglight = seglight - ((255 - seglight) << 1);
#endif
    if (seglight < MINLIGHT)
        seglight = MINLIGHT;
    if (seglight > lightmax)
        seglight = lightmax;
    lightmin = (unsigned)seglight;

    lightsub = 160 * (lightmax - lightmin) / (800 - 160);
    lightcoef = ((lightmax - lightmin) << FRACBITS) / (800 - 160);
#endif

    numwords = Mars_R_WordsForActionbits(actionbits);

    p = Mars_RB_GetWriteBuf(&marsrb, 1);
    *p = segl - viswalls;
    Mars_RB_CommitWrite(&marsrb);

    if (numwords == 0)
        return;

    do
    {
        short t;
        byte* b = (byte *)&t;

        scale = scalefrac >> FIXEDTOSCALE;
        scalefrac += scalestep;

        if (scale >= 0x7fff)
            scale = 0x7fff; // fix the scale to maximum

        p = Mars_RB_GetWriteBuf(&marsrb, numwords);

        //
        // texture only stuff
        //
        if (actionbits & AC_CALCTEXTURE)
        {
            // calculate texture offset
            unsigned short* up;

            fixed_t r = FixedMul(distance,
                finetangent((centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT));

            // other texture drawing info
            texturecol = (offset - r) >> FRACBITS;
            iscale = (1 << (FRACBITS + SCALEBITS)) / (unsigned)scale;

            up = (unsigned short*)p, p += 2;
            up[0] = (iscale >> 16) & 0xffff;
            up[1] = (iscale) & 0xffff;
            *p = texturecol, p += 1; // colnum

#ifdef GRADIENTLIGHT
            // calc light level
            texturelight = ((scale * lightcoef) / FRACUNIT);
            if (texturelight <= lightsub)
                texturelight = lightmin;
            else
            {
                texturelight -= lightsub;
                if (texturelight < lightmin)
                    texturelight = lightmin;
                else if (texturelight > lightmax)
                    texturelight = lightmax;
            }
            *p = texturelight, p += 1;
#endif

            //
            // draw textures
            //
            if (actionbits & AC_TOPTEXTURE)
            {
                top = CENTERY - ((scale * t_topheight) >> (HEIGHTBITS + SCALEBITS)) + 1;
                bottom = CENTERY - 1 - ((scale * t_bottomheight) >> (HEIGHTBITS + SCALEBITS)) + 1;

                b[0] = top < 0 ? 0 : (top > 255 ? 255 : top); // t_top
                b[1] = bottom < 0 ? 0 : (bottom > 255 ? 255 : bottom); // t_bottom
                *p = t, p += 1;
            }

            if (actionbits & AC_BOTTOMTEXTURE)
            {
                top = CENTERY - ((scale * b_topheight) >> (HEIGHTBITS + SCALEBITS)) + 1;
                bottom = CENTERY - 1 - ((scale * b_bottomheight) >> (HEIGHTBITS + SCALEBITS)) + 1;

                b[0] = top < 0 ? 0 : (top > 255 ? 255 : top); // b_top
                b[1] = bottom < 0 ? 0 : (bottom > 255 ? 255 : bottom); // b_bottom
                *p = t, p += 1;
            }
        }

        if (actionbits & (AC_ADDFLOOR | AC_ADDCEILING))
        {
            t = 0;

            //
            // floor
            //
            if (actionbits & AC_ADDFLOOR)
            {
                top = CENTERY - ((scale * floorheight) >> (HEIGHTBITS + SCALEBITS)) + 1;
                b[0] = top < 0 ? 0 : (top > 255 ? 255 : top);
            }

            //
            // ceiling
            //
            if (actionbits & AC_ADDCEILING)
            {
                bottom = CENTERY - 1 - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS)) + 1;
                b[1] = bottom < 0 ? 0 : (bottom > 255 ? 255 : bottom);
            }

            *p = t, p += 1;
        }

        //
        // calc high and low
        //
        if (actionbits & (AC_NEWFLOOR | AC_NEWCEILING | AC_TOPSIL | AC_BOTTOMSIL))
        {
            low = CENTERY - ((scale * floornewheight) >> (HEIGHTBITS + SCALEBITS)) + 1;
            high = CENTERY - 1 - ((scale * ceilingnewheight) >> (HEIGHTBITS + SCALEBITS)) + 1;

            b[0] = low < 0 ? 0 : (low > 255 ? 255 : low);
            b[1] = high < 0 ? 0 : (high > 255 ? 255 : high);
            *p = t, p += 1;
        }

        // sky mapping
        if (actionbits & AC_ADDSKY)
        {
            bottom = (CENTERY - ((scale * ceilingheight) >> (HEIGHTBITS + SCALEBITS))) + 1;
            *p = bottom, p += 1; // bottom
        }

        Mars_RB_CommitWrite(&marsrb);
    } while (++x <= stop);
}

void Mars_Slave_R_SegCommands(void)
{
    viswall_t* segl;

    Mars_RB_ResetWrite(&marsrb);

    for (segl = viswalls; segl < lastwallcmd; segl++)
        Mars_Slave_R_ComputeSeg(segl);

    Mars_RB_FinishWrite(&marsrb);
}

//
// Main seg clipping loop
//
static void Mars_R_SegLoop(viswall_t *segl, int *clipbounds, drawtex_t *toptex, drawtex_t *bottomtex)
{
   int x, stop;
   int low, high, top, bottom;
   visplane_t *ceiling, *floor;
   unsigned iscale;
   unsigned texturecol;
   unsigned texturelight;
   unsigned actionbits;
   unsigned seglight;
   int floorclipx, ceilingclipx;
   short numwords;

   x = segl->start;
   stop = segl->stop;
   actionbits = segl->actionbits;
   seglight = segl->seglightlevel;

   numwords = Mars_R_WordsForActionbits(actionbits);
   if (numwords == 0)
       return;

#ifndef GRADIENTLIGHT
   texturelight = seglight;
   texturelight = (((255 - texturelight) >> 3) & 31) * 256;
#endif

   // force R_FindPlane for both planes
   floor = ceiling = visplanes;
   low = high = 0;

   do
   {
      short *p;
      byte* b;

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      ceilingclipx = clipbounds[x];
      floorclipx   = ceilingclipx & 0x00ff;
      ceilingclipx = ((ceilingclipx & 0xff00) >> 8) - 1;

      p = Mars_RB_GetReadBuf(&marsrb, numwords);

      //
      // texture only stuff
      //
      if(actionbits & AC_CALCTEXTURE)
      {
         unsigned iscale2;
         unsigned short *up;

         // other texture drawing info
         up = (unsigned short*)p, p += 2;
         texturecol = *p, p += 1;

#ifdef GRADIENTLIGHT
         texturelight = *p, p += 1;
         texturelight = (((255 - texturelight) >> 3) & 31) * 256;
#endif

         iscale = (up[0] << 16) | up[1];
         iscale2 = iscale >> 4;
         iscale2 = iscale + (iscale2 >> 1) + (iscale2 >> 2);

         //
         // draw textures
         //
         if (actionbits & AC_TOPTEXTURE)
         {
             b = (byte*)p, p++;

             top = (int)b[0] - 1;
             bottom = (int)b[1] - 1;
             if(top <= ceilingclipx)
                 top = ceilingclipx + 1;
             if(bottom >= floorclipx)
                 bottom = floorclipx - 1;

             Mars_R_DrawTexture(toptex, x, top, bottom, texturecol, texturelight, iscale, iscale2);
         }
         if (actionbits & AC_BOTTOMTEXTURE)
         {
             b = (byte*)p, p++;

             top = (int)b[0] - 1;
             bottom = (int)b[1] - 1;
             if(top <= ceilingclipx)
                 top = ceilingclipx + 1;
             if(bottom >= floorclipx)
                 bottom = floorclipx - 1;

             Mars_R_DrawTexture(bottomtex, x, top, bottom, texturecol, texturelight, iscale, iscale2);
         }
      }

      //
      // floor
      //
      if (actionbits & (AC_ADDFLOOR | AC_ADDCEILING))
      {
          b = (byte*)p, p += 1;

          if (actionbits & AC_ADDFLOOR)
          {
              top = (int)b[0] - 1;
              if (top <= ceilingclipx)
                  top = ceilingclipx + 1;
              bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (floor->open[x] != OPENMARK)
                  {
                      floor = R_FindPlane(floor + 1, segl->floorheight, segl->floorpicnum,
                          seglight, x, stop);
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
              bottom = (int)b[1] - 1;
              if (bottom >= floorclipx)
                  bottom = floorclipx - 1;

              if (top <= bottom)
              {
                  if (ceiling->open[x] != OPENMARK)
                  {
                      ceiling = R_FindPlane(ceiling + 1, segl->ceilingheight, segl->ceilingpicnum,
                          seglight, x, stop);
                  }
                  ceiling->open[x] = (unsigned short)((top << 8) + bottom);
              }
          }
      }

      //
      // calc high and low
      //
      if (actionbits & (AC_NEWFLOOR|AC_NEWCEILING|AC_TOPSIL|AC_BOTTOMSIL))
      {
          b = (byte*)p, p++;
          low = (int)b[0] - 1;
          if (low < 0)
              low = 0;
          else if(low > floorclipx)
              low = floorclipx;

          high = (int)b[1] - 1;
          if (high < ceilingclipx)
              high = ceilingclipx;
          else if (high > SCREENHEIGHT - 1)
              high = SCREENHEIGHT - 1;

          // bottom sprite clip sil
          if(actionbits & AC_BOTTOMSIL)
              segl->bottomsil[x] = low;

          // top sprite clip sil
          if(actionbits & AC_TOPSIL)
             segl->topsil[x] = high + 1;
      }

      // sky mapping
      if(actionbits & AC_ADDSKY)
      {
         top = ceilingclipx + 1;
         bottom = *p++;
         
         if(bottom >= floorclipx)
            bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            int colnum = ((vd.viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT) & 0xff;
            inpixel_t * data = skytexturep->data + colnum * skytexturep->height;
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

      Mars_RB_CommitRead(&marsrb);
   }
   while(++x <= stop);
}

void Mars_R_SegCommands(void)
{
   int i;
   int numsegs;
   int* clipbounds, *clip;
   static drawtex_t toptex = { 0 }, bottomtex = { 0 };

   Mars_RB_ResetAll(&marsrb);

   Mars_R_BeginComputeSeg();

   // initialize the clipbounds array
   clipbounds = (int*)&r_workbuf[0];
   clip = clipbounds;
   for (i = 0; i < SCREENWIDTH / 4; i++)
   {
       *clip++ = SCREENHEIGHT;
       *clip++ = SCREENHEIGHT;
       *clip++ = SCREENHEIGHT;
       *clip++ = SCREENHEIGHT;
   }

   numsegs = lastwallcmd - viswalls;
   for (i = 0; i < numsegs; i++)
   {
      short *p;
      short segnum;
      viswall_t* segl;

      p = Mars_RB_GetReadBuf(&marsrb, 1);
      segnum = *p;
      Mars_RB_CommitRead(&marsrb);

      segl = viswalls + segnum;

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

      Mars_R_SegLoop(segl, clipbounds, &toptex, &bottomtex);

      if(segl->actionbits & AC_TOPTEXTURE)
      {
         R_AddPixelsToTexCache(&r_wallscache, segl->t_texturenum, toptex.pixelcount);
      }

      if(segl->actionbits & AC_BOTTOMTEXTURE)
      {
         R_AddPixelsToTexCache(&r_wallscache, segl->b_texturenum, bottomtex.pixelcount);
      }
   }

   Mars_RB_FinishRead(&marsrb);

   Mars_R_EndComputeSeg();
}
