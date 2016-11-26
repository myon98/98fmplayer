#ifndef LIBOPNA_OPNA_H_INCLUDED
#define LIBOPNA_OPNA_H_INCLUDED

#include "opnafm.h"
#include "opnassg.h"
#include "opnadrum.h"
#include "opnaadpcm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct opna {
  struct opna_fm fm;
  struct opna_ssg ssg;
  struct opna_drum drum;
  struct opna_adpcm adpcm;
  struct opna_ssg_resampler resampler;

};

void opna_reset(struct opna *opna);
void opna_writereg(struct opna *opna, unsigned reg, unsigned val);
void opna_mix(struct opna *opna, int16_t *buf, unsigned samples);

#ifdef __cplusplus
}
#endif

#endif // LIBOPNA_OPNA_H_INCLUDED
