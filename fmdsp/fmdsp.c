#include "fmdsp.h"
#include "fmdsp_sprites.h"
#include "fmdriver/fmdriver.h"

static void vramblit(uint8_t *vram, int x, int y,
                     const uint8_t *data, int w, int h) {
  for (int yi = 0; yi < h; yi++) {
    for (int xi = 0; xi < w; xi++) {
      vram[(y+yi)*PC98_W+(x+xi)] = data[yi*w+xi];
    }
  }
}

static void vramblit_color(uint8_t *vram, int x, int y,
                     const uint8_t *data, int w, int h,
                     uint8_t color) {
  for (int yi = 0; yi < h; yi++) {
    for (int xi = 0; xi < w; xi++) {
      vram[(y+yi)*PC98_W+(x+xi)] = data[yi*w+xi] ? color : 0;
    }
  }
}

static void vramblit_key(uint8_t *vram, int x, int y,
                         const uint8_t *data, int w, int h,
                         uint8_t key, uint8_t color) {
  for (int yi = 0; yi < h; yi++) {
    for (int xi = 0; xi < w; xi++) {
      uint8_t d = data[yi*w+xi];
      if (d == (key+1)) {
        vram[(y+yi)*PC98_W+(x+xi)] = color;
      }
    }
  }
}

void fmdsp_init(struct fmdsp *fmdsp) {
  for (int i = 0; i < FMDSP_PALETTE_COLORS; i++) {
    fmdsp->palette[i*3+0] = s_palettes[0][i*3+0];
    fmdsp->palette[i*3+1] = s_palettes[0][i*3+1];
    fmdsp->palette[i*3+2] = s_palettes[0][i*3+2];
  }
}


static bool sjis_is_mb_start(uint8_t c) {
  if (0x81 <= c && c <= 0x9f) return true;
  if (0xe0 <= c && c <= 0xef) return true;
  return false;
}

static uint16_t sjis2jis(uint8_t sjis_1st, uint8_t sjis_2nd) {
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

static void vram_putchar(uint16_t ptr, uint8_t *vram, const uint8_t *font,
                         int x, int y, uint8_t color) {
  for (int yi = 0; yi < 16; yi++) {
    for (int xi = 0; xi < 8; xi++) {
      if (font[(ptr<<4)+yi] & (1<<(7-xi))) {
        vram[(y+yi)*PC98_W+(x+xi)] = color;
      }
    }
  }
}

static void fmdsp_putline(const char *strptr, uint8_t *vram, const uint8_t *font,
                   int y, uint8_t color) {
  const uint8_t *cp932str = (const uint8_t *)strptr;
  bool sjis_is2nd = false;
  uint8_t sjis_1st;
  int x = 0;

  while (*cp932str) {
    if (!sjis_is2nd) {
      if (!sjis_is_mb_start(*cp932str)) {
        if (*cp932str == '\t') {
          if ((x+8*8) > PC98_W) return;
          x += 8*8;
          x &= ~(8*8-1);
          cp932str++;
        } else {
          if ((x+8) > PC98_W) return;
          vram_putchar(0x8000+*cp932str++, vram, font, x, y, color);
          x += 8;
        }
      } else {
        sjis_is2nd = true;
        sjis_1st = *cp932str++;
      }
    } else {
      uint8_t sjis_2nd = *cp932str++;
      uint16_t jis = sjis2jis(sjis_1st, sjis_2nd);
      uint8_t jis_1st = jis >> 8;
      uint8_t jis_2nd = jis;
      bool half = (jis_1st == 0x29);
      if ((x+(half ? 8 : 16)) > PC98_W) return;
      vram_putchar((jis_2nd<<8) | (jis_1st-0x20), vram, font, x, y, color);
      x += 8;
      if (!half) {
        vram_putchar((jis_2nd<<8) | (jis_1st-0x20+0x80), vram, font, x, y, color);
        x += 8;
      }
      sjis_is2nd = false;
    }
  }
}

void fmdsp_vram_init(struct fmdsp *fmdsp,
                     struct fmdriver_work *work,
                     const uint8_t *font,
                     uint8_t *vram) {
  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  for (int t = 0; t < 10; t++) {
    vramblit(vram, 1, TRACK_H*t+7, s_track, TNAME_W, TNAME_H);
    vramblit(vram, KEY_LEFT_X, TRACK_H*t+KEY_Y, s_key_left, KEY_LEFT_W, KEY_H);
    for (int i = 0; i < KEY_OCTAVES; i++) {
      vramblit(vram, KEY_X+KEY_W*i, TRACK_H*t+KEY_Y,
                s_key_bg, KEY_W, KEY_H);
    }
    vramblit(vram, KEY_X+KEY_W*KEY_OCTAVES, TRACK_H*t+KEY_Y,
             s_key_right, KEY_RIGHT_W, KEY_H);
    vramblit_color(vram, BAR_L_X, TRACK_H*t+BAR_Y,
                   s_bar_l, BAR_L_W, BAR_H, 3);
    for (int i = 0; i < BAR_CNT; i++) {
      vramblit_color(vram, BAR_X+BAR_W*i, TRACK_H*t+BAR_Y,
                s_bar, BAR_W, BAR_H, 3);
    }
  }
  vramblit(vram, PLAYING_X, PLAYING_Y,
           s_playing, PLAYING_W, PLAYING_H);
  for (int x = 74; x < PC98_W; x++) {
    vram[332*PC98_W+x] = 7;
  }
  int height = (16+3)*3+8;
  for (int y = PC98_H-height; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      vram[y*PC98_W+x] = (y&1)^(x&1) ? 3 : 0;
    }
  }
  vram[(PC98_H-height)*PC98_W] = 0;
  vram[(PC98_H-1)*PC98_W] = 0;
  for (int i = 0; i < 3; i++) {
    fmdsp_putline(work->comment[i], vram, font, COMMENT_Y+COMMENT_H*i, 2);
  }
}

