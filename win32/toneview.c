#include "toneview.h"
#include <stdbool.h>
#include <windowsx.h>

enum {
  TIMER_UPDATE = 1
};

enum {
  ID_COPY0 = 0x10,
  ID_COPY1,
  ID_COPY2,
  ID_COPY3,
  ID_COPY4,
  ID_COPY5,
  ID_NORMALIZE,
  ID_LIST,
};

struct toneview_g toneview_g = {
  .flag = ATOMIC_FLAG_INIT
};

static struct {
  HINSTANCE hinst;
  HWND toneviewer;
  HWND tonelabel[6], copybutton[6];
  ATOM toneviewer_class;
  struct fmplayer_tonedata tonedata;
  struct fmplayer_tonedata tonedata_n;
  struct fmplayer_tonedata tonedata_n_disp;
  char strbuf[FMPLAYER_TONEDATA_STR_SIZE];
  wchar_t strbuf_w[FMPLAYER_TONEDATA_STR_SIZE];
  enum fmplayer_tonedata_format format, format_disp;
  bool normalize;
  HFONT font;
  HFONT font_mono;
  HWND checkbox;
  HWND formatlist;
  WNDPROC static_defproc;
  void (*closecb)(void *ptr);
  void *cbptr;
} g = {
  .normalize = true,
  .format_disp = -1
};

extern HWND g_currentdlg;

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  for (int i = 0; i < 6; i++) {
    DestroyWindow(g.tonelabel[i]);
    DestroyWindow(g.copybutton[i]);
  }
  DestroyWindow(g.checkbox);
  DestroyWindow(g.formatlist);
  g.toneviewer = 0;
  if (g.closecb) {
    g.closecb(g.cbptr);
  }
}

enum {
  LIST_W = 200,
  NORMALIZE_X = 10 + LIST_W + 5,
  NORMALIZE_W = 200,
  TOP_H = 25,
  TONELABEL_X = 10,
  TONELABEL_H = 120,
  TONELABEL_W = 390,
  COPY_X = TONELABEL_X + TONELABEL_W + 5,
  COPY_W = 100,
  WIN_H = 10 + TOP_H + 5 + TONELABEL_H*6 + 5*5 + 10,
  WIN_W = 10 + TONELABEL_W + 5 + COPY_W + 10,
};

static void on_command(HWND hwnd, int id, HWND hwnd_c, UINT code) {
  if (code == BN_CLICKED && ((ID_COPY0 <= id) && (id <= ID_COPY5))) {
    int i = id - ID_COPY0;
    wchar_t buf[FMPLAYER_TONEDATA_STR_SIZE];
    GetWindowText(g.tonelabel[i], buf, FMPLAYER_TONEDATA_STR_SIZE*sizeof(wchar_t));
    HGLOBAL gmem = GlobalAlloc(GMEM_MOVEABLE, (FMPLAYER_TONEDATA_STR_SIZE+10)*sizeof(wchar_t));
    if (!gmem) return;
    wchar_t *gbuf = GlobalLock(gmem);
    wchar_t *c = buf;
    while (*c) {
      if (*c == L'\n') {
        *gbuf++ = L'\r';
      }
      *gbuf++ = *c++;
    }
    *gbuf = 0;
    GlobalUnlock(gmem);
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, gmem);
    CloseClipboard();
  } else if (id == ID_NORMALIZE && code == BN_CLICKED) {
    g.normalize ^= 1;
    Button_SetCheck(hwnd_c, g.normalize);
  } else if (id == ID_LIST && code == CBN_SELCHANGE) {
    g.format = ComboBox_GetCurSel(g.formatlist);
  }
}

static void static_on_paint(HWND hwnd) {
  PAINTSTRUCT ps;
  RECT cr;
  GetClientRect(hwnd, &cr);
  HDC hdc = BeginPaint(hwnd, &ps);
  HDC mdc = CreateCompatibleDC(hdc);
  HBITMAP bitmap = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
  SelectObject(mdc, bitmap);
  HBRUSH brush = GetSysColorBrush(COLOR_BTNFACE);
  FillRect(mdc, &cr, brush);
  CallWindowProc(g.static_defproc, hwnd, WM_PRINTCLIENT, (WPARAM)mdc, 0);
  BitBlt(hdc, 0, 0, cr.right, cr.bottom, mdc, 0, 0, SRCCOPY);
  SelectObject(mdc, 0);
  DeleteObject(bitmap);
  DeleteDC(mdc);
  EndPaint(hwnd, &ps);
}

static LRESULT static_wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  case WM_ERASEBKGND:
    return 1;
  HANDLE_MSG(hwnd, WM_PAINT, static_on_paint);
  }
  return CallWindowProc(g.static_defproc, hwnd, msg, wParam, lParam);
}

