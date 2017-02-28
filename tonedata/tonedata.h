#ifndef MYON_FMPLAYER_TONEDATA_H_INCLUDED
#define MYON_FMPLAYER_TONEDATA_H_INCLUDED

#include <stdint.h>

struct fmplayer_tonedata {
  struct fmplayer_tonedata_channel {
    struct fmplayer_tonedata_slot {
      uint8_t ar;
      uint8_t dr;
      uint8_t sr;
      uint8_t rr;
      uint8_t sl;
      uint8_t tl;
      uint8_t ks;
      uint8_t ml;
      uint8_t dt;
      uint8_t ams;
    } slot[4];
    uint8_t fb;
    uint8_t alg;
  } ch[6];
};

struct opna;
void tonedata_from_opna(
  struct fmplayer_tonedata *tonedata,
  const struct opna *opna
);

enum fmplayer_tonedata_format {
  FMPLAYER_TONEDATA_FMT_PMD,
  FMPLAYER_TONEDATA_FMT_FMP
};

enum {
  FMPLAYER_TONEDATA_STR_SIZE = 0x100
};
void tonedata_ch_normalize_tl(struct fmplayer_tonedata_channel *ch);
void tonedata_ch_string(
  enum fmplayer_tonedata_format format,
  char *buf,
  const struct fmplayer_tonedata_channel *ch,
  uint8_t tonenum
);

#endif // MYON_FMPLAYER_TONEDATA_H_INCLUDED

