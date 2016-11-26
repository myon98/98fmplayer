#include "opna.h"

void opna_reset(struct opna *opna) {
  opna_fm_reset(&opna->fm);
  opna_ssg_reset(&opna->ssg);
  opna_ssg_resampler_reset(&opna->resampler);
  opna_drum_reset(&opna->drum);
  opna_adpcm_reset(&opna->adpcm);
}

void opna_writereg(struct opna *opna, unsigned reg, unsigned val) {
  val &= 0xff;
  opna_fm_writereg(&opna->fm, reg, val);
  opna_ssg_writereg(&opna->ssg, reg, val);
  opna_drum_writereg(&opna->drum, reg, val);
  opna_adpcm_writereg(&opna->adpcm, reg, val);
}

void opna_mix(struct opna *opna, int16_t *buf, unsigned samples) {
  opna_fm_mix(&opna->fm, buf, samples);
  opna_ssg_mix_55466(&opna->ssg, &opna->resampler, buf, samples);
  opna_drum_mix(&opna->drum, buf, samples);
  opna_adpcm_mix(&opna->adpcm, buf, samples);
}
