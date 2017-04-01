#include "fmdsp/fmdsp.h"

void fmdsp_vramlookup_c(uint8_t *vram32, const uint8_t *vram, const uint8_t *palette, int stride) {
  uint32_t palette32[FMDSP_PALETTE_COLORS];
  for (int i = 0; i < FMDSP_PALETTE_COLORS; i++) {
    uint8_t r = palette[i*3+0];
    uint8_t g = palette[i*3+1];
    uint8_t b = palette[i*3+2];
    palette32[i] = (((uint32_t)r)<<16) | (((uint32_t)g)<<8) | ((uint32_t)b);
  }
  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      uint32_t *row = (uint32_t *)(vram32 + y*stride);
      row[x] = palette32[vram[y*PC98_W+x]];
    }
  }
}
