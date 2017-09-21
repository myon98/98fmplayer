#ifdef PACC_GL_ES
#  ifdef PACC_GL_3
#include <GLES3/gl3.h>
#  else
#include <GLES2/gl2.h>
#  endif
#else
#define GL_GLEXT_PROTOTYPES
#  ifdef PACC_GL_3
#include <GL/glcorearb.h>
#  else
#include <GL/gl.h>
#include <GL/glext.h>
#  endif
#endif
