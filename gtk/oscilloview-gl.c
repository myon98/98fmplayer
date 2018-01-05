#include <gtk/gtk.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include <stdlib.h>
#include <string.h>
#include "oscilloview.h"

struct oscilloview oscilloview_g = {
  .flag = ATOMIC_FLAG_INIT
};

enum {
  VIEW_SAMPLES = 1024,
};

static struct {
  GtkWidget *win;
  struct oscillodata oscillodata[LIBOPNA_OSCILLO_TRACK_COUNT];
  GLuint program;
  GLuint vs, fs;
  GLuint vao;
  GLuint vbo_linear;
  GLuint vbo_data;
  GLint uni_xpos;
  GLint uni_ypos;
} g;

static void on_glarea_unrealize(GtkWidget *w, gpointer ptr) {
  (void)w;
  (void)ptr;
  GtkGLArea *area = GTK_GL_AREA(w);
  gtk_gl_area_make_current(area);
  if (gtk_gl_area_get_error(area)) return;
  glDeleteProgram(g.program);
  glDeleteShader(g.vs);
  glDeleteShader(g.fs);
  glDeleteBuffers(1, &g.vbo_linear);
  glDeleteBuffers(1, &g.vbo_data);
  glDeleteVertexArrays(1, &g.vao);
}

static void on_destroy(GtkWidget *w, gpointer ptr) {
  (void)w;
  (void)ptr;
  g.win = 0;
}

static const char v_sh[] =
"#version 110\n"
"attribute float coordx, coordy;\n"
"uniform float xpos, ypos;\n"
"void main(void) {\n"
"  gl_Position = vec4(vec2(coordx/3.0 + xpos, coordy*2.0/3.0 + ypos), 0.0, 1.0);\n"
"}\n";

static const char f_sh[] =
"#version 110\n"
"void main(void) {\n"
"  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
"}\n";

static GLuint create_shader(const char *shader, GLenum type) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &shader, 0);
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

static void on_realize(GtkWidget *w, gpointer ptr) {
  (void)ptr;
  GtkGLArea *area = GTK_GL_AREA(w);
  gtk_gl_area_make_current(area);
  if (gtk_gl_area_get_error(area)) return;
  g.program = glCreateProgram();
  g.vs = create_shader(v_sh, GL_VERTEX_SHADER);
  g.fs = create_shader(f_sh, GL_FRAGMENT_SHADER);
  glAttachShader(g.program, g.vs);
  glAttachShader(g.program, g.fs);
  glBindAttribLocation(g.program, 0, "coordx");
  glBindAttribLocation(g.program, 1, "coordy");
  glLinkProgram(g.program);
  glUseProgram(g.program);
  g.uni_xpos = glGetUniformLocation(g.program, "xpos");
  g.uni_ypos = glGetUniformLocation(g.program, "ypos");
  glGenVertexArrays(1, &g.vao);
  glBindVertexArray(g.vao);
  glGenBuffers(1, &g.vbo_linear);
  glBindBuffer(GL_ARRAY_BUFFER, g.vbo_linear);
  static GLfloat linear_pos[VIEW_SAMPLES];
  for (int i = 0; i < VIEW_SAMPLES; i++) {
    linear_pos[i] = 2.0f*((i-512) / (float)VIEW_SAMPLES);
  }
  glBufferData(GL_ARRAY_BUFFER, sizeof(linear_pos), linear_pos, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, 0);
  glGenBuffers(1, &g.vbo_data);
  glBindBuffer(GL_ARRAY_BUFFER, g.vbo_data);
  glBufferData(GL_ARRAY_BUFFER, 2*VIEW_SAMPLES, 0, GL_STREAM_DRAW);
  glVertexAttribPointer(1, 1, GL_SHORT, GL_TRUE, 0, 0);
}

static gboolean on_render(GtkGLArea *area,
                          GdkGLContext *ctx,
                          gpointer ptr) {
  (void)area;
  (void)ctx;
  (void)ptr;
  if (!atomic_flag_test_and_set_explicit(
    &oscilloview_g.flag, memory_order_acquire)) {
    memcpy(g.oscillodata,
           oscilloview_g.oscillodata,
           sizeof(oscilloview_g.oscillodata));
    atomic_flag_clear_explicit(&oscilloview_g.flag, memory_order_release);
  }
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glClear(GL_COLOR_BUFFER_BIT);

  for (int x = 0; x < 3; x++) {
    for (int y = 0; y < 3; y++) {
      int start = OSCILLO_SAMPLE_COUNT - VIEW_SAMPLES;
      start -= (g.oscillodata[x*3+y].offset >> OSCILLO_OFFSET_SHIFT);
      glBindBuffer(GL_ARRAY_BUFFER, g.vbo_data);
      glBufferData(GL_ARRAY_BUFFER, 2*VIEW_SAMPLES, &g.oscillodata[x*3+y].buf[start], GL_STREAM_DRAW);
      glUniform1f(g.uni_xpos, (x - 1) * (2.0f/3.0f));
      glUniform1f(g.uni_ypos, (y - 1) * (-2.0f/3.0f));
      glDrawArrays(GL_LINE_STRIP, 0, 1024);
    }
  }
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  return TRUE;
}

static gboolean tick_cb(GtkWidget *w, GdkFrameClock *clock, gpointer ptr) {
  (void)clock;
  (void)ptr;
  gtk_widget_queue_draw(w);
  return G_SOURCE_CONTINUE;
}

void show_oscilloview(void) {
  if (!g.win) {
    g.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g.win), "Oscilloscope view");
    g_signal_connect(g.win, "destroy", G_CALLBACK(on_destroy), 0);
    GtkWidget *glarea = gtk_gl_area_new();
    g_signal_connect(G_OBJECT(glarea), "unrealize", G_CALLBACK(on_glarea_unrealize), 0);
    gtk_gl_area_set_required_version(GTK_GL_AREA(glarea), 3, 2);
    g_signal_connect(glarea, "render", G_CALLBACK(on_render), 0);
    g_signal_connect(glarea, "realize", G_CALLBACK(on_realize), 0);
    gtk_container_add(GTK_CONTAINER(g.win), glarea);
    gtk_widget_add_tick_callback(glarea, tick_cb, 0, 0);
    gtk_widget_show_all(g.win);
  } else {
    gtk_window_present(GTK_WINDOW(g.win));
  }
}
