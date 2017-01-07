#include "soundout.h"
#include "dsoundout.h"
#include "waveout.h"

static void soundout_dsound_pause(struct sound_state *state, int pause) {
  dsound_pause((struct dsound_state *)state->driver_state, pause);
}

static void soundout_dsound_delete(struct sound_state *state) {
  dsound_delete((struct dsound_state *)state->driver_state);
  HeapFree(GetProcessHeap(), 0, state);
}

static void soundout_waveout_pause(struct sound_state *state, int pause) {
  waveout_pause((struct waveout_state *)state->driver_state, pause);
}

static void soundout_waveout_delete(struct sound_state *state) {
  waveout_delete((struct waveout_state *)state->driver_state);
  HeapFree(GetProcessHeap(), 0, state);
}

struct sound_state *sound_init(HWND hwnd, unsigned srate, unsigned sectlen,
                               sound_callback cbfunc, void *userptr) {
  HANDLE heap = GetProcessHeap();
  struct sound_state *sound = HeapAlloc(heap, 0, sizeof(struct sound_state));
  if (!sound) return 0;
  struct dsound_state *dsound = dsound_init(hwnd, srate, sectlen, cbfunc, userptr);
  if (dsound) {
    sound->driver_state = dsound;
    sound->pause = soundout_dsound_pause;
    sound->delete = soundout_dsound_delete;
    return sound;
  }
  struct waveout_state *waveout = waveout_init(hwnd, srate, sectlen, cbfunc, userptr);
  if (waveout) {
    sound->driver_state = waveout;
    sound->pause = soundout_waveout_pause;
    sound->delete = soundout_waveout_delete;
    return sound;
  }
  HeapFree(heap, 0, sound);
  return 0;
}
