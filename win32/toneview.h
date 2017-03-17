#ifndef MYON_FMPLAYER_WIN32_TONEVIEW_H_INCLUDED
#define MYON_FMPLAYER_WIN32_TONEVIEW_H_INCLUDED

#include "tonedata/tonedata.h"
#include <stdatomic.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern struct toneview_g {
  struct fmplayer_tonedata tonedata;
  atomic_flag flag;
} toneview_g;

void show_toneview(HINSTANCE hinst, HWND parent);

#endif // MYON_FMPLAYER_WIN32_TONEVIEW_H_INCLUDED
