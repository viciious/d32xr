#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw_md.h"
#include "cdfh.h"

extern int DENTRY_OFFSET;
extern int DENTRY_LENGTH;
extern short CURR_OFFSET;

extern int init_cd(void);

int mystrlen(const char* string)
{
	volatile int rc = 0;

	while (*(string++))
		rc++;

	return rc;
}

static int cd_init(void)
{
    static int icd = ERR_NO_DISC;
    if (icd < 0) {
        icd = init_cd();
        if (icd < 0) {
            return icd;
        }
    }
    return icd;
}

int64_t open_file(const char *name)
{
    int32_t i;
    int32_t length, offset;
    char temp[256];

    if (cd_init() < 0)
        return -1;

    i = mystrlen(name);
    while (i && (name[i] != '/'))
        i--;
    if (name[i] == '/')
    {
        if (i)
        {
            memcpy(temp, name, i);
            temp[i] = 0;
        }
        else
        {
            temp[0] = '/';
            temp[1] = 0;
        }
        if (set_cwd(temp) < 0)
        {
            // error setting working directory
            return -1;
        }
        memcpy(temp, &name[i+1], mystrlen(&name[i+1])+1);
        temp[255] = 0;
    }
    else
    {
        if (set_cwd("/") < 0)
        {
            // error setting working directory
            return -1;
        }
        memcpy(temp, name, mystrlen(name)+1);
        temp[255] = 0;
    }

    if (find_dir_entry(temp) < 0)
    {
        // error finding entry
        return -1;
    }

    offset = DENTRY_OFFSET;
    length = DENTRY_LENGTH;
    return ((int64_t)length << 32) | offset;
}

int read_sectors(uint8_t *dest, int blk, int len)
{
    int r = read_cd(blk, len, (void *)dest);
    CURR_OFFSET = blk + len - 1;
    return r;
}
