#ifndef MYON_FMDSP_GL_H_INCLUDED
#define MYON_FMDSP_GL_H_INCLUDED

#include "fmdsp/fmdsp.h"

struct fmdsp_gl;

struct fmdsp_gl *fmdsp_gl_init(struct fmdsp *fmdsp, float xscale, float yscale);
void fmdsp_gl_render(struct fmdsp_gl *fmdsp_gl, uint8_t *vram);

#endif // MYON_FMDSP_GL_H_INCLUDED
