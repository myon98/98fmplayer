#include "wavesave.h"
#include <commdlg.h>
#include <stdint.h>
#include "common/fmplayer_file.h"
#include "common/fmplayer_common.h"
#include "libopna/opnatimer.h"
#include "libopna/opna.h"
#include "wavewrite.h"
#include <stdlib.h>
#include <windowsx.h>
#include <commctrl.h>
#include "configdialog.h"

enum {
  SRATE = 55467,
  LOOPCNT = 2,
};

static struct {
  ATOM class;
} g;

struct fadeout {
  struct opna_timer *timer;
  struct fmdriver_work *work;
  uint64_t vol;
  uint8_t loopcnt;
};

static bool fadeout_mix(
  struct fadeout *fadeout,
  int16_t *buf, unsigned frames
) {
  opna_timer_mix(fadeout->timer, buf, frames);
  for (unsigned i = 0; i < frames; i++) {
    int vol = fadeout->vol >> 16;
    buf[i*2+0] = (buf[i*2+0] * vol) >> 16;
    buf[i*2+1] = (buf[i*2+1] * vol) >> 16;
    if (fadeout->work->loop_cnt >= fadeout->loopcnt) {
      fadeout->vol = (fadeout->vol * 0xffff0000ull) >> 32;
    }
  }
  return fadeout->vol;
}

struct wavesave_instance {
  struct opna opna;
  struct opna_timer timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  struct fadeout fadeout;
  struct fmplayer_file *fmfile;
  struct wavefile *wavefile;
  HWND wnd;
  HWND pbar;
  int ppos;
  HANDLE thread;
  DWORD th_exit;
  uint8_t adpcm_ram[OPNA_ADPCM_RAM_SIZE];
};

static DWORD CALLBACK thread_write(void *ptr) {
  struct wavesave_instance *inst = ptr;
  enum {
    BUFLEN = 1024,
  };
  int16_t buf[BUFLEN*2];
  for (;;) {
    if (inst->th_exit) return 0;
    memset(buf, 0, sizeof(buf));
    bool end = !fadeout_mix(&inst->fadeout, buf, BUFLEN);
    if (wavewrite_write(inst->wavefile, buf, BUFLEN) != BUFLEN) {
      break;
    }
    int newpos = 100 * inst->work.timerb_cnt / inst->work.loop_timerb_cnt;
    if (newpos != inst->ppos) {
      inst->ppos = newpos;
      PostMessage(inst->pbar, PBM_SETPOS, newpos, 0);
    }
    if (end) break;
  }
  PostMessage(inst->wnd, WM_USER, 0, 0);
  return 0;
}

static void wavesave(HWND parent,
                     struct fmplayer_file *fmfile,
                     const wchar_t *savepath) {
  struct wavefile *wavefile = 0;
  struct wavesave_instance *inst = 0;
  wavefile = wavewrite_open_w(savepath, SRATE);
  if (!wavefile) {
    MessageBox(parent, L"Cannot open output wave file", L"Error", MB_ICONSTOP);
    goto err;
  }
  inst = malloc(sizeof(*inst));
  if (!inst) {
    MessageBox(parent, L"Cannot allocate memory", L"Error", MB_ICONSTOP);
    goto err;
  }
  *inst = (struct wavesave_instance){0};
  fmplayer_init_work_opna(&inst->work, &inst->ppz8, &inst->opna, &inst->timer, inst->adpcm_ram);
  opna_ssg_set_mix(&inst->opna.ssg, fmplayer_config.ssg_mix);
  opna_ssg_set_ymf288(&inst->opna.ssg, &inst->opna.resampler, fmplayer_config.ssg_ymf288);
  ppz8_set_interpolation(&inst->ppz8, fmplayer_config.ppz8_interp);
  opna_fm_set_hires_sin(&inst->opna.fm, fmplayer_config.fm_hires_sin);
  opna_fm_set_hires_env(&inst->opna.fm, fmplayer_config.fm_hires_env);
  fmplayer_file_load(&inst->work, fmfile, LOOPCNT);
  inst->fadeout.timer = &inst->timer;
  inst->fadeout.work = &inst->work;
  inst->fadeout.vol = 1ull<<32;
  inst->fadeout.loopcnt = LOOPCNT;
  inst->fmfile = fmfile;
  inst->wavefile = wavefile;
  CreateWindow(MAKEINTATOM(g.class),
               L"Progress",
               WS_CAPTION | WS_SYSMENU,
               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
               parent, 0, GetModuleHandle(0), inst);
  return;
err:
  if (wavefile) wavewrite_close(wavefile);
  free(inst);
}

static bool on_create(HWND hwnd, const CREATESTRUCT *cs) {
  struct wavesave_instance *inst = cs->lpCreateParams;
  SetWindowLongPtr(hwnd, GWLP_USERDATA, (intptr_t)inst);
  inst->wnd = hwnd;
  RECT wr = {
    .left = 0,
    .top = 0,
    .right = 200,
    .bottom = 100,
  };
  DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
  DWORD exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  AdjustWindowRectEx(&wr, style, 0, exstyle);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, wr.right-wr.left, wr.bottom-wr.top,
                SWP_NOZORDER | SWP_NOMOVE);

  inst->pbar = CreateWindow(PROGRESS_CLASS, 0,
                            WS_CHILD | WS_VISIBLE,
                            10, 10, 180, 15,
                            hwnd, 0, cs->hInstance, 0);

  ShowWindow(hwnd, SW_SHOW);
  inst->thread = CreateThread(0, 0, thread_write, inst, 0, 0);
  return true;
}

static void on_destroy(HWND hwnd) {
  struct wavesave_instance *inst = (struct wavesave_instance *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  inst->th_exit = 1;
  WaitForSingleObject(inst->thread, INFINITE);
  fmplayer_file_free(inst->fmfile);
  wavewrite_close(inst->wavefile);
  free(inst);
}

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  case WM_USER:
    DestroyWindow(hwnd);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void wavesave_dialog(HWND parent, const wchar_t *fmpath) {
  HINSTANCE hinst = GetModuleHandle(0);
  if (!g.class) {
    WNDCLASS wc = {
      .lpfnWndProc = wndproc,
      .hInstance = hinst,
      .hIcon = LoadIcon(hinst, MAKEINTRESOURCE(1)),
      .hCursor = LoadCursor(0, IDC_ARROW),
      .hbrBackground = (HBRUSH)(COLOR_BTNFACE+1),
      .lpszClassName = L"myon_fmplayer_ym2608_wavesave_progress",
    };
    g.class = RegisterClass(&wc);
    if (!g.class) {
      MessageBox(parent, L"Cannot register wavesave window class", L"Error", MB_ICONSTOP);
      return;
    }
  }
  wchar_t path[MAX_PATH] = {0};

  OPENFILENAME ofn = {
    .lStructSize = sizeof(ofn),
    .hwndOwner = parent,
    .lpstrFilter = L"RIFF WAVE (*.wav)\0"
                    "*.wav\0\0",
    .lpstrFile = path,
    .nMaxFile = sizeof(path)/sizeof(path[0]),
    .Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
  };
  if (!GetSaveFileName(&ofn)) return;
  struct fmplayer_file *fmfile = fmplayer_file_alloc(fmpath, 0);
  if (!fmfile) {
    MessageBox(parent, L"Cannot open file", L"Error", MB_ICONSTOP);
    return;
  }
  wavesave(parent, fmfile, path);
}
