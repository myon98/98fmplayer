#ifndef MYON_FMPLAYER_WIN32_CONFIGDIALOG_H_INCLUDED
#define MYON_FMPLAYER_WIN32_CONFIGDIALOG_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include "fmdriver/ppz8.h"

extern struct fmplayer_config {
  bool fm_hires_env;
  bool fm_hires_sin;
  bool ssg_ymf288;
  uint32_t ssg_mix;
  enum ppz8_interp ppz8_interp;
} fmplayer_config;

void configdialog_open(HINSTANCE hinst, HWND parent, void (*closecb)(void *), void *closecbptr, void (*changecb)(void *), void *changecbptr);
void configdialog_close(void);

#endif // MYON_FMPLAYER_WIN32_CONFIGDIALOG_H_INCLUDED
