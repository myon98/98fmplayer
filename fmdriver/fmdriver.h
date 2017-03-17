#ifndef MYON_FMDRIVER_H_INCLUDED
#define MYON_FMDRIVER_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include "ppz8.h"

enum {
  FMDRIVER_TRACK_FM_1,
  FMDRIVER_TRACK_FM_2,
  FMDRIVER_TRACK_FM_3,
  FMDRIVER_TRACK_FM_3_EX_1,
  FMDRIVER_TRACK_FM_3_EX_2,
  FMDRIVER_TRACK_FM_3_EX_3,
  FMDRIVER_TRACK_FM_4,
  FMDRIVER_TRACK_FM_5,
  FMDRIVER_TRACK_FM_6,
  FMDRIVER_TRACK_SSG_1,
  FMDRIVER_TRACK_SSG_2,
  FMDRIVER_TRACK_SSG_3,
  FMDRIVER_TRACK_ADPCM,
  FMDRIVER_TRACK_NUM
};
enum {
  // 1 line = 80 characters, may contain half-width doublebyte characters
  FMDRIVER_TITLE_BUFLEN = 80*2+1,
};

enum fmdriver_track_type {
  FMDRIVER_TRACKTYPE_FM,
  FMDRIVER_TRACKTYPE_SSG,
  FMDRIVER_TRACKTYPE_ADPCM,
  FMDRIVER_TRACKTYPE_CNT,
};

enum fmdriver_track_info {
  FMDRIVER_TRACK_INFO_NORMAL,
  FMDRIVER_TRACK_INFO_SSG,
  FMDRIVER_TRACK_INFO_FM3EX,
  FMDRIVER_TRACK_INFO_PPZ8,
  FMDRIVER_TRACK_INFO_PDZF,
  FMDRIVER_TRACK_INFO_SSGEFF,
};

struct fmdriver_track_status {
  bool playing;
  enum fmdriver_track_info info;
  uint8_t ticks;
  uint8_t ticks_left;
  uint8_t key;
  // key after pitchbend, LFO, etc. applied
  uint8_t actual_key;
  uint8_t tonenum;
  uint8_t volume;
  uint8_t gate;
  int8_t detune;
  char status[9];
  bool fmslotmask[4];
  // for FMP, ppz8 channel+1 or 0
  // use for track mask or display
  uint8_t ppz8_ch;
  bool ssg_tone;
  bool ssg_noise;
};

struct fmdriver_work {
  // set by driver, called by opna
  void (*driver_opna_interrupt)(struct fmdriver_work *work);
  void (*driver_deinit)(struct fmdriver_work *work);
  // driver internal
  void *driver;


  // set by opna, called by driver in the interrupt functions
  unsigned (*opna_readreg)(struct fmdriver_work *work, unsigned addr);
  void (*opna_writereg)(struct fmdriver_work *work, unsigned addr, unsigned data);
  uint8_t (*opna_status)(struct fmdriver_work *work, bool a1);
  void *opna;

  const struct ppz8_functbl *ppz8_functbl;
  struct ppz8 *ppz8;

  // CP932 encoded
  //const char *title;
  char comment[3][FMDRIVER_TITLE_BUFLEN];
  // only single-byte uppercase cp932
  char filename[FMDRIVER_TITLE_BUFLEN];
  // driver status (for display)
  uint8_t ssg_noise_freq;
  struct fmdriver_track_status track_status[FMDRIVER_TRACK_NUM];
  // fm3ex part map
};

#endif // MYON_FMDRIVER_H_INCLUDED
