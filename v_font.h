#include "doomdef.h"

typedef struct
{
    short lumpStart;
    char lumpStartChar;
    int8_t fixedWidthSize;
    int8_t spaceWidthSize;
    int8_t verticalOffset;
    char minChar;
    char maxChar;
    boolean fixedWidth;
} font_t;

extern font_t menuFont;
extern font_t titleFont;
extern font_t creditFont;
extern font_t titleNumberFont;
extern font_t hudNumberFont;

void V_FontInit();

int V_DrawStringLeft(const font_t *font, int x, int y, const char *string);
int V_DrawStringLeftWithColormap(const font_t *font, int x, int y, const char *string, int colormap);
int V_DrawStringRight(const font_t *font, int x, int y, const char *string);
int V_DrawStringCenter(const font_t *font, int x, int y, const char *string);

void V_DrawValueLeft(const font_t *font, int x, int y, int value);
void V_DrawValueRight(const font_t *font, int x, int y, int value);
void V_DrawValueCenter(const font_t *font, int x, int y, int value);

void V_DrawValuePaddedRight(const font_t *font, int x, int y, int value, int pad);

void valtostr(char *string,int val);
