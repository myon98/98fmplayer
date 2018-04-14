#include <stddef.h>
#include <stdbool.h>
#include <windows.h>
#include <shlwapi.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/fmdriver_pmd.h"
#include "common/fmplayer_file.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "soundout.h"
#include "version.h"
#include "toneview.h"
#include "oscillo/oscillo.h"
#include "oscilloview.h"
#include "about.h"
#include "common/fmplayer_common.h"
#include "common/fmplayer_drumrom.h"
#include "common/fmplayer_fontrom.h"
#include "wavesave.h"
#include "fft/fft.h"
#include "configdialog.h"
#include "fmdsp/font.h"
#include "pacc/pacc-win.h"
#include "fmdsp/fmdsp-pacc.h"

enum {
  ID_OPENFILE = 0x10,
  ID_PAUSE,
  ID_2X,
  ID_TONEVIEW,
  ID_OSCILLOVIEW,
  ID_ABOUT,
  ID_WAVESAVE,
  ID_CONFIG,
};

enum {
  WM_PACC_RESET = WM_APP,
};

#define FMPLAYER_CLASSNAME L"myon_fmplayer_ym2608_win32"
#define FMPLAYER_CDSTAG 0xFD809800UL

enum {
  TIMER_FMDSP = 1,
  SRATE = 55467,
  SECTLEN = 4096,
  PPZ8MIX = 0xa000,
};

#define ENABLE_WM_DROPFILES
// #define ENABLE_IDROPTARGET

static struct {
  HINSTANCE hinst;
  HANDLE heap;
  struct sound_state *sound;
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  struct fmplayer_file *fmfile;
  struct fmdsp_font font;
  uint8_t opna_adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  bool paused;
  HWND mainwnd;
  HWND fmdspwnd;
  ATOM fmdspwndclass;
  WNDPROC btn_defproc;
  HWND button_2x, button_toneview, button_oscilloview, button_about, button_config;
  bool toneview_on, oscilloview_on, about_on, config_on;
  const wchar_t *lastopenpath;
  bool fmdsp_2x;
  struct oscillodata oscillodata_audiothread[LIBOPNA_OSCILLO_TRACK_COUNT];
  bool drum_loaded;
  atomic_flag at_fftdata_flag;
  struct fmplayer_fft_data at_fftdata;
  struct fmplayer_fft_input_data fftdata;
  atomic_flag opna_flag;
  struct pacc_ctx *pc;
  struct pacc_vtable pacc;
  struct pacc_win_vtable pacc_win;
  struct fmdsp_pacc *fp;
} g = {
  .at_fftdata_flag = ATOMIC_FLAG_INIT,
  .opna_flag = ATOMIC_FLAG_INIT,
};

HWND g_currentdlg;

static void sound_cb(void *p, int16_t *buf, unsigned frames) {
  struct opna_timer *timer = (struct opna_timer *)p;
  ZeroMemory(buf, sizeof(int16_t)*frames*2);
  while (atomic_flag_test_and_set_explicit(&g.opna_flag, memory_order_acquire));
  opna_timer_mix_oscillo(timer, buf, frames, g.oscillodata_audiothread);
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);
  if (!atomic_flag_test_and_set_explicit(
      &toneview_g.flag, memory_order_acquire)) {
    tonedata_from_opna(&toneview_g.tonedata, &g.opna);
    atomic_flag_clear_explicit(&toneview_g.flag, memory_order_release);
  }
  if (!atomic_flag_test_and_set_explicit(
    &oscilloview_g.flag, memory_order_acquire)) {
    memcpy(oscilloview_g.oscillodata, g.oscillodata_audiothread, sizeof(oscilloview_g.oscillodata));
    atomic_flag_clear_explicit(&oscilloview_g.flag, memory_order_release);
  }
  if (!atomic_flag_test_and_set_explicit(
    &g.at_fftdata_flag, memory_order_acquire)) {
    fft_write(&g.at_fftdata, buf, frames);
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
}

