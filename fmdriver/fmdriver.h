#ifndef MYON_FMDRIVER_H_INCLUDED
#define MYON_FMDRIVER_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include "ppz8.h"

enum {
  FMDRIVER_TRACK_NUM = 10,
  // 1 line = 80 characters, may contain half-width doublebyte characters
  FMDRIVER_TITLE_BUFLEN = 80*2+1,
};

enum fmdriver_track_type {
  FMDRIVER_TRACK_FM,
  FMDRIVER_TRACK_SSG,
  FMDRIVER_TRACK_ADPCM,
};

enum fmdriver_track_info {
  FMDRIVER_TRACK_INFO_NORMAL,
  FMDRIVER_TRACK_INFO_SSG_NOISE_ONLY,
  FMDRIVER_TRACK_INFO_SSG_NOISE_MIX,
  FMDRIVER_TRACK_INFO_PPZ8
};

struct fmdriver_track_status {
  bool playing;
  enum fmdriver_track_type type;
  enum fmdriver_track_info info;
  uint8_t num;
  uint8_t ticks;
  uint8_t ticks_left;
  uint8_t key;
  // key after pitchbend, LFO, etc. applied
  uint8_t actual_key;
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
  // driver status
  struct fmdriver_track_status track_status[FMDRIVER_TRACK_NUM];
  // fm3ex part map
};

#endif // MYON_FMDRIVER_H_INCLUDED
