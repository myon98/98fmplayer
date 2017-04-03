#ifndef MYON_FMPLAYER_WIN32_ABOUT_H_INCLUDED
#define MYON_FMPLAYER_WIN32_ABOUT_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>

void about_open(HINSTANCE hinst, HWND parent, void (*closecb)(void *ptr), void *cbptr);
void about_close(void);
void about_setsoundapiname(const wchar_t *apiname);
void about_set_fontrom_loaded(bool loaded);
void about_set_adpcmrom_loaded(bool loaded);

#endif // MYON_FMPLAYER_WIN32_ABOUT_H_INCLUDED