void fmdsp_update(struct fmdsp *fmdsp,
                  const struct fmdriver_work *work, uint8_t *vram) {
  for (int t = 0; t < 10; t++) {
    struct fmdriver_track_status *track = &work->track_status[t];
    uint8_t *track_type;
    switch (track->type) {
    case FMDRIVER_TRACK_FM:
      track_type = s_t_fm;
      break;
    case FMDRIVER_TRACK_SSG:
      track_type = s_t_ssg;
      break;
    case FMDRIVER_TRACK_ADPCM:
      track_type = s_t_adpcm;
      break;
    case FMDRIVER_TRACK_PPZ8:
      track_type = s_t_ppz8;
      break;
    }
    vramblit(vram, 1, TRACK_H*t+1, track_type, TNAME_W, TNAME_H);
    vramblit(vram, NUM_X+NUM_W*0, TRACK_H*t+1, s_num[(track->num/10)%10], NUM_W, NUM_H);
    vramblit(vram, NUM_X+NUM_W*1, TRACK_H*t+1, s_num[track->num%10], NUM_W, NUM_H);
    for (int i = 0; i < KEY_OCTAVES; i++) {
      vramblit(vram, KEY_X+KEY_W*i, TRACK_H*t+KEY_Y,
                s_key_bg, KEY_W, KEY_H);
      if (track->playing) {
        if (track->actual_key >> 4 == i) {
          vramblit_key(vram, KEY_X+KEY_W*i, TRACK_H*t+KEY_Y,
                      s_key_mask, KEY_W, KEY_H,
                      track->actual_key & 0xf, 8);
        }
        if (track->key >> 4 == i) {
          vramblit_key(vram, KEY_X+KEY_W*i, TRACK_H*t+KEY_Y,
                      s_key_mask, KEY_W, KEY_H,
                      track->key & 0xf, 6);
        }
      }
    }
    uint8_t color_on = track->key == 0xff ? 7 : 2;
    if (!track->playing) color_on = 3;
    vramblit_color(vram, BAR_L_X, TRACK_H*t+BAR_Y,
                   s_bar_l, BAR_L_W, BAR_H, color_on);
    for (int i = 0; i < BAR_CNT; i++) {
      int c = (i < (track->ticks_left>>2)) ? color_on : 3;
      vramblit_color(vram, BAR_X+BAR_W*i, TRACK_H*t+BAR_Y,
                s_bar, BAR_W, BAR_H, c);
    }
    vramblit_color(vram, BAR_X+BAR_W*(track->ticks>>2), TRACK_H*t+BAR_Y,
                   s_bar, BAR_W, BAR_H, 7);
  }
}

void fmdsp_vrampalette(struct fmdsp *fmdsp, const uint8_t *vram, uint8_t *vram32, int stride) {
  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      uint8_t r = fmdsp->palette[vram[y*PC98_W+x]*3+0];
      uint8_t g = fmdsp->palette[vram[y*PC98_W+x]*3+1];
      uint8_t b = fmdsp->palette[vram[y*PC98_W+x]*3+2];
      uint32_t data = (((uint32_t)r)<<16) | (((uint32_t)g)<<8) | ((uint32_t)b);
      uint32_t *row = (uint32_t *)(vram32 + y*stride);
      row[x] = data;
    }
  }
}

//2/1 - 7/14
// 0x21 - 0x7e
static void fontrom_copy_rows(uint8_t *font, const uint8_t *fontrom,
                              int rowstart, int rowend) {
  for (int row = rowstart; row < rowend; row++) {
    for (int cell = 0x20; cell < 0x80; cell++) {
      for (int y = 0; y < 16; y++) {
        // left
        font[0x000+((row-0x20)<<4)+(cell<<12)+y] = fontrom[0x800+(0x60*16*2*(row-0x20))+(cell<<5)+y];
        // right
        font[0x800+((row-0x20)<<4)+(cell<<12)+y] = fontrom[0x800+(0x60*16*2*(row-0x20))+(cell<<5)+y+16];
      }
    }
  }
}

void fmdsp_font_from_fontrom(uint8_t *font, const uint8_t *fontrom) {
  // ANK
  for (int i = 0; i < 256*16; i++) {
    font[0x80000+i] = fontrom[0x800+i];
  }
  fontrom_copy_rows(font, fontrom, 0x21, 0x50);
  fontrom_copy_rows(font, fontrom, 0x50, 0x76);
  fontrom_copy_rows(font, fontrom, 0x78, 0x7d);
}
