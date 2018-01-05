#include "pacc-gl.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pacc-gl-inc.h"

#ifdef NDEBUG
#define DPRINTF(fmt, ...)
#else
#include <stdio.h>
#define DPRINTF(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#endif

/*
  OpenGL versions:
  OpenGL 2.0      / GLSL 1.10
  OpenGL 3.2 core / GLSL 1.50 core (#define PACC_GL_3)
  OpenGL ES 2.0   / GLSL ES 1.00   (#define PACC_GL_ES)
  OpenGL ES 3.0   / GLSL ES 3.00   (#define PACC_GL_ES, #define PACC_GL_3)
*/

#ifdef PACC_GL_ES

#ifdef PACC_GL_3
#include "glsl/es3header.inc"
#else
#include "glsl/esheader.inc"
#endif

#else

#ifdef PACC_GL_3
#include "glsl/ds3header.inc"
#else
#include "glsl/dsheader.inc"
#endif

#endif

#include "glsl/blit.vert.inc"
#include "glsl/copy.frag.inc"
#include "glsl/color.frag.inc"
#include "glsl/color_trans.frag.inc"

struct pacc_ctx {
  int w;
  int h;
  uint8_t pal[256*3];
  bool pal_changed;
  uint8_t clearcolor[3];
  GLuint tex_pal;
  GLuint progs[pacc_mode_count];
  GLint uni_color, uni_color_trans;
  uint8_t color;
  bool color_changed;
  enum pacc_mode curr_mode;
};

struct pacc_buf {
  GLuint buf_obj;
  GLuint va_obj; // for OpenGL 3.2 Core
  GLfloat *buf;
  struct pacc_tex *tex;
  int len;
  int buflen;
  GLenum usage;
  bool changed;
};

struct pacc_tex {
  GLuint tex_obj;
  int w, h;
  bool changed;
  uint8_t *buf;
};

enum {
  VAI_COORD,
};

enum {
  PACC_BUF_DEF_LEN = 32,
  PRINTBUFLEN = 160,
};

static void pacc_delete(struct pacc_ctx *pc) {
  if (pc) {
    glDeleteTextures(1, &pc->tex_pal);
    for (int i = 0; i < pacc_mode_count; i++) {
      glDeleteProgram(pc->progs[i]);
    }
    free(pc);
  }
}

static GLuint compile_shader(const uint8_t *ss, GLenum type) {
  GLuint s = glCreateShader(type);
  if (!s) goto err;
  const char *sourcelist[2] = {
#ifdef PACC_GL_ES
#ifdef PACC_GL_3
    (const char *)es3header,
#else
    (const char *)esheader,
#endif
#else
#ifdef PACC_GL_3
    (const char *)ds3header,
#else
    (const char *)dsheader,
#endif
#endif
    (const char *)ss
  };
  glShaderSource(s, 2, sourcelist, 0);
  glCompileShader(s);
  GLint res;
  glGetShaderiv(s, GL_COMPILE_STATUS, &res);
  if (!res) {
#ifndef NDEBUG
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &res);
    char *msgbuf = malloc(res);
    if (msgbuf) {
      glGetShaderInfoLog(s, res, 0, msgbuf);
      DPRINTF("%s shader error: \n%s\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment", msgbuf);
      DPRINTF("s:\n%s\n", ss);
      free(msgbuf);
    }
#endif
    goto err;
  }
  return s;
err:
  glDeleteShader(s);
  return 0;
}

