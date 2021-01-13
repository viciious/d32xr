/*
  CALICO

  Renderer phase 7 - Visplanes
*/

#include "r_local.h"

static fixed_t planeheight;
static angle_t planeangle;
static fixed_t planex, planey;
static int     plane_lightcoef, plane_lightsub;
static int     plane_lightmin, plane_lightmax;

static fixed_t basexscale, baseyscale;

static int *pl_stopfp;
static int *pl_fp;

static int spanstart[/*SCREENHEIGHT*/256];
static pixel_t *ds_source;

//
// Render the horizontal spans determined by R_PlaneLoop
//
static void R_MapPlane(void)
{
   int x, y, x2, parm;
   int remaining;
   fixed_t distance, length, xfrac, yfrac, xstep, ystep;
   angle_t angle;
   int light;

   do
   {
      --pl_fp;
      parm = *pl_fp;
      x2 = parm >> FRACBITS;
      y  = (parm >> 8) & 0xff;
      x  = parm & 0xff;
      remaining = x2 - x + 1;

      if(!remaining)
         continue; // nothing to draw (shouldn't happen)

      distance = (planeheight * yslope[y]) >> 12;
      length   = (distance * distscale[x]) >> 14;
      angle    = (planeangle + xtoviewangle[x]) >> ANGLETOFINESHIFT;
      
      xfrac = planex + (((finecosine[angle] >> 1) * length) >> 4);
      yfrac = planey - (((  finesine[angle] >> 1) * length) >> 4);
   
      xstep = (distance * basexscale) >> 4;   
   
      light = plane_lightcoef / distance;

      ystep = (baseyscale * distance) >> 4;

      // finish light calculations
      light -= plane_lightsub;
      if(light > plane_lightmax)
         light = plane_lightmax;
      if(light < plane_lightmin)
         light = plane_lightmin;

      // transform to hardware value
      light = -((255 - light) << 14) & 0xffffff;

      // CALICO: invoke I_DrawSpan here.
      I_DrawSpan(y, x, x2, light, xfrac, yfrac, xstep, ystep, ds_source);

      // Jag-specific blitter setup (equivalent to R_MakeSpans/R_DrawSpan)
      /*
      ; *(int *)0xf0221c = (ystep&0xffff0000)+((xstep>>16)&0xffff); // a1 increment 
      move  mp_ystep,scratch                                    scratch = mp_ystep
      and   mp_ffff0000,scratch                                 scratch &= mp_ffff0000
      move  mp_xstep,scratch2                                   scratch2 = mp_xstep
      shrq  #16,scratch2                                        scratch2 >>= 16
      or    scratch2,scratch                                    scratch |= scratch2
      store	scratch,(mp_blitter+7)                              *(mp_blitter+7) = scratch
      	
      ; *(int *)0xf02220 = (xstep&0xffff) + (ystep<<16); // a1 increment frac 	
      move  mp_xstep,scratch                                    scratch = mp_xstep
      and   mp_ffff,scratch                                     scratch &= mp_ffff
      move  mp_ystep,scratch2                                   scratch2 = mp_ystep
      shlq  #16,scratch2                                        scratch2 <<= 16
      or    scratch2,scratch                                    scratch |= scratch2
      store scratch,(mp_blitter+8)                              *(mp_blitter+8) = scratch
    
      ; *(int *)0xf02218 = (yfrac<<16)+(xfrac&0xffff); // a1 pixel frac 
      move  mp_yfrac,scratch                                    scratch = mp_yfrac
      shlq  #16,scratch                                         scratch <<= 16
      move  mp_xfrac,scratch2                                   scratch2 = mp_xfrac
      and   mp_ffff,scratch2                                    scratch2 &= mp_ffff
      or    scratch2,scratch                                    scratch |= scratch2
      store scratch,(mp_blitter+6)                              *(mp_blitter+6) = scratch
      
      ; *(int *)0xf02230 = (y<<16) + x; // a2 pixel pointers
      shlq  #16,mp_y                                            mp_y <<= 16
      add   mp_x,mp_y                                           mp_y += mp_x
      store mp_y,(mp_blitter+12)                                *(mp_blitter+12) = mp_y
   
      ; *(int *)0xf02270 = light; // iinc
      movei #$f02270,scratch                                    scratch = 0xf02270 // B_IINC
      store mp_light,(scratch)                                  *scratch = mp_light
    
      ;	count = 0;
      moveq #0,mp_count                                         mp_count = 0
    
      mp_stillremaining:
      ;===============
      ;
      ; x axis
      ;
      ;===============
      ; xfrac = (xfrac + count*xstep)&0x3fffff;
      ; xpos = xfrac;
      ; if(xstep >= 0)
      ;    xpos = 0x400000 - xpos;
      ; xremain = (xpos / axstep);
      move  mp_axstep,scratch                                   scratch = mp_axstep
      move  mp_axstep,scratch2                                  scratch2 = mp_axstep
      shrq  #16,scratch2                                        scratch2 >>= 16
      mult  mp_count,scratch                                    scratch *= mp_count
      mult  mp_count,scratch2                                   scratch2 *= mp_count
      shlq  #16,scratch2                                        scratch2 <<= 16
      add   scratch2,scratch                                    scratch += scratch2
      or    mp_xstep,mp_xstep                                   mp_xstep |= mp_xstep
      jr    PL,mp_addxpos                                       if( <= ) goto mp_addxpos //????
      nop
      sub   scratch,mp_xfrac                                    mp_xfrac -= scratch
      and   mp_3fffff,mp_xfrac                                  mp_xfrac &= mp_3fffff
      jr    T,mp_xadded                                         goto mp_xadded
      move  mp_xfrac,mp_xremain ; delay slot                       mp_xremain = mp_xfrac // before jump
    
      mp_addxpos:
      add      scratch,mp_xfrac                                 mp_xfrac += scratch
      and      mp_3fffff,mp_xfrac                               mp_xfrac &= mp_3fffff
      movefa   alt_400000,mp_xremain                            mp_xremain = alt_400000
      sub      mp_xfrac,mp_xremain                              mp_xremain -= mp_xfrac
      mp_xadded:
      div      mp_axstep,mp_xremain                             mp_xremain /= mp_axstep
    
      ;===============
      ;
      ; y axis
      ;
      ;===============
      ; yfrac = (yfrac + count*ystep)&0x3fffff;
      ; yremain = (ypos / aystep) + 1;
      move  mp_aystep,scratch                                   scratch = mp_aystep
      move  mp_aystep,scratch2                                  scratch2 = mp_aystep
      shrq  #16,scratch2                                        scratch2 >>= 16
      mult  mp_count,scratch                                    scratch *= mp_count
      mult  mp_count,scratch2                                   scratch2 *= mp_count
      shlq  #16,scratch2                                        scratch2 <<= 16
      add   scratch2,scratch                                    scratch += scratch2
      or    mp_ystep,mp_ystep                                   mp_ystep |= mp_ystep
      jr    PL,mp_addypos                                       if( <= ) goto mp_addypos // ????
      nop
      sub   scratch,mp_yfrac                                    mp_yfrac -= scratch
      and   mp_3fffff,mp_yfrac                                  mp_yfrac &= mp_3fffff
      jr    T,mp_yadded                                         goto mp_yadded
      move  mp_yfrac,mp_yremain ; delay slot                       mp_yremain = mp_yfrac // before jump
      
      mp_addypos:
      add      scratch,mp_yfrac                                 mp_yfrac += scratch
      and      mp_3fffff,mp_yfrac                               mp_yfrac &= mp_3fffff
      movefa   alt_400000,mp_yremain                            mp_yremain = alt_400000
      sub      mp_yfrac,mp_yremain                              mp_yremain -= mp_yfrac
      
      mp_yadded:
      div      mp_aystep,mp_yremain                             mp_yremain /= mp_aystep
    
      ;====================
      ; prepare blitter registers
      ;====================
      
      ; *(int *)0xf0220c = (yfrac&0xffff0000)+(xfrac>>16); // a1 pixel 
      move  mp_yfrac,mp_a1pixel                                 mp_a1pixel = mp_yfrac
      and   mp_ffff0000,mp_a1pixel                              mp_a1pixel &= mp_ffff0000
      move  mp_xfrac,scratch2                                   scratch2 = mp_xfrac
      shrq  #16,scratch2                                        scratch2 >>= 16
      or    scratch2,mp_a1pixel                                 mp_a1pixel |= scratch2

      ;=====================
      ;		count = remaining;
      ;		if (xremain < count)
      ;			count = xremain;
      ;		if (yremain < count)
      ;			count = yremain;
      ; ramaining will allways be at least 1
      ; xremain and yremain will allways be 0 or greater
      ;=====================
    
      move  mp_remaining,mp_count                               mp_count = mp_remaining
      subq  #1,mp_count                                         mp_count -= 1
      cmp   mp_xremain,mp_count                                 if(mp_xremain > mp_count)
      jr    U_GT,mp_notxremain                                     goto mp_notxremain
      nop
      move  mp_xremain,mp_count                                 mp_count = mp_xremain
      mp_notxremain:
      cmp   mp_yremain,mp_count                                 if(mp_yremain > mp_count)
      jr    U_GT,mp_notyremain                                     goto mp_notyremain
      nop
      move  mp_yremain,mp_count                                 mp_count = mp_yremain
      mp_notyremain:
      
      addq  #1,mp_count                                         mp_count += 1
      
      ;=====================
      ; program blitter
      ;=====================
      
      store mp_a1pixel,(mp_blitter+3) ; source location         *(mp_blitter+3) = mp_a1pixel
      bset  #16,scratch ; one outer loop                        scratch |= (1<<16)
      store scratch,(mp_blitter+15) ; count register            *(mp_blitter+15) = scratch
      store mp_blitcommand,(mp_blitter+14) ; command register   *(mp_blitter+14) = mp_blitcommand
      ;===========================
      
      ; remaining -= count;
      movei #mp_stillremaining,scratch                          scratch = &mp_stillremaining
      sub   mp_count,mp_remaining ; delay slot                  mp_remaining -= mp_count
      jump  NE,(scratch)                                        if(mp_remaining) goto *scratch
      nop
    
      ;
      ; all done with this line, see if there are more to do
      ;
      mp_linedone:
      */
   }
   while(pl_fp != pl_stopfp);
}

