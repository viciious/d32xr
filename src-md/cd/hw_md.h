#ifndef _HW_MD_H
#define _HW_MD_H

// Global Variable offsets - must match boot loader
typedef struct
{
    int VBLANK_HANDLER;
    int VBLANK_PARAM;
    int INIT_CD;
    int READ_CD;
    int SET_CWD;
    int FIRST_DIR_SEC;
    int NEXT_DIR_SEC;
    int FIND_DIR_ENTRY;
    int NEXT_DIR_ENTRY;
    int LOAD_FILE;
    short DISC_TYPE;
    short DIR_ENTRY;
    int CWD_OFFSET;
    int CWD_LENGTH;
    int CURR_OFFSET;
    int CURR_LENGTH;
    int ROOT_OFFSET;
    int ROOT_LENGTH;
    int DENTRY_OFFSET;
    int DENTRY_LENGTH;
    short DENTRY_FLAGS;
    char DENTRY_NAME[256];
    char TEMP_NAME[256];
} globals_t;

extern globals_t *global_vars;

// CDFS Error codes - must match boot loader
enum {
    ERR_READ_FAILED     = -2,
    ERR_NO_PVD          = -3,
    ERR_NO_MORE_ENTRIES = -4,
    ERR_BAD_ENTRY       = -5,
    ERR_NAME_NOT_FOUND  = -6,
    ERR_NO_DISC         = -7
};

#ifdef __cplusplus
extern "C" {
#endif

extern int init_cd(void);
extern int read_cd(int lba, int len, void *buffer);
extern int set_cwd(char *path);
extern int first_dir_sec(void);
extern int next_dir_sec(void);
extern int find_dir_entry(char *name);
extern int next_dir_entry(void);
extern int load_file(char *filename, void *buffer);

#ifdef __cplusplus
}
#endif

#endif
