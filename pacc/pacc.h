#ifndef MYON_PACC_H_INCLUDED
#define MYON_PACC_H_INCLUDED

#include <stdint.h>
#include <stdarg.h>

enum pacc_mode {
  pacc_mode_copy,
  pacc_mode_color,
  pacc_mode_color_trans,
  pacc_mode_count,
};

enum pacc_buf_mode {
  pacc_buf_mode_static,
  pacc_buf_mode_stream,
};

struct pacc_ctx;
struct pacc_tex;
struct pacc_buf;

struct pacc_vtable {
  void (*pacc_delete)(struct pacc_ctx *pc);
  struct pacc_buf *(*gen_buf)(
      struct pacc_ctx *pc, struct pacc_tex *pt, enum pacc_buf_mode mode);
  struct pacc_tex *(*gen_tex)(struct pacc_ctx *pc, int w, int h);
  void (*buf_delete)(struct pacc_buf *buf);
  uint8_t *(*tex_lock)(struct pacc_tex *tex);
  void (*tex_unlock)(struct pacc_tex *tex);
  void (*tex_delete)(struct pacc_tex *tex);
  void (*buf_rect)(
      const struct pacc_ctx *ctx, struct pacc_buf *buf,
      int x, int y, int w, int h);
  void (*buf_rect_off)(
      const struct pacc_ctx *ctx, struct pacc_buf *buf,
      int x, int y, int w, int h, int xoff, int yoff);
  void (*buf_vprintf)(
      const struct pacc_ctx *ctx, struct pacc_buf *buf,
      int x, int y, const char *fmt, va_list ap);
  void (*buf_printf)(
      const struct pacc_ctx *ctx, struct pacc_buf *buf,
      int x, int y, const char *fmt, ...);
  void (*buf_clear)(struct pacc_buf *buf);
  void (*palette)(struct pacc_ctx *ctx, const uint8_t *rgb, int colors);
  void (*color)(struct pacc_ctx *ctx, uint8_t pal);
  void (*begin_clear)(struct pacc_ctx *ctx);
  void (*draw)(struct pacc_ctx *ctx, struct pacc_buf *buf, enum pacc_mode mode);
  void (*viewport_scale)(struct pacc_ctx *ctx, int scale);
};

struct pacc_ctx *pacc_init_gl(int w, int h, struct pacc_vtable *vt);

#endif // MYON_PACC_H_INCLUDED