static void openfile(HWND hwnd, const wchar_t *path) {
  enum fmplayer_file_error error;
  struct fmplayer_file *fmfile = fmplayer_file_alloc(path, &error);
  if (!fmfile) {
    const wchar_t *msg = L"Cannot open file: ";
    const wchar_t *errmsg = fmplayer_file_strerror_w(error);
    wchar_t *msgbuf = malloc((wcslen(msg) + wcslen(errmsg) + 1) * sizeof(wchar_t));
    if (msgbuf) {
      wcscpy(msgbuf, msg);
      wcscat(msgbuf, errmsg);
    }
    MessageBox(hwnd, msgbuf ? msgbuf : L"Cannot open file", L"Error", MB_ICONSTOP);
    free(msgbuf);
    goto err;
  }
  if (g.sound) {
    g.sound->pause(g.sound, 1);
  }
  fmplayer_file_free(g.fmfile);
  g.fmfile = fmfile;
  unsigned mask = opna_get_mask(&g.opna);
  fmplayer_init_work_opna(&g.work, &g.ppz8, &g.opna, &g.opna_timer, g.opna_adpcm_ram);
  if (!g.drum_loaded && fmplayer_drum_loaded()) {
    g.drum_loaded = true;
    about_set_adpcmrom_loaded(true);
  }
  opna_set_mask(&g.opna, mask);
  opna_ssg_set_mix(&g.opna.ssg, fmplayer_config.ssg_mix);
  opna_ssg_set_ymf288(&g.opna.ssg, &g.opna.resampler, fmplayer_config.ssg_ymf288);
  ppz8_set_interpolation(&g.ppz8, fmplayer_config.ppz8_interp);
  opna_fm_set_hires_sin(&g.opna.fm, fmplayer_config.fm_hires_sin);
  opna_fm_set_hires_env(&g.opna.fm, fmplayer_config.fm_hires_env);
  WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, path, -1, g.work.filename, sizeof(g.work.filename), 0, 0);
  fmplayer_file_load(&g.work, g.fmfile, 1);
  if (g.fmfile->filename_sjis) {
    fmdsp_pacc_set_filename_sjis(g.fp, g.fmfile->filename_sjis);
  }
  fmdsp_pacc_update_file(g.fp);
  fmdsp_pacc_comment_reset(g.fp);
  if (!g.sound) {
    g.sound = sound_init(hwnd, SRATE, SECTLEN,
                         sound_cb, &g.opna_timer);
    about_setsoundapiname(g.sound->apiname);
  }
  if (!g.sound) goto err;
  g.sound->pause(g.sound, 0);
  g.paused = false;
  g.work.paused = false;
  wchar_t *pathcpy = HeapAlloc(g.heap, 0, (lstrlen(path)+1)*sizeof(wchar_t));
  if (pathcpy) {
    lstrcpy(pathcpy, path);
  }
  if (g.lastopenpath) HeapFree(g.heap, 0, (void *)g.lastopenpath);
  g.lastopenpath = pathcpy;
  return;
err:
  fmplayer_file_free(fmfile);
}

static void openfiledialog(HWND hwnd) {
  wchar_t path[MAX_PATH] = {0};
  OPENFILENAME ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.hInstance = g.hinst;
  ofn.lpstrFilter = L"All supported files (*.m;*.m2;*.mz;*.opi;*.ovi;*.ozi;*.m26;*.m86)\0"
                     "*.m;*.m2;*.mz;*.opi;*.ovi;*.ozi;*.m26;*.m86\0"
                     "PMD files (*.m;*.m2;*.mz)\0"
                     "*.m;*.m2;*.mz\0"
                     "FMP files (*.opi;*.ovi;*.ozi)\0"
                     "*.opi;*.ovi;*.ozi\0"
                     "PLAY6 files (*.m26;*.m86)\0"
                     "*.m26;*.m86\0"
                     "All Files (*.*)\0"
                     "*.*\0\0";
  ofn.lpstrFile = path;
  ofn.nMaxFile = sizeof(path)/sizeof(path[0]);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  if (!GetOpenFileName(&ofn)) return;
  openfile(hwnd, path);
}

