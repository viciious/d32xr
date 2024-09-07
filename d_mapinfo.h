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
#ifndef D_MAPINFO_H__
#define D_MAPINFO_H__

#define MI_BARON_SPECIAL 	1
#define MI_CYBER_SPECIAL 	2
#define MI_SPIDER_SPECIAL 	4
#define MI_FATSO_SPECIAL 	8
#define MI_BABY_SPECIAL 	16
#define MI_CYBER_SPECIAL2 	32
#define MI_SPIDER_SPECIAL2 	64
#define MI_PISTOL_START 	128

#define MAX_SPCM_PACKS 		3

typedef struct
{
	VINT specials;
	VINT mapNumber;
	VINT skyTexture;
	uint8_t songNum;
	uint8_t cdaNum;
	char *name;
	char *next;
	char *secretNext;
	char *lumpName;
	char *interText;
	char *secretInterText;
} dmapinfo_t;

typedef struct
{
	char *borderFlat;
	char *endFlat;
	char *creditsPage;
	char *titlePage;
	char *titleMus;
	char *intermissionMus;
	char *victoryMus;
	char *endMus;
	char spcmDirList[MAX_SPCM_PACKS][9];
	VINT borderFlatNum;
	VINT titleTime;
	VINT creditsTime;
	VINT endShowCast;
	VINT noAttractDemo;
	VINT stopFireTime;
	VINT titleStartPos;
	char* endText;
	void* data;
} dgameinfo_t;

int G_BuiltinMapNumForMapName(const char* map);
int G_FindMapinfo(const char *lumpname, dmapinfo_t *mi, char *outmem);
dmapinfo_t** G_LoadMaplist(int*pmapcount, dgameinfo_t* gi);

#endif // D_MAPINFO_H__

