#include "fmdsp-pacc.h"
#include "pacc/pacc.h"
#include "font.h"
#include "fmdriver/fmdriver.h"
#include "libopna/opna.h"

#include "fmdsp_sprites.h"
#include <stdlib.h>
#include <string.h>

enum {
  FADEDELTA = 16,
};

enum {
  KEY_S_OFF_Y = 4,
  LOGO_W = LOGO_FM_W + 2 + LOGO_DS_W + 2 + LOGO_P_W,
};

static const struct {
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

static const uint8_t track_disp_table_opna[] = {
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
  FMDRIVER_TRACK_NUM
};
static const uint8_t track_disp_table_opn[] = {
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
  FMDRIVER_TRACK_NUM,
};
static const uint8_t track_disp_table_ppz8[] = {
  FMDRIVER_TRACK_PPZ8_1,
  FMDRIVER_TRACK_PPZ8_2,
  FMDRIVER_TRACK_PPZ8_3,
  FMDRIVER_TRACK_PPZ8_4,
  FMDRIVER_TRACK_PPZ8_5,
  FMDRIVER_TRACK_PPZ8_6,
  FMDRIVER_TRACK_PPZ8_7,
  FMDRIVER_TRACK_PPZ8_8,
  FMDRIVER_TRACK_ADPCM,
  FMDRIVER_TRACK_NUM,
};
static const uint8_t track_disp_table_13[] = {
  FMDRIVER_TRACK_FM_1,
  FMDRIVER_TRACK_FM_2,
  FMDRIVER_TRACK_FM_3,
  FMDRIVER_TRACK_FM_3_EX_1,
  FMDRIVER_TRACK_FM_3_EX_2,
  FMDRIVER_TRACK_FM_3_EX_3,
  FMDRIVER_TRACK_FM_4,
  FMDRIVER_TRACK_FM_5,
  FMDRIVER_TRACK_FM_6,
  FMDRIVER_TRACK_SSG_1,
  FMDRIVER_TRACK_SSG_2,
  FMDRIVER_TRACK_SSG_3,
  FMDRIVER_TRACK_ADPCM,
  FMDRIVER_TRACK_NUM,
};


struct fmdsp_pacc {
  struct pacc_ctx *pc;
  struct pacc_vtable pacc;
  struct pacc_tex *tex_font, *tex_checker, *tex_key_left, *tex_key_right, *tex_key_mask, *tex_key_bg, *tex_num, *tex_dt_sign, *tex_solid, *tex_vertical, *tex_horizontal, *tex_logo;
  struct pacc_buf *buf_font_2, *buf_font_2_d, *buf_font_1, *buf_font_1_d, *buf_checker, *buf_key_left, *buf_key_right, *buf_key_mask, *buf_key_mask_sub, *buf_key_bg, *buf_num, *buf_dt_sign, *buf_solid_2, *buf_solid_3, *buf_solid_7, *buf_vertical_2, *buf_vertical_3, *buf_vertical_7, *buf_logo;
  struct opna *opna;
  struct fmdriver_work *work;
  uint8_t curr_palette[FMDSP_PALETTE_COLORS*3];
  uint8_t target_palette[FMDSP_PALETTE_COLORS*3];
  enum fmdsp_left_mode lmode;
  enum fmdsp_right_mode rmode;
  bool mode_changed;
  bool masked[FMDRIVER_TRACK_NUM];
  bool masked_rhythm;
};

static struct pacc_tex *tex_from_font(
    struct pacc_ctx *pc, const struct pacc_vtable *pacc,
    const struct fmdsp_font *font) {
  struct pacc_tex *tex = pacc->gen_tex(pc, font->width_half*256, font->height);
  if (!tex) return 0;
  uint8_t *buf = pacc->tex_lock(tex);
  for (int c = 0; c < 256; c++) {
    const uint8_t *data = font->get(font, c, FMDSP_FONT_ANK);
    for (int y = 0; y < font->height; y++) {
      for (int x = 0; x < font->width_half; x++) {
        buf[font->width_half*(256*y+c)+x] = (data[y*(font->width_half/8+1)+(x/8)] & (1<<(7-x))) ? 0xff : 0x00;
      }
    }
  }
  pacc->tex_unlock(tex);
  return tex;
}

void fmdsp_pacc_release(struct fmdsp_pacc *fp) {
  if (fp) {
    if (fp->pc) {
      fp->pacc.buf_delete(fp->buf_font_1);
      fp->pacc.buf_delete(fp->buf_font_1_d);
      fp->pacc.buf_delete(fp->buf_font_2);
      fp->pacc.buf_delete(fp->buf_font_2_d);
      fp->pacc.buf_delete(fp->buf_checker);
      fp->pacc.buf_delete(fp->buf_key_left);
      fp->pacc.buf_delete(fp->buf_key_right);
      fp->pacc.buf_delete(fp->buf_key_mask);
      fp->pacc.buf_delete(fp->buf_key_mask_sub);
      fp->pacc.buf_delete(fp->buf_key_bg);
      fp->pacc.buf_delete(fp->buf_num);
      fp->pacc.buf_delete(fp->buf_dt_sign);
      fp->pacc.buf_delete(fp->buf_solid_2);
      fp->pacc.buf_delete(fp->buf_solid_3);
      fp->pacc.buf_delete(fp->buf_solid_7);
      fp->pacc.buf_delete(fp->buf_vertical_2);
      fp->pacc.buf_delete(fp->buf_vertical_3);
      fp->pacc.buf_delete(fp->buf_vertical_7);
      fp->pacc.buf_delete(fp->buf_logo);
      fp->pacc.tex_delete(fp->tex_font);
      fp->pacc.tex_delete(fp->tex_checker);
      fp->pacc.tex_delete(fp->tex_key_left);
      fp->pacc.tex_delete(fp->tex_key_right);
      fp->pacc.tex_delete(fp->tex_key_mask);
      fp->pacc.tex_delete(fp->tex_key_bg);
      fp->pacc.tex_delete(fp->tex_num);
      fp->pacc.tex_delete(fp->tex_dt_sign);
      fp->pacc.tex_delete(fp->tex_solid);
      fp->pacc.tex_delete(fp->tex_vertical);
      fp->pacc.tex_delete(fp->tex_horizontal);
      fp->pacc.tex_delete(fp->tex_logo);
    }
    free(fp);
  }
}

static void init_track_without_key(
    struct fmdsp_pacc *fp,
    int t,
    int x, int y) {
  const char *track_type = "";
  switch (track_type_table[t].type) {
  case FMDRIVER_TRACKTYPE_FM:
    track_type = "FM";
    break;
  case FMDRIVER_TRACKTYPE_SSG:
    track_type = "SSG";
    break;
  case FMDRIVER_TRACKTYPE_ADPCM:
    track_type = "ADPCM";
    break;
  case FMDRIVER_TRACKTYPE_PPZ8:
    track_type = "PPZ8";
    break;
  }
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_2, x+1, y,
      "%s", track_type);
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1, x+1, y+6,
      "TRACK.");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_X, y+6, "KN:");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_TN_X, y+6, "TN:");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_VL_X, y+6, "Vl");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_VL_C_X, y+6, ":");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_GT_X, y+6, "GT:");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_DT_X, y+6, "DT:");
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1,
      x+TDETAIL_M_X, y+6, "M:");
}

