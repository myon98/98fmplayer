#ifndef MYON_WAVEOUT_H_INCLUDED
#define MYON_WAVEOUT_H_INCLUDED

#include "soundout.h"

struct sound_state *waveout_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                   sound_callback cbfunc, void *userptr);

#endif // MYON_WAVEOUT_H_INCLUDED
