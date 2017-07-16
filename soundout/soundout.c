#include "soundout.h"
#include "jackout.h"
#include "pulseout.h"
#include "alsaout.h"

struct sound_state *sound_init(const char *clientname, unsigned srate, sound_callback cbfunc, void *userptr) {
  struct sound_state *ss = 0;
#ifdef ENABLE_JACK
  ss = jackout_init(clientname, srate, cbfunc, userptr);
#endif
  if (ss) return ss;
#ifdef ENABLE_PULSE
  ss = pulseout_init(clientname, srate, cbfunc, userptr);
#endif
  if (ss) return ss;
#ifdef ENABLE_ALSA
  ss = alsaout_init(clientname, srate, cbfunc, userptr);
#endif
  return ss;
}
