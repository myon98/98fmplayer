#ifndef MYON_SOUNDOUT_H_INCLUDED
#define MYON_SOUNDOUT_H_INCLUDED

#include <stdint.h>

typedef void (*sound_callback)(void *userptr, int16_t *buf, unsigned frames);

struct sound_state {
  void (*pause)(struct sound_state *state, int pause, int flush);
  void (*free)(struct sound_state *state);
  const char *apiname;
};

struct sound_state *sound_init(const char *clientname, unsigned srate, sound_callback cbfunc, void *userptr);

#endif // MYON_SOUNDOUT_H_INCLUDED
