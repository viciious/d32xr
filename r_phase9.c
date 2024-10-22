/*
  CALICO

  Renderer phase 9 - Refresh
*/
#include "doomdef.h"
#include "r_local.h"

#ifdef MARS

//
// R_UpdateCache
static void R_UpdateCache(void)
{
   viswall_t *wall;
   int i;
   int bestmips[MIPLEVELS];
   int minplanemip, maxplanemip;

	  if (debugmode == DEBUGMODE_NOTEXCACHE)
		  return;

   for (i = 0; i < MIPLEVELS; i++) {
      bestmips[i] = -1;
   }

   minplanemip = 0;
   maxplanemip = 0;
   for (wall = vd->viswalls; wall < vd->lastwallcmd; wall++)
   {
      int minmip = wall->miplevels[0], maxmip = wall->miplevels[1];

      if (wall->realstart > wall->realstop)
        continue;

      if ((wall->actionbits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE|AC_MIDTEXTURE)) && (maxmip >= minmip))
      {
        if (wall->actionbits & AC_TOPTEXTURE)
        {
            texture_t* tex = &textures[wall->t_texturenum];
#if MIPLEVELS > 1
            int mipcount = tex->mipcount;
#else
            int mipcount = 1;
#endif
            for (i = minmip; i <= maxmip; i++) {
                if (i >= mipcount)
                  break;
                if (!R_TouchIfInTexCache(&r_texcache, tex->data[i]) && (bestmips[i] < 0)) {
                    bestmips[i] = wall->t_texturenum;
                }
            }
        }

        if (wall->actionbits & AC_BOTTOMTEXTURE)
        {
            texture_t* tex = &textures[wall->b_texturenum];
#if MIPLEVELS > 1
            int mipcount = tex->mipcount;
#else
            int mipcount = 1;
#endif
            for (i = minmip; i <= maxmip; i++) {
                if (i >= mipcount)
                  break;
                if (!R_TouchIfInTexCache(&r_texcache, tex->data[i]) && (bestmips[i] < 0)) {
                    bestmips[i] = wall->b_texturenum;
                }
            }
        }

        if (wall->actionbits & AC_MIDTEXTURE)
        {
            texture_t* tex = &textures[wall->m_texturenum];
#if MIPLEVELS > 1
            int mipcount = tex->mipcount;
#else
            int mipcount = 1;
#endif
            for (i = minmip; i <= maxmip; i++) {
                if (i >= mipcount)
                  break;
                if (!R_TouchIfInTexCache(&r_texcache, tex->data[0]) && (bestmips[i] < 0)) {
                    bestmips[i] = wall->m_texturenum;
                }
            }
        }

        // offset the min mip level for planes by -1:
        // assume planes can be slightly closer than 
        // the adjacent walls
        if (minmip-1 > minplanemip)
            minplanemip = minmip-1;
        if (maxmip > maxplanemip)
            maxplanemip = maxmip;
      }

      if (detailmode != detmode_potato)
      {
        flattex_t *flat = &flatpixels[wall->floorpicnum];

        if (minplanemip < 0)
          minplanemip = 0;
        if (maxplanemip >= MIPLEVELS)
          maxplanemip = MIPLEVELS-1;

        for (i = minplanemip; i <= maxplanemip; i++) {
            if (!R_TouchIfInTexCache(&r_texcache, flat->data[i]) && (bestmips[i] < 0)) {
                bestmips[i] = numtextures+wall->floorpicnum;
            }
        }

        if (wall->ceilingpicnum == -1) {
            continue;
        }

        flat = &flatpixels[wall->ceilingpicnum];
        for (i = minplanemip; i <= maxplanemip; i++) {
            if (!R_TouchIfInTexCache(&r_texcache, flat->data[i]) && (bestmips[i] < 0)) {
                bestmips[i] = numtextures+wall->ceilingpicnum;
            }
        }
      }
   }

   for (i = 0; i < MIPLEVELS; i++) {
      int id;
      void **data, **pdata;
      unsigned w = 64, h = 64, m, pixels;
      boolean masked = false;

      id = bestmips[i];
      if (id == -1) {
        continue;
      }

      masked = false;

      if (id >= numtextures) {
        flattex_t *flat = &flatpixels[id - numtextures];
        data = (void **)flat->data;
        pdata = (void**)&data[i];
        pixels = w * h;
      } else {
        texture_t* tex = &textures[id];
        int lump = tex->lumpnum;

        data = (void **)tex->data;
        if (lump >= firstsprite && lump < firstsprite + numsprites) {
          masked = true;
          pixels = W_LumpLength(lump+1);
          pdata = (void**)&data[0];
        } else {
          w = tex->width, h = tex->height;
          pixels = w * h;
          pdata = (void**)&data[i];
        }
      }

      if (i > 0 && !masked) {
        m = i;
        do {
            w >>= 1;
            h >>= 1;
        } while (--m);
        if (w < 1)
          w = 1;
        if (h < 1)
          h = 1;
        pixels = w * h;
      }

      if (R_InTexCache(&r_texcache, *pdata)) {
        continue;
      }

      R_AddToTexCache(&r_texcache, id+((unsigned)i<<2), pixels, pdata);

      if (debugmode == DEBUGMODE_TEXCACHE)
        continue;

      if (id < numtextures && !masked) {
        int j;
        texture_t* tex = &textures[id];
        uint8_t *src = *pdata;
        uint8_t *dst;

        for (j = 0; j < tex->width; j++) {
          boolean decaled;

          I_GetThreadLocalVar(DOOMTLS_COLUMNCACHE, dst);

          decaled = R_CompositeColumn(j, tex->decals & 0x3, &decals[tex->decals >> 2],
            src, dst, h, i);
          if (decaled) {
            D_memcpy(src, dst, h);
          }

          src += h;
        }
      }
   }

    R_PostTexCacheFrame(&r_texcache);
}

