/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"
#include "mars.h"
#include "r_local.h"

#define SRAM_MAGIC1		0xDE
#define SRAM_MAGIC2		0xAD

#define SRAM_VERSION	4
#define SRAM_OPTVERSION	4
#define SRAM_MAXSLOTS	10
#define SRAM_SLOTSIZE	200

typedef struct __attribute((packed))
{
	uint8_t version;
	uint8_t skill;
	uint8_t netgame;
	uint8_t mapnumber;
	char wadname[32];
	char mapname[32];

	playerresp_t resp[MAXPLAYERS];
} savegame_t;

typedef struct __attribute((packed))
{
	int8_t version;
	int8_t detailmode;
	int8_t controltype;
	int8_t viewport;
	int8_t sfxvolume;
	int8_t musicvolume;
	int8_t musictype;
	uint8_t magic1;
	int8_t alwaysrun;
	int8_t strafebtns;
	uint8_t magic2;
	int8_t anamorphic;
	int8_t yabcdpad;
	int8_t sfxdriver;
	char spcmDir[9];
	int8_t lowres;
} saveopts_t;

static char saveslotguard[SRAM_SLOTSIZE - sizeof(savegame_t)] __attribute__((unused));
static char optslotguard[SRAM_SLOTSIZE - sizeof(saveopts_t)] __attribute__((unused));

const uint16_t optslotnumber = SRAM_MAXSLOTS - 1;
const uint16_t optslotoffset = optslotnumber * SRAM_SLOTSIZE;


void ReadGame(int slotnumber)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return;

	D_memset(&sg, 0, sizeof(savegame_t));

	Mars_ReadSRAM((void*)&sg, offset, sizeof(savegame_t));

	if (sg.version != SRAM_VERSION)
		return;

	startskill = sg.skill;
	starttype = sg.netgame;
	startmap = sg.mapnumber;
	starttype = sg.netgame;
	D_memcpy(playersresp, sg.resp, sizeof(playerresp_t)*MAXPLAYERS);
}

static void SaveGameExt(int slotnumber, int mapnum, const char *mapname)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return;

	D_memset(&sg, 0, sizeof(sg));
	sg.version = SRAM_VERSION;
	sg.skill = gameskill;
	sg.netgame = netgame;
	sg.mapnumber = mapnum & 0xFF;
	D_snprintf(sg.mapname, sizeof(sg.mapname), "%s", mapname);
	D_snprintf(sg.wadname, sizeof(sg.wadname), "%s", cd_pwad_name);
	D_memcpy(sg.resp, playersresp, sizeof(playerresp_t)*MAXPLAYERS);

	Mars_WriteSRAM((void*)&sg, offset, sizeof(savegame_t));
}

void SaveGame(int slotnumber)
{
	SaveGameExt(slotnumber, gamemapinfo->mapNumber, DMAPINFO_STRFIELD(gamemapinfo, name));
}

void QuickSave(int nextmap, const char *mapname)
{
	SaveGameExt(0, nextmap, mapname);
}

boolean GetSaveInfo(int slotnumber, VINT* mapnum, VINT* skill, VINT *mode, char *wadname, char *mapname)
{
	int i;
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return false;

	D_memset(&sg, 0, sizeof(savegame_t));

	Mars_ReadSRAM((void*)&sg, offset, sizeof(savegame_t));

	if (sg.version != SRAM_VERSION)
		return false;
	if (sg.netgame >= gt_deathmatch)
		sg.netgame = gt_single;

	*mapnum = sg.mapnumber;
	*skill = sg.skill;
	*mode = sg.netgame;

	// strip the leading /'s
	for (i = 0; sg.wadname[i] == '/'; i++) {
		;
	}
	D_snprintf(wadname, sizeof(sg.wadname) -  i, "%s", &sg.wadname[i]);

	D_snprintf(mapname, sizeof(sg.mapname), "%s", sg.mapname);

	return true;
}