static GLuint compile_and_link(const uint8_t *vss, const uint8_t *fss) {
  GLuint p = 0, vs = 0, fs = 0;
  p = glCreateProgram();
  if (!p) goto err;
  vs = compile_shader(vss, GL_VERTEX_SHADER);
  if (!vs) goto err;
  fs = compile_shader(fss, GL_FRAGMENT_SHADER);
  if (!fs) goto err;
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glBindAttribLocation(p, VAI_COORD, "coord");
  glLinkProgram(p);
  GLint res;
  glGetProgramiv(p, GL_LINK_STATUS, &res);
  if (!res) {
#ifndef NDEBUG
    glGetProgramiv(p, GL_INFO_LOG_LENGTH, &res);
    char *msgbuf = malloc(res);
    if (msgbuf) {
      glGetProgramInfoLog(p, res, 0, msgbuf);
      DPRINTF("program link error: \n%s\n", msgbuf);
      DPRINTF("vs:\n%s\n", vss);
      DPRINTF("fs:\n%s\n", fss);
      free(msgbuf);
    }
#endif
    goto err;
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  glUseProgram(p);
  glUniform1i(glGetUniformLocation(p, "tex"), 1);
  return p;
err:
  glDeleteProgram(p);
  glDeleteShader(vs);
  glDeleteShader(fs);
  return 0;
}

static void pacc_buf_delete(struct pacc_buf *pb) {
  if (pb) {
    free(pb->buf);
    glDeleteBuffers(1, &pb->buf_obj);
#ifdef PACC_GL_3
    glDeleteVertexArrays(1, &pb->va_obj);
#endif
    free(pb);
  }
}

static struct pacc_buf *pacc_gen_buf(
    struct pacc_ctx *pc, struct pacc_tex *pt, enum pacc_buf_mode mode) {
  (void)pc;
  struct pacc_buf *pb = malloc(sizeof(*pb));
  if (!pb) goto err;
  *pb = (struct pacc_buf) {
    .buflen = PACC_BUF_DEF_LEN,
    .tex = pt,
    .usage = (mode == pacc_buf_mode_static) ?
      GL_STATIC_DRAW : GL_STREAM_DRAW,
  };
  pb->buf = malloc(sizeof(*pb->buf) * pb->buflen);
  if (!pb->buf) goto err;
  glGenBuffers(1, &pb->buf_obj);
  if (!pb->buf_obj) goto err;
#ifdef PACC_GL_3
  glGenVertexArrays(1, &pb->va_obj);
  if (!pb->va_obj) goto err;
  glBindVertexArray(pb->va_obj);
  glEnableVertexAttribArray(VAI_COORD);
  glBindBuffer(GL_ARRAY_BUFFER, pb->buf_obj);
  glVertexAttribPointer(VAI_COORD, 4, GL_FLOAT, GL_FALSE, 0, 0);
#endif
  return pb;
err:
  pacc_buf_delete(pb);
  return 0;
}

static bool buf_reserve(struct pacc_buf *pb, int len) {
  if (pb->len + len > pb->buflen) {
    int newlen = pb->buflen;
    while (pb->len + len > newlen) newlen *= 2;
    GLfloat *newbuf = realloc(pb->buf, newlen * sizeof(pb->buf[0]));
    if (!newbuf) return false;
    pb->buflen = newlen;
    pb->buf = newbuf;
  }
  return true;
}

static void pacc_calc_scale(float *ret, int w, int h, int wdest, int hdest) {
  ret[0] = ((float)w) / wdest;
  ret[1] = ((float)h) / hdest;
}

static void pacc_calc_off_tex(float *ret,
                                     int tw, int th, int xsrc, int ysrc) {
  ret[0] = ((float)xsrc) / tw;
  ret[1] = ((float)ysrc) / th;
}

static void pacc_calc_off(
    float *ret, int xdest, int ydest, int w, int h, int wdest, int hdest) {
  ret[0] = ((float)(xdest * 2 + w - wdest)) / wdest;
  ret[1] = ((float)(ydest * 2 + h - hdest)) / hdest;
}

static void pacc_buf_rect_off(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h, int xoff, int yoff) {
  if (!w && !h) return;
  float scale[2];
  float off[2];
  float tscale[2];
  float toff[2];
  pacc_calc_off(off, x, y, w, h, pc->w, pc->h);
  pacc_calc_scale(scale, w, h, pc->w, pc->h);
  pacc_calc_off_tex(toff, pb->tex->w, pb->tex->h, xoff, yoff);
  pacc_calc_scale(tscale, w, h, pb->tex->w, pb->tex->h);
  GLfloat coord[16] = {
    -1.0f * scale[0] + off[0], -1.0f * scale[1] - off[1],
     0.0f * tscale[0] + toff[0],  1.0f * tscale[1] + toff[1],

    -1.0f * scale[0] + off[0],  1.0f * scale[1] - off[1],
     0.0f * tscale[0] + toff[0],  0.0f * tscale[1] + toff[1],

     1.0f * scale[0] + off[0], -1.0f * scale[1] - off[1],
     1.0f * tscale[0] + toff[0],  1.0f * tscale[1] + toff[1],

     1.0f * scale[0] + off[0],  1.0f * scale[1] - off[1],
     1.0f * tscale[0] + toff[0],  0.0f * tscale[1] + toff[1],
  };
  if (!buf_reserve(pb, 24)) return;
  int indices[6] = {0, 1, 2, 2, 1, 3};
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 4; j++) {
      pb->buf[pb->len+i*4+j] = coord[indices[i]*4+j];
    }
  }
  pb->len += 24;
  pb->changed = true;
}

