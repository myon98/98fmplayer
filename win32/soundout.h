#ifndef MYON_SOUNDOUT_H_INCLUDED
#define MYON_SOUNDOUT_H_INCLUDED

#include <stdint.h>
#include <windows.h>

typedef void (*sound_callback)(void *userdata, int16_t *buf, unsigned frames);
struct sound_state {
  void (*pause)(struct sound_state *state, int pause);
  void (*free)(struct sound_state *state);
  const wchar_t *apiname;
};

struct sound_state *sound_init(HWND hwnd, unsigned srate, unsigned sectlen,
                               sound_callback cbfunc, void *userptr);

#endif // MYON_SOUNDOUT_H_INCLUDED
