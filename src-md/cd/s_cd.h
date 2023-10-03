#ifndef _S_CD_H
#define _S_CD_H

#include <stdint.h>

int S_CD_LoadBuffers(sfx_buffer_t *buf, int numsfx, const char *name, const int32_t *offsetlen);
extern int mystrlen(const char* string);

#endif
