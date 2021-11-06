/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits, Derek John Evans, id Software and ZeniMax Media

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

#define SRAM_VERSION	1
#define SRAM_OPTVERSION	2
#define SRAM_MAXSLOTS	10
#define SRAM_SLOTSIZE	200

typedef struct __attribute((packed))
{
	uint8_t version;
	uint8_t skill;
	uint8_t netgame;
	uint8_t mapnumber;
	uint8_t pad[12];

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
	int8_t unused1;
	int8_t alwaysrun;
	int8_t strafebtns;
	int8_t unused2;
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

	Mars_StopSoundMixer();

	Mars_ReadSRAM((void*)&sg, offset, sizeof(savegame_t));

	Mars_StartSoundMixer();

	if (sg.version != SRAM_VERSION)
		return;

	startskill = sg.skill;
	starttype = sg.netgame;
	startmap = sg.mapnumber;
	D_memcpy(playersresp, sg.resp, sizeof(playersresp));
}

static void SaveGameExt(int slotnumber, int mapnum)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return;

	sg.version = SRAM_VERSION;
	sg.skill = gameskill;
	sg.netgame = netgame;
	sg.mapnumber = mapnum & 0xFF;
	D_memcpy(sg.resp, playersresp, sizeof(playersresp));

	Mars_StopSoundMixer();

	Mars_WriteSRAM((void*)&sg, offset, sizeof(savegame_t));

	Mars_StartSoundMixer();
}

void SaveGame(int slotnumber)
{
	SaveGameExt(slotnumber, gamemapinfo.mapNumber);
}

void QuickSave(int nextmap)
{
	SaveGameExt(0, nextmap);
}

boolean GetSaveInfo(int slotnumber, VINT* mapnum, VINT* skill)
{
	savegame_t sg;
	const int offset = slotnumber * SRAM_SLOTSIZE;

	if (slotnumber >= optslotnumber)
		return false;

	D_memset(&sg, 0, sizeof(savegame_t));

	Mars_StopSoundMixer();

	Mars_ReadSRAM((void*)&sg, offset, sizeof(savegame_t));

	Mars_StartSoundMixer();

	if (sg.version != SRAM_VERSION)
		return false;

	*mapnum = sg.mapnumber;
	*skill = sg.skill;
	return true;
}

int SaveCount(void)
{
	int i;
	uint8_t temp;
	int offset = 0;

	Mars_StopSoundMixer();

	// the last slot is used for storing game options
	for (i = 0; i < optslotnumber; i++) {
		Mars_ReadSRAM(&temp, offset, 1);
		if (temp != SRAM_VERSION)
			break;
		offset += SRAM_SLOTSIZE;
	}

	Mars_StartSoundMixer();

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

	Mars_StopSoundMixer();

	Mars_WriteSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));

	Mars_StartSoundMixer();
}

static void ReadOptions(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	Mars_StopSoundMixer();

	Mars_ReadSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));

	Mars_StartSoundMixer();

	if (so.version != SRAM_OPTVERSION)
		return;

	if (so.sfxvolume > 64)
		so.sfxvolume = 64;
	if (so.controltype >= NUMCONTROLOPTIONS)
		so.controltype = 0;
	if (so.detailmode >= MAXDETAILMODES)
		so.detailmode = detmode_medium;
	if (so.musictype < mustype_none || so.musictype > mustype_cd)
		so.musictype = mustype_fm;
	if (so.musictype == mustype_cd && !mars_cd_ok)
		so.musictype = mustype_fm;
	if (so.alwaysrun < 0 || so.alwaysrun > 1)
		so.alwaysrun = 0;
	if (so.strafebtns < 0 || so.strafebtns > 3)
		so.strafebtns = 0;

	sfxvolume = so.sfxvolume;
	musicvolume = so.musicvolume;
	controltype = so.controltype;
	detailmode = so.detailmode;
	viewportNum = so.viewport;
	musictype = so.musictype;
	ticsperframe = MINTICSPERFRAME;
	alwaysrun = so.alwaysrun;
	strafebtns = so.strafebtns;
}

void ClearEEProm(void)
{
	saveopts_t so;

	D_memset(&so, 0, sizeof(saveopts_t));

	Mars_StopSoundMixer();

	Mars_WriteSRAM((void*)&so, optslotoffset, sizeof(saveopts_t));

	Mars_StartSoundMixer();
}

void ReadEEProm(void)
{
	controltype = 0;
	sfxvolume = 64;
	musicvolume = 64;
	detailmode = detmode_medium;
	viewportNum = 0;
	musictype = mustype_fm;
	alwaysrun = 0;
	strafebtns = 0;
	ticsperframe = MINTICSPERFRAME;

	ReadOptions();
}

void WriteEEProm(void)
{
	SaveOptions();
}
