#include "about.h"
#include <windowsx.h>
#include <stdbool.h>
#include <wchar.h>
#include <stdlib.h>
#include "version.h"

enum {
  ID_OK = 0x10
};

extern HWND g_currentdlg;

static struct {
  HINSTANCE hinst;
  void (*closecb)(void *ptr);
  void *cbptr;
  HWND about;
  ATOM about_class;
  HFONT font, font_large, font_small;
  HWND static_main, static_icon, static_help, static_info, button_ok;
  wchar_t *soundapiname;
  bool adpcm_rom, font_rom;
} g;

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  DestroyWindow(g.static_main);
  DestroyWindow(g.static_info);
  DestroyWindow(g.static_help);
  DestroyWindow(g.static_icon);
  DestroyWindow(g.button_ok);
  if (g.closecb) {
    g.closecb(g.cbptr);
  }
  g.about = 0;
}

static void update_status(void) {
  static wchar_t buf[1024];
  swprintf(buf, sizeof(buf)/sizeof(buf[0]),
           L"Audio API: %ls\r\n"
           "ym2608_adpcm_rom.bin: %lsavailable\r\n"
           "font.rom: %ls\r\n"
           "SSE2 (for SIMD SSG resampling): %lsavailable",
           g.soundapiname ? g.soundapiname : L"",
           g.adpcm_rom ? L"" : L"un",
           g.font_rom ? L"available" : L"unavailable, using MS Gothic",
           __builtin_cpu_supports("sse2") ? L"" : L"un");
  SetWindowText(g.static_info, buf);
}

static bool on_create(HWND hwnd, const CREATESTRUCT *cs) {
  (void)cs;
  RECT wr = {
    .left = 0,
    .top = 0,
    .right = 480,
    .bottom = 400
  };
  DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
  DWORD exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  AdjustWindowRectEx(&wr, style, 0, exstyle);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, wr.right-wr.left, wr.bottom-wr.top,
               SWP_NOZORDER | SWP_NOMOVE);
  if (!g.font) {
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g.font_small = CreateFontIndirect(&ncm.lfMessageFont);
    LONG height = ncm.lfMessageFont.lfHeight;
    ncm.lfMessageFont.lfHeight = height * 1.2;
    ncm.lfMessageFont.lfWidth = 0;
    g.font = CreateFontIndirect(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfHeight = height * 1.5;
    ncm.lfMessageFont.lfWidth = 0;
    g.font_large = CreateFontIndirect(&ncm.lfMessageFont);
  }
  g.static_icon = CreateWindowEx(0, L"static",
                                 L"#1",
                                 WS_CHILD | WS_VISIBLE | SS_ICON,
                                 100, 10, 32, 32,
                                 hwnd, 0, g.hinst, 0);
#ifdef _WIN64
#define ABOUT_ARCH "(amd64)"
#else
#define ABOUT_ARCH "(i586)"
#endif
  g.static_info = CreateWindowEx(0, L"static",
                                 L"",
                                 WS_CHILD | WS_VISIBLE,
                                 75, 50, 400, 110,
                                 hwnd, 0, g.hinst, 0);
  SetWindowFont(g.static_info, g.font, TRUE);
  update_status();
  g.static_help = CreateWindowEx(0, L"static",
                                 L"F11: toggle track display page\r\n"
                                 "Shift+F11: toggle right display page\r\n"
                                 "F12: display FMDSP at 2x\r\n"
                                 "Ctrl+F1-F10: FMDSP color palette\r\n\r\n"
                                 "1-9,0,-: toggle track mask (Ctrl: PPZ8)\r\n"
                                 "Shift+1-9,0,-: track solo\r\n"
                                 "=: invert all mask\r\n"
                                 "\\: all tracks on",
                                 WS_CHILD | WS_VISIBLE,
                                 75, 150, 400, 215,
                                 hwnd, 0, g.hinst, 0);
  SetWindowFont(g.static_help, g.font, TRUE);
  g.static_main = CreateWindowEx(0, L"static",
                                 L"98FMPlayer/Win32 " ABOUT_ARCH " v" FMPLAYER_VERSION_STR,
                                 WS_CHILD | WS_VISIBLE,
                                 150, 10, 400, 40,
                                 hwnd, 0, g.hinst, 0);
  SetWindowFont(g.static_main, g.font_large, TRUE);
  g.button_ok = CreateWindowEx(0, L"button",
                               L"&OK",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                               200, 370, 80, 25,
                               hwnd, (HMENU)ID_OK, g.hinst, 0);
  SetWindowFont(g.button_ok, g.font_small, TRUE);
  ShowWindow(hwnd, SW_SHOW);
  return true;
}

static void on_activate(HWND hwnd, bool activate, HWND targetwnd, WINBOOL state) {
  (void)targetwnd;
  (void)state;
  if (activate) g_currentdlg = hwnd;
  else g_currentdlg = 0;
}

static void on_command(HWND hwnd, int id, HWND hwnd_c, UINT code) {
  (void)hwnd_c;
  if (code == BN_CLICKED && (id == ID_OK)) {
    DestroyWindow(hwnd);
  }
}

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  HANDLE_MSG(hwnd, WM_ACTIVATE, on_activate);
  HANDLE_MSG(hwnd, WM_COMMAND, on_command);
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void about_open(HINSTANCE hinst, HWND parent, void (*closecb)(void *ptr), void *cbptr) {
  g.closecb = closecb;
  g.cbptr = cbptr;
  g.hinst = hinst;
  if (!g.about) {
    if (!g.about_class) {
      WNDCLASS wc = {0};
      wc.lpfnWndProc = wndproc;
      wc.hInstance = g.hinst;
      wc.hIcon = LoadIcon(g.hinst, MAKEINTRESOURCE(1));
      wc.hCursor = LoadCursor(0, IDC_ARROW);
      wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = L"myon_fmplayer_ym2608_about";
      g.about_class = RegisterClass(&wc);
    }
    if (!g.about_class) {
      MessageBox(parent, L"Cannot register class", L"Error", MB_ICONSTOP);
      return;
    }
    g.about = CreateWindowEx(0,
                             MAKEINTATOM(g.about_class),
                             L"About",
                             WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             parent, 0, g.hinst, 0);
  } else {
    SetForegroundWindow(g.about);
  }
}

void about_setsoundapiname(const wchar_t *apiname) {
  free(g.soundapiname);
  g.soundapiname = wcsdup(apiname);
  if (g.about) update_status();
}

void about_set_fontrom_loaded(bool loaded) {
  g.font_rom = loaded;
  if (g.about) update_status();
}

void about_set_adpcmrom_loaded(bool loaded) {
  g.adpcm_rom = loaded;
  if (g.about) update_status();
}

void about_close(void) {
  if (g.about) {
    g.closecb = 0;
    DestroyWindow(g.about);
  }
}