#endif

void R_Update(void)
{
#ifndef MARS
   // CALICO: Invoke I_Update
   I_Update();
#endif

#ifdef MARS
   R_UpdateCache();
#endif

   // NB: appears completely Jag-specific; our drawing may end in phase 8.
   /*
 subq #16,FP

 movei #_worklist,r0
 load (r0),r0
 shlq #2,r0
 movei #_listbuffers,r1
 add r1,r0
 load (r0),r0
 move r0,r15 ;(worklist_p)
 move r15,r0 ;(worklist_p)
 movei #_listbuffer,r1
 sub r1,r0
 move r0,r18 ;(delta)
 move FP,r0
 addq #4,r0 ; &branch1
 movei #_a_vde,r1
 loadw (r1),r1
 movei #$ffff8000,scratch
 add scratch,r1
 xor scratch,r1
 movei #-3,r2
 sha r2,r1
 movei #32771,r3
 add r3,r1
 store r1,(r0)
 move FP,r0
 addq #8,r0 ; &branch2
 movei #_a_vdb,r1
 loadw (r1),r1
 movei #$ffff8000,scratch
 add scratch,r1
 xor scratch,r1
 sha r2,r1
 movei #16387,r2
 add r2,r1
 store r1,(r0)
 movei #_stopobj,r0
 sharq #3,r0
 move r0,r16 ;(link)
 move r16,r0 ;(link)
 shrq #8,r0
 store r0,(r15) ;(worklist_p)

 move r15,r0 ;(worklist_p)
 addq #4,r0
 move r16,r1 ;(link)
 shlq #24,r1
 load (FP+1),r2 ; local branch1
 add r2,r1
 store r1,(r0)

 moveq #8,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r2 ;(link)
 sh r0,r2
 move r2,r0
 store r0,(r1)

 move r15,r0 ;(worklist_p)
 addq #12,r0
 move r16,r1 ;(link)
 shlq #24,r1
 load (FP+2),r2 ; local branch2
 add r2,r1
 store r1,(r0)

 movei #48,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r1,r0
 sub r18,r0 ;(delta)
 sharq #3,r0
 move r0,r16 ;(link)
 move r15,r0 ;(worklist_p)
 addq #16,r0
 move r16,r1 ;(link)
 shrq #8,r1
 store r1,(r0)

 move r15,r0 ;(worklist_p)
 addq #20,r0
 move r16,r1 ;(link)
 shlq #24,r1
 movei #16384,r2
 add r2,r1
 movei #3670016,r2
 add r2,r1
 movei #_BASEORGY,r2
 load (r2),r2
 movei #180,r3
 add r3,r2
 addq #1,r2
 shlq #4,r2
 add r2,r1
 addq #3,r1
 store r1,(r0)

 move r15,r0 ;(worklist_p)
 addq #24,r0
 move r16,r1 ;(link)
 shrq #8,r1
 store r1,(r0)

 move r15,r0 ;(worklist_p)
 addq #28,r0
 move r16,r1 ;(link)
 shlq #24,r1
 movei #32768,r2
 add r2,r1
 movei #3670016,r2
 add r2,r1
 movei #_BASEORGY,r2
 load (r2),r2
 movei #180,r3
 add r3,r2
 addq #1,r2
 shlq #4,r2
 add r2,r1
 addq #3,r1
 store r1,(r0)

 move r15,r0 ;(worklist_p)
 addq #32,r0
 moveq #0,r1
 store r1,(r0)

 movei #36,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #_BASEORGY,r0
 load (r0),r0
 movei #180,r2
 add r2,r0
 addq #1,r0
 shlq #4,r0
 addq #2,r0
 store r0,(r1)

 movei #_stopobj,r0
 sharq #3,r0
 move r0,r16 ;(link)
 movei #40,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r0 ;(link)
 shrq #8,r0
 store r0,(r1)

 movei #44,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r0 ;(link)
 shlq #24,r0
 movei #36827,r2
 add r2,r0
 store r0,(r1)

 movei #_shadepixel,r0
 load (r0),r0
 moveq #0,r1
 cmp r0,r1
 movei #L52,scratch
 jump EQ,(scratch)
 nop

 movei #64,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r1,r0
 sub r18,r0 ;(delta)
 sharq #3,r0
 move r0,r16 ;(link)

 movei #L53,r0
 jump T,(r0)
 nop

L52:
 movei #80,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r1,r0
 sub r18,r0 ;(delta)
 sharq #3,r0
 move r0,r16 ;(link)

L53:
 movei #40,r0
 move r0,r17 ;(pwidth)
 movei #48,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #_workingscreen,r0
 load (r0),r0
 shlq #8,r0
 move r16,r2 ;(link)
 shrq #8,r2
 add r2,r0
 store r0,(r1)

 movei #52,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r0 ;(link)
 shlq #24,r0
 movei #2949120,r2
 add r2,r0
 movei #_BASEORGY,r2
 load (r2),r2
 shlq #4,r2
 add r2,r0
 store r0,(r1)

 movei #56,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shrq #4,r0
 store r0,(r1)

 move FP,r0 ; &temp
 move r17,r1 ;(pwidth)
 shlq #28,r1
 move r17,r2 ;(pwidth)
 shlq #18,r2
 add r2,r1
 movei #32768,r2
 add r2,r1
 movei #16384,r2
 add r2,r1
 addq #7,r1
 store r1,(r0)
 movei #60,r1
 move r15,r2 ;(worklist_p)
 add r1,r2
 load (r0),r0
 store r0,(r2)

 movei #80,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r1,r0
 sub r18,r0 ;(delta)
 sharq #3,r0
 move r0,r16 ;(link)
 movei #64,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #_screenshade,r0
 load (r0),r0
 shlq #8,r0
 move r16,r2 ;(link)
 shrq #8,r2
 add r2,r0
 store r0,(r1)

 movei #68,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r0 ;(link)
 shlq #24,r0
 movei #2949120,r2
 add r2,r0
 movei #_BASEORGY,r2
 load (r2),r2
 shlq #4,r2
 add r2,r0
 store r0,(r1)

 movei #72,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #16386,r0
 store r0,(r1)

 movei #76,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #40,r0
 shlq #28,r0
 moveq #0,r2
 add r2,r0
 add r2,r0
 movei #16384,r2
 add r2,r0
 addq #7,r0
 store r0,(r1)

 movei #96,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r1,r0
 sub r18,r0 ;(delta)
 sharq #3,r0
 move r0,r16 ;(link)
 movei #40,r0
 move r0,r17 ;(pwidth)
 movei #80,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #_sbar,r0
 load (r0),r0
 addq #16,r0
 shlq #8,r0
 move r16,r2 ;(link)
 shrq #8,r2
 add r2,r0
 store r0,(r1)

 move FP,r0
 addq #12,r0 ; &temp
 movei #_BASEORGY,r1
 load (r1),r1
 movei #180,r2
 add r2,r1
 addq #1,r1
 store r1,(r0)
 movei #84,r1
 move r15,r2 ;(worklist_p)
 add r1,r2
 move r16,r1 ;(link)
 shlq #24,r1
 movei #_sbar,r3
 load (r3),r3
 addq #2,r3
 loadw (r3),r3
 movei #$ffff8000,scratch
 add scratch,r3
 xor scratch,r3
 shlq #14,r3
 add r3,r1
 load (r0),r0
 shlq #4,r0
 add r0,r1
 move r1,r0
 store r0,(r2)

 movei #88,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shrq #4,r0
 store r0,(r1)

 movei #92,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shlq #28,r0
 move r17,r2 ;(pwidth)
 shlq #18,r2
 add r2,r0
 movei #32768,r2
 add r2,r0
 movei #12288,r2
 add r2,r0
 store r0,(r1)

 movei #_debugscreenactive,r0
 load (r0),r0
 moveq #0,r1
 cmp r0,r1
 movei #L54,scratch
 jump EQ,(scratch)
 nop

 movei #112,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r1,r0
 sub r18,r0 ;(delta)
 sharq #3,r0
 move r0,r16 ;(link)

 movei #L55,r0
 jump T,(r0)
 nop

L54:
 movei #_stopobj,r0
 sharq #3,r0
 move r0,r16 ;(link)

L55:
 movei #96,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #_sbartop,r0
 load (r0),r0
 shlq #8,r0
 move r16,r2 ;(link)
 shrq #8,r2
 add r2,r0
 store r0,(r1)

 movei #100,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r0 ;(link)
 shlq #24,r0
 movei #_sbar,r2
 load (r2),r2
 addq #2,r2
 loadw (r2),r2
 movei #$ffff8000,scratch
 add scratch,r2
 xor scratch,r2
 shlq #14,r2
 add r2,r0
 movei #_BASEORGY,r2
 load (r2),r2
 movei #180,r3
 add r3,r2
 addq #1,r2
 shlq #4,r2
 add r2,r0
 store r0,(r1)

 movei #104,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shrq #4,r0
 movei #32768,r2
 add r2,r0
 store r0,(r1)

 movei #108,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shlq #28,r0
 move r17,r2 ;(pwidth)
 shlq #18,r2
 add r2,r0
 movei #32768,r2
 add r2,r0
 movei #12288,r2
 add r2,r0
 store r0,(r1)

 movei #_stopobj,r0
 sharq #3,r0
 move r0,r16 ;(link)
 moveq #4,r0
 move r0,r17 ;(pwidth)
 movei #112,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 movei #_debugscreen,r0
 load (r0),r0
 shlq #8,r0
 move r16,r2 ;(link)
 shrq #8,r2
 add r2,r0
 store r0,(r1)

 movei #116,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r16,r0 ;(link)
 shlq #24,r0
 movei #3538944,r2
 add r2,r0
 movei #_BASEORGY,r2
 load (r2),r2
 shlq #4,r2
 add r2,r0
 store r0,(r1)

 movei #120,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shrq #4,r0
 movei #39936,r2
 add r2,r0
 store r0,(r1)

 movei #124,r0
 move r15,r1 ;(worklist_p)
 add r0,r1
 move r17,r0 ;(pwidth)
 shlq #28,r0
 move r17,r2 ;(pwidth)
 shlq #18,r2
 add r2,r0
 movei #32768,r2
 add r2,r0
 addq #7,r0
 store r0,(r1)

L56:
 movei #_junk,r0
 movei #_ticcount,r1
 load (r1),r1
 store r1,(r0)

L57:
 movei #_junk,r0
 load (r0),r0
 movei #_lastticcount,r1
 load (r1),r1
 sub r1,r0
 moveq #2,r1
 cmp r0,r1
 movei #L56,scratch
 jump S_LT,(scratch)
 nop

 movei #_isrvmode,r0
 movei #3777,r1
 store r1,(r0)
 movei #_readylist_p,r0
 store r15,(r0) ;(worklist_p)

L59:
 movei #_junk,r0
 movei #_displaylist_p,r1
 load (r1),r1
 store r1,(r0)

L60:
 movei #_junk,r0
 load (r0),r0
 movei #_readylist_p,r1
 load (r1),r1
 cmp r0,r1
 movei #L59,scratch
 jump NE,(scratch)
 nop

 movei #_lasttics,r0
 movei #_ticcount,r1
 load (r1),r2
 movei #_lastticcount,r3
 load (r3),r4
 sub r4,r2
 store r2,(r0)
 load (r1),r0
 store r0,(r3)
 movei #_screenshade,r0
 load (r0),r0
 move r0,r1
 addq #4,r1
 movei #_shadepixel,r2
 load (r2),r2
 move r2,r3
 shlq #16,r3
 add r2,r3
 store r3,(r1)
 store r3,(r0)

 movei #_workpage,r0
 load (r0),r1
 moveq #1,r2
 xor r2,r1
 store r1,(r0)
 movei #_worklist,r0
 load (r0),r1
 xor r2,r1
 store r1,(r0)

L51:
 jump T,(RETURNPOINT)
 addq #16,FP ; delay slot
   */
}

// EOF