static void init_track_13(struct fmdsp_pacc *fp, int x) {
  const uint8_t *track_table = track_disp_table_13;
  for (int i = 0; i < 13; i++) {
    int t = track_table[i];
    init_track_without_key(fp, t, x, TRACK_H_S*i);
    fp->pacc.buf_rect_off(fp->pc, fp->buf_key_left, x+KEY_LEFT_X, TRACK_H_S*i+KEY_Y, KEY_LEFT_W, KEY_H_S, 0, KEY_S_OFF_Y);
    for (int j = 0; j < KEY_OCTAVES; j++) {
      fp->pacc.buf_rect_off(fp->pc, fp->buf_key_bg, x+KEY_X+KEY_W*j, TRACK_H_S*i+KEY_Y, KEY_W, KEY_H_S, 0, KEY_S_OFF_Y);
    }
    fp->pacc.buf_rect_off(fp->pc, fp->buf_key_right, x+KEY_X+KEY_W*KEY_OCTAVES, TRACK_H_S*i+KEY_Y, KEY_RIGHT_W, KEY_H_S, 0, KEY_S_OFF_Y);
  }
}

static void init_track_10(struct fmdsp_pacc *fp, const uint8_t *track_table, int x) {
  for (int i = 0; i < 10; i++) {
    int t = track_table[i];
    if (t == FMDRIVER_TRACK_NUM) break;
    init_track_without_key(fp, t, x, TRACK_H*i);
    fp->pacc.buf_rect(fp->pc, fp->buf_key_left, x+KEY_LEFT_X, TRACK_H*i+KEY_Y, KEY_LEFT_W, KEY_H);
    for (int j = 0; j < KEY_OCTAVES; j++) {
      fp->pacc.buf_rect(fp->pc, fp->buf_key_bg, x+KEY_X+KEY_W*j, TRACK_H*i+KEY_Y, KEY_W, KEY_H);
    }
    fp->pacc.buf_rect(fp->pc, fp->buf_key_right, x+KEY_X+KEY_W*KEY_OCTAVES, TRACK_H*i+KEY_Y, KEY_RIGHT_W, KEY_H);
  }
}

