#ifndef MYON_FMPLAYER_WIN32_WAVESAVE_H_INCLUDED
#define MYON_FMPLAYER_WIN32_WAVESAVE_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void wavesave_dialog(HWND parent, const wchar_t *fmpath);

#endif // MYON_FMPLAYER_WIN32_WAVESAVE_H_INCLUDED
