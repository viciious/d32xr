/*
  CALICO

  Renderer phase 9 - Refresh
*/

#include "doomdef.h"
#include "r_local.h"

#ifdef MARS

//
// R_FindBestCacheObject
// Update pixel counts for walls and flats
static void R_FindBestCacheObject(void)
{
   unsigned pixcount;
   viswall_t *wall;
   int firstflat = numtextures*2;
   int i, obj_id, objects[64], num_objs;
   const int max_objects = sizeof(objects) / sizeof(objects[0]);

   num_objs = 0;
   for (wall = viswalls; wall < lastwallcmd; wall++)
   {
      int wobj[4], wpxcnt[4], nwobj;
      if (wall->start > wall->stop)
        continue;

      nwobj = 0;
      if (wall->actionbits & AC_TOPTEXTURE)
      {
         pixcount = *((volatile unsigned *)((uintptr_t)&wall->t_pixelcount[0] | 0x20000000));
         wobj[0] = wall->t_texturenum, wpxcnt[0] = pixcount>>16; // mip 0
         wobj[1] = numtextures+wall->t_texturenum, wpxcnt[1] = pixcount&0xffff; // mip 1
         nwobj = 2;
      }

      if (wall->actionbits & AC_BOTTOMTEXTURE)
      {
         pixcount = *((volatile unsigned *)((uintptr_t)&wall->b_pixelcount[0] | 0x20000000));
         wobj[nwobj] = wall->b_texturenum, wpxcnt[nwobj] = pixcount>>16; // mip 0
         wobj[nwobj+1] = numtextures+wall->b_texturenum, wpxcnt[nwobj+1] = pixcount&0xffff; // mip 1
         nwobj += 2;
      }

      for (i = 0; i < nwobj; i++) {
        obj_id = wobj[i];
        if (R_AddPixelsToTexCache(&r_texcache, obj_id, wpxcnt[i]) && (num_objs < max_objects)) {
            objects[num_objs++] = obj_id;
        }
      }
   }

   if (detailmode != detmode_potato)
   {
    visplane_t* pl;
    for (pl = visplanes + 1; pl < lastvisplane; pl++)
    {
        pixcount = *((volatile unsigned *)((uintptr_t)&pl->pixelcount | 0x20000000));

        // mip 0
        obj_id = firstflat+pl->flatnum;
        if (R_AddPixelsToTexCache(&r_texcache, obj_id, pixcount>>16) && (num_objs < max_objects)) {
            objects[num_objs++] = obj_id;
        }

        // mip 1
        obj_id += numflats;
        if (R_AddPixelsToTexCache(&r_texcache, obj_id, pixcount&0xffff) && (num_objs < max_objects)) {
            objects[num_objs++] = obj_id;
        }
    }
   }

   R_PostTexCacheFrame(&r_texcache);

   for (i = 0; i < num_objs; i++)
   {
        R_TestTexCacheCandidate(&r_texcache, objects[i]);
   }
}

static void R_CacheBestObject(void)
{
    int pixels;
    void **pdata;
    const int id = r_texcache.bestobj;
    unsigned miplevel, w, h;
    int firstflat = numtextures*2;
    void **data;

    if (id == -1)
        return;

    miplevel = 0;
    if (id < firstflat)
    {
        texture_t* tex = &textures[id];
        if (id >= numtextures) {
            miplevel = 1;
            tex = &textures[id-numtextures];
        }
        w = tex->width, h = tex->height;
        data = (void **)tex->data;
    }
    else
    {
        flattex_t *flat = &flatpixels[id - firstflat];
        if (id >= firstflat+numflats) {
            miplevel = 1;
            flat = &flatpixels[id-firstflat-numflats];
        }
        w = 64, h = 64;
        data = (void **)flat->data;
    }

    if (miplevel) {
        pixels = (w>>1) * (h>>1);
    } else {
        pixels = w * h;
    }
    pdata = (void**)&data[miplevel];

    R_AddToTexCache(&r_texcache, id, pixels, pdata);
}

static void R_UpdateCache(void)
{
    R_FindBestCacheObject();

    R_CacheBestObject();
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