int SaveCount(void)
{
	int i;
	uint8_t temp;
	int offset = 0;

	// the last slot is used for storing game options
	for (i = 0; i < optslotnumber; i++) {
		Mars_ReadSRAM(&temp, offset, 1);
		if (temp != SRAM_VERSION)
			break;
		offset += SRAM_SLOTSIZE;
	}

	return i;
}

int MaxSaveCount(void)
{
	return optslotnumber;
}

static void SaveOptions(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	so.version = SRAM_OPTVERSION;
	so.detailmode = detailmode;
	so.controltype = controltype;
	so.viewport = viewportNum;
	so.sfxvolume = sfxvolume;
	so.musicvolume = musicvolume;
	so.musictype = musictype;
	so.alwaysrun = alwaysrun;
	so.strafebtns = strafebtns;
	so.anamorphic = anamorphicview;
	so.sfxdriver = sfxdriver;
	so.yabcdpad = yabcdpad;
	so.magic1 = SRAM_MAGIC1;
	so.magic2 = SRAM_MAGIC2;
	so.lowres = lowres;
	D_snprintf(so.spcmDir, sizeof(so.spcmDir), "%s", spcmDir);

	Mars_WriteSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));
}

static void ReadOptions(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	Mars_ReadSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));

	if (so.version != SRAM_OPTVERSION)
		return;

	if (so.detailmode < detmode_potato || so.detailmode >= MAXDETAILMODES)
		so.detailmode = detmode_normal;
	if (so.sfxvolume > 64)
		so.sfxvolume = 64;
	if (so.musicvolume > 64)
		so.musicvolume = 64;
	if (so.controltype >= NUMCONTROLOPTIONS)
		so.controltype = 0;
	if (so.musictype < mustype_none || so.musictype > mustype_spcm)
		so.musictype = mustype_fm;
	if (so.musictype >= mustype_cd && !S_CDAvailable())
		so.musictype = mustype_fm;
	if (so.alwaysrun < 0 || so.alwaysrun > 1)
		so.alwaysrun = 0;
	if (so.strafebtns < 0 || so.strafebtns > 3)
		so.strafebtns = 0;
	if (so.viewport < 0 || so.viewport >= numViewports)
		so.viewport = R_DefaultViewportSize();
	if (so.strafebtns < 0 || so.strafebtns > 3)
		so.strafebtns = 0;
	if (so.anamorphic < 0 || so.anamorphic > 1)
		so.anamorphic = 0;
	if (so.sfxdriver < 0 || so.sfxdriver > 2)
		so.sfxdriver = 0;
	if (so.yabcdpad < 0 || so.yabcdpad > 1)
		so.yabcdpad = 0;

	detailmode = so.detailmode;
	sfxvolume = so.sfxvolume;
	musicvolume = so.musicvolume;
	controltype = so.controltype;
	viewportNum = so.viewport;
	musictype = so.musictype;
	ticsperframe = MINTICSPERFRAME;
	alwaysrun = so.alwaysrun;
	strafebtns = so.strafebtns;
	anamorphicview = so.anamorphic;
	sfxdriver = so.sfxdriver;
	yabcdpad = so.yabcdpad;
	lowres = so.lowres;

	so.spcmDir[sizeof(so.spcmDir)-1] = '\0';
	D_snprintf(spcmDir, sizeof(spcmDir), "%s", so.spcmDir);
}

void ClearEEProm(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	Mars_WriteSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));
}

void ReadEEProm(void)
{
	controltype = 0;
	sfxvolume = 64;
	musicvolume = 64;
	viewportNum = R_DefaultViewportSize();
	musictype = mustype_fm;
	alwaysrun = 0;
	strafebtns = 0;
	ticsperframe = MINTICSPERFRAME;
	anamorphicview = 0;
	sfxdriver = 0;
	detailmode = detmode_normal;
	lowres = false;

	ReadOptions();
}

void WriteEEProm(void)
{
	SaveOptions();
}