static void update_track_without_key(
    struct fmdsp_pacc *fp,
    int t,
    int x, int y) {
  const struct fmdriver_track_status *track = &fp->work->track_status[t];
  int tracknum = track_type_table[t].num;
  int num1 = (tracknum/10) % 10;
  int num2 = tracknum % 10;
  fp->pacc.buf_rect_off(
      fp->pc, fp->buf_num,
      x+NUM_X+NUM_W*0, y+1,
      NUM_W, NUM_H, 0, NUM_H*num1);
  fp->pacc.buf_rect_off(
      fp->pc, fp->buf_num,
      x+NUM_X+NUM_W*1, y+1,
      NUM_W, NUM_H, 0, NUM_H*num2);
  if (track->playing || track->info == FMDRIVER_TRACK_INFO_SSGEFF) {
    switch (track->info) {
    case FMDRIVER_TRACK_INFO_PPZ8:
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_2_d, x+TINFO_X, y+6, "PPZ8");
      break;
    case FMDRIVER_TRACK_INFO_PDZF:
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_2_d, x+TINFO_X, y+6, "PDZF");
      break;
    case FMDRIVER_TRACK_INFO_SSGEFF:
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_2_d, x+TINFO_X+3, y, "EFF");
      /* FALLTHRU */
    case FMDRIVER_TRACK_INFO_SSG:
      if (track->ssg_noise) {
        fp->pacc.buf_printf(
            fp->pc, fp->buf_font_2_d, x+TINFO_X+3, y+6,
            "%c%02X", track->ssg_tone ? 'M' : 'N', fp->work->ssg_noise_freq);
      }
      break;
    case FMDRIVER_TRACK_INFO_FM3EX:
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_2_d, x+TINFO_X+3, y, "EX");
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_2_d, x+TINFO_X, y+6,
          "%c%c%c%c",
          track->fmslotmask[0] ? ' ' : '1',
          track->fmslotmask[1] ? ' ' : '2',
          track->fmslotmask[2] ? ' ' : '3',
          track->fmslotmask[3] ? ' ' : '4');
      break;
    }
  }
  if (!track->playing) {
    fp->pacc.buf_printf(
        fp->pc, fp->buf_font_1_d,
        x+TDETAIL_KN_V_X+5, y+6, "S");
  } else {
    if ((track->key & 0xf) == 0xf) {
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_1_d,
          x+TDETAIL_KN_V_X+5, y+6, "R");
    } else {
      const char *keystr = "";
      static const char *keytable[0x10] = {
        "C", "C+", "D", "D+", "E", "F", "F+", "G", "G+", "A", "A+", "B",
      };
      if (keytable[track->key&0xf]) keystr = keytable[track->key&0xf];
      fp->pacc.buf_printf(
          fp->pc, fp->buf_font_1_d,
          x+TDETAIL_KN_V_X, y+6, "o%d%s", track->key>>4, keystr);
    }
  }
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1_d,
      x+TDETAIL_TN_V_X, y+6, "%03d", track->tonenum);
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1_d,
      x+TDETAIL_VL_V_X, y+6, "%03d", track->volume);
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1_d,
      x+TDETAIL_GT_V_X, y+6, "%03d", track->gate);
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1_d,
      x+TDETAIL_DT_V_X, y+6, "%03d", (track->detune > 0) ? track->detune : -track->detune);
  fp->pacc.buf_printf(
      fp->pc, fp->buf_font_1_d,
      x+TDETAIL_M_V_X, y+6, "%s", track->status);
  int sign;
  if (!track->detune) sign = 0;
  else if (track->detune < 0) sign = 1;
  else sign = 2;
  fp->pacc.buf_rect_off(
      fp->pc, fp->buf_dt_sign,
      x+TDETAIL_DT_S_X, y+6+2, DT_SIGN_W, DT_SIGN_H, 0, DT_SIGN_H*sign);
  struct pacc_buf *buf_rect = ((track->key & 0xf) == 0xf) ? fp->buf_solid_7 : fp->buf_solid_2;
  if (!track->playing) buf_rect = fp->buf_solid_3;
  struct pacc_buf *buf_vertical = ((track->key & 0xf) == 0xf) ? fp->buf_vertical_7 : fp->buf_vertical_2;
  if (!track->playing) buf_vertical = fp->buf_vertical_3;
  fp->pacc.buf_rect(fp->pc, buf_rect, x+BAR_L_X, y+BAR_Y, BAR_L_W-1, BAR_H);
  int width = track->ticks_left>>2;
  fp->pacc.buf_rect(fp->pc, buf_vertical,
      x+BAR_X, y+BAR_Y, BAR_W*width, BAR_H);
  fp->pacc.buf_rect(fp->pc, fp->buf_vertical_3,
      x+BAR_X+BAR_W*width, y+BAR_Y, BAR_W*(64-width), BAR_H);
  fp->pacc.buf_rect(fp->pc, fp->buf_vertical_7,
      x+BAR_X+BAR_W*(track->ticks>>2), y+BAR_Y, BAR_W, BAR_H);
}

