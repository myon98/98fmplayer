#include <SDL.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "pacc/pacc.h"
#include "fmdsp/fmdsp-pacc.h"
#include "fmdsp/font.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdriver/fmdriver.h"
#include "common/fmplayer_file.h"
#include "common/fmplayer_common.h"
#include "common/fmplayer_fontrom.h"
#include "fft/fft.h"

bool loadgl(void);

enum {
  SRATE = 55467,
  BUFLEN = 1024,
};

static struct {
  struct opna opna;
  struct opna_timer timer;
  struct fmdriver_work work;
  struct fmplayer_file *fmfile;
  struct fmplayer_fft_data fftdata;
  struct fmplayer_fft_input_data fftin;
  const char *currpath;
  SDL_Window *win;
  SDL_AudioDeviceID adev;
  struct ppz8 ppz8;
  char adpcmram[OPNA_ADPCM_RAM_SIZE];
  struct fmdsp_pacc *fp;
  atomic_flag fftdata_flag;
  int scale;
  struct fmdsp_font font16;
} g = {
  .fftdata_flag = ATOMIC_FLAG_INIT,
  .scale = 1,
};

static void audiocb(void *ptr, Uint8 *bufptr, int len) {
  int frames = len / (sizeof(int16_t)*2);
  int16_t *buf = (int16_t *)bufptr;
  memset(buf, 0, len);
  opna_timer_mix(&g.timer, buf, frames);
  if (!atomic_flag_test_and_set_explicit(
        &g.fftdata_flag, memory_order_acquire)) {
    fft_write(&g.fftdata, buf, frames);
    atomic_flag_clear_explicit(&g.fftdata_flag, memory_order_release);
  }
}

static void openfile(const char *path) {
  enum fmplayer_file_error error;
  struct fmplayer_file *file = fmplayer_file_alloc(path, &error);
  if (!file) {
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,
        "cannot open file",
        fmplayer_file_strerror(error),
        g.win);
    return;
  }
  SDL_LockAudioDevice(g.adev);
  fmplayer_file_free(g.fmfile);
  g.fmfile = file;
  fmplayer_init_work_opna(&g.work, &g.ppz8, &g.opna, &g.timer, &g.adpcmram);
  fmplayer_file_load(&g.work, g.fmfile, 1);
  fmdsp_pacc_comment_reset(g.fp);
  SDL_UnlockAudioDevice(g.adev);
  SDL_PauseAudioDevice(g.adev, 0);
}

