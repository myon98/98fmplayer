#include <stddef.h>
#include <stdbool.h>
#include <windows.h>
#include <shlwapi.h>
#include <windowsx.h>
#include <commctrl.h>

#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/fmdriver_pmd.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdsp/fmdsp.h"
#include "soundout.h"
#include "winfont.h"
#include "version.h"
#include "toneview.h"

enum {
  ID_OPENFILE = 0x10,
  ID_PAUSE,
  ID_2X,
  ID_TONEVIEW,
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

union drivers {
  struct driver_pmd pmd;
  struct driver_fmp fmp;
};

static struct {
  HINSTANCE hinst;
  HANDLE heap;
  struct sound_state *sound;
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  union drivers *driver;
  uint8_t *data;
  struct fmdsp fmdsp;
  uint8_t vram[PC98_W*PC98_H];
  struct fmdsp_font font;
  uint8_t fontrom[FONT_ROM_FILESIZE];
  bool font_loaded;
  void *drum_rom;
  uint8_t opna_adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  void *ppz8_buf;
  bool paused;
  HWND mainwnd;
  WNDPROC btn_defproc;
  HWND driverinfo;
  HWND button_2x;
  const wchar_t *lastopenpath;
  bool fmdsp_2x;
} g;

HWND g_currentdlg;

static void opna_int_cb(void *userptr) {
  struct fmdriver_work *work = (struct fmdriver_work *)userptr;
  work->driver_opna_interrupt(work);
}

static void opna_mix_cb(void *userptr, int16_t *buf, unsigned samples) {
  struct ppz8 *ppz8 = (struct ppz8 *)userptr;
  ppz8_mix(ppz8, buf, samples);
}

static void opna_writereg_libopna(struct fmdriver_work *work, unsigned addr, unsigned data) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  opna_timer_writereg(timer, addr, data);
}

static unsigned opna_readreg_libopna(struct fmdriver_work *work, unsigned addr) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  return opna_readreg(&g.opna, addr);
}

static uint8_t opna_status_libopna(struct fmdriver_work *work, bool a1) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  uint8_t status = opna_timer_status(timer);
  if (!a1) {
    status &= 0x83;
  }
  return status;
}

static void sound_cb(void *p, int16_t *buf, unsigned frames) {
  struct opna_timer *timer = (struct opna_timer *)p;
  ZeroMemory(buf, sizeof(int16_t)*frames*2);
  opna_timer_mix(timer, buf, frames);
  if (!atomic_flag_test_and_set_explicit(
      &toneview_g.flag, memory_order_acquire)) {
    tonedata_from_opna(&toneview_g.tonedata, &g.opna);
    atomic_flag_clear_explicit(&toneview_g.flag, memory_order_release);
  }  
}

static void on_timer(HWND hwnd, UINT id) {
  if (id == TIMER_FMDSP) {
    InvalidateRect(hwnd, 0, FALSE);
  }
}

