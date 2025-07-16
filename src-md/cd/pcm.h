#ifndef _PCM_H
#define _PCM_H

#include <stdint.h>

// PCM chip registers
#define PCM_ENV   *((volatile uint8_t *)0xFF0001)
#define PCM_PAN   *((volatile uint8_t *)0xFF0003)
#define PCM_FDL   *((volatile uint8_t *)0xFF0005)
#define PCM_FDH   *((volatile uint8_t *)0xFF0007)
#define PCM_LSL   *((volatile uint8_t *)0xFF0009)
#define PCM_LSH   *((volatile uint8_t *)0xFF000B)
#define PCM_START *((volatile uint8_t *)0xFF000D)
#define PCM_CTRL  *((volatile uint8_t *)0xFF000F)
#define PCM_ONOFF *((volatile uint8_t *)0xFF0011)
#define PCM_RAMPTR ((volatile uint8_t *)0xFF0021)
#define PCM_WAVE  *((volatile uint8_t *)0xFF2001)

#ifdef __cplusplus
extern "C" {
#endif

#define PCM_U8_AMPLIFICATION 3

// convert from 8-bit signed samples to sign/magnitude samples
#define pcm_s8_to_sm(s) (((s) < 0) ? ((s) < -127 ? 127 : -(s)) : (((s) > 126 ? 126 : (s))|128))

/* from pcm.c */
extern void pcm_init(void);
uint16_t pcm_load_samples(uint16_t start, uint8_t *samples, uint16_t length);
extern uint16_t pcm_load_samples_u8(uint16_t start, uint8_t *samples, uint16_t length);
uint16_t pcm_load_stereo_samples_u8(uint16_t start, uint16_t start2, uint8_t *samples, uint16_t length);
extern void pcm_load_zero(uint16_t start, uint16_t length);
extern void pcm_reset(void);
extern void pcm_set_ctrl(uint8_t val);
extern uint8_t pcm_get_ctrl(void);
extern void pcm_set_off(uint8_t index);
extern void pcm_set_on(uint8_t index);
extern uint8_t pcm_is_off(uint8_t index);
extern void pcm_set_start(uint8_t start, uint16_t offset);
extern void pcm_set_loop(uint16_t loopstart);
extern void pcm_set_env(uint8_t vol);
extern void pcm_set_pan(uint8_t pan);
extern void pcm_loop_markers(uint16_t start);
extern uint8_t pcm_u8_to_sm_lut[256];
/* from pcm-io.s */
extern uint8_t pcm_lcf(uint8_t pan);
extern void pcm_delay(void);
extern void pcm_set_period(uint32_t period);
extern void pcm_set_freq(uint32_t freq);
extern void pcm_set_timer(uint16_t bpm);
extern void pcm_stop_timer(void);
extern void pcm_start_timer(void (*callback)(void *), void *);

#ifdef __cplusplus
}
#endif

#endif
