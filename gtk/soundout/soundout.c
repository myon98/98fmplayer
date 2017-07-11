#include "soundout.h"
#include "jackout.h"
#include "pulseout.h"
#include "alsaout.h"

struct sound_state *sound_init(const char *clientname, unsigned srate, sound_callback cbfunc, void *userptr) {
  struct sound_state *ss = 0;
  //ss = jackout_init(clientname, srate, cbfunc, userptr);
  if (ss) return ss;
  //ss = pulseout_init(clientname, srate, cbfunc, userptr);
  if (ss) return ss;
  ss = alsaout_init(clientname, srate, cbfunc, userptr);
  return ss;
}
