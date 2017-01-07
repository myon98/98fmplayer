#ifndef MYON_DSOUNDOUT_H_INCLUDED
#define MYON_DSOUNDOUT_H_INCLUDED

#include "soundout.h"

struct dsound_state;

struct dsound_state *dsound_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                 sound_callback cbfunc, void *userptr);
void dsound_delete(struct dsound_state *dsound);
void dsound_pause(struct dsound_state *dsound, int pause);

#endif // MYON_DSOUNDOUT_H_INCLUDED