#ifdef ENABLE_IDROPTARGET
struct fmplayer_droptarget {
  IDropTarget idt;
  ULONG refcnt;
};

static HRESULT fmplayer_droptarget_addref(
  IDropTarget *ptr) {
  struct fmplayer_droptarget *this_ = (struct fmplayer_droptarget *)ptr;
  return ++this_->refcnt;
}

static HRESULT fmplayer_droptarget_release(
  IDropTarget *ptr) {
  struct fmplayer_droptarget *this_ = (struct fmplayer_droptarget *)ptr;
  ULONG refcnt = --this_->refcnt;
  if (!refcnt) HeapFree(g.heap, 0, this_);
  return refcnt;
}

static HRESULT fmplayer_droptarget_queryinterface(
  IDropTarget *ptr, GUID *iid, void **obj) {
  struct fmplayer_droptarget *this_ = (struct fmplayer_droptarget *)ptr;
  if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IDropTarget)) {
    *obj = ptr;
    fmplayer_droptarget_addref(ptr);
    return S_OK;
  } else {
    *obj = 0;
    return E_NOINTERFACE;
  }
}

struct fmplayer_droptarget *fmplayer_droptarget(void) {
  struct fmplayer_droptarget *fdt = HeapAlloc(g.heap, HEAP_ZERO_MEMORY, sizeof(*fdt));
  if (!fdt) return 0;
  static bool vtblinit = false;
  static IDropTargetVtbl vtbl;
  if (!vtblinit) {
    vtbl.QueryInterface = fmplayer_droptarget_queryinterface;
    vtbl.AddRef = fmplayer_droptarget_addref;
    vtbl.Release = fmplayer_droptarget_release;
    vtblinit = true;
  }
  fdt->idt.lpVtbl = &vtbl;
  fdt->refcnt = 1;
}
#endif // ENABLE_IDROPTARGET

#ifdef ENABLE_WM_DROPFILES
static void on_dropfiles(HWND hwnd, HDROP hdrop) {
  wchar_t path[MAX_PATH] = {0};
  if (DragQueryFile(hdrop, 0, path, sizeof(path)/sizeof(path[0]))) {
    openfile(hwnd, path);
  }  
  DragFinish(hdrop);
}
#endif // ENABLE_WM_DROPFILES

static void mask_set(int p, bool shift, bool control) {
  if (!control) {
    if (p >= 11) return;
    static const unsigned masktbl[11] = {
      LIBOPNA_CHAN_FM_1,
      LIBOPNA_CHAN_FM_2,
      LIBOPNA_CHAN_FM_3,
      LIBOPNA_CHAN_FM_4,
      LIBOPNA_CHAN_FM_5,
      LIBOPNA_CHAN_FM_6,
      LIBOPNA_CHAN_SSG_1,
      LIBOPNA_CHAN_SSG_2,
      LIBOPNA_CHAN_SSG_3,
      LIBOPNA_CHAN_DRUM_ALL,
      LIBOPNA_CHAN_ADPCM,
    };
    unsigned mask = masktbl[p];
    if (shift) {
      opna_set_mask(&g.opna, ~mask);
      ppz8_set_mask(&g.ppz8, -1);
    } else {
      opna_set_mask(&g.opna, opna_get_mask(&g.opna) ^ mask);
    }
  } else {
    if (p >= 8) return;
    unsigned mask = 1u<<p;
    if (shift) {
      ppz8_set_mask(&g.ppz8, ~mask);
      opna_set_mask(&g.opna, -1);
    } else {
      ppz8_set_mask(&g.ppz8, ppz8_get_mask(&g.ppz8) ^ mask);
    }
  }
}

