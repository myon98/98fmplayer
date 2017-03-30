#include "fmdsp/fmdsp.h"

void fmdsp_vramlookup_c(uint8_t *vram32, const uint8_t *vram, const uint8_t *palette, int stride) {
  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      uint8_t r = palette[vram[y*PC98_W+x]*3+0];
      uint8_t g = palette[vram[y*PC98_W+x]*3+1];
      uint8_t b = palette[vram[y*PC98_W+x]*3+2];
      uint32_t data = (((uint32_t)r)<<16) | (((uint32_t)g)<<8) | ((uint32_t)b);
      uint32_t *row = (uint32_t *)(vram32 + y*stride);
      row[x] = data;
    }
  }
}
