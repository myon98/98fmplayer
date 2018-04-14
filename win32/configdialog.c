#include "configdialog.h"
#include <windowsx.h>
#include <commctrl.h>
#include <math.h>
#include <wchar.h>
#include <stdlib.h>

static double mix_to_db(uint32_t mix) {
  return 20.0 * log10((double)mix / 0x10000);
}

static uint32_t db_to_mix(double db) {
  return round(pow(10.0, db / 20.0) * 0x10000);
}

static struct {
  HINSTANCE hinst;
  HWND configwnd;
  ATOM config_class;
  HFONT font;
  void (*closecb)(void *);
  void *closecbptr;
  void (*changecb)(void *);
  void *changecbptr;
  HWND group_fm, group_ssg, group_ppz8;
  HWND check_fm_hires_sin;
  HWND check_fm_hires_env;
  HWND radio_ssg_opna;
  HWND radio_ssg_ymf288;
  HWND static_volume_offset, static_volume_info;
  HWND edit_ssg_mix;
  HWND radio_ppz8_none, radio_ppz8_linear, radio_ppz8_sinc;
  HWND updown_ssg_mix;
  WNDPROC groupbox_defproc;
  bool edit_ssg_mix_set;
  double ssg_mix_db;
} g;

enum {
  WIN_W = 500,
  WIN_H = 370,
  GROUP_X = 5,
  GROUP_FM_Y = 5,
  GROUP_W = 490,
  GROUP_FM_H = 75,
  BOX_X = 15,
  CHECK_FM_HIRES_SIN_Y = GROUP_FM_Y + 20,
  BOX_W = 450,
  CHECK_H = 25,
  CHECK_FM_HIRES_ENV_Y = CHECK_FM_HIRES_SIN_Y + CHECK_H,
  GROUP_SSG_Y = GROUP_FM_Y+GROUP_FM_H+5,
  GROUP_SSG_H = 170,
  RADIO_SSG_OPNA_Y = GROUP_SSG_Y + 20,
  STATIC_VOLUME_OFFSET_X = 25,
  STATIC_VOLUME_OFFSET_Y = RADIO_SSG_OPNA_Y + CHECK_H + 5,
  STATIC_VOLUME_OFFSET_W = 110,
  STATIC_VOLUME_INFO_X = 35,
  STATIC_VOLUME_INFO_Y = STATIC_VOLUME_OFFSET_Y + CHECK_H + 5,
  EDIT_SSG_MIX_X = 150,
  EDIT_SSG_MIX_Y = STATIC_VOLUME_OFFSET_Y - 5,
  EDIT_SSG_MIX_W = 100,
  EDIT_SSG_MIX_H = 25,
  RADIO_SSG_YMF288_Y = STATIC_VOLUME_INFO_Y + (EDIT_SSG_MIX_H+5)*2,
  GROUP_PPZ8_Y = GROUP_SSG_Y+GROUP_SSG_H+5,
  GROUP_PPZ8_H = 100,
  RADIO_PPZ8_NONE_Y = GROUP_PPZ8_Y + 20,
  RADIO_PPZ8_LINEAR_Y = RADIO_PPZ8_NONE_Y + CHECK_H,
  RADIO_PPZ8_SINC_Y = RADIO_PPZ8_LINEAR_Y + CHECK_H,
};

enum {
  ID_CHECK_FM_HIRES_SIN = 0x10,
  ID_CHECK_FM_HIRES_ENV,
  ID_RADIO_SSG_OPNA,
  ID_RADIO_SSG_YMF288,
  ID_EDIT_SSG_MIX,
  ID_RADIO_PPZ8_NONE,
  ID_RADIO_PPZ8_LINEAR,
  ID_RADIO_PPZ8_SINC,
  ID_UPDOWN_SSG_MIX,
};

struct fmplayer_config fmplayer_config = {
  .ssg_mix = 0x10000,
  .ppz8_interp = PPZ8_INTERP_SINC,
};

extern HWND g_currentdlg;

static bool groupbox_on_erasebkgnd(HWND hwnd, HDC hdc) {
  RECT cr;
  GetClientRect(hwnd, &cr);
  FillRect(hdc, &cr, (HBRUSH)(COLOR_BTNFACE+1));
  return true;
}

static LRESULT groupbox_wndproc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
) {
  switch (msg) {
  HANDLE_MSG(hwnd, WM_ERASEBKGND, groupbox_on_erasebkgnd);
  }
  return CallWindowProc(g.groupbox_defproc, hwnd, msg, wParam, lParam);
}

