#ifndef MYON_FMDSP_H_INCLUDED
#define MYON_FMDSP_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include "font.h"
#include "fmdriver/fmdriver.h"
#include "fft/fft.h"

#ifdef __cplusplus
extern "C" {
#endif

struct opna;

enum {
  PC98_W = 640,
  PC98_H = 400
};

enum {
  FMDSP_PALETTE_COLORS = 10,
  FMDSP_LEVEL_COUNT = 19,
};

enum FMDSP_DISPSTYLE {
  FMDSP_DISPSTYLE_ORIGINAL,
  FMDSP_DISPSTYLE_DEFAULT,
  FMDSP_DISPSTYLE_OPN,
  FMDSP_DISPSTYLE_PPZ8,
  FMDSP_DISPSTYLE_13,
  FMDSP_DISPSTYLE_CNT
};

struct fmdsp {
  uint8_t palette[FMDSP_PALETTE_COLORS*3];
  uint8_t target_palette[FMDSP_PALETTE_COLORS*3];
  const struct fmdsp_font *font98;
  enum FMDSP_DISPSTYLE style;
  bool style_updated;
  bool masked[FMDRIVER_TRACK_NUM];
  bool masked_rhythm;
  uint8_t fftdata[FFTDISPLEN];
  uint8_t fftcnt[FFTDISPLEN];
  uint8_t fftdropdiv[FFTDISPLEN];
  uint64_t framecnt;
  int cpuusage;
  int fps;
  uint8_t leveldata[FMDSP_LEVEL_COUNT];
  uint8_t levelcnt[FMDSP_LEVEL_COUNT];
  uint8_t leveldropdiv[FMDSP_LEVEL_COUNT];
};

struct fmdriver_work;
void fmdsp_init(struct fmdsp *fmdsp, const struct fmdsp_font *font);
void fmdsp_vram_init(struct fmdsp *fmdsp,
                     struct fmdriver_work *work,
                     uint8_t *vram);
void fmdsp_update(struct fmdsp *fmdsp, struct fmdriver_work *work,
                  struct opna *opna, uint8_t *vram,
                  struct fmplayer_fft_input_data *idata
                 );
void fmdsp_vrampalette(struct fmdsp *fmdsp, const uint8_t *vram, uint8_t *vram32, int stride);
void fmdsp_font_from_fontrom(uint8_t *font, const uint8_t *fontrom);
void fmdsp_palette_set(struct fmdsp *fmdsp, int p);
void fmdsp_dispstyle_set(struct fmdsp *fmdsp, enum FMDSP_DISPSTYLE style);

typedef void (*fmdsp_vramlookup_type)(uint8_t *vram32,
                                      const uint8_t *vram,
                                      const uint8_t *palette,
                                      int stride);
extern fmdsp_vramlookup_type fmdsp_vramlookup_func;
void fmdsp_vramlookup_c(uint8_t *vram32,
                        const uint8_t *vram,
                        const uint8_t *palette,
                        int stride) __attribute__((hot,optimize(3)));

void fmdsp_vramlookup_neon(uint8_t *, const uint8_t *, const uint8_t *, int);
void fmdsp_vramlookup_ssse3(uint8_t *, const uint8_t *, const uint8_t *, int) __attribute__((hot,optimize(3)));
#ifdef __cplusplus
}
#endif

#endif // MYON_FMDSP_H_INCLUDED