static bool on_create(HWND hwnd, const CREATESTRUCT *cs) {
  (void)cs;
  g.format_disp = -1;
  RECT wr;
  wr.left = 0;
  wr.right = WIN_W;
  wr.top = 0;
  wr.bottom = WIN_H;
  DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
  DWORD exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  AdjustWindowRectEx(&wr, style, 0, exstyle);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, wr.right-wr.left, wr.bottom-wr.top,
                SWP_NOZORDER | SWP_NOMOVE);

  if (!g.font_mono) {
    g.font_mono = CreateFont(
      16, 0, 0, 0,
      FW_NORMAL, FALSE, FALSE, FALSE,
      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, FIXED_PITCH, 0);
  }

  if (!g.font) {
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g.font = CreateFontIndirect(&ncm.lfMessageFont);
  }
  g.formatlist = CreateWindowEx(0, L"combobox", 0,
                                   WS_TABSTOP | WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                   10, 10, LIST_W, 100,
                                   hwnd, (HMENU)ID_LIST, g.hinst, 0);
  SetWindowFont(g.formatlist, g.font, TRUE);
  ComboBox_AddString(g.formatlist, L"PMD");
  ComboBox_AddString(g.formatlist, L"FMP");
  ComboBox_AddString(g.formatlist, L"VOPM");
  ComboBox_SetCurSel(g.formatlist, g.format);
  g.checkbox = CreateWindowEx(0, L"button",
                                 L"&Normalize",
                                 WS_CHILD | WS_VISIBLE | BS_CHECKBOX | WS_TABSTOP,
                                 NORMALIZE_X, 10, NORMALIZE_W, 25,
                                 hwnd, (HMENU)ID_NORMALIZE, g.hinst, 0
                                );
  Button_SetCheck(g.checkbox, g.normalize);
  SetWindowFont(g.checkbox, g.font, TRUE);
  for (int i = 0; i < 6; i++) {
    g.tonelabel[i] = CreateWindowEx(WS_EX_CLIENTEDGE, L"static",
                                 L"@ 0\n123 123",
                                 WS_VISIBLE | WS_CHILD,
                                 10, 40 + (TONELABEL_H+5)*i, TONELABEL_W, TONELABEL_H,
                                 hwnd, 0, g.hinst, 0);
    g.static_defproc = (WNDPROC)GetWindowLongPtr(g.tonelabel[i], GWLP_WNDPROC);
    SetWindowLongPtr(g.tonelabel[i], GWLP_WNDPROC, (intptr_t)static_wndproc);
    SetWindowFont(g.tonelabel[i], g.font_mono, TRUE);
    wchar_t text[] = L"Copy (& )";
    text[7] = L'1' + i;
    g.copybutton[i] = CreateWindowEx(0, L"button",
                                     text,
                                     WS_VISIBLE | WS_CHILD | WS_TABSTOP,
                                     COPY_X, 40 + (TONELABEL_H+5)*i, 100, TONELABEL_H,
                                     hwnd, (HMENU)((intptr_t)(ID_COPY0+i)), g.hinst, 0);
    SetWindowFont(g.copybutton[i], g.font, TRUE);
  }
  ShowWindow(hwnd, SW_SHOW);
  SetTimer(hwnd, TIMER_UPDATE, 16, 0);
  return true;
}

static void on_timer(HWND hwnd, UINT id) {
  (void)hwnd;
  if (id == TIMER_UPDATE) {
    if (!atomic_flag_test_and_set_explicit(
        &toneview_g.flag, memory_order_acquire)) {
      g.tonedata = toneview_g.tonedata;
      atomic_flag_clear_explicit(&toneview_g.flag, memory_order_release);
    }
    g.tonedata_n = g.tonedata;
    for (int c = 0; c < 6; c++) {
      if (g.normalize) {
        tonedata_ch_normalize_tl(&g.tonedata_n.ch[c]);
      }
      if (g.format != g.format_disp ||
          !fmplayer_tonedata_channel_isequal(&g.tonedata_n.ch[c], &g.tonedata_n_disp.ch[c])) {
        g.tonedata_n_disp.ch[c] = g.tonedata_n.ch[c];
        tonedata_ch_string(g.format, g.strbuf, &g.tonedata_n_disp.ch[c], 0);
        for (int i = 0; i < FMPLAYER_TONEDATA_STR_SIZE; i++) {
          g.strbuf_w[i] = g.strbuf[i];
        }
        DefWindowProc(g.tonelabel[c], WM_SETTEXT, 0, (LPARAM)g.strbuf_w);
        InvalidateRect(g.tonelabel[c], 0, FALSE);
      }
    }
    g.format_disp = g.format;
  }
}

static void on_activate(HWND hwnd, bool activate, HWND targetwnd, WINBOOL state) {
  (void)targetwnd;
  (void)state;
  if (activate) g_currentdlg = hwnd;
  else g_currentdlg = 0;
}

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  HANDLE_MSG(hwnd, WM_TIMER, on_timer);
  HANDLE_MSG(hwnd, WM_COMMAND, on_command);
  HANDLE_MSG(hwnd, WM_ACTIVATE, on_activate);
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void toneview_open(HINSTANCE hinst, HWND parent, void (*closecb)(void *ptr), void *cbptr) {
  g.closecb = closecb;
  g.cbptr = cbptr;
  g.hinst = hinst;
  if (!g.toneviewer) {
    if (!g.toneviewer_class) {
      WNDCLASS wc = {0};
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc = wndproc;
      wc.hInstance = hinst;
      wc.hIcon = LoadIcon(g.hinst, MAKEINTRESOURCE(1));
      wc.hCursor = LoadCursor(NULL, IDC_ARROW);
      wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = L"myon_fmplayer_ym2608_toneviewer";
      g.toneviewer_class = RegisterClass(&wc);
    }
    if (!g.toneviewer_class) {
      MessageBox(parent, L"Cannot register class", L"Error", MB_ICONSTOP);
      return;
    }
    g.toneviewer = CreateWindowEx(0,
                                  MAKEINTATOM(g.toneviewer_class),
                                  L"FMPlayer Tone Viewer",
                                  WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  parent, 0, g.hinst, 0);
  } else {
    SetForegroundWindow(g.toneviewer);
  }
}

void toneview_close(void) {
  if (g.toneviewer) {
    g.closecb = 0;
    DestroyWindow(g.toneviewer);
  }
}
