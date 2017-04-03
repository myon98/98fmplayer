#include "tonedata.h"
#include "libopna/opna.h"
#include <stdio.h>
void tonedata_from_opna(
  struct fmplayer_tonedata *tonedata,
  const struct opna *opna
) {
  const struct opna_fm *fm = &opna->fm;
  for (int c = 0; c < 6; c++) {
    const struct opna_fm_channel *op_ch = &fm->channel[c];
    struct fmplayer_tonedata_channel *td_ch = &tonedata->ch[c];
    for (int s = 0; s < 4; s++) {
      const struct opna_fm_slot *op_sl = &op_ch->slot[s];
      struct fmplayer_tonedata_slot *td_sl = &td_ch->slot[s];
      td_sl->ar = op_sl->ar;
      td_sl->dr = op_sl->dr;
      td_sl->sr = op_sl->sr;
      td_sl->rr = op_sl->rr;
      td_sl->sl = op_sl->sl;
      td_sl->tl = op_sl->tl;
      td_sl->ks = op_sl->ks;
      td_sl->ml = op_sl->mul;
      td_sl->dt = op_sl->det;
      // TODO: hardware LFO not implemented
      td_sl->ams = 0;
    }
    td_ch->alg = op_ch->alg;
    td_ch->fb = op_ch->fb;
  }
}

void tonedata_ch_normalize_tl(
  struct fmplayer_tonedata_channel *ch
) {
  static const uint8_t alg_out[8] = {
    0x8, 0x8, 0x8, 0x8, 0xa, 0xe, 0xe, 0xf
  };
  uint8_t outbit = alg_out[ch->alg & 7];
  uint8_t max_tl = 127;
  for (int s = 0; s < 4; s++) {
    if (!(outbit & (1<<s))) continue;
    if (max_tl > ch->slot[s].tl) max_tl = ch->slot[s].tl;
  }
  for (int s = 0; s < 4; s++) {
    if (!(outbit & (1<<s))) continue;
    ch->slot[s].tl -= max_tl;
  }
}

void tonedata_ch_string(
  enum fmplayer_tonedata_format format,
  char *buf,
  const struct fmplayer_tonedata_channel *ch,
  uint8_t tonenum
) {
  switch (format) {
  case FMPLAYER_TONEDATA_FMT_PMD:
    snprintf(buf, FMPLAYER_TONEDATA_STR_SIZE,
            "@%3d %1d %1d\n"
            " %2d %2d %2d %2d %2d %3d %1d %2d %1d %1d\n"
            " %2d %2d %2d %2d %2d %3d %1d %2d %1d %1d\n"
            " %2d %2d %2d %2d %2d %3d %1d %2d %1d %1d\n"
            " %2d %2d %2d %2d %2d %3d %1d %2d %1d %1d\n",
            tonenum, ch->alg, ch->fb,
            ch->slot[0].ar, ch->slot[0].dr, ch->slot[0].sr, ch->slot[0].rr,
            ch->slot[0].sl, ch->slot[0].tl, ch->slot[0].ks, ch->slot[0].ml,
            ch->slot[0].dt, ch->slot[0].ams,
            ch->slot[1].ar, ch->slot[1].dr, ch->slot[1].sr, ch->slot[1].rr,
            ch->slot[1].sl, ch->slot[1].tl, ch->slot[1].ks, ch->slot[1].ml,
            ch->slot[1].dt, ch->slot[1].ams,
            ch->slot[2].ar, ch->slot[2].dr, ch->slot[2].sr, ch->slot[2].rr,
            ch->slot[2].sl, ch->slot[2].tl, ch->slot[2].ks, ch->slot[2].ml,
            ch->slot[2].dt, ch->slot[2].ams,
            ch->slot[3].ar, ch->slot[3].dr, ch->slot[3].sr, ch->slot[3].rr,
            ch->slot[3].sl, ch->slot[3].tl, ch->slot[3].ks, ch->slot[3].ml,
            ch->slot[3].dt, ch->slot[3].ams
    );
    break;
  case FMPLAYER_TONEDATA_FMT_FMP:
    snprintf(buf, FMPLAYER_TONEDATA_STR_SIZE,
            "'@%3d\n"
            "'@ %2d, %2d, %2d, %2d, %2d, %3d, %1d, %2d, %1d\n"
            "'@ %2d, %2d, %2d, %2d, %2d, %3d, %1d, %2d, %1d\n"
            "'@ %2d, %2d, %2d, %2d, %2d, %3d, %1d, %2d, %1d\n"
            "'@ %2d, %2d, %2d, %2d, %2d, %3d, %1d, %2d, %1d\n"
            "'@ %1d, %1d",
            tonenum,
            ch->slot[0].ar, ch->slot[0].dr, ch->slot[0].sr, ch->slot[0].rr,
            ch->slot[0].sl, ch->slot[0].tl, ch->slot[0].ks, ch->slot[0].ml,
            ch->slot[0].dt,
            ch->slot[1].ar, ch->slot[1].dr, ch->slot[1].sr, ch->slot[1].rr,
            ch->slot[1].sl, ch->slot[1].tl, ch->slot[1].ks, ch->slot[1].ml,
            ch->slot[1].dt,
            ch->slot[2].ar, ch->slot[2].dr, ch->slot[2].sr, ch->slot[2].rr,
            ch->slot[2].sl, ch->slot[2].tl, ch->slot[2].ks, ch->slot[2].ml,
            ch->slot[2].dt,
            ch->slot[3].ar, ch->slot[3].dr, ch->slot[3].sr, ch->slot[3].rr,
            ch->slot[3].sl, ch->slot[3].tl, ch->slot[3].ks, ch->slot[3].ml,
            ch->slot[3].dt,
            ch->alg, ch->fb
    );
    break;
  case FMPLAYER_TONEDATA_FMT_VOPM:
    snprintf(buf, FMPLAYER_TONEDATA_STR_SIZE,
            "@:%d\n"
            "LFO:  0   0   0   0   0\n"
            "CH: 64 %3d %3d   0   0 120   0\n"
            "M1:%3d %3d %3d %3d %3d %3d %3d %3d %3d   0   0\n"
            "C1:%3d %3d %3d %3d %3d %3d %3d %3d %3d   0   0\n"
            "M2:%3d %3d %3d %3d %3d %3d %3d %3d %3d   0   0\n"
            "C2:%3d %3d %3d %3d %3d %3d %3d %3d %3d   0   0",
            tonenum,
            ch->fb, ch->alg, 
            ch->slot[0].ar, ch->slot[0].dr, ch->slot[0].sr, ch->slot[0].rr,
            ch->slot[0].sl, ch->slot[0].tl, ch->slot[0].ks, ch->slot[0].ml,
            ch->slot[0].dt,
            ch->slot[1].ar, ch->slot[1].dr, ch->slot[1].sr, ch->slot[1].rr,
            ch->slot[1].sl, ch->slot[1].tl, ch->slot[1].ks, ch->slot[1].ml,
            ch->slot[1].dt,
            ch->slot[2].ar, ch->slot[2].dr, ch->slot[2].sr, ch->slot[2].rr,
            ch->slot[2].sl, ch->slot[2].tl, ch->slot[2].ks, ch->slot[2].ml,
            ch->slot[2].dt,
            ch->slot[3].ar, ch->slot[3].dr, ch->slot[3].sr, ch->slot[3].rr,
            ch->slot[3].sl, ch->slot[3].tl, ch->slot[3].ks, ch->slot[3].ml,
            ch->slot[3].dt
    );
    break;
  default:
    buf[0] = 0;
    break;
  }
}
