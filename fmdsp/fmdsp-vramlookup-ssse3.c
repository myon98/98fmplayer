#include "fmdsp/fmdsp.h"
#include <tmmintrin.h>

void fmdsp_vramlookup_ssse3(uint8_t *vram32, const uint8_t *vram, const uint8_t *palette, int stride) {
  __m128i z = _mm_setzero_si128();
  __m128i p[3];
  {
    union {
      __m128i xmm;
      uint8_t u8[16];
    } pi[3];
    for (int i = 0; i < FMDSP_PALETTE_COLORS; i++) {
      for (int c = 0; c < 3; c++) {
        pi[c].u8[i] = palette[i*3+c];
      }
    }
    for (int c = 0; c < 3; c++) {
      p[c] = _mm_load_si128(&pi[c].xmm);
    }
  }

  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < 40; x++) {
      // 16 pixels
      __m128i v = _mm_loadu_si128((__m128i *)&vram[y*PC98_W+x*16]);

      __m128i r = _mm_shuffle_epi8(p[0], v);
      __m128i g = _mm_shuffle_epi8(p[1], v);
      __m128i b = _mm_shuffle_epi8(p[2], v);

      __m128i gb[2], zr[2];
      gb[0] = _mm_unpacklo_epi8(b, g);
      gb[1] = _mm_unpackhi_epi8(b, g);
      zr[0] = _mm_unpacklo_epi8(r, z);
      zr[1] = _mm_unpackhi_epi8(r, z);
      
      __m128i o[4];
      o[0] = _mm_unpacklo_epi16(gb[0], zr[0]);
      o[1] = _mm_unpackhi_epi16(gb[0], zr[0]);
      o[2] = _mm_unpacklo_epi16(gb[1], zr[1]);
      o[3] = _mm_unpackhi_epi16(gb[1], zr[1]);
      for (int i = 0; i < 4; i++) {
        _mm_storeu_si128((__m128i *)&vram32[(x*4+i)*16], o[i]);
      }
    }
    vram32 += stride;
  }
}
