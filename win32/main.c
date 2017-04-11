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
#include "fmdsp/fmdsp.h"
#include "soundout.h"
#include "winfont.h"
#include "version.h"
#include "toneview.h"
#include "oscillo/oscillo.h"
#include "oscilloview.h"
#include "about.h"
#include "common/fmplayer_common.h"
#include "wavesave.h"

enum {
  ID_OPENFILE = 0x10,
  ID_PAUSE,
  ID_2X,
  ID_TONEVIEW,
  ID_OSCILLOVIEW,
  ID_ABOUT,
  ID_WAVESAVE,
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
  struct fmdsp fmdsp;
  uint8_t vram[PC98_W*PC98_H];
  //uint8_t *vram;
  struct fmdsp_font font;
  uint8_t fontrom[FONT_ROM_FILESIZE];
  bool font_loaded;
  uint8_t opna_adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  bool paused;
  HWND mainwnd;
  WNDPROC btn_defproc;
  HWND button_2x, button_toneview, button_oscilloview, button_about;
  bool toneview_on, oscilloview_on, about_on;
  const wchar_t *lastopenpath;
  bool fmdsp_2x;
  struct oscillodata oscillodata_audiothread[LIBOPNA_OSCILLO_TRACK_COUNT];
  UINT mmtimer;
  HBITMAP bitmap_vram;
  uint8_t *vram32;
  bool drum_loaded;
} g;

HWND g_currentdlg;

static void sound_cb(void *p, int16_t *buf, unsigned frames) {
  struct opna_timer *timer = (struct opna_timer *)p;
  ZeroMemory(buf, sizeof(int16_t)*frames*2);
  opna_timer_mix_oscillo(timer, buf, frames, g.oscillodata_audiothread);
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
}

static bool loadfontrom(void) {
  const wchar_t *path = L"font.rom";
  wchar_t exepath[MAX_PATH];
  if (GetModuleFileName(0, exepath, MAX_PATH)) {
    PathRemoveFileSpec(exepath);
    if ((lstrlen(exepath) + lstrlen(path) + 1) < MAX_PATH) {
      lstrcat(exepath, L"\\");
      lstrcat(exepath, path);
      path = exepath;
    }
  }
  HANDLE file = CreateFile(path, GENERIC_READ,
                            0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) goto err;
  DWORD filesize = GetFileSize(file, 0);
  if (filesize != FONT_ROM_FILESIZE) goto err_file;
  DWORD readbytes;
  if (!ReadFile(file, g.fontrom, FONT_ROM_FILESIZE, &readbytes, 0)
      || readbytes != FONT_ROM_FILESIZE) goto err_file;
  CloseHandle(file);
  fmdsp_font_from_font_rom(&g.font, g.fontrom);
  g.font_loaded = true;
  about_set_fontrom_loaded(true);
  return true;
err_file:
  CloseHandle(file);
err:
  return false;
}

static void loadfont(void) {
  if (loadfontrom()) return;

  if (fmdsp_font_win(&g.font)) {
    g.font_loaded = true;
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
  WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, path, -1, g.work.filename, sizeof(g.work.filename), 0, 0);
  fmplayer_file_load(&g.work, g.fmfile, 1);
  if (!g.sound) {
    g.sound = sound_init(hwnd, SRATE, SECTLEN,
                         sound_cb, &g.opna_timer);
    about_setsoundapiname(g.sound->apiname);
  }
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  if (!g.sound) goto err;
  g.sound->pause(g.sound, 0);
  g.paused = false;
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
  Button_SetCheck(g.button_2x, g.fmdsp_2x);
}

