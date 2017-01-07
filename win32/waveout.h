#ifndef MYON_WAVEOUT_H_INCLUDED
#define MYON_WAVEOUT_H_INCLUDED

#include "soundout.h"

struct waveout_state;

struct waveout_state *waveout_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                   sound_callback cbfunc, void *userptr);
void waveout_delete(struct waveout_state *waveout);
void waveout_pause(struct waveout_state *waveout, int pause);

#endif // MYON_WAVEOUT_H_INCLUDED
