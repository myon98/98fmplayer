#ifndef MYON_FMDSP_H_INCLUDED
#define MYON_FMDSP_H_INCLUDED

#include <stdint.h>
#include "font.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PC98_W = 640,
  PC98_H = 400
};

enum {
  FMDSP_PALETTE_COLORS = 9
};

struct fmdsp {
  uint8_t palette[FMDSP_PALETTE_COLORS*3];
  uint8_t target_palette[FMDSP_PALETTE_COLORS*3];
  const struct fmdsp_font *font98;

};

struct fmdriver_work;
void fmdsp_init(struct fmdsp *fmdsp, const struct fmdsp_font *font);
void fmdsp_vram_init(struct fmdsp *fmdsp,
                     struct fmdriver_work *work,
                     uint8_t *vram);
void fmdsp_update(struct fmdsp *fmdsp, const struct fmdriver_work *work, uint8_t *vram);
void fmdsp_vrampalette(struct fmdsp *fmdsp, const uint8_t *vram, uint8_t *vram32, int stride);
void fmdsp_font_from_fontrom(uint8_t *font, const uint8_t *fontrom);
#ifdef __cplusplus
}
#endif

#endif // MYON_FMDSP_H_INCLUDED
