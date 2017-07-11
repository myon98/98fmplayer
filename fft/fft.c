#include "fft.h"
#include <math.h>
#include <string.h>

void fft_write(struct fmplayer_fft_data *data, const int16_t *buf, unsigned len) {
  if (len > FFTLEN) {
    unsigned discard = len - FFTLEN;
    buf += discard*2;
    len = FFTLEN;
  }
  unsigned towrite = FFTLEN - data->ind;
  if (towrite > len) towrite = len;
  for (unsigned i = 0; i < towrite; i++) {
    data->buf[data->ind+i] = ((uint32_t)buf[2*i+0] + buf[2*i+1]) / 2;
  }
  data->ind = (data->ind + towrite) % FFTLEN;
  buf += towrite*2;
  len -= towrite;

  for (unsigned i = 0; i < len; i++) {
    data->buf[i] = ((uint32_t)buf[2*i+0] + buf[2*i+1]) / 2;
  }
  data->ind = (data->ind + len) % FFTLEN;
}

static const uint16_t fftfreqtab[FFTDISPLEN+1] = {
      0,
     18,    19,    21,    22,    23,    25,    26,    28,    29,    31,
     33,    35,    37,    39,    42,    44,    47,    50,    53,    56,
     59,    63,    66,    70,    75,    79,    84,    89,    94,   100,
    106,   112,   119,   126,   133,   141,   150,   158,   168,   178,
    189,   200,   212,   224,   238,   252,   267,   283,   300,   317,
    336,   356,   378,   400,   424,   449,   476,   504,   534,   566,
    600,   635,   673,   713,   756,   801,   848,   899,   952,  1009,
};

enum {
  HFFTLENBIT = 12,
  HFFTLEN = 1<<HFFTLENBIT,
};

static uint16_t window[FFTLEN];
static float tritab[FFTLEN + FFTLEN/4];

void fft_init_table(void) {
  const double pi = acos(0.0) * 2.0;
  double alpha = 0.54;
  double beta = 1.0 - alpha;
  for (unsigned i = 0; i < FFTLEN; i++) {
    double v = alpha - beta * cos(2.0*pi*i/(FFTLEN-1));
    window[i] = v * (1<<16);
  }
  for (unsigned i = 0; i < (FFTLEN + FFTLEN/4); i++) {
    tritab[i] = sin(2.0*pi*i/FFTLEN);
  }
}

static float coscalc(unsigned i) {
  return tritab[(i & (FFTLEN-1)) + FFTLEN/4];
}

static float sincalc(unsigned i) {
  return tritab[i & (FFTLEN-1)];
}

static float ar(unsigned i) {
  return 0.5f*(1.0f-sincalc(i));
}

static float ai(unsigned i) {
  return -0.5f*coscalc(i);
}

static float br(unsigned i) {
  return 0.5f*(1.0f+sincalc(i));
}

static float bi(unsigned i) {
  return 0.5f*coscalc(i);
}

static void fft_real(float *fftbuf) {
  unsigned b = 0;
  for (unsigned i = 0; i < HFFTLEN; i++) {
    unsigned ii = 0;
    for (unsigned bit = 0; bit < HFFTLENBIT; bit++) {
      ii |= ((i >> bit) & 1u) << (HFFTLENBIT-bit-1);
    }
    fftbuf[(!b)*FFTLEN+i*2+0] = fftbuf[b*FFTLEN+ii*2+0];
    fftbuf[(!b)*FFTLEN+i*2+1] = fftbuf[b*FFTLEN+ii*2+1];
  }
  b = !b;
  for (unsigned bit = 0; bit < HFFTLENBIT; bit++) {
    for (unsigned i = 0; i < HFFTLEN; i++) {
      unsigned ei = i & ~(1u<<bit);
      unsigned oi = i | (1u<<bit);
      float are = fftbuf[b*FFTLEN+oi*2+0];
      float aim = fftbuf[b*FFTLEN+oi*2+1];
      float bre = coscalc(i<<(HFFTLENBIT-bit));
      float bim = sincalc(i<<(HFFTLENBIT-bit));
      float cre = are*bre - aim*bim;
      float cim = are*bim + aim*bre;
      fftbuf[(!b)*FFTLEN+i*2+0] = fftbuf[b*FFTLEN+ei*2+0] + cre;
      fftbuf[(!b)*FFTLEN+i*2+1] = fftbuf[b*FFTLEN+ei*2+1] + cim;
    }
    b = !b;
  }
  for (unsigned i = 0; i < HFFTLEN; i++) {
    float xr = fftbuf[b*FFTLEN+i*2+0];
    float rxr = fftbuf[b*FFTLEN+0];
    if (i) rxr = fftbuf[b*FFTLEN+(HFFTLEN-i)*2+0];
    float xi = fftbuf[b*FFTLEN+i*2+1];
    float rxi = fftbuf[b*FFTLEN+1];
    if (i) rxi = fftbuf[b*FFTLEN+(HFFTLEN-i)*2+1];
    fftbuf[(!b)*FFTLEN+i*2+0] = xr * ar(i) - xi * ai(i) + rxr * br(i) + rxi * bi(i);
    fftbuf[(!b)*FFTLEN+i*2+1] = xi * ar(i) + xr * ai(i) + rxr * bi(i) - rxi * br(i);
  }
  if (!b) {
    memcpy(fftbuf, fftbuf+FFTLEN, FFTLEN*sizeof(fftbuf[0]));
  }
}

void fft_calc(struct fmplayer_fft_disp_data *ddata, struct fmplayer_fft_input_data *idata) {
  for (int i = 0; i < FFTLEN; i++) {
    int fi = (i + idata->fdata.ind) % FFTLEN;
    idata->work[i] = (((int32_t)idata->fdata.buf[fi]) * window[i]) >> 16;
  }
  for (int i = 0; i < FFTLEN; i++) {
    idata->fwork[i] = ((float)idata->work[i])/32768;
  }
  fft_real(idata->fwork);
  for (int i = 1; i < FFTLEN/2; i++) {
    float re = idata->fwork[i*2];
    float im = idata->fwork[i*2+1];
    idata->fwork[i] = sqrtf((re*re) + (im*im));
  }
  for (int i = 0; i < FFTLEN/2; i++) {
    idata->fwork[i] = idata->fwork[i] / sqrtf(FFTLEN);
  }
  float dbuf[FFTDISPLEN];
  for (int i = 0; i < FFTDISPLEN; i++) {
    dbuf[i] = 0.0f;
    for (int j = fftfreqtab[i]; j < fftfreqtab[i+1]; j++) {
      dbuf[i] += idata->fwork[j];
    }
    dbuf[i] /= fftfreqtab[i+1] - fftfreqtab[i];
  }
  for (int i = 0; i < FFTDISPLEN; i++) {
    float res = (dbuf[i] > (1.0f / 256)) ? (4.0f*log2f(dbuf[i]) + 32.0f) : 0.0f;
    if (res > 31.0f) res = 31.0f;
    if (res < 0.0f) res = 0.0f;
    ddata->buf[i] = res;
  }
}
