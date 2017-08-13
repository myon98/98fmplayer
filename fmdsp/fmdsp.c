#include "fmdsp.h"
#include "fmdsp_sprites.h"
#include "font.h"
#include "fmdriver/fmdriver.h"
#include <stdio.h>
#include "libopna/opna.h"
#include "fmdsp_platform_info.h"
#include "version.h"
#include <math.h>
#include <string.h>

fmdsp_vramlookup_type fmdsp_vramlookup_func = fmdsp_vramlookup_c;

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

void fmdsp_init(struct fmdsp *fmdsp, const struct fmdsp_font *font98) {
  for (int i = 0; i < FMDSP_PALETTE_COLORS*3; i++) {
    fmdsp->palette[i] = 0;//s_palettes[0][i];
    fmdsp->target_palette[i] = s_palettes[0][i];
  }
  fmdsp->font98 = font98;
  fmdsp->style = FMDSP_DISPSTYLE_ORIGINAL;
  fmdsp->style_updated = true;
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
          if ((x+xo+fw) > PC98_W) return;
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
      if ((x+xo+fw*(half ? 1 : 2)) > PC98_W) return;
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

static struct {
  uint8_t type;
  uint8_t num;
} track_type_table[FMDRIVER_TRACK_NUM] = {
  {FMDRIVER_TRACKTYPE_FM, 1},
  {FMDRIVER_TRACKTYPE_FM, 2},
  {FMDRIVER_TRACKTYPE_FM, 3},
  {FMDRIVER_TRACKTYPE_FM, 3},
  {FMDRIVER_TRACKTYPE_FM, 3},
  {FMDRIVER_TRACKTYPE_FM, 3},
  {FMDRIVER_TRACKTYPE_FM, 4},
  {FMDRIVER_TRACKTYPE_FM, 5},
  {FMDRIVER_TRACKTYPE_FM, 6},
  {FMDRIVER_TRACKTYPE_SSG, 1},
  {FMDRIVER_TRACKTYPE_SSG, 2},
  {FMDRIVER_TRACKTYPE_SSG, 3},
  {FMDRIVER_TRACKTYPE_ADPCM, 1},
  {FMDRIVER_TRACKTYPE_PPZ8, 1},
  {FMDRIVER_TRACKTYPE_PPZ8, 2},
  {FMDRIVER_TRACKTYPE_PPZ8, 3},
  {FMDRIVER_TRACKTYPE_PPZ8, 4},
  {FMDRIVER_TRACKTYPE_PPZ8, 5},
  {FMDRIVER_TRACKTYPE_PPZ8, 6},
  {FMDRIVER_TRACKTYPE_PPZ8, 7},
  {FMDRIVER_TRACKTYPE_PPZ8, 8},
};

enum {
  FMDSP_TRACK_DISP_CNT_DEFAULT = 10
};

static int8_t track_disp_table_default[FMDSP_TRACK_DISP_CNT_DEFAULT] = {
  FMDRIVER_TRACK_FM_1,
  FMDRIVER_TRACK_FM_2,
  FMDRIVER_TRACK_FM_3,
  FMDRIVER_TRACK_FM_4,
  FMDRIVER_TRACK_FM_5,
  FMDRIVER_TRACK_FM_6,
  FMDRIVER_TRACK_SSG_1,
  FMDRIVER_TRACK_SSG_2,
  FMDRIVER_TRACK_SSG_3,
  FMDRIVER_TRACK_ADPCM,
};
static int8_t track_disp_table_opn[FMDSP_TRACK_DISP_CNT_DEFAULT] = {
  FMDRIVER_TRACK_FM_1,
  FMDRIVER_TRACK_FM_2,
  FMDRIVER_TRACK_FM_3,
  FMDRIVER_TRACK_FM_3_EX_1,
  FMDRIVER_TRACK_FM_3_EX_2,
  FMDRIVER_TRACK_FM_3_EX_3,
  FMDRIVER_TRACK_SSG_1,
  FMDRIVER_TRACK_SSG_2,
  FMDRIVER_TRACK_SSG_3,
  FMDRIVER_TRACK_ADPCM,
};
static int8_t track_disp_table_ppz8[FMDSP_TRACK_DISP_CNT_DEFAULT] = {
  FMDRIVER_TRACK_PPZ8_1,
  FMDRIVER_TRACK_PPZ8_2,
  FMDRIVER_TRACK_PPZ8_3,
  FMDRIVER_TRACK_PPZ8_4,
  FMDRIVER_TRACK_PPZ8_5,
  FMDRIVER_TRACK_PPZ8_6,
  FMDRIVER_TRACK_PPZ8_7,
  FMDRIVER_TRACK_PPZ8_8,
  FMDRIVER_TRACK_ADPCM,
  -1
};

static void fmdsp_track_init_13(struct fmdsp *fmdsp,
                                uint8_t *vram) {
  for (int y = 0; y < TRACK_H*FMDSP_TRACK_DISP_CNT_DEFAULT; y++) {
    for (int x = 0; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  for (int i = 0; i < FMDRIVER_TRACK_PPZ8_1; i++) {
    int t = i;
    const char *track_type = "     ";
    switch (track_type_table[t].type) {
    case FMDRIVER_TRACKTYPE_FM:
      track_type = "FM   ";
      break;
    case FMDRIVER_TRACKTYPE_SSG:
      track_type = "SSG  ";
      break;
    case FMDRIVER_TRACKTYPE_ADPCM:
      track_type = "ADPCM";
      break;
    }
    fmdsp_putline(track_type, vram, &font_fmdsp_small, 1, TRACK_H_S*i, 2, true);

    fmdsp_putline("TRACK.", vram, &font_fmdsp_small, 1, TRACK_H_S*i+6, 1, true);
    vramblit(vram, KEY_LEFT_X, TRACK_H_S*i+KEY_Y, s_key_left + KEY_LEFT_S_OFFSET, KEY_LEFT_W, KEY_H_S);
    for (int j = 0; j < KEY_OCTAVES; j++) {
      vramblit(vram, KEY_X+KEY_W*j, TRACK_H_S*i+KEY_Y,
                s_key_bg + KEY_S_OFFSET, KEY_W, KEY_H_S);
    }
    vramblit(vram, KEY_X+KEY_W*KEY_OCTAVES, TRACK_H_S*i+KEY_Y,
             s_key_right + KEY_RIGHT_S_OFFSET, KEY_RIGHT_W, KEY_H_S);
    vramblit_color(vram, BAR_L_X, TRACK_H_S*i+BAR_Y,
                   s_bar_l, BAR_L_W, BAR_H, 3);
    for (int j = 0; j < BAR_CNT; j++) {
      vramblit_color(vram, BAR_X+BAR_W*j, TRACK_H_S*i+BAR_Y,
                s_bar, BAR_W, BAR_H, 3);
    }
  }
}

static void fmdsp_track_init_10(struct fmdsp *fmdsp,
                                uint8_t *vram) {
  for (int y = 0; y < TRACK_H*FMDSP_TRACK_DISP_CNT_DEFAULT; y++) {
    for (int x = 0; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  for (int i = 0; i < FMDSP_TRACK_DISP_CNT_DEFAULT; i++) {
    int t;
    if (fmdsp->style == FMDSP_DISPSTYLE_DEFAULT || fmdsp->style == FMDSP_DISPSTYLE_ORIGINAL) t = track_disp_table_default[i];
    else if (fmdsp->style == FMDSP_DISPSTYLE_OPN) t = track_disp_table_opn[i];
    else t = track_disp_table_ppz8[i];
    if (t < 0) continue;
    const char *track_type = "     ";
    switch (track_type_table[t].type) {
    case FMDRIVER_TRACKTYPE_FM:
      track_type = "FM   ";
      break;
    case FMDRIVER_TRACKTYPE_SSG:
      track_type = "SSG  ";
      break;
    case FMDRIVER_TRACKTYPE_ADPCM:
      track_type = "ADPCM";
      break;
    case FMDRIVER_TRACKTYPE_PPZ8:
      track_type = "PPZ8 ";
      break;
    }
    fmdsp_putline(track_type, vram, &font_fmdsp_small, 1, TRACK_H*i, 2, true);

    fmdsp_putline("TRACK.", vram, &font_fmdsp_small, 1, TRACK_H*i+6, 1, true);
    vramblit(vram, KEY_LEFT_X, TRACK_H*i+KEY_Y, s_key_left, KEY_LEFT_W, KEY_H);
    for (int j = 0; j < KEY_OCTAVES; j++) {
      vramblit(vram, KEY_X+KEY_W*j, TRACK_H*i+KEY_Y,
                s_key_bg, KEY_W, KEY_H);
    }
    vramblit(vram, KEY_X+KEY_W*KEY_OCTAVES, TRACK_H*i+KEY_Y,
             s_key_right, KEY_RIGHT_W, KEY_H);
    vramblit_color(vram, BAR_L_X, TRACK_H*i+BAR_Y,
                   s_bar_l, BAR_L_W, BAR_H, 3);
    for (int j = 0; j < BAR_CNT; j++) {
      vramblit_color(vram, BAR_X+BAR_W*j, TRACK_H*i+BAR_Y,
                s_bar, BAR_W, BAR_H, 3);
    }
  }
  if (fmdsp->style == FMDSP_DISPSTYLE_ORIGINAL) {
    vramblit(vram, LOGO_FM_X, LOGO_Y, s_logo_fm, LOGO_FM_W, LOGO_H);
    vramblit(vram, LOGO_DS_X, LOGO_Y, s_logo_ds, LOGO_DS_W, LOGO_H);
    vramblit(vram, LOGO_P_X, LOGO_Y, s_logo_p, LOGO_P_W, LOGO_H);
    fmdsp_putline("MUS", vram, &font_fmdsp_small, TOP_MUS_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("IC", vram, &font_fmdsp_small, TOP_IC_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("F", vram, &font_fmdsp_small, TOP_F_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("ILE", vram, &font_fmdsp_small, TOP_ILE_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("SELECTOR", vram, &font_fmdsp_small, TOP_SELECTOR_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("&", vram, &font_fmdsp_small, TOP_AND_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("STATUS", vram, &font_fmdsp_small, TOP_STATUS_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("D", vram, &font_fmdsp_small, TOP_D_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline("ISPLAY", vram, &font_fmdsp_small, TOP_ISPLAY_X, TOP_MUSIC_Y, 2, true);
    vramblit(vram, TOP_VER_X, VER_Y, s_ver, VER_W, VER_H);
    fmdsp_putline(FMPLAYER_VERSION_0 ".", vram, &font_fmdsp_small, VER_0_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline(FMPLAYER_VERSION_1 ".", vram, &font_fmdsp_small, VER_1_X, TOP_MUSIC_Y, 2, true);
    fmdsp_putline(FMPLAYER_VERSION_2, vram, &font_fmdsp_small, VER_2_X, TOP_MUSIC_Y, 2, true);
    
    vramblit(vram, TOP_MUS_X, TOP_TEXT_Y, s_text, TOP_TEXT_W, TOP_TEXT_H);

    fmdsp_putline("DR", vram, &font_fmdsp_small, DRIVER_TEXT_X, DRIVER_TEXT_Y, 7, true);
    fmdsp_putline("IVER", vram, &font_fmdsp_small, DRIVER_TEXT_2_X, DRIVER_TEXT_Y, 7, true);
    vramblit_color(vram, DRIVER_TRI_X, DRIVER_TRI_Y, s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H, 7);
    vramblit(vram, CURL_LEFT_X, CURL_Y, s_curl_left, CURL_W, CURL_H);
    vramblit(vram, CURL_RIGHT_X, CURL_Y, s_curl_right, CURL_W, CURL_H);

    for (int x = 0; x < 82; x++) {
      vram[14*PC98_W+312+x] = 2;
    }
    for (int x = 0; x < 239; x++) {
      vram[14*PC98_W+395+x] = 7;
    }
    for (int x = 0; x < TIME_BAR_W; x++) {
      for (int y = 0; y < TIME_BAR_H; y++) {
        vram[(TIME_Y-2+y)*PC98_W+TIME_BAR_X+x] = 2;
        vram[(CLOCK_Y-2+y)*PC98_W+TIME_BAR_X+x] = 2;
        vram[(TIMERB_Y-2+y)*PC98_W+TIME_BAR_X+x] = 2;
        vram[(LOOPCNT_Y-2+y)*PC98_W+TIME_BAR_X+x] = 2;
        vram[(VOLDOWN_Y-2+y)*PC98_W+TIME_BAR_X+x] = 2;
        vram[(PGMNUM_Y-2+y)*PC98_W+TIME_BAR_X+x] = 2;
      }
    }
    for (int i = 0; i < 6; i++) {
      vramblit(vram, TIME_TRI_X, TIME_Y+8+19*i, s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H);
    }
    fmdsp_putline("PASSED", vram, &font_fmdsp_small, TIME_TEXT_X, TIME_Y-2, 2, true);
    fmdsp_putline("T", vram, &font_fmdsp_small, TIME_TEXT_X+11, TIME_Y+5, 2, true);
    fmdsp_putline("IME", vram, &font_fmdsp_small, TIME_TEXT_X+15, TIME_Y+5, 2, true);
    fmdsp_putline("CLOCK", vram, &font_fmdsp_small, TIME_TEXT_X, CLOCK_Y-2, 2, true);
    fmdsp_putline(" COUNT", vram, &font_fmdsp_small, TIME_TEXT_X, CLOCK_Y+5, 2, true);
    fmdsp_putline("T", vram, &font_fmdsp_small, TIME_TEXT_X, TIMERB_Y-2, 2, true);
    fmdsp_putline("IMER", vram, &font_fmdsp_small, TIME_TEXT_X+4, TIMERB_Y-2, 2, true);
    fmdsp_putline(" CYCLE", vram, &font_fmdsp_small, TIME_TEXT_X, TIMERB_Y+5, 2, true);
    fmdsp_putline("LOOP", vram, &font_fmdsp_small, TIME_TEXT_X, LOOPCNT_Y-2, 2, true);
    fmdsp_putline(" COUNT", vram, &font_fmdsp_small, TIME_TEXT_X, LOOPCNT_Y+5, 2, true);
    fmdsp_putline("VOLUME", vram, &font_fmdsp_small, TIME_TEXT_X, VOLDOWN_Y-2, 2, true);
    fmdsp_putline("  DOWN", vram, &font_fmdsp_small, TIME_TEXT_X, VOLDOWN_Y+5, 2, true);
    fmdsp_putline("PGM", vram, &font_fmdsp_small, TIME_TEXT_X, PGMNUM_Y-2, 2, true);
    fmdsp_putline("NUMBER", vram, &font_fmdsp_small, TIME_TEXT_X, PGMNUM_Y+5, 2, true);

    for (int x = 0; x < TIME_BAR_W; x++) {
      for (int y = 0; y < TIME_BAR_H; y++) {
        vram[(CPU_Y+y)*PC98_W+CPU_BAR_X+x] = 2;
      }
    }
    fmdsp_putline("CPU", vram, &font_fmdsp_small, CPU_X, CPU_Y, 2, true);
    fmdsp_putline("POWER", vram, &font_fmdsp_small, CPU_X+17, CPU_Y, 2, true);
    fmdsp_putline("COUNT", vram, &font_fmdsp_small, CPU_X+17, CPU_Y+7, 2, true);
    vramblit(vram, CPU_TRI_X, CPU_TRI_Y, s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H);
    for (int x = 0; x < TIME_BAR_W; x++) {
      for (int y = 0; y < TIME_BAR_H; y++) {
        vram[(CPU_Y+y)*PC98_W+FPS_BAR_X+x] = 2;
      }
    }
    fmdsp_putline("FRAMES", vram, &font_fmdsp_small, FPS_X, CPU_Y, 2, true);
    fmdsp_putline("PER", vram, &font_fmdsp_small, FPS_X+32, CPU_Y, 2, true);
    fmdsp_putline("SECOND", vram, &font_fmdsp_small, FPS_X+17, CPU_Y+7, 2, true);
    vramblit(vram, FPS_TRI_X, CPU_TRI_Y, s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H);
    for (int x = 0; x < 322; x++) {
      vram[132*PC98_W+312+x] = 7;
    }
    fmdsp_putline("SENS", vram, &font_fmdsp_small, SPECTRUM_X-40, SPECTRUM_Y-6, 7, true);
    fmdsp_putline("-48", vram, &font_fmdsp_small, SPECTRUM_X-19, SPECTRUM_Y-6, 7, true);
    fmdsp_putline("0", vram, &font_fmdsp_small, SPECTRUM_X-9, SPECTRUM_Y-63, 7, true);
    fmdsp_putline("dB", vram, &font_fmdsp_small, SPECTRUM_X-14, SPECTRUM_Y-71, 7, true);
    fmdsp_putline("SPECTRUM", vram, &font_fmdsp_small, SPECTRUM_X+197, SPECTRUM_Y-71, 7, true);
    fmdsp_putline("ANAL", vram, &font_fmdsp_small, SPECTRUM_X+241, SPECTRUM_Y-71, 7, true);
    fmdsp_putline("YzER", vram, &font_fmdsp_small, SPECTRUM_X+260, SPECTRUM_Y-71, 7, true);
    for (int y = 0; y < 63; y++) {
      vram[(SPECTRUM_Y-y)*PC98_W+SPECTRUM_X-2] = 2;
      if (!(y % 2)) {
        vram[(SPECTRUM_Y-y)*PC98_W+SPECTRUM_X-3] = 2;
      }
      if (!(y % 8)) {
        vram[(SPECTRUM_Y-y)*PC98_W+SPECTRUM_X-4] = 2;
      }
    }
    fmdsp_putline("FREQ", vram, &font_fmdsp_small, SPECTRUM_X-24, SPECTRUM_Y+1, 1, true);
    for (int x = 0; x < 17; x++) {
      vram[(SPECTRUM_Y+4)*PC98_W+SPECTRUM_X+1+2*x] = 1;
    }
    fmdsp_putline("250", vram, &font_fmdsp_small, SPECTRUM_X+36, SPECTRUM_Y+1, 1, true);
    for (int x = 0; x < 15; x++) {
      vram[(SPECTRUM_Y+4)*PC98_W+SPECTRUM_X+52+2*x] = 1;
    }
    fmdsp_putline("500", vram, &font_fmdsp_small, SPECTRUM_X+83, SPECTRUM_Y+1, 1, true);
    for (int x = 0; x < 17; x++) {
      vram[(SPECTRUM_Y+4)*PC98_W+SPECTRUM_X+99+2*x] = 1;
    }
    fmdsp_putline("1", vram, &font_fmdsp_small, SPECTRUM_X+133, SPECTRUM_Y+1, 1, true);
    fmdsp_putline("k", vram, &font_fmdsp_small, SPECTRUM_X+133+6, SPECTRUM_Y+1, 1, true);
    for (int x = 0; x < 19; x++) {
      vram[(SPECTRUM_Y+4)*PC98_W+SPECTRUM_X+144+2*x] = 1;
    }
    fmdsp_putline("2k", vram, &font_fmdsp_small, SPECTRUM_X+183, SPECTRUM_Y+1, 1, true);
    for (int x = 0; x < 18; x++) {
      vram[(SPECTRUM_Y+4)*PC98_W+SPECTRUM_X+193+2*x] = 1;
    }
    fmdsp_putline("4k", vram, &font_fmdsp_small, SPECTRUM_X+230, SPECTRUM_Y+1, 1, true);
    for (int x = 0; x < 20; x++) {
      vram[(SPECTRUM_Y+4)*PC98_W+SPECTRUM_X+240+2*x] = 1;
    }
    fmdsp_putline("ON", vram, &font_fmdsp_small,
                  LEVEL_TEXT_X+5, LEVEL_TEXT_Y, 1, true);
    fmdsp_putline("PAN", vram, &font_fmdsp_small,
                  LEVEL_TEXT_X, LEVEL_TEXT_Y+8, 1, true);
    fmdsp_putline("PROG", vram, &font_fmdsp_small,
                  LEVEL_TEXT_X-5, LEVEL_TEXT_Y+16, 1, true);
    fmdsp_putline("KEY", vram, &font_fmdsp_small,
                  LEVEL_TEXT_X, LEVEL_TEXT_Y+23, 1, true);
    fmdsp_putline("FM1", vram, &font_fmdsp_small,
                  LEVEL_X+LEVEL_W*0, LEVEL_TRACK_Y, 7, true);
    fmdsp_putline("FM4", vram, &font_fmdsp_small,
                  LEVEL_X+LEVEL_W*3, LEVEL_TRACK_Y, 7, true);
    fmdsp_putline("SSG", vram, &font_fmdsp_small,
                  LEVEL_X+LEVEL_W*6, LEVEL_TRACK_Y, 7, true);
    fmdsp_putline("RHY", vram, &font_fmdsp_small,
                  LEVEL_X+LEVEL_W*9, LEVEL_TRACK_Y, 7, true);
    fmdsp_putline("ADP", vram, &font_fmdsp_small,
                  LEVEL_X+LEVEL_W*10, LEVEL_TRACK_Y, 7, true);
    fmdsp_putline("PPZ", vram, &font_fmdsp_small,
                  LEVEL_X+LEVEL_W*11, LEVEL_TRACK_Y, 7, true);
    for (int y = 0; y < 63; y++) {
      vram[(LEVEL_Y+y)*PC98_W+LEVEL_X-2] = 2;
      if ((y % 2) == 0) vram[(LEVEL_Y+y)*PC98_W+LEVEL_X-3] = 2;
      if ((y % 8) == 6) vram[(LEVEL_Y+y)*PC98_W+LEVEL_X-4] = 2;
    }
    fmdsp_putline("0", vram, &font_fmdsp_small,
                  LEVEL_X-9, LEVEL_Y-1, 7, true);
    fmdsp_putline("-48", vram, &font_fmdsp_small,
                  LEVEL_X-19, LEVEL_Y+56, 7, true);
  }
}

void fmdsp_vram_init(struct fmdsp *fmdsp,
                     struct fmdriver_work *work,
                     uint8_t *vram) {
  fmdsp->style_updated = true;
  for (int y = 0; y < PC98_H; y++) {
    for (int x = 0; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  vramblit(vram, PLAYING_X, PLAYING_Y,
           s_playing, PLAYING_W, PLAYING_H);
  vramblit(vram, FILEBAR_X, PLAYING_Y,
           s_filebar, FILEBAR_W, FILEBAR_H);
  fmdsp_putline("MUS", vram, &font_fmdsp_small,
                FILEBAR_MUS_X, PLAYING_Y+1, 2, false);
  fmdsp_putline("IC", vram, &font_fmdsp_small,
                FILEBAR_IC_X, PLAYING_Y+1, 2, false);
  fmdsp_putline("F", vram, &font_fmdsp_small,
                FILEBAR_F_X, PLAYING_Y+1, 2, false);
  fmdsp_putline("ILE", vram, &font_fmdsp_small,
                FILEBAR_ILE_X, PLAYING_Y+1, 2, false);
  vramblit(vram, FILEBAR_TRI_X, FILEBAR_TRI_Y,
           s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H);
  for (int x = 74; x < PC98_W; x++) {
    vram[332*PC98_W+x] = 7;
  }
  fmdsp_putline(work->filename, vram, &font_fmdsp_medium,
                FILEBAR_FILENAME_X, PLAYING_Y, 2, false);
  vramblit(vram, PCM1FILEBAR_X, PLAYING_Y,
           s_filebar, FILEBAR_W, FILEBAR_H);
  fmdsp_putline("PCM1", vram, &font_fmdsp_small,
                PCM1FILETXT_X, PLAYING_Y+1, 2, false);
  vramblit(vram, PCM1FILETRI_X, FILEBAR_TRI_Y,
           s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H);
  fmdsp_putline(work->pcmname[0], vram, &font_fmdsp_medium,
                PCM1FILENAME_X, PLAYING_Y, 2 + work->pcmerror[0], false);
  vramblit(vram, PCM2FILEBAR_X, PLAYING_Y,
           s_filebar, FILEBAR_W, FILEBAR_H);
  fmdsp_putline("PCM2", vram, &font_fmdsp_small,
                PCM2FILETXT_X, PLAYING_Y+1, 2, false);
  vramblit(vram, PCM2FILETRI_X, FILEBAR_TRI_Y,
           s_filebar_tri, FILEBAR_TRI_W, FILEBAR_TRI_H);
  fmdsp_putline(work->pcmname[1], vram, &font_fmdsp_medium,
                PCM2FILENAME_X, PLAYING_Y, 2 + work->pcmerror[1], false);
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

static void fmdsp_track_info_fm(const struct opna *opna,
                                int chi, const bool *slotmask,
                                int x, int y,
                                uint8_t *vram
) {
  const struct opna_fm_channel *ch = &opna->fm.channel[chi];
  for (int si = 0; si < 4; si++) {
    if (slotmask && slotmask[si]) continue;
    const struct opna_fm_slot *s = &ch->slot[si];
    int level = s->tl << 5;                // (0 - 4064)
    int envlevel = level + (s->env<<2);
    int leveld = (4096 - level) / 64;      // (0 - 63)
    if (leveld < 0) leveld = 0;
    int envleveld = (4096 - envlevel) / 64;
    if (envleveld < 0) envleveld = 0;
    for (int px = 0; px < 64; px++) {
      int color = 3;
      if (px < envleveld) color = 2;
      if (px == leveld) color = 7;
      for (int py = 0; py < 4; py++) {
        vram[(y+2+py+si*6)*PC98_W+(x+px*2)] = color;
      }
    }
    const char *envstr = "";
    switch (s->env_state) {
    case ENV_ATTACK:
      envstr = "ATT";
      break;
    case ENV_DECAY:
      envstr = "DEC";
      break;
    case ENV_SUSTAIN:
      envstr = "SUS";
      break;
    case ENV_RELEASE:
      envstr = "REL";
      break;
    }
    fmdsp_putline(envstr, vram, &font_fmdsp_small, x+130, y+si*6, 1, false);
    char strbuf[4];
    snprintf(strbuf, sizeof(strbuf), "%03X", s->env);
    fmdsp_putline(strbuf, vram, &font_fmdsp_small, x+150, y+si*6, 1, false);
  }
  char strbuf[5];
  snprintf(strbuf, sizeof(strbuf), "%04X", ch->fnum);
  fmdsp_putline(strbuf, vram, &font_fmdsp_small, x+170, y, 1, false);
}

static void fmdsp_track_info_ssg(const struct opna *opna,
                                 int chi,
                                 int x, int y,
                                 uint8_t *vram
) {
  int envleveld = opna_ssg_channel_level(&opna->ssg, chi);
  for (int px = 0; px < 64; px++) {
    int color = 3;
    if (px < (envleveld+31)) color = 2;
    if (px < 32) color = 7;
    for (int py = 0; py < 4; py++) {
      vram[(y+2+py)*PC98_W+(x+px*2)] = color;
    }
  }
  char strbuf[5];
  snprintf(strbuf, sizeof(strbuf), " %03X", opna_ssg_tone_period(&opna->ssg, chi));
  fmdsp_putline(strbuf, vram, &font_fmdsp_small, x+170, y, 1, false);
}

static void fmdsp_track_info_adpcm(const struct opna *opna,
                                   int x, int y,
                                   uint8_t *vram) {
  fmdsp_putline("VOL DELTA  START    PTR    END",
                vram, &font_fmdsp_small, x, y, 1, false);
  char buf[7];
  snprintf(buf, sizeof(buf), "%3d", opna->adpcm.vol);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%04X", opna->adpcm.delta);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+25, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%06X", (opna->adpcm.start)<<5);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+50, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%06X", (opna->adpcm.ramptr)<<5);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+85, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%06X", ((opna->adpcm.end+1)<<5)-1);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+120, y+6, 1, false);
}

static void fmdsp_track_info_ppz8(const struct ppz8 *ppz8,
                                  int chi,
                                  int x, int y,
                                  uint8_t *vram) {
  if (!ppz8) return;
  const struct ppz8_channel *ch = &ppz8->channel[chi];
  fmdsp_putline("PAN VOL     FREQ      PTR      END    LOOPS    LOOPE",
                vram, &font_fmdsp_small, x, y, 1, false);
  char buf[9];
  if (ch->pan) {
    int pan = ch->pan-5;
    snprintf(buf, sizeof(buf), "%+d", pan);
    char c = ' ';
    if (pan < 0) c = 'L';
    if (pan > 0) c = 'R';
    buf[0] = c;
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+5, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%03d", ch->vol);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+20, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%08X", ch->freq);
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+40, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%08X", (unsigned)(ch->ptr>>16));
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+85, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%08X", (unsigned)(ch->endptr>>16));
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+130, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%08X", (unsigned)(ch->loopstartptr>>16));
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+175, y+6, 1, false);
  snprintf(buf, sizeof(buf), "%08X", (unsigned)(ch->loopendptr>>16));
  fmdsp_putline(buf, vram, &font_fmdsp_small, x+220, y+6, 1, false);
}

static void fmdsp_track_without_key(
  struct fmdsp *fmdsp,
  const struct fmdriver_work *work,
  const struct fmdriver_track_status *track,
  int t,
  int y,
  uint8_t *vram
) {
  int tracknum = track_type_table[t].num;
  const uint8_t *num1 = s_num[(tracknum/10)%10];
  const uint8_t *num2 = s_num[tracknum%10];
  if (fmdsp->masked[t]) {
    num1 = num2 = s_num[10];
  }
  
  vramblit(vram, NUM_X+NUM_W*0, y+1, num1, NUM_W, NUM_H);
  vramblit(vram, NUM_X+NUM_W*1, y+1, num2, NUM_W, NUM_H);
  const char *track_info1 = "    ";
  char track_info2[5] = "    ";
  if (track->playing || track->info == FMDRIVER_TRACK_INFO_SSGEFF) {
    switch (track->info) {
    case FMDRIVER_TRACK_INFO_PPZ8:
      snprintf(track_info2, sizeof(track_info2), "PPZ8");
      break;
    case FMDRIVER_TRACK_INFO_PDZF:
      snprintf(track_info2, sizeof(track_info2), "PDZF");
      break;
    case FMDRIVER_TRACK_INFO_SSGEFF:
      track_info1 = "EFF ";
      /* FALLTHRU */
    case FMDRIVER_TRACK_INFO_SSG:
      if (track->ssg_noise) {
        snprintf(track_info2, sizeof(track_info2), "%c%02X ",
                 track->ssg_tone ? 'M' : 'N',
                 work->ssg_noise_freq);
      }
      break;
    case FMDRIVER_TRACK_INFO_FM3EX:
      track_info1 = "EX  ";
      for (int c = 0; c < 4; c++) {
        track_info2[c] = track->fmslotmask[c] ? ' ' : ('1'+c);
      }
      break;
    default:
      break;
    }
  }
  fmdsp_putline(track_info1, vram, &font_fmdsp_small, TINFO_X, y+0, 2, true);
  fmdsp_putline(track_info2, vram, &font_fmdsp_small, TINFO_X, y+6, 2, true);
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
  fmdsp_putline("KN:", vram, &font_fmdsp_small, TDETAIL_X, y+6, 1, true);
  fmdsp_putline(notestr, vram, &font_fmdsp_small, TDETAIL_KN_V_X, y+6, 1, true);
  fmdsp_putline("TN:", vram, &font_fmdsp_small, TDETAIL_TN_X, y+6, 1, true);
  snprintf(numbuf, sizeof(numbuf), "%03d", track->tonenum);
  fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_TN_V_X, y+6, 1, true);
  fmdsp_putline("Vl", vram, &font_fmdsp_small, TDETAIL_VL_X, y+6, 1, true);
  fmdsp_putline(":", vram, &font_fmdsp_small, TDETAIL_VL_C_X, y+6, 1, true);
  snprintf(numbuf, sizeof(numbuf), "%03d", track->volume);
  fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_VL_V_X, y+6, 1, true);
  fmdsp_putline("GT:", vram, &font_fmdsp_small, TDETAIL_GT_X, y+6, 1, true);
  snprintf(numbuf, sizeof(numbuf), "%03d", track->gate);
  fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_GT_V_X, y+6, 1, true);
  fmdsp_putline("DT:", vram, &font_fmdsp_small, TDETAIL_DT_X, y+6, 1, true);
  snprintf(numbuf, sizeof(numbuf), "%03d", (track->detune > 0) ? track->detune : -track->detune);
  fmdsp_putline(numbuf, vram, &font_fmdsp_small, TDETAIL_DT_V_X, y+6, 1, true);
  int sign;
  if (!track->detune) sign = 0;
  else if (track->detune < 0) sign = 1;
  else sign = 2;
  vramblit(vram, TDETAIL_DT_S_X, y+6+2, s_dt_sign[sign], DT_SIGN_W, DT_SIGN_H);
  fmdsp_putline("M:", vram, &font_fmdsp_small, TDETAIL_M_X, y+6, 1, true);
  fmdsp_putline(track->status, vram, &font_fmdsp_small, TDETAIL_M_V_X, y+6, 1, true);
  uint8_t color_on = ((track->key == 0xff) || fmdsp->masked[t]) ? 7 : 2;
  if (!track->playing) color_on = 3;
  vramblit_color(vram, BAR_L_X, y+BAR_Y,
                  s_bar_l, BAR_L_W, BAR_H, color_on);
  for (int i = 0; i < BAR_CNT; i++) {
    int c = (i < (track->ticks_left>>2)) ? color_on : 3;
    vramblit_color(vram, BAR_X+BAR_W*i, y+BAR_Y,
              s_bar, BAR_W, BAR_H, c);
  }
  vramblit_color(vram, BAR_X+BAR_W*(track->ticks>>2), y+BAR_Y,
                  s_bar, BAR_W, BAR_H, 7);
}

static void fmdsp_update_10(struct fmdsp *fmdsp,
                  struct fmdriver_work *work,
                  struct opna *opna,
                  uint8_t *vram,
                  struct fmplayer_fft_input_data *idata) {
  if (fmdsp->style != FMDSP_DISPSTYLE_ORIGINAL) {
    for (int y = 0; y < 320; y++) {
      for (int x = 320; x < PC98_W; x++) {
        vram[y*PC98_W+x] = 0;
      }
    }
  }
  for (int it = 0; it < FMDSP_TRACK_DISP_CNT_DEFAULT; it++) {
    int t;
    if (fmdsp->style == FMDSP_DISPSTYLE_DEFAULT || fmdsp->style == FMDSP_DISPSTYLE_ORIGINAL) t = track_disp_table_default[it];
    else if (fmdsp->style == FMDSP_DISPSTYLE_OPN) t = track_disp_table_opn[it];
    else t = track_disp_table_ppz8[it];
    if (t < 0) continue;
    const struct fmdriver_track_status *track = &work->track_status[t];

    if (fmdsp->style != FMDSP_DISPSTYLE_ORIGINAL) {
      if (((track->info == FMDRIVER_TRACK_INFO_PPZ8)
          || (track->info == FMDRIVER_TRACK_INFO_PDZF))
          && track->ppz8_ch) {
        fmdsp_track_info_ppz8(work->ppz8, track->ppz8_ch-1,
                              320, TRACK_H*it+6, vram);
      } else {
        switch (track_type_table[t].type) {
        case FMDRIVER_TRACKTYPE_FM:
          fmdsp_track_info_fm(opna,
                              track_type_table[t].num-1,
                              track->info == FMDRIVER_TRACK_INFO_FM3EX ? track->fmslotmask : 0,
                              320, TRACK_H*it+6, vram);
          break;
        case FMDRIVER_TRACKTYPE_SSG:
          fmdsp_track_info_ssg(opna,
                              track_type_table[t].num-1,
                              320, TRACK_H*it+6, vram);
          break;
        case FMDRIVER_TRACKTYPE_ADPCM:
          fmdsp_track_info_adpcm(opna, 320, TRACK_H*it+6, vram);
          break;
        case FMDRIVER_TRACKTYPE_PPZ8:
          fmdsp_track_info_ppz8(work->ppz8, track_type_table[t].num-1,
                                320, TRACK_H*it+6, vram);
          break;
        }
      }
    }
    fmdsp_track_without_key(fmdsp, work, track, t, TRACK_H*it, vram);
    for (int i = 0; i < KEY_OCTAVES; i++) {
      vramblit(vram, KEY_X+KEY_W*i, TRACK_H*it+KEY_Y,
                s_key_bg, KEY_W, KEY_H);
      if (track->playing || track->info == FMDRIVER_TRACK_INFO_SSGEFF) {
        if (track->actual_key >> 4 == i) {
          vramblit_key(vram, KEY_X+KEY_W*i, TRACK_H*it+KEY_Y,
                      s_key_mask, KEY_W, KEY_H,
                      track->actual_key & 0xf, 8);
        }
        if (track->key >> 4 == i) {
          vramblit_key(vram, KEY_X+KEY_W*i, TRACK_H*it+KEY_Y,
                      s_key_mask, KEY_W, KEY_H,
                      track->key & 0xf, fmdsp->masked[t] ? 8 : 6);
        }
      }
    }
  }
  if (fmdsp->style == FMDSP_DISPSTYLE_ORIGINAL) {
    // control status
    bool playing = work->playing && !work->paused;
    bool stopped = !work->playing;
    bool paused = work->paused;
    vramblit_color(vram, PLAY_X, PLAY_Y, s_play, PLAY_W, PLAY_H, playing ? 2 : 3);
    vramblit_color(vram, STOP_X, STOP_Y, s_stop, STOP_W, STOP_H, stopped ? 2 : 3);
    vramblit_color(vram, PAUSE_X, PAUSE_Y, s_pause, PAUSE_W, PAUSE_H, paused ? 2 : 3);
    vramblit(vram, FADE_X, FADE_Y, s_fade, FADE_W, FADE_H);
    vramblit(vram, FF_X, FF_Y, s_ff, FF_W, FF_H);
    vramblit(vram, REW_X, REW_Y, s_rew, REW_W, REW_H);
    vramblit(vram, FLOPPY_X, FLOPPY_Y, s_floppy, FLOPPY_W, FLOPPY_H);
    const uint8_t *num[8];
    // passed time
    {
      uint64_t frames = opna->generated_frames;
      int ssec = (int)(frames % 55467u) * 100 / 55467;
      uint64_t sec = frames / 55467u;
      uint64_t min = sec / 60u;
      sec %= 60u;
      num[0] = s_num[(min/10)%10];
      num[1] = s_num[min%10];
      vramblit(vram, TIME_X+NUM_W*0, TIME_Y, num[0], NUM_W, NUM_H);
      vramblit(vram, TIME_X+NUM_W*1, TIME_Y, num[1], NUM_W, NUM_H);
      vramblit(vram, TIME_X+NUM_W*2, TIME_Y, s_num_colon[sec%2u], NUM_W, NUM_H);
      num[0] = s_num[(sec/10)%10];
      num[1] = s_num[sec%10];
      vramblit(vram, TIME_X+NUM_W*3, TIME_Y, num[0], NUM_W, NUM_H);
      vramblit(vram, TIME_X+NUM_W*4, TIME_Y, num[1], NUM_W, NUM_H);
      vramblit(vram, TIME_X+NUM_W*5, TIME_Y, s_num_bar, NUM_W, NUM_H);
      num[0] = s_num[(ssec/10)%10];
      num[1] = s_num[ssec%10];
      vramblit(vram, TIME_X+NUM_W*6, TIME_Y, num[0], NUM_W, NUM_H);
      vramblit(vram, TIME_X+NUM_W*7, TIME_Y, num[1], NUM_W, NUM_H);
    }
    // clock count
    {
      uint64_t clock = work->timerb_cnt;
      for (int i = 0; i < 8; i++) {
        num[7-i] = s_num[clock%10u];
        clock /= 10u;
      }
      for (int i = 0; i < 8; i++) {
        vramblit(vram, TIME_X+NUM_W*i, CLOCK_Y, num[i], NUM_W, NUM_H);
      }
    }
    // timerb
    {
      uint8_t timerb = work->timerb;
      for (int i = 0; i < 3; i++) {
        num[2-i] = s_num[timerb%10];
        timerb /= 10;
      }
      for (int i = 0; i < 3; i++) {
        vramblit(vram, TIME_X+NUM_W*(5+i), TIMERB_Y, num[i], NUM_W, NUM_H);
      }
    }
    // loop count
    {
      uint8_t loop = work->loop_cnt;
      for (int i = 0; i < 4; i++) {
        num[3-i] = s_num[loop%10];
        loop /= 10;
      }
      for (int i = 0; i < 4; i++) {
        vramblit(vram, TIME_X+NUM_W*(4+i), LOOPCNT_Y, num[i], NUM_W, NUM_H);
      }
    }
    //
    int pos = 0;
    if (work->loop_timerb_cnt) pos = work->timerb_cnt_loop * (72+1-4) / work->loop_timerb_cnt;
    for (int x = 0; x < 72; x++) {
      if (x == 0 || x == 36 || x == 71) {
        vram[(70-2)*PC98_W+352+x*2] = 7;
      } else if (!(x % 9)) {
        vram[(70-2)*PC98_W+352+x*2] = 3;
      }
      uint8_t c = 3;
      if (work->playing && ((pos <= x) && (x < (pos+4)))) c = 2;
      for (int y = 0; y < 4; y++) {
        vram[(70+y)*PC98_W+352+x*2] = c;
      }
    }
    for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 4; y++) {
        vram[(70+y)*PC98_W+496+x] = work->loop_cnt ? 7 : 3;
      }
    }
    // cpu
    int cpuusage = fmdsp->cpuusage;
    for (int i = 0; i < 3; i++) {
      num[2-i] = s_num[cpuusage % 10];
      cpuusage /= 10;
    }
    for (int i = 0; i < 3; i++) {
      vramblit(vram, CPU_NUM_X+NUM_W*i, CPU_NUM_Y, num[i], NUM_W, NUM_H);
    }
    // fps
    int fps = fmdsp->fps;
    for (int i = 0; i < 3; i++) {
      num[2-i] = s_num[fps % 10];
      fps /= 10;
    }
    for (int i = 0; i < 3; i++) {
      vramblit(vram, FPS_NUM_X+NUM_W*i, CPU_NUM_Y, num[i], NUM_W, NUM_H);
    }
    // circle
    for (int y = 0; y < CIRCLE_H; y++) {
      for (int x = 0; x < CIRCLE_W; x++) {
        int c = 0;
        int clock = (work->timerb_cnt / 8) % 8;
        int p;
        if ((p = s_circle[y*CIRCLE_W+x])) {
          c = (work->playing && (!work->paused || (fmdsp->framecnt % 60) < 30) && (p == (clock + 1))) ? 2 : 3;
        }
        vram[(CIRCLE_Y+y)*PC98_W+CIRCLE_X+x] = c;
      }
    }
    // fft
    struct fmplayer_fft_disp_data ddata;
    fft_calc(&ddata, idata);
    for (int x = 0; x < FFTDISPLEN; x++) {
      for (int y = 0; y < 32; y++) {
        int px = SPECTRUM_X+x*4;
        int py = SPECTRUM_Y-y*2;
        int c = y < ddata.buf[x] ? 2 : 3;
        vram[py*PC98_W+px+0] = c;
        vram[py*PC98_W+px+1] = c;
        vram[py*PC98_W+px+2] = c;
      }
    }
    for (int i = 0; i < FFTDISPLEN; i++) {
      if (fmdsp->fftdata[i] <= ddata.buf[i]) {
        fmdsp->fftdata[i] = ddata.buf[i];
        fmdsp->fftcnt[i] = 30;
      } else {
        if (fmdsp->fftcnt[i]) {
          fmdsp->fftcnt[i]--;
        } else {
          if (fmdsp->fftdata[i]) {
            if (fmdsp->fftdropdiv[i]) {
              fmdsp->fftdropdiv[i]--;
            } else {
              static const uint8_t divtab[16] = {
                32, 16, 8, 8, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2,
              };
              fmdsp->fftdropdiv[i] = divtab[fmdsp->fftdata[i] / 2];
              fmdsp->fftdata[i]--;
            }
          }
        }
      }
    }
    for (int x = 0; x < FFTDISPLEN; x++) {
      int px = SPECTRUM_X+x*4;
      int py = SPECTRUM_Y-fmdsp->fftdata[x]*2;
      vram[py*PC98_W+px+0] = 7;
      vram[py*PC98_W+px+1] = 7;
      vram[py*PC98_W+px+2] = 7;
    }
    // level
    struct {
      unsigned level;
      int t;
      bool masked;
      uint8_t pan;
      uint8_t prog;
      uint8_t key;
      bool playing;
    } levels[FMDSP_LEVEL_COUNT] = {0};
    for (int c = 0; c < 6; c++) {
      levels[c].level = leveldata_read(&opna->fm.channel[c].leveldata);
      static const int table[4] = {5, 4, 0, 2};
      levels[c].pan = table[opna->fm.lselect[c]*2 + opna->fm.rselect[c]];
    }
    levels[0].t = FMDRIVER_TRACK_FM_1;
    levels[1].t = FMDRIVER_TRACK_FM_2;
    levels[2].t = FMDRIVER_TRACK_FM_3;
    levels[3].t = FMDRIVER_TRACK_FM_4;
    levels[4].t = FMDRIVER_TRACK_FM_5;
    levels[5].t = FMDRIVER_TRACK_FM_6;
    
    for (int c = 0; c < 3; c++) {
      levels[6+c].level = leveldata_read(&opna->resampler.leveldata[c]);
      levels[6+c].t = FMDRIVER_TRACK_SSG_1+c;
      levels[6+c].pan = 2;
    }
    {
      unsigned dl = 0;
      for (int d = 0; d < 6; d++) {
        unsigned l = leveldata_read(&opna->drum.drums[d].leveldata);
        if (l > dl) dl = l;
      }
      levels[9].level = dl;
      levels[9].pan = 2;
    }
    levels[10].level = leveldata_read(&opna->adpcm.leveldata);
    levels[10].t = FMDRIVER_TRACK_ADPCM;
    {
      static const int table[4] = {5, 4, 0, 2};
      int ind = 0;
      if (opna->adpcm.control2 & 0x80) ind |= 2;
      if (opna->adpcm.control2 & 0x40) ind |= 1;
      levels[10].pan = table[ind];
    }
    for (int p = 0; p < 8; p++) {
      levels[11+p].pan = 5;
      levels[11+p].t = FMDRIVER_TRACK_PPZ8_1+p;
    }
    if (work->ppz8) {
      for (int p = 0; p < 8; p++) {
        levels[11+p].level = leveldata_read(&work->ppz8->channel[p].leveldata);
        static const int table[10] = {5, 0, 1, 1, 1, 2, 3, 3, 3, 4};
        levels[11+p].pan = table[work->ppz8->channel[p].pan];
      }
    }
    for (int c = 0; c < FMDSP_LEVEL_COUNT; c++) {
      levels[c].masked = c == 9 ? fmdsp->masked_rhythm : fmdsp->masked[levels[c].t];
      levels[c].prog = work->track_status[levels[c].t].tonenum;
      levels[c].key = work->track_status[levels[c].t].key;
      levels[c].playing = work->track_status[levels[c].t].playing;
      if (work->track_status[levels[c].t].info == FMDRIVER_TRACK_INFO_PDZF ||
          work->track_status[levels[c].t].info == FMDRIVER_TRACK_INFO_PPZ8) {
        levels[c].playing = false;
      }
      if (!levels[c].playing) levels[c].pan = 5;
    }
    
    for (int c = 0; c < FMDSP_LEVEL_COUNT; c++) {
      unsigned level = levels[c].level;
      unsigned llevel = 0;
      if (level) {
        float db = 20.0f * log10f((float)level / (1<<15));
        float fllevel = (db / 48.0f + 1.0f) * 32.0f;
        if (fllevel > 0.0f) llevel = fllevel;
      }
      
      if (fmdsp->leveldata[c] <= llevel) {
        fmdsp->leveldata[c] = llevel;
        fmdsp->levelcnt[c] = 30;
      } else {
        if (fmdsp->levelcnt[c]) {
          fmdsp->levelcnt[c]--;
        } else {
          if (fmdsp->leveldata[c]) {
            if (fmdsp->leveldropdiv[c]) {
              fmdsp->leveldropdiv[c]--;
            } else {
              static const uint8_t divtab[16] = {
                32, 16, 8, 8, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2,
              };
              fmdsp->leveldropdiv[c] = divtab[fmdsp->leveldata[c] / 2];
              fmdsp->leveldata[c]--;
            }
          }
        }
      }
      for (unsigned y = 0; y < 64; y += 2) {
        unsigned plevel = (63 - y) / 2;
        for (int x = 0; x < LEVEL_DISP_W; x++) {
          uint8_t color = llevel > plevel ? 2 : 3;
          if (plevel == fmdsp->leveldata[c]) color = 7;
          vram[(y+LEVEL_Y)*PC98_W+LEVEL_X+LEVEL_W*c+x] = color;
        }
      }
      vramblit_color(vram,
                     LEVEL_X+LEVEL_W*c-1, PANPOT_Y,
                     s_panpot[levels[c].pan], PANPOT_W, PANPOT_H,
                     levels[c].masked ? 5 : 1);
      char buf[4];
      if (c != 9) {
        snprintf(buf, sizeof(buf), "%03d", levels[c].prog);
        fmdsp_putline(buf, vram, &font_fmdsp_small,
                      LEVEL_X+LEVEL_W*c, LEVEL_PROG_Y,
                      1, true);
      }
      strcpy(buf, "---");
      if (c != 9 && levels[c].playing) {
        uint8_t oct = levels[c].key >> 4;
        uint8_t n = levels[c].key & 0xf;
        if (n < 12) {
          snprintf(buf, sizeof(buf), "%03d", oct*12+n);
        }
      }
      fmdsp_putline(buf, vram, &font_fmdsp_small,
                    LEVEL_X+LEVEL_W*c, LEVEL_KEY_Y,
                    1, true);
    }
  }
}
static void fmdsp_update_13(struct fmdsp *fmdsp,
                  const struct fmdriver_work *work,
                  const struct opna *opna,
                  uint8_t *vram) {
  for (int y = 0; y < 320; y++) {
    for (int x = 320; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  for (int it = 0; it < FMDRIVER_TRACK_PPZ8_1; it++) {
    int t = it;
    const struct fmdriver_track_status *track = &work->track_status[t];

    if (((track->info == FMDRIVER_TRACK_INFO_PPZ8)
         || (track->info == FMDRIVER_TRACK_INFO_PDZF))
        && track->ppz8_ch) {
      fmdsp_track_info_ppz8(work->ppz8, track->ppz8_ch-1,
                            320, TRACK_H_S*it, vram);
    } else {
      switch (track_type_table[t].type) {
      case FMDRIVER_TRACKTYPE_FM:
        fmdsp_track_info_fm(opna,
                            track_type_table[t].num-1,
                            track->info == FMDRIVER_TRACK_INFO_FM3EX ? track->fmslotmask : 0,
                            320, TRACK_H_S*it, vram);
        break;
      case FMDRIVER_TRACKTYPE_SSG:
        fmdsp_track_info_ssg(opna,
                            track_type_table[t].num-1,
                            320, TRACK_H_S*it, vram);
        break;
      case FMDRIVER_TRACKTYPE_ADPCM:
        fmdsp_track_info_adpcm(opna, 320, TRACK_H_S*it, vram);
      }
    }
    fmdsp_track_without_key(fmdsp, work, track, t, TRACK_H_S*it, vram);
    for (int i = 0; i < KEY_OCTAVES; i++) {
      vramblit(vram, KEY_X+KEY_W*i, TRACK_H_S*it+KEY_Y,
                s_key_bg + KEY_S_OFFSET, KEY_W, KEY_H_S);
      if (track->playing || track->info == FMDRIVER_TRACK_INFO_SSGEFF) {
        if (track->actual_key >> 4 == i) {
          vramblit_key(vram, KEY_X+KEY_W*i, TRACK_H_S*it+KEY_Y,
                      s_key_mask + KEY_S_OFFSET, KEY_W, KEY_H_S,
                      track->actual_key & 0xf, 8);
        }
        if (track->key >> 4 == i) {
          vramblit_key(vram, KEY_X+KEY_W*i, TRACK_H_S*it+KEY_Y,
                      s_key_mask + KEY_S_OFFSET, KEY_W, KEY_H_S,
                      track->key & 0xf, fmdsp->masked[t] ? 8 : 6);
        }
      }
    }
  }
}

void fmdsp_update(struct fmdsp *fmdsp,
                  struct fmdriver_work *work,
                  struct opna *opna,
                  uint8_t *vram,
                  struct fmplayer_fft_input_data *idata) {
  if (fmdsp->style_updated) {
    if (fmdsp->style == FMDSP_DISPSTYLE_13) {
      fmdsp_track_init_13(fmdsp, vram);
    } else {
      fmdsp_track_init_10(fmdsp, vram);
    }
  }
  unsigned mask = opna_get_mask(opna);
  fmdsp->masked[FMDRIVER_TRACK_FM_1] = mask & LIBOPNA_CHAN_FM_1;
  fmdsp->masked[FMDRIVER_TRACK_FM_2] = mask & LIBOPNA_CHAN_FM_2;
  fmdsp->masked[FMDRIVER_TRACK_FM_3] = mask & LIBOPNA_CHAN_FM_3;
  fmdsp->masked[FMDRIVER_TRACK_FM_3_EX_1] = mask & LIBOPNA_CHAN_FM_3;
  fmdsp->masked[FMDRIVER_TRACK_FM_3_EX_2] = mask & LIBOPNA_CHAN_FM_3;
  fmdsp->masked[FMDRIVER_TRACK_FM_3_EX_3] = mask & LIBOPNA_CHAN_FM_3;
  fmdsp->masked[FMDRIVER_TRACK_FM_4] = mask & LIBOPNA_CHAN_FM_4;
  fmdsp->masked[FMDRIVER_TRACK_FM_5] = mask & LIBOPNA_CHAN_FM_5;
  fmdsp->masked[FMDRIVER_TRACK_FM_6] = mask & LIBOPNA_CHAN_FM_6;
  fmdsp->masked[FMDRIVER_TRACK_SSG_1] = mask & LIBOPNA_CHAN_SSG_1;
  fmdsp->masked[FMDRIVER_TRACK_SSG_2] = mask & LIBOPNA_CHAN_SSG_2;
  fmdsp->masked[FMDRIVER_TRACK_SSG_3] = mask & LIBOPNA_CHAN_SSG_3;
  fmdsp->masked[FMDRIVER_TRACK_ADPCM] = mask & LIBOPNA_CHAN_ADPCM;
  fmdsp->masked_rhythm = (mask & LIBOPNA_CHAN_DRUM_ALL) == LIBOPNA_CHAN_DRUM_ALL;
  unsigned ppz8mask = 0;
  if (work->ppz8) {
    ppz8mask = ppz8_get_mask(work->ppz8);
  }
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_1] = ppz8mask & (1u<<0);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_2] = ppz8mask & (1u<<1);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_3] = ppz8mask & (1u<<2);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_4] = ppz8mask & (1u<<3);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_5] = ppz8mask & (1u<<4);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_6] = ppz8mask & (1u<<5);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_7] = ppz8mask & (1u<<6);
  fmdsp->masked[FMDRIVER_TRACK_PPZ8_8] = ppz8mask & (1u<<7);
  fmdsp->style_updated = false;
  if (fmdsp->style == FMDSP_DISPSTYLE_13) {
    fmdsp_update_13(fmdsp, work, opna, vram);
  } else {
    fmdsp_update_10(fmdsp, work, opna, vram, idata);
  }
  fmdsp_palette_fade(fmdsp);
  if (!(fmdsp->framecnt % 30)) {
    fmdsp->cpuusage = fmdsp_cpu_usage();
    fmdsp->fps = fmdsp_fps_30();
  }
  fmdsp->framecnt++;
}

void fmdsp_vrampalette(struct fmdsp *fmdsp, const uint8_t *vram, uint8_t *vram32, int stride) {
  fmdsp_vramlookup_func(vram32, vram, fmdsp->palette, stride);
}

void fmdsp_dispstyle_set(struct fmdsp *fmdsp, enum FMDSP_DISPSTYLE style) {
  if (style < 0) return;
  if (style >= FMDSP_DISPSTYLE_CNT) return;
  fmdsp->style = style;
  fmdsp->style_updated = true;
}