static HANDLE pvisearch(const wchar_t *filename, const char *pviname_a) {
  enum {
    WPVINAMELEN = 8*2+1+3+1,
  };
  wchar_t pviname[WPVINAMELEN];
  wchar_t pvipath[MAX_PATH];
  if (MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
                          pviname_a, -1, pviname, WPVINAMELEN) == 0) {
    return INVALID_HANDLE_VALUE;
  }
  lstrcat(pviname, L".PVI");
  if (lstrlen(filename) >= MAX_PATH) return INVALID_HANDLE_VALUE;
  lstrcpy(pvipath, filename);
  PathRemoveFileSpec(pvipath);
  if (lstrlen(pvipath) + lstrlen(pviname) + 1 >= MAX_PATH) {
    return INVALID_HANDLE_VALUE;
  }
  lstrcat(pvipath, L"\\");
  lstrcat(pvipath, pviname);
  return CreateFile(pvipath, GENERIC_READ,
                            0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
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

static void loadrom(void) {
  const wchar_t *path = L"ym2608_adpcm_rom.bin";
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
  if (filesize != OPNA_ROM_SIZE) goto err_file;
  void *buf = HeapAlloc(g.heap, 0, OPNA_ROM_SIZE);
  if (!buf) goto err_file;
  DWORD readbytes;
  if (!ReadFile(file, buf, OPNA_ROM_SIZE, &readbytes, 0)
      || readbytes != OPNA_ROM_SIZE) goto err_buf;
  CloseHandle(file);
  g.drum_rom = buf;
  return;
err_buf:
  HeapFree(g.heap, 0, buf);
err_file:
  CloseHandle(file);
err:
  return;
}

static bool loadpvi(struct fmdriver_work *work,
                    struct driver_fmp *fmp,
                    const wchar_t *filename) {
  if (!fmp->pvi_name[0]) return true;
  HANDLE pvifile = pvisearch(filename, fmp->pvi_name);
  if (pvifile == INVALID_HANDLE_VALUE) goto err;
  DWORD filesize = GetFileSize(pvifile, 0);
  if (filesize == INVALID_FILE_SIZE) goto err_file;
  void *data = HeapAlloc(g.heap, 0, filesize);
  if (!data) goto err_file;
  DWORD readbytes;
  if (!ReadFile(pvifile, data, filesize, &readbytes, 0)
      || readbytes != filesize) goto err_data;
  if (!fmp_adpcm_load(work, data, filesize)) goto err_data;
  HeapFree(g.heap, 0, data);
  CloseHandle(pvifile);
  return true;
err_data:
  HeapFree(g.heap, 0, data);
err_file:
  CloseHandle(pvifile);
err:
  return false;
}

static bool loadppzpvi(struct fmdriver_work *work,
                    struct driver_fmp *fmp,
                    const wchar_t *filename) {
  if (!fmp->ppz_name[0]) return true;
  HANDLE pvifile = pvisearch(filename, fmp->ppz_name);
  if (pvifile == INVALID_HANDLE_VALUE) goto err;
  DWORD filesize = GetFileSize(pvifile, 0);
  if (filesize == INVALID_FILE_SIZE) goto err_file;
  void *data = HeapAlloc(g.heap, 0, filesize);
  if (!data) goto err_file;
  void *buf = HeapAlloc(g.heap, 0, ppz8_pvi_decodebuf_samples(filesize) * sizeof(int16_t));
  if (!buf) goto err_data;
  DWORD readbytes;
  if (!ReadFile(pvifile, data, filesize, &readbytes, 0)
      || readbytes != filesize) goto err_buf;
  if (!ppz8_pvi_load(work->ppz8, 0, data, filesize, buf)) goto err_buf;
  if (g.ppz8_buf) HeapFree(g.heap, 0, g.ppz8_buf);
  g.ppz8_buf = buf;
  HeapFree(g.heap, 0, data);
  CloseHandle(pvifile);
  return true;
err_buf:
  HeapFree(g.heap, 0, buf);
err_data:
  HeapFree(g.heap, 0, data);
err_file:
  CloseHandle(pvifile);
err:
  return false;
}

static void openfile(HWND hwnd, const wchar_t *path) {
  enum {
    DRIVER_PMD,
    DRIVER_FMP
  } driver_type;
  HANDLE file = CreateFile(path, GENERIC_READ,
                            0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    MessageBox(hwnd, L"Cannot open file", L"Error", MB_ICONSTOP);
    return;
  }
  LARGE_INTEGER li;
  if (!GetFileSizeEx(file, &li)) {
    MessageBox(hwnd, L"Cannot open file", L"Error", MB_ICONSTOP);
    goto err_file;
  }
  if (li.QuadPart > 0xffff) {
    MessageBox(hwnd, L"Invalid File (Filesize too large)", L"Error", MB_ICONSTOP);
    goto err_file;
  }
  void *data = HeapAlloc(g.heap, 0, li.QuadPart);
  if (!data) {
    MessageBox(hwnd, L"Cannot allocate memory for file", L"Error", MB_ICONSTOP);
    goto err_file;
  }
  DWORD readbytes;
  if (!ReadFile(file, data, li.QuadPart, &readbytes, 0) || readbytes != li.QuadPart) {
    MessageBox(hwnd, L"Cannot read file", L"Error", MB_ICONSTOP);
    goto err_data;
  }
  union drivers *driver = HeapAlloc(g.heap, HEAP_ZERO_MEMORY, sizeof(*driver));
  if (!driver) {
    MessageBox(hwnd, L"Cannot allocate memory for fmp", L"Error", MB_ICONSTOP);
    goto err_data;
  }
  if (fmp_load(&driver->fmp, data, li.QuadPart)) {
    driver_type = DRIVER_FMP;
  } else {
    ZeroMemory(driver, sizeof(*driver));
    if (pmd_load(&driver->pmd, data, li.QuadPart)) {
      driver_type = DRIVER_PMD;
    } else {
      MessageBox(hwnd, L"Invalid File (not FMP or PMD)", L"Error", MB_ICONSTOP);
      goto err_driver;
    }
  }
  if (g.sound) {
    g.sound->pause(g.sound, 1);
  }
  if (g.driver) HeapFree(g.heap, 0, g.driver);
  g.driver = driver;
  if (g.data) HeapFree(g.heap, 0, g.data);
  g.data = data;
  unsigned mask = opna_get_mask(&g.opna);
  opna_reset(&g.opna);
  opna_set_mask(&g.opna, mask);
  if (g.drum_rom) opna_drum_set_rom(&g.opna.drum, g.drum_rom);
  opna_adpcm_set_ram_256k(&g.opna.adpcm, g.opna_adpcm_ram);
  opna_timer_reset(&g.opna_timer, &g.opna);
  ppz8_init(&g.ppz8, SRATE, PPZ8MIX);
  ZeroMemory(&g.work, sizeof(g.work));
  g.work.opna_writereg = opna_writereg_libopna;
  g.work.opna_readreg = opna_readreg_libopna;
  g.work.opna_status = opna_status_libopna;
  g.work.opna = &g.opna_timer;
  g.work.ppz8 = &g.ppz8;
  g.work.ppz8_functbl = &ppz8_functbl;
  WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, path, -1, g.work.filename, sizeof(g.work.filename), 0, 0);
  opna_timer_set_int_callback(&g.opna_timer, opna_int_cb, &g.work);
  opna_timer_set_mix_callback(&g.opna_timer, opna_mix_cb, &g.ppz8);
  if (driver_type == DRIVER_PMD) {
    pmd_init(&g.work, &g.driver->pmd);
  } else {
    fmp_init(&g.work, &g.driver->fmp);
    loadpvi(&g.work, &g.driver->fmp, path);
    loadppzpvi(&g.work, &g.driver->fmp, path);
  }
  if (!g.sound) {
    g.sound = sound_init(hwnd, SRATE, SECTLEN,
                         sound_cb, &g.opna_timer);
    SetWindowText(g.driverinfo, g.sound->apiname);
  }
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  if (!g.sound) goto err_driver;
  g.sound->pause(g.sound, 0);
  g.paused = false;
  CloseHandle(file);
  wchar_t *pathcpy = HeapAlloc(g.heap, 0, (lstrlen(path)+1)*sizeof(wchar_t));
  if (pathcpy) {
    lstrcpy(pathcpy, path);
  }
  if (g.lastopenpath) HeapFree(g.heap, 0, (void *)g.lastopenpath);
  g.lastopenpath = pathcpy;
  return;
err_driver:
  HeapFree(g.heap, 0, driver);
err_data:
  HeapFree(g.heap, 0, data);
err_file:
  CloseHandle(file);
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

static void mask_set(unsigned mask, bool shift) {
  if (shift) {
    opna_set_mask(&g.opna, ~mask);
  } else {
    opna_set_mask(&g.opna, opna_get_mask(&g.opna) ^ mask);
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
      switch (vk) {
      case '1':
        mask_set(LIBOPNA_CHAN_FM_1, shift);
        return true;
      case '2':
        mask_set(LIBOPNA_CHAN_FM_2, shift);
        return true;
      case '3':
        mask_set(LIBOPNA_CHAN_FM_3, shift);
        return true;
      case '4':
        mask_set(LIBOPNA_CHAN_FM_4, shift);
        return true;
      case '5':
        mask_set(LIBOPNA_CHAN_FM_5, shift);
        return true;
      case '6':
        mask_set(LIBOPNA_CHAN_FM_6, shift);
        return true;
      case '7':
        mask_set(LIBOPNA_CHAN_SSG_1, shift);
        return true;
      case '8':
        mask_set(LIBOPNA_CHAN_SSG_2, shift);
        return true;
      case '9':
        mask_set(LIBOPNA_CHAN_SSG_3, shift);
        return true;
      case '0':
        mask_set(LIBOPNA_CHAN_DRUM_ALL, shift);
        return true;
      case VK_OEM_MINUS:
        mask_set(LIBOPNA_CHAN_ADPCM, shift);
        return true;
      case VK_OEM_PLUS:
        opna_set_mask(&g.opna, ~opna_get_mask(&g.opna));
        return true;
      case VK_OEM_5:
        opna_set_mask(&g.opna, 0);
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

static bool on_create(HWND hwnd, CREATESTRUCT *cs) {
  (void)cs;
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
  g.driverinfo = CreateWindowEx(
    0,
    L"STATIC",
    L"",
    WS_VISIBLE | WS_CHILD,
    110, 15,
    100, 25,
    hwnd, 0, g.hinst, 0
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
  HWND button_toneview = CreateWindowEx(
    0,
    L"BUTTON",
    L"Tone &viewer",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD,
    250, 10,
    100, 25,
    hwnd, (HMENU)ID_TONEVIEW, g.hinst, 0
  );
  g.btn_defproc = (WNDPROC)GetWindowLongPtr(button, GWLP_WNDPROC);
  SetWindowLongPtr(button, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(pbutton, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(g.button_2x, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  SetWindowLongPtr(button_toneview, GWLP_WNDPROC, (intptr_t)btn_wndproc);
  NONCLIENTMETRICS ncm;
  ncm.cbSize = sizeof(ncm);
  SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  HFONT font = CreateFontIndirect(&ncm.lfMessageFont);
  SetWindowFont(button, font, TRUE);
  SetWindowFont(pbutton, font, TRUE);
  SetWindowFont(g.driverinfo, font, TRUE);
  SetWindowFont(g.button_2x, font, TRUE);
  SetWindowFont(button_toneview, font, TRUE);
  loadrom();
  loadfont();
  fmdsp_init(&g.fmdsp, g.font_loaded ? &g.font : 0);
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  SetTimer(hwnd, TIMER_FMDSP, 16, 0);
#ifdef ENABLE_WM_DROPFILES
  DragAcceptFiles(hwnd, TRUE);
#endif
  return true;
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
    show_toneview(g.hinst, hwnd);
    break;
  }
}

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  if (g.sound) g.sound->free(g.sound);
  if (g.driver) HeapFree(g.heap, 0, g.driver);
  if (g.data) HeapFree(g.heap, 0, g.data);
  if (g.drum_rom) HeapFree(g.heap, 0, g.drum_rom);
  if (g.ppz8_buf) HeapFree(g.heap, 0, g.ppz8_buf);
  PostQuitMessage(0);
}

static void on_paint(HWND hwnd) {
  fmdsp_update(&g.fmdsp, &g.work, &g.opna, g.vram);
  PAINTSTRUCT ps;
  static BITMAPINFO *bi = 0;
  if (!bi) {
    bi = HeapAlloc(g.heap, HEAP_ZERO_MEMORY,
              sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*FMDSP_PALETTE_COLORS);
    if (!bi) return;
    bi->bmiHeader.biSize = sizeof(bi->bmiHeader);
    bi->bmiHeader.biWidth = PC98_W;
    bi->bmiHeader.biHeight = -PC98_H;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = 8;
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biClrUsed = FMDSP_PALETTE_COLORS;
  }
  for (int p = 0; p < FMDSP_PALETTE_COLORS; p++) {
    bi->bmiColors[p].rgbRed = g.fmdsp.palette[p*3+0];
    bi->bmiColors[p].rgbGreen = g.fmdsp.palette[p*3+1];
    bi->bmiColors[p].rgbBlue = g.fmdsp.palette[p*3+2];
  }
  HDC dc = BeginPaint(hwnd, &ps);
  HDC mdc = CreateCompatibleDC(dc);
  HBITMAP bitmap = CreateDIBitmap(
    dc,
    &bi->bmiHeader, CBM_INIT,
    g.vram,
    bi, DIB_RGB_COLORS);
  SelectObject(mdc, bitmap);
  if (g.fmdsp_2x) {
    StretchBlt(dc, 0, 80, 1280, 800, mdc, 0, 0, 640, 400, SRCCOPY);
  } else {
    BitBlt(dc, 0, 80, 640, 400, mdc, 0, 0, SRCCOPY);
  }
  DeleteDC(mdc);
  DeleteObject(bitmap);
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

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  HANDLE_MSG(hwnd, WM_COMMAND, on_command);
  HANDLE_MSG(hwnd, WM_PAINT, on_paint);
  HANDLE_MSG(hwnd, WM_TIMER, on_timer);
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
  DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  DWORD exStyle = 0;
  RECT wr;
  wr.left = 0;
  wr.right = 640;
  wr.top = 0;
  wr.bottom = 480;
  AdjustWindowRectEx(&wr, style, 0, exStyle);
  g.mainwnd = CreateWindowEx(
    exStyle,
    (wchar_t*)((uintptr_t)wcatom), L"FMPlayer/Win32 v" FMPLAYER_VERSION_STR,
    style,
    CW_USEDEFAULT, CW_USEDEFAULT,
    wr.right-wr.left, wr.bottom-wr.top,
    0, 0, g.hinst, 0
  );
  ShowWindow(g.mainwnd, cmdshow);

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
  return msg.wParam;
}

