#ifndef MYON_WASAPIOUT_H_INCLUDED
#define MYON_WASAPIOUT_H_INCLUDED

#include "soundout.h"

struct sound_state *wasapi_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                 sound_callback cbfunc, void *userptr);

#endif // MYON_WASAPIOUT_H_INCLUDED

