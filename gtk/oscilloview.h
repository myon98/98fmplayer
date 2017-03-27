#ifndef MYON_FMPLAYER_GTK_OSCILLOVIEW_H_INCLUDED
#define MYON_FMPLAYER_GTK_OSCILLOVIEW_H_INCLUDED

#include "libopna/opna.h"
#include "oscillo/oscillo.h"

#include <stdatomic.h>

extern struct oscilloview {
  atomic_flag flag;
  struct oscillodata oscillodata[LIBOPNA_OSCILLO_TRACK_COUNT];
} oscilloview_g;

void show_oscilloview(void);

#endif // MYON_FMPLAYER_GTK_OSCILLOVIEW_H_INCLUDED

