#include "fmdsp.h"
#include "fmdsp_sprites.h"
#include "font.h"
#include "fmdriver/fmdriver.h"
#include <stdio.h>

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

static void vramfill_color(uint8_t *vram, int x, int y,
                           int w, int h,
                           uint8_t color) {
  for (int yi = 0; yi < h; yi++) {
    for (int xi = 0; xi < w; xi++) {
      vram[(y+yi)*PC98_W+(x+xi)] = color;
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

void fmdsp_init(struct fmdsp *fmdsp, const struct fmdsp_font *font98) {
  for (int i = 0; i < FMDSP_PALETTE_COLORS*3; i++) {
    fmdsp->palette[i] = 0;//s_palettes[0][i];
    fmdsp->target_palette[i] = s_palettes[0][i];
  }
  fmdsp->font98 = font98;
}



static void vram_putchar(uint8_t *vram, const void *data,
                         int x, int y, int w, int h, uint8_t color,
                         bool bg
                        ) {
  const uint8_t *font = data;
  for (int yi = 0; yi < h; yi++) {
    for (int xi = 0; xi < w; xi++) {
      if (font[yi] & (1<<(7-xi))) {
        vram[(y+yi)*PC98_W+(x+xi)] = color;
      } else {
        if (bg) vram[(y+yi)*PC98_W+(x+xi)] = 0;
      }
    }
  }
}

static void fmdsp_putline(const char *strptr, uint8_t *vram,
                          const struct fmdsp_font *font,
                          int x, int y, uint8_t color, bool bg) {
  const uint8_t *cp932str = (const uint8_t *)strptr;
  bool sjis_is2nd = false;
  uint8_t sjis_1st;
  int fw = font->width_half;
  int fh = font->height;
  int xo = 0;

  while (*cp932str) {
    if (!sjis_is2nd) {
      if (!sjis_is_mb_start(*cp932str)) {
        if (*cp932str == '\t') {
          xo += fw*8;
          xo -= (xo % (fw*8));
          cp932str++;
        } else {
          if ((x+xo+8) > PC98_W) return;
          const void *fp = font->get(font, *cp932str++, FMDSP_FONT_ANK);
          if (fp) {
            vram_putchar(vram, fp, x+xo, y, fw, fh, color, bg);
          }
          xo += fw;
        }
      } else {
        sjis_is2nd = true;
        sjis_1st = *cp932str++;
      }
    } else {
      uint8_t sjis_2nd = *cp932str++;
      uint16_t jis = sjis2jis(sjis_1st, sjis_2nd);
      bool half = jis_is_halfwidth(jis);
      if ((x+xo+(half ? 8 : 16)) > PC98_W) return;
      const void *fp = font->get(font, jis, FMDSP_FONT_JIS_LEFT);
      if (fp) {
        vram_putchar(vram, fp, x+xo, y, fw, fh, color, bg);
      }
      xo += fw;
      if (!half) {
        fp = font->get(font, jis, FMDSP_FONT_JIS_RIGHT);
        if (fp) {
          vram_putchar(vram, fp, x+xo, y, fw, fh, color, bg);
        }
        xo += 8;
      }
      sjis_is2nd = false;
    }
  }
}

void fmdsp_vram_init(struct fmdsp *fmdsp,
                     struct fmdriver_work *work,
                     uint8_t *vram) {
  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  for (int t = 0; t < 10; t++) {
    struct fmdriver_track_status *track = &work->track_status[t];
    const char *track_type;
    switch (track->type) {
    case FMDRIVER_TRACK_FM:
      track_type = "FM   ";
      break;
    case FMDRIVER_TRACK_SSG:
      track_type = "SSG  ";
      break;
    case FMDRIVER_TRACK_ADPCM:
      track_type = "ADPCM";
      break;
    }
    fmdsp_putline(track_type, vram, &font_fmdsp_small, 1, TRACK_H*t, 2, true);
    vramblit(vram, NUM_X+NUM_W*0, TRACK_H*t+1, s_num[(track->num/10)%10], NUM_W, NUM_H);
    vramblit(vram, NUM_X+NUM_W*1, TRACK_H*t+1, s_num[track->num%10], NUM_W, NUM_H);

    //vramblit(vram, 1, TRACK_H*t+7, s_track, TNAME_W, TNAME_H);
    fmdsp_putline("TRACK.", vram, &font_fmdsp_small, 1, TRACK_H*t+6, 1, true);
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
  if (fmdsp->font98) {
    for (int i = 0; i < 3; i++) {
      fmdsp_putline(work->comment[i], vram, fmdsp->font98,
                    0, COMMENT_Y+COMMENT_H*i, 2, false);
    }
  }
}

enum {
  FADEDELTA = 16
};

void fmdsp_palette_set(struct fmdsp *fmdsp, int p) {
  if (p < 0) return;
  if (p >= PALETTE_NUM) return;
  for (int i = 0; i < FMDSP_PALETTE_COLORS*3; i++) {
    fmdsp->target_palette[i] = s_palettes[p][i];
  }
}

static void fmdsp_palette_fade(struct fmdsp *fmdsp) {
  for (int i = 0; i < FMDSP_PALETTE_COLORS*3; i++) {
    uint8_t p = fmdsp->palette[i];
    uint8_t t = fmdsp->target_palette[i];
    if (p < t) {
      if ((p + FADEDELTA) < t) {
        p += FADEDELTA;
      } else {
        p = t;
      }
    } else if (p > t) {
      if (p > (t + FADEDELTA)) {
        p -= FADEDELTA;
      } else {
        p = t;
      }
    }
    fmdsp->palette[i] = p;
  }
}

void fmdsp_update(struct fmdsp *fmdsp,
                  const struct fmdriver_work *work, uint8_t *vram) {
  for (int t = 0; t < 10; t++) {
    const struct fmdriver_track_status *track = &work->track_status[t];
    char track_info[5] = "    ";
    if (track->playing) {
      switch (track->info) {
      case FMDRIVER_TRACK_INFO_PPZ8:
        snprintf(track_info, sizeof(track_info), "PPZ8");
        break;
      case FMDRIVER_TRACK_INFO_SSG_NOISE_ONLY:
        snprintf(track_info, sizeof(track_info), "N%02X ", work->ssg_noise_freq);
        break;
      case FMDRIVER_TRACK_INFO_SSG_NOISE_MIX:
        snprintf(track_info, sizeof(track_info), "M%02X ", work->ssg_noise_freq);
        break;
      }
    }
    fmdsp_putline(track_info, vram, &font_fmdsp_small, TINFO_X, TRACK_H*t+6, 2, true);
    char notestr[5] = " S  ";
    if (track->playing) {
      if ((track->key&0xf) == 0xf) {
        snprintf(notestr, sizeof(notestr), " R  ");
      } else {
        const char *keystr = "  ";
        static const char *keytable[0x10] = {
          "C ", "C+", "D ", "D+", "E ", "F ", "F+", "G ", "G+", "A ", "A+", "B "
        };
        if (keytable[track->key&0xf]) keystr = keytable[track->key&0xf];
        snprintf(notestr, sizeof(notestr), "o%d%s", track->key>>4, keystr);
      }
    }
    char numbuf[5];
    fmdsp_putline("KN:", vram, &font_fmdsp_small, TDETAIL_X, TRACK_H*t+6, 1, true);
    fmdsp_putline(notestr, vram, &font_fmdsp_small, TDETAIL_KN_V_X, TRACK_H*t+6, 1, true);
    fmdsp_putline("TN:", vram, &font_fmdsp_small, TDETAIL_TN_X, TRACK_H*t+6, 1, true);
    snprintf(numbuf, sizeof(numbuf), "%03d", track->tonenum);
    fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_TN_V_X, TRACK_H*t+6, 1, true);
    fmdsp_putline("VL:", vram, &font_fmdsp_small, TDETAIL_VL_X, TRACK_H*t+6, 1, true);
    snprintf(numbuf, sizeof(numbuf), "%03d", track->volume);
    fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_VL_V_X, TRACK_H*t+6, 1, true);
    fmdsp_putline("GT:", vram, &font_fmdsp_small, TDETAIL_GT_X, TRACK_H*t+6, 1, true);
    //snprintf(numbuf, sizeof(numbuf), "%03d", track->tonenum);
    //fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_GT_V_X, TRACK_H*t+6, 1, true);
    fmdsp_putline("DT:", vram, &font_fmdsp_small, TDETAIL_DT_X, TRACK_H*t+6, 1, true);
    if (track->detune) {
      snprintf(numbuf, sizeof(numbuf), "%+04d", track->detune);
    } else {
      snprintf(numbuf, sizeof(numbuf), " 000");
    }
    fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_DT_V_X, TRACK_H*t+6, 1, true);
    fmdsp_putline("M:", vram, &font_fmdsp_small, TDETAIL_M_X, TRACK_H*t+6, 1, true);
    fmdsp_putline(track->status, vram, &font_fmdsp_small, TDETAIL_M_V_X, TRACK_H*t+6, 1, true);
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
  fmdsp_palette_fade(fmdsp);
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