//
// Determine the horizontal spans of a single visplane
//
static void R_PlaneLoop(visplane_t *pl)
{
   int pl_x, pl_stopx;
   unsigned short *pl_openptr;
   unsigned short t1, t2, b1, b2, pl_oldtop, pl_oldbottom;

   pl_x       = pl->minx;
   pl_stopx   = pl->maxx;

   // see if there is any open space
   if(pl_x > pl_stopx)
      return; // nothing to map

   pl_stopx += 2;

   // CALICO: use the temp buffer, as the native stack cannot be pushed/popped here
   pl_stopfp = (int *)(I_TempBuffer());
   pl_fp = pl_stopfp;

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
            *pl_fp++ = ((pl_x - 1) << FRACBITS) | (t1 << 8) | spanstart[t1];
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
            *pl_fp++ = ((pl_x - 1) << FRACBITS) | (b1 << 8) | spanstart[b1];
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

   // all done calculating, so execute the plane commands
   if(pl_fp != pl_stopfp)
      R_MapPlane();
}

//
// Render all visplanes
//
void R_DrawPlanes(void)
{
   angle_t angle;
   visplane_t *pl;

   planex =  viewx;
   planey = -viewy;

   planeangle = viewangle;
   angle = (planeangle - ANG90) >> ANGLETOFINESHIFT;

   basexscale =  (finecosine[angle] / (SCREENWIDTH / 2));
   baseyscale = -(  finesine[angle] / (SCREENWIDTH / 2));

   // Jag-specific setup
   /*
   movei #15737348,r0                    r0 = 0xf02204 // BLIT_A1FLAGS
   movei #,r1                            r1 = 208928   // 0x33020 = pixel size 2^4, width 64, xadd 0b11 (add increment)
   store r1,(r0)                         *r0 = r1
  
   movei #15737384,r0                    r0 = 0xf02228 // BLIT_A2FLAGS
   movei #80416,r1                       r1 = 80416    // 0x13A20 = pixel size 2^4, width 160, xadd 0b01 (add pixel size)
   store r1,(r0)                         *r0 = r1
   */

   pl = visplanes + 1;
   while(pl < lastvisplane)
   {
      if(pl->minx <= pl->maxx)
      {
         int light;

         ds_source = pl->picnum;

         planeheight = D_abs(pl->height);

         light = pl->lightlevel;
         plane_lightmin = light - ((255 - light) << 1);
         if(plane_lightmin < 0)
            plane_lightmin = 0;
         plane_lightmax  = light;
         plane_lightsub  = ((light - plane_lightmin) * 160) / 640;
         plane_lightcoef = (light - plane_lightmin) << SLOPEBITS;

         pl->open[pl->maxx + 1] = OPENMARK;
         pl->open[pl->minx - 1] = OPENMARK;

         R_PlaneLoop(pl);
      }

      ++pl;
   }
}

// EOF

