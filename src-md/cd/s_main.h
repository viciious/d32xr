#ifndef _S_MAIN_H
#define _S_MAIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void S_Init(void);

void S_Clear(void);

void S_Update(void);

void S_SetBufferData(uint16_t buf_id, uint8_t *data, uint32_t data_len);
void S_CopyBufferData(uint16_t buf_id, const uint8_t *data, uint32_t data_len);

uint8_t S_PlaySource(uint8_t src_id, uint16_t buf_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);
void S_UpdateSource(uint8_t src_id, uint16_t freq, uint8_t pan, uint8_t vol, uint8_t autoloop);
void S_RewindSource(uint8_t src_id);
void S_StopSource(uint8_t src_id);
void S_PUnPSource(uint8_t src_id, uint8_t pause);
uint16_t S_GetSourcePosition(uint8_t src_id);

int S_PlaySPCMTrack(const char *name, int repeat);
void S_StopSPCMTrack(void);

#ifdef __cplusplus
}
#endif

#endif
