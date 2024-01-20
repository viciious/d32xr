/* m_main.c -- main menu */

#include "doomdef.h"

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define CURSORX		(80)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define STARTY			24
#define ITEMSPACE	20
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
#define	NUMLCHARS 64
#define MAXITEMS    8

typedef struct
{
	VINT	x;
	VINT 	y;
	char 	name[16];
} menuitem_t;

typedef struct
{
	VINT numitems;
    menuitem_t items[MAXITEMS];
} menuscreen_t;

static VINT m_skull1lump, m_skull2lump;
static VINT m_skilllump;
static VINT numslump;

static VINT	cursorframe;
static VINT cursordelay;
static VINT	cursorpos;
static VINT	movecount;

static menuscreen_t *gs_menu;

extern VINT	uchar, snums;
extern void print(int x, int y, const char* string);

void GS_Start(void)
{
    int i;
    int n;
    char *buf;

    /* cache all needed graphics	 */
    m_skull1lump = W_CheckNumForName("M_SKULL1");
    m_skull2lump = W_CheckNumForName("M_SKULL2");

    m_skilllump = W_CheckNumForName("SKILL0");

    numslump = W_CheckNumForName("NUM_0");

    uchar = W_CheckNumForName("CHAR_065");

    snums = W_CheckNumForName("NUM_0");

    cursorframe = -1;
    cursorpos = 0;
    cursordelay = MOVEWAIT;
    gs_menu = NULL;

    n = I_ReadCDDirectory("/");
    if (n <= 0)
        return;

    gs_menu = Z_Malloc(sizeof(*gs_menu), PU_STATIC);
    D_memset(gs_menu, 0, sizeof(*gs_menu));

    buf = I_GetCDFileBuffer();
    for (i = 0; i < n; i++)
    {
        char *name;
        int flags;
        int namelen;
        menuitem_t *mi;

        if (gs_menu->numitems == MAXITEMS)
            break;

        namelen = mystrlen(buf);

        name = buf;
        buf += namelen + 1;
        buf += 4; // length
        flags = (uint8_t)*buf;
        buf += 1;

        if (flags & 2) // flags -- directory
            continue;

        if (namelen >= (int)sizeof(mi->name))
            continue;
        if (namelen <= 4)
            continue;

        if (!D_strcasecmp(name, SOUNDS_PWAD_NAME))
            continue;

        if (D_strncasecmp(&name[namelen - 4], ".wad", 4))
            continue;

        mi = &gs_menu->items[gs_menu->numitems];
        D_memcpy(mi->name, name, namelen);
        mi->x = ITEMX;
        mi->y = CURSORY(gs_menu->numitems);
        gs_menu->numitems++;
    }

    DoubleBufferSetup();
}

void GS_Stop(void)
{
    if (gs_menu == NULL)
        return;

    D_snprintf(cd_pwad_name, sizeof(cd_pwad_name), "%s", gs_menu->items[cursorpos].name);
    Z_Free(gs_menu);
    gs_menu = NULL;

    S_Clear();
}

/*
=================
=
= GS_Ticker
=
=================
*/

int GS_Ticker (void)
{
    int buttons, oldbuttons;
    menuscreen_t *menuscr = gs_menu;
    boolean newcursor = false;
    int sound = sfx_None;

    if (menuscr->numitems <= 1)
        return ga_startnew;

    if (cursorframe == -1)
    {
        cursorframe = 0;
        cursordelay = MOVEWAIT + MOVEWAIT / 2;
        S_StartSound(NULL, sfx_swtchn);
    }

    /* animate skull */
    if (gametic != prevgametic && (gametic & 3) == 0)
    {
        cursorframe ^= 1;
    }

    buttons = ticrealbuttons & MENU_BTNMASK;
    oldbuttons = oldticrealbuttons & MENU_BTNMASK;

    /* exit menu if button press */
    if ((buttons & (BT_A | BT_LMBTN | BT_START)) && !(oldbuttons & (BT_A | BT_LMBTN | BT_START)))
        return ga_startnew;

    if (buttons == 0)
    {
        cursordelay = 0;
        return ga_nothing;
    }

    cursordelay -= vblsinframe;
    if (cursordelay > 0)
        return ga_nothing;

    cursordelay = MOVEWAIT;

    /* check for movement */
    if (!(buttons & (BT_UP | BT_DOWN | BT_LEFT | BT_RIGHT)))
        movecount = 0; /* move immediately on next press */
    else
    {
        movecount = 0; /* repeat move */

        if (++movecount == 1)
        {
            int oldcursorpos = cursorpos;

            if (buttons & BT_DOWN)
            {
                if (++cursorpos == menuscr->numitems)
                    cursorpos = 0;
            }

            if (buttons & BT_UP)
            {
                if (--cursorpos == -1)
                    cursorpos = menuscr->numitems - 1;
            }

            newcursor = cursorpos != oldcursorpos;

            if (newcursor)
                sound = sfx_pistol;
        }
    }

    if (sound != sfx_None)
        S_StartSound(NULL, sound);

    return ga_nothing;
}

/*
=================
=
= GS_Drawer
=
=================
*/

void GS_Drawer (void)
{
    int i;
    menuitem_t *items = gs_menu->items;
    int y_offset = 4;

    I_ClearFrameBuffer();

    /* Draw main menu */
    print(84, 4, "SELECT GAME");
    y_offset += ITEMSPACE + 4;

    /* draw new skull */
    if (cursorframe)
        DrawJagobjLump(m_skull2lump, CURSORX, y_offset + items[cursorpos].y - 2, NULL, NULL);
    else
        DrawJagobjLump(m_skull1lump, CURSORX, y_offset + items[cursorpos].y - 2, NULL, NULL);

    /* draw menu items */
    for (i = 0; i < gs_menu->numitems; i++)
    {
        int len;
        char name[16];
        int y = y_offset + items[i].y;

        len = mystrlen(items[i].name);
        D_memcpy(name, items[i].name, len-4);
        name[len-4] = 0; // strip file extension

        print(items[i].x, y, name);
    }
}
