#ifndef MYON_FMDSP_FONT_H_INCLUDED
#define MYON_FMDSP_FONT_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline bool sjis_is_mb_start(uint8_t c) {
  if (0x81 <= c && c <= 0x9f) return true;
  if (0xe0 <= c && c <= 0xef) return true;
  return false;
}

static inline bool jis_is_halfwidth(uint16_t jis) {
  uint8_t row = jis >> 8;
  return row == 0x29 || row == 0x2a;
}

static inline uint16_t sjis2jis(uint8_t sjis_1st, uint8_t sjis_2nd) {
  uint16_t jis;
  if (sjis_1st >= 0xe0) sjis_1st -= 0x40;
  sjis_1st -= 0x81;
  jis = sjis_1st << 9;
  if (sjis_2nd >= 0x80) sjis_2nd--;
  if (sjis_2nd >= 0x9e) {
    jis |= 0x100 | (sjis_2nd - 0x9e);
  } else {
    jis |= (sjis_2nd - 0x40);
  }
  jis += 0x2121;
  return jis;
}

static inline uint16_t jis2sjis(uint16_t jis) {
  jis += 0x217e;
  jis ^= 0x4000;
  bool c = jis&0x100;
  jis = (jis & 0xff) | (((jis>>1)|0x8000)&0xff00);
  if (!c) {
    uint16_t jisl = jis & 0xff;
    jisl -= 0xde;
    if (jisl >> 8) jisl--;
    jisl -= 0x80;
    jis = (jis&0xff00) | (jisl&0xff);
  }
  return jis;
}

enum fmdsp_font_type {
  FMDSP_FONT_ANK,
  FMDSP_FONT_JIS_LEFT,
  FMDSP_FONT_JIS_RIGHT
};

struct fmdsp_font {
  const void *(* get)(const struct fmdsp_font *font, 
                uint16_t addr, enum fmdsp_font_type type);
  const void *data;
  uint8_t width_half;
  uint8_t height;
};

enum {
  FONT_ROM_FILESIZE = 0x46800
};

void fmdsp_font_from_font_rom(struct fmdsp_font *font, const void *data);

extern const struct fmdsp_font font_fmdsp_small;
extern const struct fmdsp_font font_fmdsp_medium;

#ifdef __cplusplus
}
#endif

#endif // MYON_FMDSP_FONT_H_INCLUDED
