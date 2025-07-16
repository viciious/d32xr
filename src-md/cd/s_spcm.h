#ifndef _S_SPCM_H
#define _S_SPCM_H

int S_SCM_PlayTrack(const char *name, int repeat);
void S_SPCM_StopTrack(void);
void S_SPCM_Suspend(void);
void S_SPCM_Unsuspend(void);

#endif // _S_SPCM_H
