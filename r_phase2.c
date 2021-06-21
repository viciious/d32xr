/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static sector_t emptysector = { 0, 0, -2, -2, -2 };

static void R_WallPrepStride(const int start, const int stride) ATTR_DATA_CACHE_ALIGN;
static void R_FinishWallPrep(void) ATTR_DATA_CACHE_ALIGN;
void R_WallPrep(void) ATTR_DATA_CACHE_ALIGN;

static void R_WallPrepStride(const int start, const int stride)
{
   viswall_t *segl;
   viswall_t *lastwall = lastwallcmd;
   seg_t     *seg;
   line_t    *li;
   side_t    *si;
   sector_t  *front_sector, *back_sector;
   fixed_t    f_floorheight, f_ceilingheight;
   fixed_t    b_floorheight, b_ceilingheight;
   int        f_lightlevel, b_lightlevel;
   int        f_ceilingpic, b_ceilingpic;
   int        b_texturemid, t_texturemid;
   boolean    skyhack;
   unsigned int actionbits;
   int        side;

   for(segl = viswalls + start; segl < lastwall; segl += stride)
   {
      seg  = segl->seg;
      li   = seg->linedef;
      side = seg->side;
      si   = &sides[li->sidenum[seg->side]];

      front_sector    = &sectors[sides[li->sidenum[side]].sector];
      f_ceilingpic    = front_sector->ceilingpic;
      f_lightlevel    = front_sector->lightlevel;
      f_floorheight   = front_sector->floorheight   - vd.viewz;
      f_ceilingheight = front_sector->ceilingheight - vd.viewz;

      segl->floorpicnum   = flattranslation[front_sector->floorpic];
      segl->ceilingpicnum = f_ceilingpic == -1 ? -1 : flattranslation[f_ceilingpic];

      back_sector = (li->flags & ML_TWOSIDED) ? &sectors[sides[li->sidenum[side^1]].sector] : 0;
      if(!back_sector)
         back_sector = &emptysector;
      b_ceilingpic    = back_sector->ceilingpic;
      b_lightlevel    = back_sector->lightlevel;
      b_floorheight   = back_sector->floorheight   - vd.viewz;
      b_ceilingheight = back_sector->ceilingheight - vd.viewz;

      t_texturemid = b_texturemid = 0;
      actionbits = 0;

      // deal with sky ceilings (also missing in 3DO)
      if(f_ceilingpic == -1 && b_ceilingpic == -1)
         skyhack = true;
      else 
         skyhack = false;

      // add floors and ceilings if the wall needs them
      if(f_floorheight < 0 &&                                // is the camera above the floor?
         (front_sector->floorpic != back_sector->floorpic || // floor texture changes across line?
          f_floorheight   != b_floorheight                || // height changes across line?
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         segl->floorheight = segl->floornewheight = f_floorheight / (1 << FIXEDTOHEIGHT);
         actionbits |= (AC_ADDFLOOR|AC_NEWFLOOR);
      }

      if(!skyhack                                         && // not a sky hack wall
         (f_ceilingheight > 0 || f_ceilingpic == -1)      && // ceiling below camera, or sky
         (f_ceilingpic    != b_ceilingpic                 || // ceiling texture changes across line?
          f_ceilingheight != b_ceilingheight              || // height changes across line?              
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         segl->ceilingheight = segl->ceilingnewheight = f_ceilingheight / (1 << FIXEDTOHEIGHT);
         if(f_ceilingpic == -1)
            actionbits |= (AC_ADDSKY|AC_NEWCEILING);
         else
            actionbits |= (AC_ADDCEILING|AC_NEWCEILING);
      }

      segl->t_topheight = f_ceilingheight / (1 << FIXEDTOHEIGHT); // top of texturemap

      if(back_sector == &emptysector)
      {
         // single-sided line
         segl->t_texturenum = texturetranslation[si->midtexture];

         // handle unpegging (bottom of texture at bottom, or top of texture at top)
         if(li->flags & ML_DONTPEGBOTTOM)
            t_texturemid = f_floorheight + (textures[segl->t_texturenum].height << FRACBITS);
         else
            t_texturemid = f_ceilingheight;

         t_texturemid += si->rowoffset;                               // add in sidedef texture offset
         segl->t_bottomheight = f_floorheight / (1 << FIXEDTOHEIGHT); // set bottom height
         actionbits |= (AC_SOLIDSIL|AC_TOPTEXTURE);                   // solid line; draw middle texture only
      }
      else
      {
         // two-sided line

         // is bottom texture visible?
         if(b_floorheight > f_floorheight)
         {
            segl->b_texturenum = texturetranslation[si->bottomtexture];
            if(li->flags & ML_DONTPEGBOTTOM)
               b_texturemid = f_ceilingheight;
            else
               b_texturemid = b_floorheight;

            b_texturemid += si->rowoffset; // add in sidedef texture offset

            segl->b_topheight = segl->floornewheight = b_floorheight / (1 << FIXEDTOHEIGHT);
            segl->b_bottomheight = f_floorheight / (1 << FIXEDTOHEIGHT);
            actionbits |= (AC_BOTTOMTEXTURE|AC_NEWFLOOR); // generate bottom wall and floor
         }

         // is top texture visible?
         if(b_ceilingheight < f_ceilingheight && !skyhack)
         {
            segl->t_texturenum = texturetranslation[si->toptexture];
            if(li->flags & ML_DONTPEGTOP)
               t_texturemid = f_ceilingheight;
            else
               t_texturemid = b_ceilingheight + (textures[segl->t_texturenum].height << FRACBITS);

            t_texturemid += si->rowoffset; // add in sidedef texture offset

            segl->t_bottomheight = segl->ceilingnewheight = b_ceilingheight / (1 << FIXEDTOHEIGHT);
            actionbits |= (AC_NEWCEILING|AC_TOPTEXTURE); // draw top texture and ceiling
         }

         // check if this wall is solid, for sprite clipping
         if(b_floorheight >= f_ceilingheight || b_ceilingheight <= f_floorheight)
            actionbits |= AC_SOLIDSIL;
         else
         {
            if((b_floorheight > 0 && b_floorheight > f_floorheight) ||
               (f_floorheight < 0 && f_floorheight > b_floorheight))
            {
               actionbits |= AC_BOTTOMSIL; // set bottom mask
            }

            if(!skyhack)
            {
               if((b_ceilingheight <= 0 && b_ceilingheight < f_ceilingheight) ||
                  (f_ceilingheight >  0 && b_ceilingheight > f_ceilingheight))
               {
                  actionbits |= AC_TOPSIL; // set top mask
               }
            }
         }
      }

      // save local data to the viswall structure
      segl->actionbits    = actionbits;
      segl->t_texturemid  = t_texturemid;
      segl->b_texturemid  = b_texturemid;
      segl->seglightlevel = f_lightlevel;
      segl->offset        = si->textureoffset + seg->offset;
   }
}

#ifdef MARS

void Mars_Slave_R_WallPrep(void)
{
    R_WallPrepStride(1, 2);
}

#endif

static void R_FinishWallPrep(void)
{
    viswall_t* segl;

    for (segl = viswalls; segl < lastwallcmd; segl++)
    {
        int rw_x, rw_stopx;
        unsigned width;
        unsigned actionbits;
        line_t* li;

        li = segl->seg->linedef;
        li->flags |= ML_MAPPED; // mark as seen

        actionbits = segl->actionbits;
        if (!(actionbits & (AC_BOTTOMSIL | AC_TOPSIL)))
            continue;

        // NB: this code is missing in 3DO (function uses parameters instead)
        rw_x = segl->start;
        rw_stopx = segl->stop + 1;
        if (rw_stopx < rw_x)
            continue;

        // get width of opening
        // note this is halved because openings are treated like bytes 
        // everywhere despite using short storage
        width = (rw_stopx - rw_x + 1) >> 1;

        if (actionbits & AC_BOTTOMSIL)
        {
            segl->bottomsil = (byte*)lastopening - rw_x;
            lastopening += width;
        }

        if (actionbits & AC_TOPSIL)
        {
            segl->topsil = (byte*)lastopening - rw_x;
            lastopening += width;
        }
    }
}

void R_WallPrep(void)
{
#ifdef MARS
    Mars_R_BeginWallPrep();

    R_WallPrepStride(0, 2);

    Mars_R_EndWallPrep();
#else
    R_WallPrepStride(0, 1);
#endif

    R_FinishWallPrep();
}

// EOF

