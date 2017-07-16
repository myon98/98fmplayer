
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>

#include "common/fmplayer_file.h"
#include "common/fmplayer_common.h"
#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/fmdriver_pmd.h"
#include "fmdriver/ppz8.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdsp/fmdsp.h"
#include "fmdsp/fmdsp_gl.h"
//#include "loadpcm.h"
#include "soundout.h"

union drivers {
  struct driver_pmd pmd;
  struct driver_fmp fmp;
};

enum driver_type {
  DRIVER_PMD,
  DRIVER_FMP,
};

#define DATADIR "/.local/share/fmplayer/"
enum {
  SRATE = 55467,
  BUFFRAMES = 1024,
  PPZ8MIX = 0xa000
};

static struct {
  struct opna opna;
  uint8_t adpcmram[OPNA_ADPCM_RAM_SIZE];
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  struct fmdsp fmdsp;
  struct fmdsp_font font98;
  uint8_t font98data[FONT_ROM_FILESIZE];
  uint8_t vram[PC98_W*PC98_H];
  char drum_rom[OPNA_ROM_SIZE];
  bool drum_rom_loaded;
  char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  struct fmplayer_file *fmfile;
  int16_t buf[BUFFRAMES*2];
  struct sound_state *ss;
  atomic_flag at_fftdata_flag;
  struct fmplayer_fft_data at_fftdata;
  struct fmplayer_fft_input_data fftdata;
} g = {
  .at_fftdata_flag = ATOMIC_FLAG_INIT
};

static void soundout_cb(void *userptr, int16_t *buf, unsigned frames) {
  struct opna_timer *timer = (struct opna_timer *)userptr;
  memset(buf, 0, sizeof(int16_t)*frames*2);
  opna_timer_mix(timer, buf, frames);

  if (!atomic_flag_test_and_set_explicit(
    &g.at_fftdata_flag, memory_order_acquire)) {
    fft_write(&g.at_fftdata, buf, frames);
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
}

static void load_fontrom(void) {
  const char *path = "font.rom";
  const char *home = getenv("HOME");
  char *dpath = 0;
  fmdsp_font_from_font_rom(&g.font98, g.font98data);
  if (home) {
    const char *datadir = DATADIR;
    dpath = malloc(strlen(home)+strlen(datadir)+strlen(path) + 1);
    if (dpath) {
      strcpy(dpath, home);
      strcat(dpath, datadir);
      strcat(dpath, path);
      path = dpath;
    }
  }
  FILE *font = fopen(path, "r");
  free(dpath);
  if (!font) goto err;
  if (fseek(font, 0, SEEK_END) != 0) goto err_file;
  long size = ftell(font);
  if (size != FONT_ROM_FILESIZE) goto err_file;
  if (fseek(font, 0, SEEK_SET) != 0) goto err_file;
  if (fread(g.font98data, 1, FONT_ROM_FILESIZE, font) != FONT_ROM_FILESIZE) {
    goto err_file;
  }
  fclose(font);
  return;
err_file:
  fclose(font);
err:
  return;
}

int main(int argc, char **argv) {
  fft_init_table();
  if (argc != 2) {
    printf("invalid arguments\n");
    return 1;
  }
  enum fmplayer_file_error error;
  g.fmfile = fmplayer_file_alloc(argv[1], &error);
  if (!g.fmfile) {
    printf("cannot load file: %s\n", fmplayer_file_strerror(error));
    return 1;
  }
  fmplayer_init_work_opna(&g.work, &g.ppz8, &g.opna, &g.opna_timer, g.adpcmram);
  char *realpathbuf = realpath(argv[1], 0);
  if (realpathbuf) {
    strncpy(g.work.filename, realpathbuf, sizeof(g.work.filename)-1);
    free(realpathbuf);
  }
  fmplayer_file_load(&g.work, g.fmfile, 1);
  g.ss = sound_init("FMPlayer glfw", SRATE, soundout_cb, &g.opna_timer);
  if (!g.ss) {
    printf("cannot open audio stream\n");
    return 1;
  }
  if (!glfwInit()) {
    fprintf(stderr, "cannot initialize glfw\n");
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  GLFWwindow *w = glfwCreateWindow(1280, 800, "FMPlayer OpenGL test", 0, 0);
  if (!w) {
    fprintf(stderr, "cannot create window\n");
    return 1;
  }
  glfwMakeContextCurrent(w);
  load_fontrom();
  fmdsp_init(&g.fmdsp, &g.font98);
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  struct fmdsp_gl *fmdsp_gl = fmdsp_gl_init(&g.fmdsp, 1.0, 1.0);
  if (!fmdsp_gl) {
    printf("cannot initialize opengl\n");
    return 1;
  }
  g.ss->pause(g.ss, 0, 0);

  while (!glfwWindowShouldClose(w)) {
    if (!atomic_flag_test_and_set_explicit(
      &g.at_fftdata_flag, memory_order_acquire)) {
      memcpy(&g.fftdata.fdata, &g.at_fftdata, sizeof(g.fftdata));
      atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
    }
    fmdsp_update(&g.fmdsp, &g.work, &g.opna, g.vram, &g.fftdata);
    fmdsp_gl_render(fmdsp_gl, g.vram);
    glfwSwapBuffers(w);
    glfwPollEvents();
  }
  g.ss->free(g.ss);
  return 0;
}