static void toggle_2x(HWND hwnd) {
  g.fmdsp_2x ^= 1;
  RECT wr;
  wr.left = 0;
  wr.right = 640;
  wr.top = 0;
  wr.bottom = 480;
  if (g.fmdsp_2x) {
    wr.right = 1280;
    wr.bottom = 880;
  }
  DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
  DWORD exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  AdjustWindowRectEx(&wr, style, 0, exstyle);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, wr.right-wr.left, wr.bottom-wr.top,
                SWP_NOZORDER | SWP_NOMOVE);
  SetWindowPos(
      g.fmdspwnd, HWND_TOP,
      0, 80, PC98_W*(g.fmdsp_2x+1), PC98_H*(g.fmdsp_2x+1),
      SWP_NOZORDER);
  Button_SetCheck(g.button_2x, g.fmdsp_2x);
}

static bool proc_key(UINT vk, bool down, int repeat) {
  (void)repeat;
  if (down) {
    if (VK_F1 <= vk && vk <= VK_F12) {
      if (GetKeyState(VK_CONTROL) & 0x8000U) {
        fmdsp_pacc_palette(g.fp, vk - VK_F1);
        return true;
      } else {
        switch (vk) {
        case VK_F6:
          if (g.lastopenpath) {
            openfile(g.mainwnd, g.lastopenpath);
          }
          return true;
        case VK_F7:
          if (g.sound) {
            g.paused = !g.paused;
            g.work.paused = g.paused;
            g.sound->pause(g.sound, g.paused);
          }
          return true;
        case VK_F11:
          if (GetKeyState(VK_SHIFT) & 0x8000u) {
            fmdsp_pacc_set_right_mode(
                g.fp, (fmdsp_pacc_right_mode(g.fp)+1) % FMDSP_RIGHT_MODE_CNT);
          } else {
            fmdsp_pacc_set_left_mode(
                g.fp, (fmdsp_pacc_left_mode(g.fp)+1) % FMDSP_LEFT_MODE_CNT);
          }
          return true;
        case VK_F12:
          toggle_2x(g.mainwnd);
        }
      }
    } else {
      bool shift = GetKeyState(VK_SHIFT) & 0x8000U;
      bool ctrl = GetKeyState(VK_CONTROL) & 0x8000U;
      switch (vk) {
      case '1':
        mask_set(0, shift, ctrl);
        return true;
      case '2':
        mask_set(1, shift, ctrl);
        return true;
      case '3':
        mask_set(2, shift, ctrl);
        return true;
      case '4':
        mask_set(3, shift, ctrl);
        return true;
      case '5':
        mask_set(4, shift, ctrl);
        return true;
      case '6':
        mask_set(5, shift, ctrl);
        return true;
      case '7':
        mask_set(6, shift, ctrl);
        return true;
      case '8':
        mask_set(7, shift, ctrl);
        return true;
      case '9':
        mask_set(8, shift, ctrl);
        return true;
      case '0':
        mask_set(9, shift, ctrl);
        return true;
      case VK_OEM_MINUS:
        mask_set(10, shift, ctrl);
        return true;
      case VK_OEM_PLUS:
        opna_set_mask(&g.opna, ~opna_get_mask(&g.opna));
        ppz8_set_mask(&g.ppz8, ~ppz8_get_mask(&g.ppz8));
        return true;
      case VK_OEM_5:
        opna_set_mask(&g.opna, 0);
        ppz8_set_mask(&g.ppz8, 0);
        return true;
      case VK_UP:
      case VK_DOWN:
    //    MessageBox(g.mainwnd, L"A", L"B", 0);
        if (shift) {
          fmdsp_pacc_comment_scroll(g.fp, vk == VK_DOWN);
          return true;
        }
        break;
      default:
        break;
      }
    }
  }
  return false;
}

