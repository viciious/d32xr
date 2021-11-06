/*
  CALICO

  Renderer phase 9 - Refresh
*/

#include "doomdef.h"
#include "r_local.h"

void R_Update(void)
{
#ifndef MARS
   // CALICO: Invoke I_Update
   I_Update();
#endif

#ifdef MARS
   R_PostTexCacheFrame(&r_texcache);
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

