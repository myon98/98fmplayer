#ifndef MYON_PACC_WIN_H_INCLUDED
#define MYON_PACC_WIN_H_INCLUDED

#include "pacc.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdbool.h>

typedef void pacc_rendercb(void *ptr);

struct pacc_win_vtable {
  void (*renderctrl)(struct pacc_ctx *ctx, bool enable);
};

struct pacc_ctx *pacc_init_d3d9(HWND hwnd, int w, int h, pacc_rendercb *rendercb, void *renderptr, struct pacc_vtable *vt, struct pacc_win_vtable *winvt, UINT msg_reset, HWND msg_wnd);

#endif // MYON_PACC_WIN_H_INCLUDED