static void pacc_buf_vprintf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, va_list ap) {
  uint8_t printbuf[PRINTBUFLEN+1];
  vsnprintf((char *)printbuf, sizeof(printbuf), fmt, ap);
  int len = strlen((const char *)printbuf);
  float scale[2];
  float off[2];
  int w = pb->tex->w / 256;
  int h = pb->tex->h;
  pacc_calc_scale(scale, w, h, pc->w, pc->h);
  pacc_calc_off(off, x, y, w, h, pc->w, pc->h);
  if (!buf_reserve(pb, len*24)) return;
  GLfloat *coords = pb->buf + pb->len;
  for (int i = 0; i < len; i++) {
    coords[24*i+0*4+0]                      = (-1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+0*4+1]                      = -1.0f * scale[1] - off[1];
    coords[24*i+1*4+0] = coords[24*i+4*4+0] = (-1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+1*4+1] = coords[24*i+4*4+1] = 1.0f * scale[1] - off[1];
    coords[24*i+2*4+0] = coords[24*i+3*4+0] = (1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+2*4+1] = coords[24*i+3*4+1] = -1.0f * scale[1] - off[1];
    coords[24*i+5*4+0]                      = (1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+5*4+1]                      = 1.0f * scale[1] - off[1];
    coords[24*i+0*4+2]                      = ((float)printbuf[i]) / 256.0f;
    coords[24*i+0*4+3]                      = 1.0f;
    coords[24*i+1*4+2] = coords[24*i+4*4+2] = ((float)printbuf[i]) / 256.0f;
    coords[24*i+1*4+3] = coords[24*i+4*4+3] = 0.0f;
    coords[24*i+2*4+2] = coords[24*i+3*4+2] = ((float)(printbuf[i]+1)) / 256.0f;
    coords[24*i+2*4+3] = coords[24*i+3*4+3] = 1.0f;
    coords[24*i+5*4+2]                      = ((float)(printbuf[i]+1)) / 256.0f;
    coords[24*i+5*4+3]                      = 0.0f;
  }
  pb->len += len * 24;
  pb->changed = true;
}

static void pacc_buf_printf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pacc_buf_vprintf(pc, pb, x, y, fmt, ap);
  va_end(ap);
}

static void pacc_buf_rect(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h) {
  pacc_buf_rect_off(pc, pb, x, y, w, h, 0, 0);
}

static void pacc_buf_clear(struct pacc_buf *pb) {
  pb->len = 0;
  pb->changed = true;
}

static void pacc_palette(struct pacc_ctx *pc, const uint8_t *rgb, int colors) {
  memcpy(pc->pal, rgb, colors*3);
  pc->pal_changed = true;
}

static void pacc_color(struct pacc_ctx *pc, uint8_t pal) {
  pc->color = pal;
  pc->color_changed = true;
}

static void pacc_begin_clear(struct pacc_ctx *pc) {
  if (pc->pal_changed) {
    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(
        GL_TEXTURE_2D,
        0, GL_RGB,
        256, 1,
        0, GL_RGB,
        GL_UNSIGNED_BYTE, pc->pal);
    pc->pal_changed = false;
    glActiveTexture(GL_TEXTURE1);
  }
  if (memcmp(pc->clearcolor, pc->pal, 3)) {
    memcpy(pc->clearcolor, pc->pal, 3);
    glClearColor(
        pc->clearcolor[0] / 255.f,
        pc->clearcolor[1] / 255.f,
        pc->clearcolor[2] / 255.f,
        1.0f);
  }
  glClear(GL_COLOR_BUFFER_BIT);
  pc->curr_mode = pacc_mode_count;
}

static void pacc_draw(struct pacc_ctx *pc, struct pacc_buf *pb, enum pacc_mode mode) {
  if (!pb->len) return;
  if (mode >= pacc_mode_count) return;
  if (pc->curr_mode != mode) {
    glUseProgram(pc->progs[mode]);
    pc->curr_mode = mode;
  }
  if (mode != pacc_mode_copy && pc->color_changed) {
    glUniform1f(pc->uni_color, pc->color / 255.f);
    glUniform1f(pc->uni_color_trans, pc->color / 255.f);
    pc->color_changed = false;
  }
  glBindTexture(GL_TEXTURE_2D, pb->tex->tex_obj);
  if (pb->tex->changed) {
    GLint format;
#if PACC_GL_3
    format = GL_RED;
#else
    format = GL_LUMINANCE;
#endif
    glTexImage2D(
        GL_TEXTURE_2D,
        0, format,
        pb->tex->w, pb->tex->h,
        0, format, GL_UNSIGNED_BYTE,
        pb->tex->buf);
    pb->tex->changed = false;
  }
  if (pb->changed) {
    glBindBuffer(GL_ARRAY_BUFFER, pb->buf_obj);
    glBufferData(
        GL_ARRAY_BUFFER,
        pb->len * sizeof(pb->buf[0]), pb->buf,
        pb->usage);
    pb->changed = false;
  }
#ifdef PACC_GL_3
  glBindVertexArray(pb->va_obj);
#else
  glEnableVertexAttribArray(VAI_COORD);
  glBindBuffer(GL_ARRAY_BUFFER, pb->buf_obj);
  glVertexAttribPointer(VAI_COORD, 4, GL_FLOAT, GL_FALSE, 0, 0);
#endif
  glDrawArrays(GL_TRIANGLES, 0, pb->len / 4);
}

