#ifndef MYON_PULSEOUT_H_INCLUDED
#define MYON_PULSEOUT_H_INCLUDED

#include "soundout.h"

struct sound_state *pulseout_init(const char *clientname, unsigned srate, sound_callback cbfunc, void *userptr);

#endif // MYON_PULSEOUT_H_INCLUDED

