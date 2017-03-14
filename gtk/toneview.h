#ifndef MYON_FMPLAYER_GTK_TONEVIEW_H_INCLUDED
#define MYON_FMPLAYER_GTK_TONEVIEW_H_INCLUDED

#include "tonedata/tonedata.h"
#include <stdatomic.h>

extern struct toneview_g {
  struct fmplayer_tonedata tonedata;
  atomic_flag flag;
} toneview_g;

void show_toneview(void);

#endif // MYON_FMPLAYER_GTK_TONEVIEW_H_INCLUDED
