
#ifdef PACC_GL_ES
#ifdef PACC_GL_3
#include <GLES3/gl3.h>
#else
#include <GLES2/gl2.h>
#endif
#else // PACC_GL_ES
#ifdef PACC_GL_3
#include <GL/glcorearb.h>
#else
#ifdef _WIN32
#define PROC_NO_GL_1_1
#include <GL/gl.h>
#include <GL/glext.h>
#else
#define PROC_NO_GL_1_3
#include <GL/gl.h>
#endif
#endif // PACC_GL_3
#endif // PACC_GL_ES

#ifdef PACC_GL_ES
bool loadgl(void) {
  return true;
}
#else // PACC_GL_ES
#include <SDL.h>
#define PROC(N, n) static PFNGL##N##PROC gl##n;
#include "pacc/pacc-gl-procs.inc"
#undef PROC
#define PROC(N, n) \
  gl##n = SDL_GL_GetProcAddress("gl" #n);\
  if (!gl##n) {\
    SDL_Log("Cannot load GL function \"gl" #n "\"\n");\
    return false;\
  }

bool loadgl(void) {
#include "pacc/pacc-gl-procs.inc"
  return true;
}
#undef PROC
#endif // PACC_GL_ES

#ifdef PROC_NO_GL_1_1
#undef PROC_NO_GL_1_1
#endif
#ifdef PROC_NO_GL_1_3
#undef PROC_NO_GL_1_3
#endif
