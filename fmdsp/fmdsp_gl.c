// supports OpenGL 2.0 / OpenGL ES 2.0
// when using OpenGL ES, define MYON_OPENGL_ES

#include "myon_opengl.h"
#include "fmdsp_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MYON_OPENGL_ES
static const char sh_version[] = "#version 100\n";
#else
static const char sh_version[] = "#version 110\n";
#endif

static const char v_sh[] =
  "attribute vec2 coord2d;\n"
  "varying vec2 texcoord;\n"
  "uniform float xscale;\n"
  "uniform float yscale;\n"
  "void main(void) {\n"
  "  vec2 coord = coord2d * vec2(xscale, yscale);\n"
  "  gl_Position = vec4(coord, 0.0, 1.0);\n"
  "  texcoord = (coord2d + 1.0) / 2.0;\n"
  "  texcoord = vec2(texcoord.x, 1.0-texcoord.y);\n"
  "}\n"
;

#ifdef MYON_OPENGL_ES
static const char f_sh[] =
  "uniform sampler2D vram;\n"
  "uniform sampler2D palette;\n"
  "varying mediump vec2 texcoord;\n"
  "void main(void) {\n"
  "  mediump float index = texture2D(vram, texcoord).x;\n"
  "  index = index * 256.0 / 9.0;\n"
  "  gl_FragColor = texture2D(palette, vec2(index, 0.0));\n"
  "}\n"
;
#else
static const char f_sh[] =
  "uniform sampler2D vram;\n"
  "uniform sampler2D palette;\n"
  "varying vec2 texcoord;\n"
  "void main(void) {\n"
  "  float index = texture2D(vram, texcoord).x;\n"
  "  index = index * 256.0 / 9.0;\n"
  "  gl_FragColor = texture2D(palette, vec2(index, 0.0));\n"
  "}\n"
;
#endif

static const GLfloat triangle_vertices[] = {
  -1.0, -1.0,
  1.0, -1.0,
  -1.0, 1.0,
  1.0, 1.0
};

static GLuint create_shader(const char *shader, GLenum type) {
  GLuint s = glCreateShader(type);
  const char *source[2] = {sh_version, shader};
  glShaderSource(s, 2, source, 0);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
      char *log = malloc(len);
      if (log) {
        glGetShaderInfoLog(s, len, 0, log);
        printf("%s shader error: \n%s\n",
               (type == GL_VERTEX_SHADER) ? "vertex" : "fragment",
               log);
        free(log);
      }
    }
    glDeleteShader(s);
    return 0;
  }
  return s;
}

struct fmdsp_gl {
  struct fmdsp *fmdsp;
  GLuint program;
  GLint attr_coord2d;
  GLint uni_vram;
  GLint uni_palette;
  GLint uni_xscale;
  GLint uni_yscale;
  GLuint tex_vram;
  GLuint tex_palette;
  uint8_t prev_palette[FMDSP_PALETTE_COLORS*3];
};

struct fmdsp_gl *fmdsp_gl_init(struct fmdsp *fmdsp, float xscale, float yscale) {
  struct fmdsp_gl *fmdsp_gl = 0;
  GLint vs = 0;
  GLint fs = 0;
  fmdsp_gl = malloc(sizeof(*fmdsp_gl));
  if (!fmdsp_gl) goto err;
  fmdsp_gl->fmdsp = fmdsp;
  fmdsp_gl->program = glCreateProgram();
  glGenTextures(1, &fmdsp_gl->tex_vram);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fmdsp_gl->tex_vram);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glActiveTexture(GL_TEXTURE1);
  glGenTextures(1, &fmdsp_gl->tex_palette);
  glBindTexture(GL_TEXTURE_2D, fmdsp_gl->tex_palette);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  vs = create_shader(v_sh, GL_VERTEX_SHADER);
  if (!vs) goto err;
  fs = create_shader(f_sh, GL_FRAGMENT_SHADER);
  if (!fs) goto err;
  glAttachShader(fmdsp_gl->program, vs);
  glAttachShader(fmdsp_gl->program, fs);
  glLinkProgram(fmdsp_gl->program);
  GLint ok;
  glGetProgramiv(fmdsp_gl->program, GL_LINK_STATUS, &ok);
  if (!ok) {
    printf("link error\n");
    goto err;
  }
  glUseProgram(fmdsp_gl->program);
  fmdsp_gl->attr_coord2d = glGetAttribLocation(fmdsp_gl->program, "coord2d");
  if (fmdsp_gl->attr_coord2d < 0) goto err;
  fmdsp_gl->uni_vram = glGetUniformLocation(fmdsp_gl->program, "vram");
  if (fmdsp_gl->uni_vram < 0) goto err;
  fmdsp_gl->uni_palette = glGetUniformLocation(fmdsp_gl->program, "palette");
  if (fmdsp_gl->uni_palette < 0) goto err;
  fmdsp_gl->uni_xscale = glGetUniformLocation(fmdsp_gl->program, "xscale");
  if (fmdsp_gl->uni_xscale < 0) goto err;
  fmdsp_gl->uni_yscale = glGetUniformLocation(fmdsp_gl->program, "yscale");
  if (fmdsp_gl->uni_yscale < 0) goto err;
  glUniform1i(fmdsp_gl->uni_vram, 0);
  glUniform1i(fmdsp_gl->uni_palette, 1);
  glUniform1f(fmdsp_gl->uni_xscale, xscale);
  glUniform1f(fmdsp_gl->uni_yscale, yscale);
  glEnableVertexAttribArray(fmdsp_gl->attr_coord2d);
  glVertexAttribPointer(fmdsp_gl->attr_coord2d,
                        2, GL_FLOAT, GL_FALSE, 0, triangle_vertices);
  return fmdsp_gl;
err:
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (fmdsp_gl) {
    glDeleteTextures(1, &fmdsp_gl->tex_vram);
    glDeleteTextures(1, &fmdsp_gl->tex_palette);
    glDeleteProgram(fmdsp_gl->program);
  }
  free(fmdsp_gl);
  return 0;
}

void fmdsp_gl_render(struct fmdsp_gl *fmdsp_gl, uint8_t *vram) {
  glActiveTexture(GL_TEXTURE0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, PC98_W, PC98_H, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, vram);
  if (memcmp(fmdsp_gl->prev_palette, fmdsp_gl->fmdsp->palette, sizeof(fmdsp_gl->prev_palette))) {
    glActiveTexture(GL_TEXTURE1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FMDSP_PALETTE_COLORS, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fmdsp_gl->fmdsp->palette);
    memcpy(fmdsp_gl->prev_palette, fmdsp_gl->fmdsp->palette, sizeof(fmdsp_gl->prev_palette));
  }
  glClear(GL_COLOR_BUFFER_BIT);
  glActiveTexture(GL_TEXTURE1);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
}
