#ifndef _HW_MD_H
#define _HW_MD_H

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
extern int begin_read_cd(int lba, int len);
extern int read_cd_pcm(void *buffer);
extern void stop_read_cd(void);
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