static void update_track_13(struct fmdsp_pacc *fp, int x) {
  const uint8_t *track_table = track_disp_table_13;
  for (int it = 0; it < 13; it++) {
    int t = track_table[it];
    const struct fmdriver_track_status *track = &fp->work->track_status[t];
    update_track_without_key(fp, t, x, TRACK_H_S*it);
    for (int i = 0; i < KEY_OCTAVES; i++) {
      if (track->playing || track->info == FMDRIVER_TRACK_INFO_SSGEFF) {
        if ((track->actual_key >> 4) == i) {
          fp->pacc.buf_rect_off(
              fp->pc, fp->buf_key_mask_sub,
              x+KEY_X+KEY_W*i, TRACK_H_S*it+KEY_Y,
              KEY_W, KEY_H_S,
              0, KEY_H*(track->actual_key&0xf) + KEY_S_OFF_Y);
        }
        if ((track->key >> 4) == i) {
          fp->pacc.buf_rect_off(
              fp->pc, fp->buf_key_mask,
              x+KEY_X+KEY_W*i, TRACK_H_S*it+KEY_Y,
              KEY_W, KEY_H_S,
              0, KEY_H*(track->key&0xf) + KEY_S_OFF_Y);
        }
      }
    }
  }
}

static void update_track_10(struct fmdsp_pacc *fp, const uint8_t *track_table, int x) {
  for (int it = 0; it < 10; it++) {
    int t = track_table[it];
    if (t == FMDRIVER_TRACK_NUM) break;
    const struct fmdriver_track_status *track = &fp->work->track_status[t];
    update_track_without_key(fp, t, x, TRACK_H*it);
    for (int i = 0; i < KEY_OCTAVES; i++) {
      if (track->playing || track->info == FMDRIVER_TRACK_INFO_SSGEFF) {
        if ((track->actual_key >> 4) == i) {
          fp->pacc.buf_rect_off(
              fp->pc, fp->buf_key_mask_sub,
              x+KEY_X+KEY_W*i, TRACK_H*it+KEY_Y,
              KEY_W, KEY_H,
              0, KEY_H*(track->actual_key&0xf));
        }
        if ((track->key >> 4) == i) {
          fp->pacc.buf_rect_off(
              fp->pc, fp->buf_key_mask,
              x+KEY_X+KEY_W*i, TRACK_H*it+KEY_Y,
              KEY_W, KEY_H,
              0, KEY_H*(track->key&0xf));
        }
      }
    }
  }
}

