#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw_md.h"
#include "cdfh.h"

extern int DENTRY_OFFSET;
extern int DENTRY_LENGTH;
extern char DENTRY_FLAGS;
extern char DENTRY_NAME[];
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
    int32_t i, r;
    int32_t length, offset;
    char temp[256];

    if (cd_init() < 0)
        return -3;

    i = mystrlen(name);
    while (i && (name[i] != '/'))
        i--;
    if (name[i] == '/')
    {
        if (i)
        {
            if (name[0] != '/')
            {
                temp[0] = '/';
                memcpy(temp + 1, name, i);
                temp[i + 1] = 0;
            }
            else
            {
                memcpy(temp, name, i);
                temp[i] = 0;
            }
        }
        else
        {
            temp[0] = '/';
            temp[1] = 0;
        }

        r = set_cwd(temp);
        if (r < 0)
        {
            // error setting working directory
            return r;
        }
        memcpy(temp, &name[i+1], mystrlen(&name[i+1])+1);
        temp[255] = 0;
    }
    else
    {
        r = set_cwd("/");
        if (r < 0)
        {
            // error setting working directory
            return r;
        }
        memcpy(temp, name, mystrlen(name)+1);
        temp[255] = 0;
    }

    while (1)
    {
        r = find_dir_entry(temp);
        if (r >= 0)
            break;

        if (r == ERR_NAME_NOT_FOUND)
        {
            r = next_dir_sec();
            if (r == ERR_NO_MORE_ENTRIES)
            {
                // error finding entry
                return -1;
            }
        }

        if (r < 0)
            return r;
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

int64_t read_directory(char *buf)
{
    int r;
    int count;
    char *obuf;

    if (cd_init() < 0)
        return -1;

    if (set_cwd(buf) < 0)
    {
        // error setting working directory
        return -1;
    }

    count = 0;
    obuf = buf;
    do
    {
        *buf = 0;

        r = next_dir_entry();
        if (!r && memcmp(DENTRY_NAME, ".", 2))
        {
            int len = strlen(DENTRY_NAME);
            memcpy(buf, DENTRY_NAME, len+1);
            buf += len + 1;
            memcpy(buf, &DENTRY_LENGTH, sizeof(int));
            buf += 4;
            memcpy(buf, &DENTRY_FLAGS, sizeof(char));
            buf += 1;
            count++;
        }

        if (r)
        {
            r = next_dir_sec();
        }
    } while (!r);

    buf++;

    return ((int64_t)(buf - obuf) << 32) | count;
}

