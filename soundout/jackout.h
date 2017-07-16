#ifndef MYON_JACKOUT_H_INCLUDED
#define MYON_JACKOUT_H_INCLUDED

#include "soundout.h"

struct sound_state *jackout_init(const char *clientname, unsigned srate, sound_callback cbfunc, void *userptr);

#endif // MYON_JACKOUT_H_INCLUDED