struct fmdsp_pacc *fmdsp_pacc_init(
    struct pacc_ctx *pc, const struct pacc_vtable *vtable) {
  struct fmdsp_pacc *fp = malloc(sizeof(*fp));
  if (!fp) goto err;
  *fp = (struct fmdsp_pacc) {
    .pc = pc,
    .pacc = *vtable,
  };
  fp->tex_font = tex_from_font(fp->pc, &fp->pacc, &font_fmdsp_small);
  if (!fp->tex_font) goto err;
  fp->tex_checker = fp->pacc.gen_tex(fp->pc, 2, 2);
  if (!fp->tex_checker) goto err;
  fp->tex_key_left = fp->pacc.gen_tex(fp->pc, KEY_LEFT_W, KEY_H);
  if (!fp->tex_key_left) goto err;
  fp->tex_key_right = fp->pacc.gen_tex(fp->pc, KEY_RIGHT_W, KEY_H);
  if (!fp->tex_key_right) goto err;
  fp->tex_key_mask = fp->pacc.gen_tex(fp->pc, KEY_W, KEY_H*12);
  if (!fp->tex_key_mask) goto err;
  fp->tex_key_bg = fp->pacc.gen_tex(fp->pc, KEY_W, KEY_H);
  if (!fp->tex_key_bg) goto err;
  fp->tex_num = fp->pacc.gen_tex(fp->pc, NUM_W, NUM_H*11);
  if (!fp->tex_num) goto err;
  fp->tex_dt_sign = fp->pacc.gen_tex(fp->pc, DT_SIGN_W, DT_SIGN_H*3);
  if (!fp->tex_dt_sign) goto err;
  fp->tex_solid = fp->pacc.gen_tex(fp->pc, 1, 1);
  if (!fp->tex_solid) goto err;
  fp->tex_vertical = fp->pacc.gen_tex(fp->pc, 2, 1);
  if (!fp->tex_vertical) goto err;
  fp->tex_horizontal = fp->pacc.gen_tex(fp->pc, 1, 2);
  if (!fp->tex_horizontal) goto err;
  fp->tex_logo = fp->pacc.gen_tex(fp->pc, LOGO_W, LOGO_H);
  if (!fp->tex_logo) goto err;

  uint8_t *buf;
  buf = fp->pacc.tex_lock(fp->tex_checker);
  buf[0] = 3;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 3;
  fp->pacc.tex_unlock(fp->tex_checker);
  buf = fp->pacc.tex_lock(fp->tex_key_left);
  memcpy(buf, s_key_left, KEY_LEFT_W*KEY_H);
  fp->pacc.tex_unlock(fp->tex_key_left);
  buf = fp->pacc.tex_lock(fp->tex_key_right);
  memcpy(buf, s_key_right, KEY_RIGHT_W*KEY_H);
  fp->pacc.tex_unlock(fp->tex_key_right);
  buf = fp->pacc.tex_lock(fp->tex_key_bg);
  memcpy(buf, s_key_bg, KEY_W*KEY_H);
  fp->pacc.tex_unlock(fp->tex_key_bg);
  buf = fp->pacc.tex_lock(fp->tex_num);
  memcpy(buf, s_num, NUM_W*NUM_H*11);
  fp->pacc.tex_unlock(fp->tex_num);
  buf = fp->pacc.tex_lock(fp->tex_dt_sign);
  memcpy(buf, s_dt_sign, DT_SIGN_W*DT_SIGN_H*3);
  fp->pacc.tex_unlock(fp->tex_dt_sign);
  buf = fp->pacc.tex_lock(fp->tex_solid);
  buf[0] = 1;
  fp->pacc.tex_unlock(fp->tex_solid);
  buf = fp->pacc.tex_lock(fp->tex_vertical);
  buf[0] = 1;
  buf[1] = 0;
  fp->pacc.tex_unlock(fp->tex_vertical);
  buf = fp->pacc.tex_lock(fp->tex_horizontal);
  buf[0] = 1;
  buf[1] = 0;
  fp->pacc.tex_unlock(fp->tex_horizontal);
  buf = fp->pacc.tex_lock(fp->tex_key_mask);
  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < KEY_W*KEY_H; j++) {
      buf[KEY_W*KEY_H*i+j] = (s_key_mask[j] == (i+1));
    }
  }
  fp->pacc.tex_unlock(fp->tex_key_mask);
  buf = fp->pacc.tex_lock(fp->tex_logo);
  for (int y = 0; y < LOGO_H; y++) {
    memcpy(
        buf + y*LOGO_W,
        s_logo_fm + y*LOGO_FM_W,
        LOGO_FM_W);
    memcpy(
        buf + y*LOGO_W + LOGO_FM_W + 2,
        s_logo_ds + y*LOGO_DS_W,
        LOGO_DS_W);
    memcpy(
        buf + y*LOGO_W + LOGO_FM_W + 2 + LOGO_DS_W + 2,
        s_logo_p + y*LOGO_P_W,
        LOGO_P_W);
  }
  fp->pacc.tex_unlock(fp->tex_logo);

  fp->buf_font_1 = fp->pacc.gen_buf(fp->pc, fp->tex_font, pacc_buf_mode_static);
  if (!fp->buf_font_1) goto err;
  fp->buf_font_1_d = fp->pacc.gen_buf(fp->pc, fp->tex_font, pacc_buf_mode_stream);
  if (!fp->buf_font_1_d) goto err;
  fp->buf_font_2 = fp->pacc.gen_buf(fp->pc, fp->tex_font, pacc_buf_mode_static);
  if (!fp->buf_font_2) goto err;
  fp->buf_font_2_d = fp->pacc.gen_buf(fp->pc, fp->tex_font, pacc_buf_mode_stream);
  if (!fp->buf_font_2_d) goto err;
  fp->buf_checker = fp->pacc.gen_buf(fp->pc, fp->tex_checker, pacc_buf_mode_static);
  if (!fp->buf_checker) goto err;
  fp->buf_key_left = fp->pacc.gen_buf(fp->pc, fp->tex_key_left, pacc_buf_mode_static);
  if (!fp->buf_key_left) goto err;
  fp->buf_key_right = fp->pacc.gen_buf(fp->pc, fp->tex_key_right, pacc_buf_mode_static);
  if (!fp->buf_key_right) goto err;
  fp->buf_key_mask = fp->pacc.gen_buf(fp->pc, fp->tex_key_mask, pacc_buf_mode_stream);
  if (!fp->buf_key_left) goto err;
  fp->buf_key_mask_sub = fp->pacc.gen_buf(fp->pc, fp->tex_key_mask, pacc_buf_mode_stream);
  if (!fp->buf_key_mask_sub) goto err;
  fp->buf_key_bg= fp->pacc.gen_buf(fp->pc, fp->tex_key_bg, pacc_buf_mode_static);
  if (!fp->buf_key_bg) goto err;
  fp->buf_num = fp->pacc.gen_buf(fp->pc, fp->tex_num, pacc_buf_mode_stream);
  if (!fp->buf_num) goto err;
  fp->buf_dt_sign = fp->pacc.gen_buf(fp->pc, fp->tex_dt_sign, pacc_buf_mode_stream);
  if (!fp->buf_dt_sign) goto err;
  fp->buf_solid_2 = fp->pacc.gen_buf(fp->pc, fp->tex_solid, pacc_buf_mode_stream);
  if (!fp->buf_solid_2) goto err;
  fp->buf_solid_3 = fp->pacc.gen_buf(fp->pc, fp->tex_solid, pacc_buf_mode_stream);
  if (!fp->buf_solid_3) goto err;
  fp->buf_solid_7 = fp->pacc.gen_buf(fp->pc, fp->tex_solid, pacc_buf_mode_stream);
  if (!fp->buf_solid_7) goto err;
  fp->buf_vertical_2 = fp->pacc.gen_buf(fp->pc, fp->tex_vertical, pacc_buf_mode_stream);
  if (!fp->buf_vertical_2) goto err;
  fp->buf_vertical_3 = fp->pacc.gen_buf(fp->pc, fp->tex_vertical, pacc_buf_mode_stream);
  if (!fp->buf_vertical_3) goto err;
  fp->buf_vertical_7 = fp->pacc.gen_buf(fp->pc, fp->tex_vertical, pacc_buf_mode_stream);
  if (!fp->buf_vertical_7) goto err;
  fp->buf_logo = fp->pacc.gen_buf(fp->pc, fp->tex_logo, pacc_buf_mode_static);
  if (!fp->buf_logo) goto err;

  fp->pacc.buf_rect_off(fp->pc, fp->buf_checker, 1, CHECKER_Y, PC98_W-1, CHECKER_H, 1, 0);
  fp->pacc.buf_rect(fp->pc, fp->buf_checker, 0, CHECKER_Y+2, 1, CHECKER_H-4);
  memcpy(fp->target_palette, s_palettes[0], sizeof(fp->target_palette));
  fp->mode_changed = true;
  return fp;
