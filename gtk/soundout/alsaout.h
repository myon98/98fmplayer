#ifndef MYON_ALSAOUT_H_INCLUDED
#define MYON_ALSAOUT_H_INCLUDED

#include "soundout.h"

struct sound_state *alsaout_init(const char *clientname, unsigned srate, sound_callback cbfunc, void *userptr);

#endif // MYON_ALSAOUT_H_INCLUDED

