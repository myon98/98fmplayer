#include "waveout.h"

struct waveout_state {
  HWAVEOUT wo;
  DWORD firstout;
  sound_callback cbfunc;
  void *userptr;
  WAVEHDR waveheaders[4];
  int16_t *wavebuf;
};

static void CALLBACK waveout_cbproc(HWAVEOUT wo, UINT msg,
                                 DWORD_PTR instance,
                                 DWORD_PTR p1, DWORD_PTR p2) {
  (void)wo;
  (void)p2;
  if (msg != WOM_DONE) return;
  struct waveout_state *waveout = (struct waveout_state *)instance;
  WAVEHDR *wh = (WAVEHDR *)p1;
  waveout->cbfunc(waveout->userptr, (int16_t *)wh->lpData, wh->dwBufferLength/4);
  waveOutWrite(waveout->wo, wh, sizeof(*wh));
}

struct waveout_state *waveout_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                   sound_callback cbfunc, void *userptr) {
  HANDLE heap = GetProcessHeap();
  struct waveout_state *waveout = HeapAlloc(heap, HEAP_ZERO_MEMORY,
                                            sizeof(struct waveout_state));
  if (!waveout) {
    MessageBoxW(hwnd, L"cannot allocate memory for WaveOut", L"Error", MB_ICONSTOP);
    goto err;
  }
  waveout->wavebuf = HeapAlloc(heap, HEAP_ZERO_MEMORY, sectlen*4);
  if (!waveout->wavebuf) {
    MessageBoxW(hwnd, L"cannot allocate buffer memory for WaveOut", L"Error", MB_ICONSTOP);
    goto err_waveout;
  }
  WAVEFORMATEX format;
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = srate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  format.cbSize = 0;
  HRESULT hr;
  hr = waveOutOpen(&waveout->wo, WAVE_MAPPER, &format,
                   (DWORD_PTR)waveout_cbproc, (DWORD_PTR)waveout, CALLBACK_FUNCTION);
  if (hr != MMSYSERR_NOERROR) {
    MessageBoxW(hwnd, L"cannot WaveOutOpen", L"Error", MB_ICONSTOP);
    goto err_wavebuf;
  }
  for (int i = 0; i < 4; i++) {
    WAVEHDR *wh = &waveout->waveheaders[i];
    wh->lpData = ((char *)waveout->wavebuf) + sectlen*i;
    wh->dwBufferLength = sectlen;
    wh->dwFlags = 0;
    waveOutPrepareHeader(waveout->wo, wh, sizeof(*wh));
  }
  waveout->firstout = 1;
  waveout->cbfunc = cbfunc;
  waveout->userptr = userptr;
  return waveout;

err_wavebuf:
  HeapFree(heap, 0, waveout->wavebuf);
err_waveout:
  HeapFree(heap, 0, waveout);
err:
  return 0;
}

void waveout_pause(struct waveout_state *waveout, int pause) {
  if (pause) {
    waveOutPause(waveout->wo);
  } else {
    if (waveout->firstout) {
      waveout->firstout = 0;
      for (int i = 0; i < 4; i++) {
        WAVEHDR *wh = &waveout->waveheaders[i];
        waveout->cbfunc(waveout->userptr, (int16_t *)wh->lpData, wh->dwBufferLength/4);
        waveOutWrite(waveout->wo, wh, sizeof(*wh));
      }
    } else {
      waveOutRestart(waveout->wo);
    }
  }
}

void waveout_delete(struct waveout_state *waveout) {
  if (!waveout) return;
  waveout_pause(waveout, 1);
  for (int i = 0; i < 4; i++) {
    WAVEHDR *wh = &waveout->waveheaders[i];
    waveOutUnprepareHeader(waveout->wo, wh, sizeof(*wh));
  }
  HANDLE heap = GetProcessHeap();
  HeapFree(heap, 0, waveout->wavebuf);
  HeapFree(heap, 0, waveout);
}
