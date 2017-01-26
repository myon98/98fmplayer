#ifndef MYON_DSOUNDOUT_H_INCLUDED
#define MYON_DSOUNDOUT_H_INCLUDED

#include "soundout.h"

struct sound_state *dsound_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                 sound_callback cbfunc, void *userptr);

#endif // MYON_DSOUNDOUT_H_INCLUDED
