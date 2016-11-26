#ifndef MYON_PPZ8_H_INCLUDED
#define MYON_PPZ8_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ppz8_pcmvoice {
  uint32_t start;
  uint32_t len;
  uint32_t loopstart;
  uint32_t loopend;
  uint16_t origfreq;
};

struct ppz8_pcmbuf {
  int16_t *data;
  uint32_t buflen;
  struct ppz8_pcmvoice voice[128];
};

struct ppz8_channel {
  uint64_t ptr;
  uint64_t loopstartptr;
  uint64_t loopendptr;
  uint64_t endptr;
  uint32_t freq;
  uint32_t loopstartoff;
  uint32_t loopendoff;
  int16_t prevout[2];
  uint8_t vol;
  uint8_t pan;
  uint8_t voice;
  bool playing;
};

struct ppz8 {
  struct ppz8_pcmbuf buf[2];
  struct ppz8_channel channel[8];
  uint16_t srate;
  uint8_t totalvol;
  uint16_t mix_volume;
};

void ppz8_init(struct ppz8 *ppz8, uint16_t srate, uint16_t mix_volume);
void ppz8_mix(struct ppz8 *ppz8, int16_t *buf, unsigned samples);
bool ppz8_pvi_load(struct ppz8 *ppz8, uint8_t buf,
                   const uint8_t *pvidata, uint32_t pvidatalen,
                   int16_t *decodebuf);

static inline uint32_t ppz8_pvi_decodebuf_samples(uint32_t pvidatalen) {
  if (pvidatalen < 0x210) return 0;
  return (pvidatalen - 0x210) * 2;
}

struct ppz8_functbl {
  void (*channel_play)(struct ppz8 *ppz8, uint8_t channel, uint8_t voice);
  void (*channel_stop)(struct ppz8 *ppz8, uint8_t channel);
  void (*channel_volume)(struct ppz8 *ppz8, uint8_t channel, uint8_t vol);
  void (*channel_freq)(struct ppz8 *ppz8, uint8_t channel, uint32_t freq);
  void (*channel_loopoffset)(struct ppz8 *ppz8, uint8_t channel,
                             uint32_t startoff, uint32_t endoff);
  void (*channel_pan)(struct ppz8 *ppz8, uint8_t channel, uint8_t pan);
  void (*total_volume)(struct ppz8 *ppz8, uint8_t vol);
};

extern const struct ppz8_functbl ppz8_functbl;

#ifdef __cplusplus
}
#endif

#endif // MYON_PPZ8_H_INCLUDED