err:
  fmdsp_pacc_release(fp);
  return 0;
}

static void mode_update(struct fmdsp_pacc *fp) {
  fp->pacc.buf_clear(fp->buf_font_1);
  fp->pacc.buf_clear(fp->buf_font_2);
  fp->pacc.buf_clear(fp->buf_key_left);
  fp->pacc.buf_clear(fp->buf_key_right);
  fp->pacc.buf_clear(fp->buf_key_bg);
  fp->pacc.buf_clear(fp->buf_logo);
  switch (fp->lmode) {
  case FMDSP_LEFT_MODE_OPNA:
    init_track_10(fp, track_disp_table_opna, 0);
    break;
  case FMDSP_LEFT_MODE_OPN:
    init_track_10(fp, track_disp_table_opn, 0);
    break;
  case FMDSP_LEFT_MODE_13:
    init_track_13(fp, 0);
    break;
  case FMDSP_LEFT_MODE_PPZ8:
    init_track_10(fp, track_disp_table_ppz8, 0);
    break;
  default:
    break;
  }
  switch (fp->rmode) {
  case FMDSP_RIGHT_MODE_DEFAULT:
    fp->pacc.buf_rect(
        fp->pc, fp->buf_logo,
        LOGO_FM_X, LOGO_Y, LOGO_W, LOGO_H);
    break;
  case FMDSP_RIGHT_MODE_TRACK_INFO:
    break;
  case FMDSP_RIGHT_MODE_PPZ8:
    init_track_10(fp, track_disp_table_ppz8, 320);
    break;
  default:
    break;
  }
}

