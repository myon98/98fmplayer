#ifndef MYON_FMPLAYER_TONEDATA_H_INCLUDED
#define MYON_FMPLAYER_TONEDATA_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

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

static inline bool fmplayer_tonedata_channel_isequal(
  const struct fmplayer_tonedata_channel *a,
  const struct fmplayer_tonedata_channel *b) {
  if (a->fb != b->fb) return false;
  if (a->alg != b->alg) return false;
  for (int s = 0; s < 4; s++) {
    const struct fmplayer_tonedata_slot *sa = &a->slot[s];
    const struct fmplayer_tonedata_slot *sb = &b->slot[s];
    if (sa->ar != sb->ar) return false;
    if (sa->dr != sb->dr) return false;
    if (sa->sr != sb->sr) return false;
    if (sa->rr != sb->rr) return false;
    if (sa->sl != sb->sl) return false;
    if (sa->tl != sb->tl) return false;
    if (sa->ks != sb->ks) return false;
    if (sa->ml != sb->ml) return false;
    if (sa->dt != sb->dt) return false;
    if (sa->ams != sb->ams) return false;
  }
  return true;
}

struct opna;
void tonedata_from_opna(
  struct fmplayer_tonedata *tonedata,
  const struct opna *opna
);

enum fmplayer_tonedata_format {
  FMPLAYER_TONEDATA_FMT_PMD,
  FMPLAYER_TONEDATA_FMT_FMP,
  FMPLAYER_TONEDATA_FMT_VOPM,
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

