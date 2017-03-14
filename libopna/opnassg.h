#ifndef LIBOPNA_OPNASSG_H_INCLUDED
#define LIBOPNA_OPNASSG_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct opna_ssg_ch {
  uint16_t tone_counter;
  bool out;
};

struct opna_ssg {
  uint8_t regs[0x10];
  struct opna_ssg_ch ch[3];
  uint8_t noise_counter;
  uint32_t lfsr;
  uint16_t env_counter;
  uint8_t env_level;
  bool env_att;
  bool env_alt;
  bool env_hld;
  bool env_holding;
  unsigned mask;
};

struct opna_ssg_resampler {
  int16_t buf[(1<<7)];
  unsigned index;
};

void opna_ssg_reset(struct opna_ssg *ssg);
void opna_ssg_resampler_reset(struct opna_ssg_resampler *resampler);
// generate raw data
// Monoral
// Output level: [-32766, 32766]
// Samplerate: clock / 8
// (on opna: masterclock / 32
// 7987200 / 32 = 249600)
void opna_ssg_generate_raw(struct opna_ssg *ssg, int16_t *buf, int samples);

// mix samplerate converted data for mixing with OPNA output
// call to buffer written with OPNA output
// samplerate: 7987200/144 Hz
//            (55466.66..) Hz
void opna_ssg_mix_55466(
  struct opna_ssg *ssg, struct opna_ssg_resampler *resampler,
  int16_t *buf, int samples);
void opna_ssg_writereg(struct opna_ssg *ssg, unsigned reg, unsigned val);
unsigned opna_ssg_readreg(const struct opna_ssg *ssg, unsigned reg);
// channel level (0 - 31)
int opna_ssg_channel_level(const struct opna_ssg *ssg, int ch);

#ifdef __cplusplus
}
#endif

#endif // LIBOPNA_OPNASSG_H_INCLUDED