static uint8_t *pacc_tex_lock(struct pacc_tex *pt) {
  return pt->buf;
}

static void pacc_tex_unlock(struct pacc_tex *pt) {
  pt->changed = true;
}

static void pacc_tex_delete(struct pacc_tex *pt) {
  if (pt) {
    glDeleteTextures(1, &pt->tex_obj);
    free(pt->buf);
    free(pt);
  }
}

static struct pacc_tex *pacc_gen_tex(struct pacc_ctx *pc, int w, int h) {
  (void)pc;
  struct pacc_tex *pt = malloc(sizeof(*pt));
  if (!pt) goto err;
  *pt = (struct pacc_tex) {
    .w = w,
    .h = h,
    .buf = calloc(w*h, 1),
  };
  if (!pt->buf) goto err;
  glGenTextures(1, &pt->tex_obj);
  glBindTexture(GL_TEXTURE_2D, pt->tex_obj);
  GLint format;
#ifdef PACC_GL_3
  format = GL_RED;
#else
  format = GL_LUMINANCE;
#endif
  glTexImage2D(
      GL_TEXTURE_2D,
      0, format,
      w, h, 0,
      format, GL_UNSIGNED_BYTE,
      pt->buf);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  if (w & (w - 1)) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  }
  if (h & (h - 1)) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  return pt;
err:
  pacc_tex_delete(pt);
  return 0;
}

static void pacc_viewport_scale(struct pacc_ctx *pc, int scale) {
  glViewport(0, 0, pc->w*scale, pc->h*scale);
}

static struct pacc_vtable pacc_gl_vtable = {
  .pacc_delete = pacc_delete,
  .gen_buf = pacc_gen_buf,
  .gen_tex = pacc_gen_tex,
  .buf_delete = pacc_buf_delete,
  .tex_lock = pacc_tex_lock,
  .tex_unlock = pacc_tex_unlock,
  .tex_delete = pacc_tex_delete,
  .buf_rect = pacc_buf_rect,
  .buf_rect_off = pacc_buf_rect_off,
  .buf_vprintf = pacc_buf_vprintf,
  .buf_printf = pacc_buf_printf,
  .buf_clear = pacc_buf_clear,
  .palette = pacc_palette,
  .color = pacc_color,
  .begin_clear = pacc_begin_clear,
  .draw = pacc_draw,
  .viewport_scale = pacc_viewport_scale,
};

struct pacc_ctx *pacc_init_gl(int w, int h, struct pacc_vtable *vt) {
  struct pacc_ctx *pc = malloc(sizeof(*pc));
  if (!pc) goto err;
  *pc = (struct pacc_ctx) {
    .w = w,
    .h = h,
  };
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glGenTextures(1, &pc->tex_pal);
  glBindTexture(GL_TEXTURE_2D, pc->tex_pal);
  glTexImage2D(
      GL_TEXTURE_2D,
      0, GL_RGB,
      256, 1, 0,
      GL_RGB, GL_UNSIGNED_BYTE,
      pc->pal);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  pc->progs[pacc_mode_copy] = compile_and_link(blit_vert, copy_frag);
  if (!pc->progs[pacc_mode_copy]) goto err;
  pc->progs[pacc_mode_color] = compile_and_link(blit_vert, color_frag);
  if (!pc->progs[pacc_mode_color]) goto err;
  pc->progs[pacc_mode_color_trans] = compile_and_link(blit_vert, color_trans_frag);
  if (!pc->progs[pacc_mode_color_trans]) goto err;
  pc->uni_color = glGetUniformLocation(pc->progs[pacc_mode_color], "color");
  pc->uni_color_trans = glGetUniformLocation(pc->progs[pacc_mode_color_trans], "color");
  glActiveTexture(GL_TEXTURE1);
  *vt = pacc_gl_vtable;
  return pc;
err:
  pacc_delete(pc);
  return 0;
}

