#ifndef MYON_FMPLAYER_FFT_FFT_H_INCLUDED
#define MYON_FMPLAYER_FFT_FFT_H_INCLUDED

#include <stdint.h>

enum {
  FFTLEN = 8192,
  FFTDISPLEN = 70,
};

struct fmplayer_fft_data {
  int16_t buf[FFTLEN];
  unsigned ind;
};

struct fmplayer_fft_input_data {
  struct fmplayer_fft_data fdata;
  int16_t work[FFTLEN];
  double dwork[FFTLEN];
  float fwork[FFTLEN*2];
};

struct fmplayer_fft_disp_data {
  // 0 - 31
  // 4 per 6db
  uint8_t buf[FFTDISPLEN];
};

void fft_init_table(void);

void fft_write(struct fmplayer_fft_data *data, const int16_t *buf, unsigned len);

void fft_calc(struct fmplayer_fft_disp_data *ddata, struct fmplayer_fft_input_data *idata);

#endif // MYON_FMPLAYER_FFT_FFT_H_INCLUDED
