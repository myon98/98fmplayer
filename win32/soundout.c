#include "soundout.h"
#include "dsoundout.h"
#include "waveout.h"
#include "wasapiout.h"

struct sound_state *sound_init(HWND hwnd, unsigned srate, unsigned sectlen,
                               sound_callback cbfunc, void *userptr) {
  struct sound_state *state;
  /*
  state = wasapi_init(hwnd, srate, sectlen, cbfunc, userptr);
  if (state) {
    return state;
  }
  */
  state = dsound_init(hwnd, srate, sectlen, cbfunc, userptr);
  if (state) {
    return state;
  }
  state = waveout_init(hwnd, srate, sectlen, cbfunc, userptr);
  if (state) {
    return state;
  }
  return 0;
}