int main(int argc, char **argv) {
  if (__builtin_cpu_supports("sse2")) opna_ssg_sinc_calc_func = opna_ssg_sinc_calc_sse2;
  fft_init_table();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("Cannot initialize SDL\n");
    return 1;
  }

  SDL_AudioSpec aspec = {
    .freq = SRATE,
    .format = AUDIO_S16SYS,
    .channels = 2,
    .samples = BUFLEN,
    .callback = audiocb,
  };
  g.adev = SDL_OpenAudioDevice(0, 0, &aspec, 0, 0);
  if (!g.adev) {
    SDL_Log("Cannot open audio device\n");
    SDL_Quit();
    return 0;
  }

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#ifdef PACC_GL_ES
#ifdef PACC_GL_3
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif
#else // PACC_GL_ES
#ifdef PACC_GL_3
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
#endif // PACC_GL_ES
  g.win = SDL_CreateWindow(
      "FMPlayer SDL",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      PC98_W, PC98_H,
      SDL_WINDOW_OPENGL);
  if (!g.win) {
    SDL_Log("Cannot create window\n");
    SDL_Quit();
    return 1;
  }

  SDL_GLContext glctx = SDL_GL_CreateContext(g.win);
  if (!glctx) {
    SDL_Log("Cannot create OpenGL context\n");
    SDL_Quit();
    return 1;
  }

  SDL_GL_SetSwapInterval(1);

  if (!loadgl()) {
    SDL_Log("Cannot load OpenGL functions\n");
    SDL_Quit();
    return 1;
  }

  struct pacc_vtable pacc;
  struct pacc_ctx *pc = pacc_init_gl(640, 400, &pacc);
  if (!pc) {
    SDL_Log("Cannot initialize pacc\n");
    SDL_Quit();
    return 1;
  }

  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) {
    SDL_Log("Cannot allocate fmdsp\n");
    SDL_Quit();
    return 1;
  }
  if (!fmdsp_pacc_init(g.fp, pc, &pacc)) {
    SDL_Log("Cannot initialize fmdsp\n");
    SDL_Quit();
    return 1;
  }
  if (!fmdsp_pacc_init(g.fp, pc, &pacc)) {
    SDL_Log("Cannot initialize fmdsp\n");
    SDL_Quit();
    return 1;
  }
  fmdsp_pacc_set(g.fp, &g.work, &g.opna, &g.fftin);
  fmplayer_font_rom_load(&g.font16);
  fmdsp_pacc_set_font16(g.fp, &g.font16);

  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  SDL_Event e;
  bool end = false;
  while (!end) {
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
        end = true;
        break;
      case SDL_DROPFILE:
        openfile(e.drop.file);
        SDL_free(e.drop.file);
        break;
      case SDL_KEYDOWN:
        switch (e.key.keysym.scancode) {
        case SDL_SCANCODE_F11:
          if (e.key.keysym.mod & KMOD_SHIFT) {
            fmdsp_pacc_set_right_mode(
                g.fp,
                (fmdsp_pacc_right_mode(g.fp) + 1) % FMDSP_RIGHT_MODE_CNT);
          } else {
            fmdsp_pacc_set_left_mode(
                g.fp,
                (fmdsp_pacc_left_mode(g.fp) + 1) % FMDSP_LEFT_MODE_CNT);
          }
          break;
        case SDL_SCANCODE_F12:
          g.scale++;
          if (g.scale > 3) g.scale = 1;
          SDL_SetWindowSize(g.win, PC98_W*g.scale, PC98_H*g.scale);
          pacc.viewport_scale(pc, g.scale);
          break;
        default:
          break;
        }
        if (e.key.keysym.mod & KMOD_SHIFT) {
          switch (e.key.keysym.scancode) {
          case SDL_SCANCODE_UP:
            fmdsp_pacc_comment_scroll(g.fp, false);
            break;
          case SDL_SCANCODE_DOWN:
            fmdsp_pacc_comment_scroll(g.fp, true);
            break;
          default:
            break;
          }
        }
        if (e.key.keysym.mod & KMOD_CTRL) {
          switch (e.key.keysym.scancode) {
          case SDL_SCANCODE_F1:
            fmdsp_pacc_palette(g.fp, 0);
            break;
          case SDL_SCANCODE_F2:
            fmdsp_pacc_palette(g.fp, 1);
            break;
          case SDL_SCANCODE_F3:
            fmdsp_pacc_palette(g.fp, 2);
            break;
          case SDL_SCANCODE_F4:
            fmdsp_pacc_palette(g.fp, 3);
            break;
          case SDL_SCANCODE_F5:
            fmdsp_pacc_palette(g.fp, 4);
            break;
          case SDL_SCANCODE_F6:
            fmdsp_pacc_palette(g.fp, 5);
            break;
          case SDL_SCANCODE_F7:
            fmdsp_pacc_palette(g.fp, 6);
            break;
          case SDL_SCANCODE_F8:
            fmdsp_pacc_palette(g.fp, 7);
            break;
          case SDL_SCANCODE_F9:
            fmdsp_pacc_palette(g.fp, 8);
            break;
          case SDL_SCANCODE_F10:
            fmdsp_pacc_palette(g.fp, 9);
            break;
          default:
            break;
          }
        }
      }
    }
    if (!atomic_flag_test_and_set_explicit(
          &g.fftdata_flag, memory_order_acquire)) {
      memcpy(&g.fftin.fdata, &g.fftdata, sizeof(g.fftdata));
      atomic_flag_clear_explicit(&g.fftdata_flag, memory_order_release);
    }
    fmdsp_pacc_render(g.fp);
    SDL_GL_SwapWindow(g.win);
  }

  fmdsp_pacc_release(g.fp);
  pacc.pacc_delete(pc);

  SDL_Quit();
  return 0;
}