static LRESULT CALLBACK btn_wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  case WM_KEYDOWN:
  case WM_KEYUP:
    if (proc_key((UINT)wParam, msg == WM_KEYDOWN, (int)(short)LOWORD(lParam))) return 0;
    break;
  case WM_GETDLGCODE:
    if ((wParam == VK_UP) || (wParam == VK_DOWN)) {
      return DLGC_WANTMESSAGE;
    }
    break;
  }
  return CallWindowProc(g.btn_defproc, hwnd, msg, wParam, lParam);
}

static bool on_create(HWND hwnd, CREATESTRUCT *cs) {
  (void)cs;
  g.fmdspwnd = CreateWindowEx(
      0, MAKEINTATOM(g.fmdspwndclass), 0,
      WS_VISIBLE | WS_CHILD,
      0, 80,
      PC98_W, PC98_H,
      hwnd, 0, g.hinst, 0);
  HWND button = CreateWindowEx(
    0,
    L"BUTTON",
    L"&Open",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
    10, 10,
    40, 25,
    hwnd, (HMENU)ID_OPENFILE, g.hinst, 0
  );
  HWND pbutton = CreateWindowEx(
    0,
    L"BUTTON",
    L"&Pause",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD,
    55, 10,
    50, 25,
    hwnd, (HMENU)ID_PAUSE, g.hinst, 0
  );
  HWND wavesavebutton = CreateWindowEx(
    0,
    L"BUTTON",
    L"&Wave output",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD,
    110, 10,
    100, 25,
    hwnd, (HMENU)ID_WAVESAVE, g.hinst, 0
  );
  g.button_2x = CreateWindowEx(
    0,
    L"BUTTON",
    L"2&x",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_PUSHLIKE,
    215, 10,
    30, 25,
    hwnd, (HMENU)ID_2X, g.hinst, 0
  );
  g.button_toneview = CreateWindowEx(
    0,
    L"BUTTON",
    L"&Tone viewer",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_PUSHLIKE,
    250, 10,
    100, 25,
    hwnd, (HMENU)ID_TONEVIEW, g.hinst, 0
  );
  g.button_oscilloview = CreateWindowEx(
    0,
    L"BUTTON",
    L"Oscillo&view",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_PUSHLIKE,
    355, 10,
    100, 25,
    hwnd, (HMENU)ID_OSCILLOVIEW, g.hinst, 0
  );
  g.button_about = CreateWindowEx(
    0,
    L"BUTTON",
    L"&About",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_PUSHLIKE,
    580, 10,
    50, 25,
    hwnd, (HMENU)ID_ABOUT, g.hinst, 0
  );
  g.button_config = CreateWindowEx(
    0,
    L"BUTTON",
    L"&Config...",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX | BS_PUSHLIKE,
    460, 10,
    100, 25,
    hwnd, (HMENU)ID_CONFIG, g.hinst, 0
  );
  g.btn_defproc = (WNDPROC)GetWindowLongPtr(button, GWLP_WNDPROC);
  SetWindowLongPtr(button, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(pbutton, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(wavesavebutton, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_2x, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_toneview, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_oscilloview, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_about, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_config, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  NONCLIENTMETRICS ncm;
  ncm.cbSize = sizeof(ncm);
  SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  HFONT font = CreateFontIndirect(&ncm.lfMessageFont);
  SetWindowFont(button, font, TRUE);
  SetWindowFont(pbutton, font, TRUE);
  SetWindowFont(wavesavebutton, font, TRUE);
  SetWindowFont(g.button_2x, font, TRUE);
  SetWindowFont(g.button_toneview, font, TRUE);
  SetWindowFont(g.button_oscilloview, font, TRUE);
  SetWindowFont(g.button_about, font, TRUE);
  SetWindowFont(g.button_config, font, TRUE);
#ifdef ENABLE_WM_DROPFILES
  DragAcceptFiles(hwnd, TRUE);
#endif
  return true;
}

static void toneview_close_cb(void *ptr) {
  (void)ptr;
  g.toneview_on = false;
  Button_SetCheck(g.button_toneview, false);
}

static void oscilloview_close_cb(void *ptr) {
  (void)ptr;
  g.oscilloview_on = false;
  Button_SetCheck(g.button_oscilloview, false);
}

static void about_close_cb(void *ptr) {
  (void)ptr;
  g.about_on = false;
  Button_SetCheck(g.button_about, false);
}

static void configdialog_close_cb(void *ptr) {
  (void)ptr;
  g.config_on = false;
  Button_SetCheck(g.button_config, false);
}

static void configdialog_change_cb(void *ptr) {
  (void)ptr;
  while (atomic_flag_test_and_set_explicit(&g.opna_flag, memory_order_acquire));
  opna_ssg_set_mix(&g.opna.ssg, fmplayer_config.ssg_mix);
  opna_ssg_set_ymf288(&g.opna.ssg, &g.opna.resampler, fmplayer_config.ssg_ymf288);
  ppz8_set_interpolation(&g.ppz8, fmplayer_config.ppz8_interp);
  opna_fm_set_hires_sin(&g.opna.fm, fmplayer_config.fm_hires_sin);
  opna_fm_set_hires_env(&g.opna.fm, fmplayer_config.fm_hires_env);
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);
}

static void on_command(HWND hwnd, int id, HWND hwnd_c, UINT code) {
  (void)code;
  (void)hwnd_c;
  switch (id) {
  case ID_OPENFILE:
    openfiledialog(hwnd);
    break;
  case ID_PAUSE:
    if (g.sound) {
      g.paused = !g.paused;
      g.work.paused = g.paused;
      g.sound->pause(g.sound, g.paused);
    }
    break;
  case ID_2X:
    toggle_2x(hwnd);
    break;
  case ID_TONEVIEW:
    if (!g.toneview_on) {
      g.toneview_on = true;
      toneview_open(g.hinst, hwnd, toneview_close_cb, 0);
      Button_SetCheck(g.button_toneview, true);
    } else {
      g.toneview_on = false;
      toneview_close();
      Button_SetCheck(g.button_toneview, false);
    }
    break;
  case ID_OSCILLOVIEW:
    if (!g.oscilloview_on) {
      g.oscilloview_on = true;
      oscilloview_open(g.hinst, hwnd, oscilloview_close_cb, 0);
      Button_SetCheck(g.button_oscilloview, true);
    } else {
      g.oscilloview_on = false;
      oscilloview_close();
      Button_SetCheck(g.button_oscilloview, false);
    }
    break;
  case ID_ABOUT:
    if (!g.about_on) {
      g.about_on = true;
      about_open(g.hinst, hwnd, about_close_cb, 0);
      Button_SetCheck(g.button_about, true);
    } else {
      g.about_on = false;
      about_close();
      Button_SetCheck(g.button_about, false);
    }
    break;
  case ID_WAVESAVE:
    {
      if (g.lastopenpath) {
        wchar_t *path = wcsdup(g.lastopenpath);
        if (path) {
          wavesave_dialog(hwnd, path);
          free(path);
        }
      }
    }
    break;
  case ID_CONFIG:
    if (!g.config_on) {
      g.config_on = true;
      configdialog_open(g.hinst, hwnd, configdialog_close_cb, 0, configdialog_change_cb, 0);
      Button_SetCheck(g.button_config, true);
    } else {
      g.config_on = false;
      configdialog_close();
      Button_SetCheck(g.button_config, false);
    }
    break;
  }
}

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  if (g.sound) g.sound->free(g.sound);
  fmplayer_file_free(g.fmfile);
  PostQuitMessage(0);
}

static LRESULT on_copydata(HWND hwnd, HWND hwndfrom, COPYDATASTRUCT *cds) {
  (void)hwndfrom;
  if (cds->dwData != FMPLAYER_CDSTAG) return FALSE;
  openfile(hwnd, cds->lpData);
  return TRUE;
}

static void on_syskey(HWND hwnd, UINT vk, BOOL down, int repeat, UINT scan) {
  if (down) {
    if (vk == VK_F10) {
      if (GetKeyState(VK_CONTROL) & 0x8000U) {
        fmdsp_pacc_palette(g.fp, 9);
        return;
      }
    }
    FORWARD_WM_SYSKEYDOWN(hwnd, vk, repeat, scan, DefWindowProc);
  } else {
    FORWARD_WM_SYSKEYUP(hwnd, vk, repeat, scan, DefWindowProc);
  }
}

static void on_key(HWND hwnd, UINT vk, BOOL down, int repeat, UINT scan) {
  if (!proc_key(vk, down, repeat)) {
    if (down) {
      FORWARD_WM_KEYDOWN(hwnd, vk, repeat, scan, DefWindowProc);
    } else {
      FORWARD_WM_KEYUP(hwnd, vk, repeat, scan, DefWindowProc);
    }
  }
}

static void on_activate(HWND hwnd, bool activate, HWND targetwnd, WINBOOL state) {
  (void)targetwnd;
  (void)state;
  if (activate) g_currentdlg = hwnd;
  else g_currentdlg = 0;
}

static void render_cb(void *ptr) {
  (void)ptr;
  if (!atomic_flag_test_and_set_explicit(
        &g.at_fftdata_flag, memory_order_acquire)) {
    memcpy(&g.fftdata.fdata, &g.at_fftdata, sizeof(g.fftdata.fdata));
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
  fmdsp_pacc_render(g.fp);
}

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  HANDLE_MSG(hwnd, WM_COMMAND, on_command);
  case WM_COPYDATA:
    return on_copydata(hwnd, (HWND)wParam, (COPYDATASTRUCT *)lParam);
#ifdef ENABLE_WM_DROPFILES
  HANDLE_MSG(hwnd, WM_DROPFILES, on_dropfiles);
#endif // ENABLE_WM_DROPFILES
  HANDLE_MSG(hwnd, WM_KEYDOWN, on_key);
  HANDLE_MSG(hwnd, WM_KEYUP, on_key);
  HANDLE_MSG(hwnd, WM_SYSKEYDOWN, on_syskey);
  HANDLE_MSG(hwnd, WM_SYSKEYUP, on_syskey);
  HANDLE_MSG(hwnd, WM_ACTIVATE, on_activate);
  case WM_GETDLGCODE:
    if ((wParam == VK_UP) || (wParam == VK_DOWN)) {
      return DLGC_WANTMESSAGE;
    }
    break;
  case WM_PACC_RESET:
    if (g.pc) {
      g.pacc_win.renderctrl(g.pc, false);
      fmdsp_pacc_deinit(g.fp);
      g.pacc.pacc_delete(g.pc);
      g.pc = 0;
    }
    g.pc = pacc_init_d3d9(g.fmdspwnd, PC98_W, PC98_H, render_cb, 0, &g.pacc, &g.pacc_win, WM_PACC_RESET, g.mainwnd);
    if (!g.pc) {
      break;
    }
    if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) {
      break;
    }
    fmdsp_pacc_set_font16(g.fp, &g.font);
    fmdsp_pacc_set(g.fp, &g.work, &g.opna, &g.fftdata);
    g.pacc_win.renderctrl(g.pc, true);
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

static ATOM register_class(HINSTANCE hinst) {
  WNDCLASS wc = {0};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wndproc;
  wc.hInstance = hinst;
  wc.hIcon = LoadIcon(g.hinst, MAKEINTRESOURCE(1));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
  wc.lpszClassName = FMPLAYER_CLASSNAME;
  return RegisterClass(&wc);
}

static ATOM register_fmdsp_class(HINSTANCE hinst) {
  WNDCLASS wc = {0};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = DefWindowProc;
  wc.hInstance = hinst;
  wc.hCursor = LoadCursor(0, IDC_ARROW);
  wc.hbrBackground = 0;
  wc.lpszClassName = FMPLAYER_CLASSNAME "fmdsp";
  return RegisterClass(&wc);
}

int CALLBACK wWinMain(HINSTANCE hinst, HINSTANCE hpinst,
                      wchar_t *cmdline_, int cmdshow) {
  (void)hpinst;
  (void)cmdline_;

  if (__builtin_cpu_supports("sse2")) opna_ssg_sinc_calc_func = opna_ssg_sinc_calc_sse2;

  fft_init_table();
  about_set_fontrom_loaded(fmplayer_font_rom_load(&g.font));

  const wchar_t *argfile = 0;
  {
    wchar_t *cmdline = GetCommandLine();
    if (cmdline) {
      int argc;
      wchar_t **argv = CommandLineToArgvW(cmdline, &argc);
      if (argc >= 2) {
        argfile = argv[1];
      }
    }
  }

  {
    HWND otherwnd = FindWindow(FMPLAYER_CLASSNAME, 0);
    if (otherwnd) {
      if (!argfile) ExitProcess(0);
      COPYDATASTRUCT cds;
      cds.dwData = FMPLAYER_CDSTAG;
      cds.cbData = (lstrlen(argfile)+1)*2;
      cds.lpData = (void *)argfile;
      FORWARD_WM_COPYDATA(otherwnd, 0, &cds, SendMessage);
      SetForegroundWindow(otherwnd);
      ExitProcess(0);
    }
  }

  g.hinst = hinst;
  g.heap = GetProcessHeap();
  ATOM wcatom = register_class(g.hinst);
  g.fmdspwndclass = register_fmdsp_class(g.hinst);
  DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
  DWORD exStyle = 0;
  RECT wr;
  wr.left = 0;
  wr.right = 640;
  wr.top = 0;
  wr.bottom = 480;
  AdjustWindowRectEx(&wr, style, 0, exStyle);
#ifdef _WIN64
#define WIN64STR "(amd64) "
#else
#define WIN64STR ""
#endif
  g.mainwnd = CreateWindowEx(
    exStyle,
    (wchar_t*)((uintptr_t)wcatom), L"98FMPlayer/Win32 " WIN64STR "v" FMPLAYER_VERSION_STR,
    style,
    CW_USEDEFAULT, CW_USEDEFAULT,
    wr.right-wr.left, wr.bottom-wr.top,
    0, 0, g.hinst, 0
  );
  g.pc = pacc_init_d3d9(g.fmdspwnd, PC98_W, PC98_H, render_cb, 0, &g.pacc, &g.pacc_win, WM_PACC_RESET, g.mainwnd);
  if (!g.pc) {
    MessageBox(g.mainwnd, L"Error", L"Cannot initialize Direct3D", MB_ICONSTOP);
    return 0;
  }
  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) {
    MessageBox(g.mainwnd, L"Error", L"Cannot allocate fmdsp-pacc", MB_ICONSTOP);
    return 0;
  }
  ShowWindow(g.mainwnd, cmdshow);
  if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) {
    MessageBox(g.mainwnd, L"Error", L"Cannot initialize fmdsp-pacc", MB_ICONSTOP);
    return 0;
  }
  fmdsp_pacc_set_font16(g.fp, &g.font);
  fmdsp_pacc_set(g.fp, &g.work, &g.opna, &g.fftdata);
  g.pacc_win.renderctrl(g.pc, true);

  if (argfile) {
    openfile(g.mainwnd, argfile);
  }
  MSG msg = {0};
  while (GetMessage(&msg, 0, 0, 0)) {
    if (!g_currentdlg || !IsDialogMessage(g_currentdlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  if (g.pc) g.pacc_win.renderctrl(g.pc, false);
  if (g.fp) fmdsp_pacc_release(g.fp);
  if (g.pc) g.pacc.pacc_delete(g.pc);
  return msg.wParam;
}

