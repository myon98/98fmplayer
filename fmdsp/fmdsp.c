#include "fmdsp.h"
#include "fmdsp_sprites.h"
#include "font.h"
#include "fmdriver/fmdriver.h"
#include <stdio.h>
#include "libopna/opna.h"

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
  fmdsp->style = FMDSP_DISPSTYLE_DEFAULT;
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
    if (fmdsp->style == FMDSP_DISPSTYLE_DEFAULT) t = track_disp_table_default[i];
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
  fmdsp_putline("VL:", vram, &font_fmdsp_small, TDETAIL_VL_X, y+6, 1, true);
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
                  const struct fmdriver_work *work,
                  const struct opna *opna,
                  uint8_t *vram) {
  for (int y = 0; y < 320; y++) {
    for (int x = 320; x < PC98_W; x++) {
      vram[y*PC98_W+x] = 0;
    }
  }
  for (int it = 0; it < FMDSP_TRACK_DISP_CNT_DEFAULT; it++) {
    int t;
    if (fmdsp->style == FMDSP_DISPSTYLE_DEFAULT) t = track_disp_table_default[it];
    else if (fmdsp->style == FMDSP_DISPSTYLE_OPN) t = track_disp_table_opn[it];
    else t = track_disp_table_ppz8[it];
    if (t < 0) continue;
    const struct fmdriver_track_status *track = &work->track_status[t];

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
                  const struct fmdriver_work *work,
                  const struct opna *opna,
                  uint8_t *vram) {
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
    fmdsp_update_10(fmdsp, work, opna, vram);
  }
  fmdsp_palette_fade(fmdsp);
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
