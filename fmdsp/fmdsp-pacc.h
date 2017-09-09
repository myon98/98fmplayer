#ifndef MYON_FMDSP_PACC_H_INCLUDED
#define MYON_FMDSP_PACC_H_INCLUDED

struct fmdsp_pacc;
struct pacc_ctx;
struct pacc_vtable;
struct fmdriver_work work;
struct opna opna;

enum {
  FMDSP_PALETTE_COLORS = 10,
};

enum {
  PC98_W = 640,
  PC98_H = 400,
  CHECKER_H = (16+3)*3+8,
  CHECKER_Y = PC98_H-CHECKER_H,
};

enum fmdsp_left_mode {
  FMDSP_LEFT_MODE_OPNA, // FM1, 2, 3, 4, 5, 6, SSG1, 2, 3, PCM
  FMDSP_LEFT_MODE_OPN,  // FM1, 2, 3, FMEX1, 2, 3, SSG1, 2, 3, PCM
  FMDSP_LEFT_MODE_13,   // FM1, 2, 3, FMEX1, 2, 3, FM4, 5, 6, SSG1, 2, 3, PCM
  FMDSP_LEFT_MODE_PPZ8, // PPZ8 1, 2, 3, 4, 5, 6, 7, 8, PCM
  FMDSP_LEFT_MODE_CNT
};

enum fmdsp_right_mode {
  FMDSP_RIGHT_MODE_DEFAULT,
  FMDSP_RIGHT_MODE_TRACK_INFO,
  FMDSP_RIGHT_MODE_PPZ8,
  FMDSP_RIGHT_MODE_CNT
};

struct fmdsp_pacc *fmdsp_pacc_init(
    struct pacc_ctx *pc, const struct pacc_vtable *vtable);
void fmdsp_pacc_set(struct fmdsp_pacc *pacc, struct fmdriver_work *work, struct opna *opna);
void fmdsp_pacc_release(struct fmdsp_pacc *fp);
void fmdsp_pacc_render(struct fmdsp_pacc *fp);

void fmdsp_pacc_palette(struct fmdsp_pacc *fp, int p);

enum fmdsp_left_mode fmdsp_pacc_left_mode(const struct fmdsp_pacc *fp);
void fmdsp_pacc_set_left_mode(struct fmdsp_pacc *fp, enum fmdsp_left_mode mode);
enum fmdsp_right_mode fmdsp_pacc_right_mode(const struct fmdsp_pacc *fp);
void fmdsp_pacc_set_right_mode(struct fmdsp_pacc *fp, enum fmdsp_right_mode mode);

#endif // MYON_FMDSP_PACC_H_INCLUDED
