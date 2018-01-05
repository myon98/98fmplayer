#include "oscilloview.h"
#include <mmsystem.h>
#include <shellapi.h>
#include <windowsx.h>

enum {
  TIMER_UPDATE = 1
};

struct oscilloview oscilloview_g = {
  .flag = ATOMIC_FLAG_INIT
};

enum {
  VIEW_SAMPLES = 1024,
  VIEW_SKIP = 2,
};

static struct {
  HINSTANCE hinst;
  HWND parent;
  HWND oscilloview;
  ATOM oscilloview_class;
  struct oscillodata oscillodata[LIBOPNA_OSCILLO_TRACK_COUNT];
  UINT mmtimer;
  HPEN whitepen;
  void (*closecb)(void *ptr);
  void *cbptr;
} g;

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  g.oscilloview = 0;
  timeKillEvent(g.mmtimer);
  DeleteObject(g.whitepen);
  if (g.closecb) g.closecb(g.cbptr);
}

static void CALLBACK mmtimer_cb(UINT timerid, UINT msg,
                                DWORD_PTR userptr,
                                DWORD_PTR dw1, DWORD_PTR dw2) {
  (void)timerid;
  (void)msg;
  (void)userptr;
  (void)dw1;
  (void)dw2;
  PostMessage(g.oscilloview, WM_USER, 0, 0);
}

static bool on_create(HWND hwnd, const CREATESTRUCT *cs) {
  (void)cs;
  g.whitepen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  ShowWindow(hwnd, SW_SHOW);
  //SetTimer(hwnd, TIMER_UPDATE, 16, 0);
  g.mmtimer = timeSetEvent(16, 16, mmtimer_cb, 0, TIME_PERIODIC);
  DragAcceptFiles(hwnd, TRUE);
  return true;
}

static void draw_track(HDC dc,
                       int x, int y, int w, int h,
                       const struct oscillodata *data) {
  int start = OSCILLO_SAMPLE_COUNT - VIEW_SAMPLES;
  start -= (data->offset >> OSCILLO_OFFSET_SHIFT);
  if (start < 0) start = 0;
  MoveToEx(dc, x, y + h/2.0 - (data->buf[start] / 16384.0) * h/2, 0);
  for (int i = 0; i < (VIEW_SAMPLES / VIEW_SKIP); i++) {
    LineTo(dc, (double)x + ((i)*w)/(VIEW_SAMPLES / VIEW_SKIP), y + h/2.0 - (data->buf[start + i*VIEW_SKIP] / 16384.0) * h/2);
  }
}

static void on_paint(HWND hwnd) {
  RECT cr;
  GetClientRect(hwnd, &cr);
  PAINTSTRUCT ps;
  HDC dc = BeginPaint(hwnd, &ps);
  HDC mdc = CreateCompatibleDC(dc);
  HBITMAP bitmap = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
  SelectObject(mdc, bitmap);

  FillRect(mdc, &cr, GetStockObject(BLACK_BRUSH));
  SelectObject(mdc, g.whitepen);
  int width = cr.right / 3;
  int height = cr.bottom / 3;
  for (int x = 0; x < 3; x++) {
    for (int y = 0; y < 3; y++) {
      draw_track(mdc, x*width, y*height, width, height, &g.oscillodata[x*3+y]);
    }
  }

  BitBlt(dc, 0, 0, cr.right, cr.bottom, mdc, 0, 0, SRCCOPY);
  SelectObject(mdc, 0);
  DeleteObject(bitmap);
  DeleteDC(mdc);
  EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_DESTROY, on_destroy);
  HANDLE_MSG(hwnd, WM_CREATE, on_create);
  //HANDLE_MSG(hwnd, WM_TIMER, on_timer);
  HANDLE_MSG(hwnd, WM_PAINT, on_paint);
  case WM_ERASEBKGND:
    return 1;
  case WM_USER:
    if (!atomic_flag_test_and_set_explicit(
      &oscilloview_g.flag, memory_order_acquire)) {
      memcpy(g.oscillodata,
             oscilloview_g.oscillodata,
             sizeof(oscilloview_g.oscillodata));
      atomic_flag_clear_explicit(&oscilloview_g.flag, memory_order_release);
    }
    InvalidateRect(hwnd, 0, FALSE);
    return 0;
  case WM_DROPFILES:
    return SendMessage(g.parent, msg, wParam, lParam);
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void oscilloview_open(HINSTANCE hinst, HWND parent, void (*closecb)(void *ptr), void *cbptr) {
  g.closecb = closecb;
  g.cbptr = cbptr;
  g.hinst = hinst;
  g.parent = parent;
  if (!g.oscilloview) {
    if (!g.oscilloview_class) {
      WNDCLASS wc = {0};
      wc.style = 0;
      wc.lpfnWndProc = wndproc;
      wc.hInstance = g.hinst;
      wc.hIcon = LoadIcon(g.hinst, MAKEINTRESOURCE(1));
      wc.hCursor = LoadCursor(NULL, IDC_ARROW);
      wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = L"myon_fmplayer_ym2608_oscilloviewer";
      g.oscilloview_class = RegisterClass(&wc);
    }
    if (!g.oscilloview_class) {
      MessageBox(parent, L"Cannot register oscilloviewer class", L"Error", MB_ICONSTOP);
      return;
    }
    g.oscilloview = CreateWindowEx(0,
                                     MAKEINTATOM(g.oscilloview_class),
                                     L"FMPlayer Oscilloview",
                                     WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | WS_SIZEBOX | WS_MAXIMIZEBOX,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     parent, 0, g.hinst, 0);
  } else {
    SetForegroundWindow(g.oscilloview);
  }
}

void oscilloview_close(void) {
  if (g.oscilloview) {
    g.closecb = 0;
    DestroyWindow(g.oscilloview);
  }
}
