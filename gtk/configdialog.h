#ifndef MYON_FMPLAYER_GTK_CONFIGDIALOG_H
#define MYON_FMPLAYER_GTK_CONFIGDIALOG_H

#include <gtk/gtk.h>
#include "libopna/opna.h"
#include "fmdriver/ppz8.h"

extern struct fmplayer_config {
  bool fm_hires_env;
  bool fm_hires_sin;
  bool ssg_ymf288;
  uint32_t ssg_mix;
  enum ppz8_interp ppz8_interp;
} fmplayer_config;

typedef void config_update_func(void *ptr);

void show_configdialog(config_update_func *func, void *ptr);

#endif // MYON_FMPLAYER_GTK_CONFIGDIALOG_H