static bool proc_key(UINT vk, bool down, int repeat) {
  if (down) {
    if (VK_F1 <= vk && vk <= VK_F12) {
      if (GetKeyState(VK_CONTROL) & 0x8000U) {
        fmdsp_palette_set(&g.fmdsp, vk - VK_F1);
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
            g.sound->pause(g.sound, g.paused);
          }
          return true;
        case VK_F11:
          fmdsp_dispstyle_set(&g.fmdsp, (g.fmdsp.style+1) % FMDSP_DISPSTYLE_CNT);
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
  }
  return CallWindowProc(g.btn_defproc, hwnd, msg, wParam, lParam);
}

static void CALLBACK mmtimer_cb(UINT timerid, UINT msg,
                                DWORD_PTR userptr,
                                DWORD_PTR dw1, DWORD_PTR dw2) {
  PostMessage(g.mainwnd, WM_USER, 0, 0);
}

static bool on_create(HWND hwnd, CREATESTRUCT *cs) {
  (void)cs;
  struct bitmap_info_fmdsp {
    BITMAPINFOHEADER head;
    RGBQUAD colors[FMDSP_PALETTE_COLORS];
  } bmi = {0};
  bmi.head.biSize = sizeof(bmi.head);
  bmi.head.biWidth = PC98_W;
  bmi.head.biHeight = -PC98_H;
  bmi.head.biPlanes = 1;
  bmi.head.biBitCount = 32;
  bmi.head.biCompression = BI_RGB;
  //bmi.head.biClrUsed = FMDSP_PALETTE_COLORS;
  g.bitmap_vram = CreateDIBSection(
    0, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&g.vram32, 0, 0
  );
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
  g.btn_defproc = (WNDPROC)GetWindowLongPtr(button, GWLP_WNDPROC);
  SetWindowLongPtr(button, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(pbutton, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(wavesavebutton, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_2x, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_toneview, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_oscilloview, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_about, GWLP_WNDPROC, (intptr_t)btn_wndproc);
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
  loadfont();
  fmdsp_init(&g.fmdsp, g.font_loaded ? &g.font : 0);
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  //SetTimer(hwnd, TIMER_FMDSP, 16, 0);
  g.mmtimer = timeSetEvent(16, 16, mmtimer_cb, 0, TIME_PERIODIC);
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
  }
}

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  timeKillEvent(g.mmtimer);
  if (g.sound) g.sound->free(g.sound);
  fmplayer_file_free(g.fmfile);
  PostQuitMessage(0);
}

static void on_paint(HWND hwnd) {
  fmdsp_update(&g.fmdsp, &g.work, &g.opna, g.vram);
  fmdsp_vrampalette(&g.fmdsp, g.vram, g.vram32, PC98_W*4);
  PAINTSTRUCT ps;
  HDC dc = BeginPaint(hwnd, &ps);
  HDC mdc = CreateCompatibleDC(dc);
  SelectObject(mdc, g.bitmap_vram);
  /*
  RGBQUAD palette[FMDSP_PALETTE_COLORS];
  for (int p = 0; p < FMDSP_PALETTE_COLORS; p++) {
    palette[p].rgbRed = g.fmdsp.palette[p*3+0];
    palette[p].rgbGreen = g.fmdsp.palette[p*3+1];
    palette[p].rgbBlue = g.fmdsp.palette[p*3+2];
  }
  SetDIBColorTable(mdc, 0, FMDSP_PALETTE_COLORS, palette);
  */
  if (g.fmdsp_2x) {
    StretchBlt(dc, 0, 80, 1280, 800, mdc, 0, 0, 640, 400, SRCCOPY);
  } else {
    BitBlt(dc, 0, 80, 640, 400, mdc, 0, 0, SRCCOPY);
  }
  DeleteDC(mdc);
  EndPaint(hwnd, &ps);
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
        fmdsp_palette_set(&g.fmdsp, 9);
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
  if (activate) g_currentdlg = hwnd;
  else g_currentdlg = 0;
}

static bool on_erasebkgnd(HWND hwnd, HDC dc) {
  RECT cr;
  GetClientRect(hwnd, &cr);
  // separate fmdsp drawing area to another window and remove this hack
  cr.bottom -= 400 * (g.fmdsp_2x + 1);
  FillRect(dc, &cr, (HBRUSH)(COLOR_BTNFACE+1));
  return true;
}

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  HANDLE_MSG(hwnd, WM_COMMAND, on_command);
  HANDLE_MSG(hwnd, WM_PAINT, on_paint);
  HANDLE_MSG(hwnd, WM_ERASEBKGND, on_erasebkgnd);
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
  case WM_USER:
    {
      RECT r = {
        .left = 0,
        .top = 80,
        .right = 640,
        .bottom = 480,
      };
      if (g.fmdsp_2x) {
        r.right = 1280;
        r.bottom = 880;
      }
      InvalidateRect(hwnd, &r, FALSE);
      return 0;
    }
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

int CALLBACK wWinMain(HINSTANCE hinst, HINSTANCE hpinst,
                      wchar_t *cmdline_, int cmdshow) {
  (void)hpinst;
  (void)cmdline_;

  if (__builtin_cpu_supports("sse2")) opna_ssg_sinc_calc_func = opna_ssg_sinc_calc_sse2;
  if (__builtin_cpu_supports("ssse3")) fmdsp_vramlookup_func = fmdsp_vramlookup_ssse3;

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
    (wchar_t*)((uintptr_t)wcatom), L"FMPlayer/Win32 " WIN64STR "v" FMPLAYER_VERSION_STR,
    style,
    CW_USEDEFAULT, CW_USEDEFAULT,
    wr.right-wr.left, wr.bottom-wr.top,
    0, 0, g.hinst, 0
  );
  ShowWindow(g.mainwnd, cmdshow);

  if (argfile) {
    openfile(g.mainwnd, argfile);
  }
  timeBeginPeriod(1);
  MSG msg = {0};
  while (GetMessage(&msg, 0, 0, 0)) {
    if (!g_currentdlg || !IsDialogMessage(g_currentdlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  timeEndPeriod(1);
  return msg.wParam;
}