static void on_destroy(HWND hwnd) {
  (void)hwnd;
  DestroyWindow(g.radio_ppz8_none);
  DestroyWindow(g.radio_ppz8_linear);
  DestroyWindow(g.radio_ppz8_sinc);
  DestroyWindow(g.radio_ssg_ymf288);
  DestroyWindow(g.updown_ssg_mix);
  DestroyWindow(g.edit_ssg_mix);
  DestroyWindow(g.static_volume_info);
  DestroyWindow(g.static_volume_offset);
  DestroyWindow(g.radio_ssg_opna);
  DestroyWindow(g.check_fm_hires_sin);
  DestroyWindow(g.check_fm_hires_env);
  DestroyWindow(g.group_fm);
  DestroyWindow(g.group_ssg);
  DestroyWindow(g.group_ppz8);
  g.configwnd = 0;
  if (g.closecb) g.closecb(g.closecbptr);
}

static void update_ssg_mix(void) {
  wchar_t buf[10];
  swprintf(buf, sizeof(buf)/sizeof(buf[0]),
      L"%2.2f", g.ssg_mix_db);
  Edit_SetText(g.edit_ssg_mix, buf);
}

static void on_command(HWND hwnd, int id, HWND hwnd_c, UINT code) {
  (void)hwnd;
  (void)hwnd_c;
  switch (id) {
  case ID_CHECK_FM_HIRES_SIN:
    fmplayer_config.fm_hires_sin = Button_GetCheck(g.check_fm_hires_sin);
    break;
  case ID_CHECK_FM_HIRES_ENV:
    fmplayer_config.fm_hires_env = Button_GetCheck(g.check_fm_hires_env);
    break;
  case ID_RADIO_SSG_OPNA:
    Button_SetCheck(g.radio_ssg_opna, true);
    Button_SetCheck(g.radio_ssg_ymf288, false);
    fmplayer_config.ssg_ymf288 = false;
    Edit_Enable(g.edit_ssg_mix, true);
    EnableWindow(g.updown_ssg_mix, true);
    break;
  case ID_RADIO_SSG_YMF288:
    Button_SetCheck(g.radio_ssg_opna, false);
    Button_SetCheck(g.radio_ssg_ymf288, true);
    fmplayer_config.ssg_ymf288 = true;
    Edit_Enable(g.edit_ssg_mix, false);
    EnableWindow(g.updown_ssg_mix, false);
    break;
  case ID_RADIO_PPZ8_NONE:
    Button_SetCheck(g.radio_ppz8_none, true);
    Button_SetCheck(g.radio_ppz8_linear, false);
    Button_SetCheck(g.radio_ppz8_sinc, false);
    fmplayer_config.ppz8_interp = PPZ8_INTERP_NONE;
    break;
  case ID_RADIO_PPZ8_LINEAR:
    Button_SetCheck(g.radio_ppz8_none, false);
    Button_SetCheck(g.radio_ppz8_linear, true);
    Button_SetCheck(g.radio_ppz8_sinc, false);
    fmplayer_config.ppz8_interp = PPZ8_INTERP_LINEAR;
    break;
  case ID_RADIO_PPZ8_SINC:
    Button_SetCheck(g.radio_ppz8_none, false);
    Button_SetCheck(g.radio_ppz8_linear, false);
    Button_SetCheck(g.radio_ppz8_sinc, true);
    fmplayer_config.ppz8_interp = PPZ8_INTERP_SINC;
    break;
  case ID_EDIT_SSG_MIX:
    if (code == EN_KILLFOCUS) {
      int len = Edit_GetTextLength(g.edit_ssg_mix) + 1;
      if (len) {
        wchar_t *buf = malloc(len * sizeof(buf[0]));
        if (buf) {
          Edit_GetText(g.edit_ssg_mix, buf, len);
          wchar_t *endp;
          g.ssg_mix_db = wcstod(buf, &endp);
          if (buf == endp || *endp) {
            g.ssg_mix_db = wcstol(buf, &endp, 0);
          }
          if (g.ssg_mix_db < -18.0) g.ssg_mix_db = -18.0;
          if (g.ssg_mix_db > 18.0) g.ssg_mix_db = 18.0;
          update_ssg_mix();
          fmplayer_config.ssg_mix = db_to_mix(g.ssg_mix_db);
          free(buf);
        }
      }
    }
    break;
  }
  g.changecb(g.changecbptr);
}

