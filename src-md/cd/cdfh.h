#ifndef _CDFH_H
#define _CDFH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int mystrlen(const char* string);
int64_t open_file(const char *name);
int read_sectors(uint8_t *dest, int blk, int len);

#ifdef __cplusplus
}
#endif

#endif