void fmdsp_pacc_render(struct fmdsp_pacc *fp) {
  if (memcmp(fp->curr_palette, fp->target_palette, sizeof(fp->target_palette))) {
    for (int i = 0; i < FMDSP_PALETTE_COLORS*3; i++) {
      uint8_t p = fp->curr_palette[i];
      uint8_t t = fp->target_palette[i];
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
      fp->curr_palette[i] = p;
    }
    fp->pacc.palette(fp->pc, fp->curr_palette, FMDSP_PALETTE_COLORS);
  }
  if (fp->mode_changed) {
    mode_update(fp);
    fp->mode_changed = false;
  }
  fp->pacc.buf_clear(fp->buf_key_mask);
  fp->pacc.buf_clear(fp->buf_key_mask_sub);
  fp->pacc.buf_clear(fp->buf_font_1_d);
  fp->pacc.buf_clear(fp->buf_font_2_d);
  fp->pacc.buf_clear(fp->buf_num);
  fp->pacc.buf_clear(fp->buf_dt_sign);
  fp->pacc.buf_clear(fp->buf_solid_2);
  fp->pacc.buf_clear(fp->buf_solid_3);
  fp->pacc.buf_clear(fp->buf_solid_7);
  fp->pacc.buf_clear(fp->buf_vertical_2);
  fp->pacc.buf_clear(fp->buf_vertical_3);
  fp->pacc.buf_clear(fp->buf_vertical_7);
  unsigned mask = 0;
  if (fp->opna) {
    mask = opna_get_mask(fp->opna);
  }
  fp->masked[FMDRIVER_TRACK_FM_1] = mask & LIBOPNA_CHAN_FM_1;
  fp->masked[FMDRIVER_TRACK_FM_2] = mask & LIBOPNA_CHAN_FM_2;
  fp->masked[FMDRIVER_TRACK_FM_3] = mask & LIBOPNA_CHAN_FM_3;
  fp->masked[FMDRIVER_TRACK_FM_3_EX_1] = mask & LIBOPNA_CHAN_FM_3;
  fp->masked[FMDRIVER_TRACK_FM_3_EX_2] = mask & LIBOPNA_CHAN_FM_3;
  fp->masked[FMDRIVER_TRACK_FM_3_EX_3] = mask & LIBOPNA_CHAN_FM_3;
  fp->masked[FMDRIVER_TRACK_FM_4] = mask & LIBOPNA_CHAN_FM_4;
  fp->masked[FMDRIVER_TRACK_FM_5] = mask & LIBOPNA_CHAN_FM_5;
  fp->masked[FMDRIVER_TRACK_FM_6] = mask & LIBOPNA_CHAN_FM_6;
  fp->masked[FMDRIVER_TRACK_SSG_1] = mask & LIBOPNA_CHAN_SSG_1;
  fp->masked[FMDRIVER_TRACK_SSG_2] = mask & LIBOPNA_CHAN_SSG_2;
  fp->masked[FMDRIVER_TRACK_SSG_3] = mask & LIBOPNA_CHAN_SSG_3;
  fp->masked[FMDRIVER_TRACK_ADPCM] = mask & LIBOPNA_CHAN_ADPCM;
  fp->masked_rhythm = (mask & LIBOPNA_CHAN_DRUM_ALL) == LIBOPNA_CHAN_DRUM_ALL;
  unsigned ppz8mask = 0;
  if (fp->work && fp->work->ppz8) {
    ppz8mask = ppz8_get_mask(fp->work->ppz8);
  }
  fp->masked[FMDRIVER_TRACK_PPZ8_1] = ppz8mask & (1u<<0);
  fp->masked[FMDRIVER_TRACK_PPZ8_2] = ppz8mask & (1u<<1);
  fp->masked[FMDRIVER_TRACK_PPZ8_3] = ppz8mask & (1u<<2);
  fp->masked[FMDRIVER_TRACK_PPZ8_4] = ppz8mask & (1u<<3);
  fp->masked[FMDRIVER_TRACK_PPZ8_5] = ppz8mask & (1u<<4);
  fp->masked[FMDRIVER_TRACK_PPZ8_6] = ppz8mask & (1u<<5);
  fp->masked[FMDRIVER_TRACK_PPZ8_7] = ppz8mask & (1u<<6);
  fp->masked[FMDRIVER_TRACK_PPZ8_8] = ppz8mask & (1u<<7);
  if (fp->work) {
    switch (fp->lmode) {
    case FMDSP_LEFT_MODE_OPNA:
      update_track_10(fp, track_disp_table_opna, 0);
      break;
    case FMDSP_LEFT_MODE_OPN:
      update_track_10(fp, track_disp_table_opn, 0);
      break;
    case FMDSP_LEFT_MODE_13:
      update_track_13(fp, 0);
      break;
    case FMDSP_LEFT_MODE_PPZ8:
      update_track_10(fp, track_disp_table_ppz8, 0);
      break;
    default:
      break;
    }
    switch (fp->rmode) {
    case FMDSP_RIGHT_MODE_DEFAULT:
      break;
    case FMDSP_RIGHT_MODE_TRACK_INFO:
      break;
    case FMDSP_RIGHT_MODE_PPZ8:
      update_track_10(fp, track_disp_table_ppz8, 320);
      break;
    default:
      break;
    }
  }
  fp->pacc.begin_clear(fp->pc);
  fp->pacc.color(fp->pc, 1);
  fp->pacc.draw(fp->pc, fp->buf_font_1, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_font_1_d, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_dt_sign, pacc_mode_color);
  fp->pacc.color(fp->pc, 2);
  fp->pacc.draw(fp->pc, fp->buf_font_2, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_font_2_d, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_solid_2, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_vertical_2, pacc_mode_color);
  fp->pacc.color(fp->pc, 3);
  fp->pacc.draw(fp->pc, fp->buf_solid_3, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_vertical_3, pacc_mode_color);
  fp->pacc.color(fp->pc, 7);
  fp->pacc.draw(fp->pc, fp->buf_solid_7, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_vertical_7, pacc_mode_color);
  fp->pacc.draw(fp->pc, fp->buf_num, pacc_mode_copy);
  fp->pacc.draw(fp->pc, fp->buf_checker, pacc_mode_copy);
  fp->pacc.draw(fp->pc, fp->buf_key_left, pacc_mode_copy);
  fp->pacc.draw(fp->pc, fp->buf_key_bg, pacc_mode_copy);
  fp->pacc.draw(fp->pc, fp->buf_key_right, pacc_mode_copy);
  fp->pacc.draw(fp->pc, fp->buf_logo, pacc_mode_copy);
  fp->pacc.color(fp->pc, 8);
  fp->pacc.draw(fp->pc, fp->buf_key_mask_sub, pacc_mode_color_trans);
  fp->pacc.color(fp->pc, 6);
  fp->pacc.draw(fp->pc, fp->buf_key_mask, pacc_mode_color_trans);
}

void fmdsp_pacc_set(struct fmdsp_pacc *fp, struct fmdriver_work *work, struct opna *opna) {
  fp->work = work;
  fp->opna = opna;
}

void fmdsp_pacc_palette(struct fmdsp_pacc *fp, int p) {
  if (p < 0) return;
  if (p >= PALETTE_NUM) return;
  memcpy(fp->target_palette, s_palettes[p], sizeof(fp->target_palette));
}

enum fmdsp_left_mode fmdsp_pacc_left_mode(const struct fmdsp_pacc *fp) {
  return fp->lmode;
}

enum fmdsp_right_mode fmdsp_pacc_right_mode(const struct fmdsp_pacc *fp) {
  return fp->rmode;
}

void fmdsp_pacc_set_left_mode(struct fmdsp_pacc *fp, enum fmdsp_left_mode mode) {
  fp->lmode = mode;
  fp->mode_changed = true;
}

void fmdsp_pacc_set_right_mode(struct fmdsp_pacc *fp, enum fmdsp_right_mode mode) {
  fp->rmode = mode;
  fp->mode_changed = true;
}