static bool on_create(HWND hwnd, const CREATESTRUCT *cs) {
  (void)cs;
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
  if (!g.font) {
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g.font = CreateFontIndirect(&ncm.lfMessageFont);
  }
  
  g.group_fm = CreateWindowEx(
    0, L"button", L"FM",
    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
    GROUP_X, GROUP_FM_Y,
    GROUP_W, GROUP_FM_H,
    hwnd, 0, g.hinst, 0);
  g.group_ssg = CreateWindowEx(
    0, L"button", L"SSG (249600 Hz to 55467 Hz resampling + DC output)",
    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
    GROUP_X, GROUP_SSG_Y,
    GROUP_W, GROUP_SSG_H,
    hwnd, 0, g.hinst, 0);
  g.group_ppz8 = CreateWindowEx(
    0, L"button", L"PPZ8 interpolation",
    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
    GROUP_X, GROUP_PPZ8_Y,
    GROUP_W, GROUP_PPZ8_H,
    hwnd, 0, g.hinst, 0);
  g.check_fm_hires_sin = CreateWindowEx(
    0, L"button", L"Enable higher resolution sine table",
    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
    BOX_X, CHECK_FM_HIRES_SIN_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_CHECK_FM_HIRES_SIN, g.hinst, 0);
  g.check_fm_hires_env = CreateWindowEx(
    0, L"button", L"Enable higher resolution envelope",
    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
    BOX_X, CHECK_FM_HIRES_ENV_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_CHECK_FM_HIRES_ENV, g.hinst, 0);
  g.radio_ssg_opna = CreateWindowEx(
    0, L"button", L"OPNA analog circuit simulation (sinc + HPF)",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON | WS_TABSTOP | WS_GROUP,
    BOX_X, RADIO_SSG_OPNA_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_RADIO_SSG_OPNA, g.hinst, 0);
  g.radio_ssg_ymf288 = CreateWindowEx(
    0, L"button", L"Bit perfect with OPN3-L aka YMF288 (average of nearest 4.5 samples)",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON,
    BOX_X, RADIO_SSG_YMF288_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_RADIO_SSG_YMF288, g.hinst, 0);
  g.static_volume_offset = CreateWindowEx(
    0, L"static", L"Volume offset (dB):",
    WS_CHILD | WS_VISIBLE,
    STATIC_VOLUME_OFFSET_X, STATIC_VOLUME_OFFSET_Y,
    STATIC_VOLUME_OFFSET_W, CHECK_H,
    hwnd, 0, g.hinst, 0);
  g.static_volume_info = CreateWindowEx(
    0, L"static", L"PC-9801-86 / YMF288: 0.0dB (reference)\nPC-9801-26 / Speakboard: 1.6dB (not verified)",
    WS_CHILD | WS_VISIBLE,
    STATIC_VOLUME_INFO_X, STATIC_VOLUME_INFO_Y,
    BOX_W, CHECK_H*2,
    hwnd, 0, g.hinst, 0);
  g.edit_ssg_mix = CreateWindowEx(
    WS_EX_CLIENTEDGE, L"edit", L"",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
    EDIT_SSG_MIX_X, EDIT_SSG_MIX_Y,
    EDIT_SSG_MIX_W, EDIT_SSG_MIX_H,
    hwnd, (HMENU)ID_EDIT_SSG_MIX, g.hinst, 0);
  g.updown_ssg_mix = CreateWindowEx(
    0, UPDOWN_CLASS, L"",
    WS_CHILD | WS_VISIBLE | UDS_AUTOBUDDY | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK,
    0, 0, 0, 0,
    hwnd, (HMENU)ID_UPDOWN_SSG_MIX, g.hinst, 0);
  g.radio_ppz8_none = CreateWindowEx(
    0, L"button", L"Nearest neighbor (ppz8.com equivalent)",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON | WS_TABSTOP | WS_GROUP,
    BOX_X, RADIO_PPZ8_NONE_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_RADIO_PPZ8_NONE, g.hinst, 0);
  g.radio_ppz8_linear = CreateWindowEx(
    0, L"button", L"Linear",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON,
    BOX_X, RADIO_PPZ8_LINEAR_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_RADIO_PPZ8_LINEAR, g.hinst, 0);
  g.radio_ppz8_sinc = CreateWindowEx(
    0, L"button", L"Sinc (best quality)",
    WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON,
    BOX_X, RADIO_PPZ8_SINC_Y,
    BOX_W, CHECK_H,
    hwnd, (HMENU)ID_RADIO_PPZ8_SINC, g.hinst, 0);
  g.groupbox_defproc = (WNDPROC)GetWindowLongPtr(g.group_fm, GWLP_WNDPROC);
  SetWindowLongPtr(g.group_fm, GWLP_WNDPROC, (intptr_t)groupbox_wndproc);
  SetWindowLongPtr(g.group_ssg, GWLP_WNDPROC, (intptr_t)groupbox_wndproc);
  SetWindowLongPtr(g.group_ppz8, GWLP_WNDPROC, (intptr_t)groupbox_wndproc);
  SetWindowFont(g.group_fm, g.font, TRUE);
  SetWindowFont(g.group_ssg, g.font, TRUE);
  SetWindowFont(g.group_ppz8, g.font, TRUE);
  SetWindowFont(g.check_fm_hires_sin, g.font, TRUE);
  SetWindowFont(g.check_fm_hires_env, g.font, TRUE);
  SetWindowFont(g.radio_ssg_opna, g.font, TRUE);
  SetWindowFont(g.radio_ssg_ymf288, g.font, TRUE);
  SetWindowFont(g.static_volume_offset, g.font, TRUE);
  SetWindowFont(g.static_volume_info, g.font, TRUE);
  SetWindowFont(g.edit_ssg_mix, g.font, TRUE);
  SetWindowFont(g.radio_ppz8_none, g.font, TRUE);
  SetWindowFont(g.radio_ppz8_linear, g.font, TRUE);
  SetWindowFont(g.radio_ppz8_sinc, g.font, TRUE);

  if (fmplayer_config.fm_hires_sin) Button_SetCheck(g.check_fm_hires_sin, true);
  if (fmplayer_config.fm_hires_env) Button_SetCheck(g.check_fm_hires_env, true);

  Button_SetCheck(fmplayer_config.ssg_ymf288 ? g.radio_ssg_ymf288 : g.radio_ssg_opna, true);
  switch (fmplayer_config.ppz8_interp) {
  case PPZ8_INTERP_LINEAR:
    Button_SetCheck(g.radio_ppz8_linear, true);
    break;
  case PPZ8_INTERP_SINC:
    Button_SetCheck(g.radio_ppz8_sinc, true);
    break;
  default:
    Button_SetCheck(g.radio_ppz8_none, true);
    break;
  }
  g.ssg_mix_db = mix_to_db(fmplayer_config.ssg_mix);
  update_ssg_mix();
  Edit_Enable(g.edit_ssg_mix, !fmplayer_config.ssg_ymf288);
  EnableWindow(g.updown_ssg_mix, !fmplayer_config.ssg_ymf288);
  ShowWindow(hwnd, SW_SHOW);
  return true;
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
  HANDLE_MSG(hwnd, WM_COMMAND, on_command);
  HANDLE_MSG(hwnd, WM_ACTIVATE, on_activate);
  case WM_NOTIFY:
    {
      const NMHDR *hdr = (NMHDR *)lParam;
      if (hdr->idFrom == ID_UPDOWN_SSG_MIX && hdr->code == UDN_DELTAPOS) {
        const NMUPDOWN *udnhdr = (NMUPDOWN *)hdr;
        g.ssg_mix_db -= udnhdr->iDelta * 0.1;
        g.ssg_mix_db = round(g.ssg_mix_db * 10.0) / 10.0;
        if (g.ssg_mix_db < -18.0) g.ssg_mix_db = -18.0;
        if (g.ssg_mix_db > 18.0) g.ssg_mix_db = 18.0;
        update_ssg_mix();
        fmplayer_config.ssg_mix = db_to_mix(g.ssg_mix_db);
      }
      break;
    }
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void configdialog_open(HINSTANCE hinst, HWND parent, void (*closecb)(void *), void *closecbptr, void (*changecb)(void *), void *changecbptr) {
  g.closecb = closecb;
  g.closecbptr = closecbptr;
  g.changecb = changecb;
  g.changecbptr = changecbptr;
  g.hinst = hinst;
  if (!g.configwnd) {
    if (!g.config_class) {
      WNDCLASS wc = {0};
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc = wndproc;
      wc.hInstance = hinst;
      wc.hIcon = LoadIcon(g.hinst, MAKEINTRESOURCE(1));
      wc.hCursor = LoadCursor(NULL, IDC_ARROW);
      wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = L"myon_fmplayer_ym2608_config";
      g.config_class = RegisterClass(&wc);
    }
    if (!g.config_class) {
      MessageBox(parent, L"Cannot register class", L"Error", MB_ICONSTOP);
      return;
    }
    g.configwnd = CreateWindowEx(0,
                                 MAKEINTATOM(g.config_class),
                                 L"FMPlayer config",
                                 WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                 parent, 0, g.hinst, 0);
  } else {
    SetForegroundWindow(g.configwnd);
  }
}

void configdialog_close(void) {
  if (g.configwnd) {
    g.closecb = 0;
    DestroyWindow(g.configwnd);
  }
}
