#ifndef MYON_FMPLAYER_WIN32_OSCILLOVIEW_H_INCLUDED
#define MYON_FMPLAYER_WIN32_OSCILLOVIEW_H_INCLUDED

#include "libopna/opna.h"
#include "oscillo/oscillo.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdatomic.h>

extern struct oscilloview {
  atomic_flag flag;
  struct oscillodata oscillodata[LIBOPNA_OSCILLO_TRACK_COUNT];
} oscilloview_g;

void show_oscilloview(HINSTANCE hinst, HWND parent);

#endif // MYON_FMPLAYER_WIN32_OSCILLOVIEW_H_INCLUDED

