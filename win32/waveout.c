#include "waveout.h"

enum {
  HDRCNT = 4
};

struct waveout_state {
  struct sound_state soundout;
  HWAVEOUT wo;
  HANDLE t_update;
  HANDLE e_waveout;
  HANDLE mtx_cbproc;
  DWORD firstout;
  DWORD paused;
  DWORD terminate;
  sound_callback cbfunc;
  void *userptr;
  WAVEHDR waveheaders[HDRCNT];
  DWORD nextout;
  int16_t *wavebuf;
};

static DWORD CALLBACK bufupdatethread(void *p) {
  struct waveout_state *waveout = (struct waveout_state *)p;
  for (;;) {
    WaitForSingleObject(waveout->e_waveout, INFINITE);
    if (waveout->terminate) ExitThread(0);
    int i, whi;
    for (i = 0;; i = (i+1)%HDRCNT) {
      whi = (i + waveout->nextout) % HDRCNT;
      WAVEHDR *wh = &waveout->waveheaders[whi];
      if (!(wh->dwFlags & WHDR_DONE)) break;
      //wh->dwFlags &= ~WHDR_DONE;
      
      if (WaitForSingleObject(waveout->mtx_cbproc, 0) == WAIT_OBJECT_0) {
        if (!waveout->paused) {
          waveout->cbfunc(waveout->userptr,
                          (int16_t *)wh->lpData, wh->dwBufferLength/4);
        } else {
          ZeroMemory(wh->lpData, wh->dwBufferLength);
        }
        ReleaseMutex(waveout->mtx_cbproc);
      } else {
        ZeroMemory(wh->lpData, wh->dwBufferLength);
      }
      waveOutWrite(waveout->wo, wh, sizeof(*wh));
    }
    waveout->nextout = whi;
  }
}

static void waveout_pause(struct sound_state *state, int pause) {
  struct waveout_state *waveout = (struct waveout_state *)state;
  if (pause) {
    WaitForSingleObject(waveout->mtx_cbproc, INFINITE);
    waveout->paused = 1;
    ReleaseMutex(waveout->mtx_cbproc);
  } else {
    if (waveout->firstout) {
      WaitForSingleObject(waveout->mtx_cbproc, INFINITE);
      waveout->firstout = 0;
      waveout->paused = 0;
      waveOutReset(waveout->wo);
      for (int i = 0; i < HDRCNT; i++) {
        WAVEHDR *wh = &waveout->waveheaders[i];
        waveout->cbfunc(waveout->userptr,
                        (int16_t *)wh->lpData, wh->dwBufferLength/4);
        waveOutWrite(waveout->wo, wh, sizeof(*wh));
      }
      ReleaseMutex(waveout->mtx_cbproc);
    } else {
      waveout->paused = 0;
    }
  }
}

static void waveout_free(struct sound_state *state) {
  struct waveout_state *waveout = (struct waveout_state *)state;
  waveout->terminate = 1;
  SetEvent(waveout->e_waveout);
  WaitForSingleObject(waveout->t_update, INFINITE);
  waveOutReset(waveout->wo);
  for (int i = 0; i < HDRCNT; i++) {
    WAVEHDR *wh = &waveout->waveheaders[i];
    waveOutUnprepareHeader(waveout->wo, wh, sizeof(*wh));
  }
  waveOutClose(waveout->wo);
  CloseHandle(waveout->mtx_cbproc);
  CloseHandle(waveout->e_waveout);
  HANDLE heap = GetProcessHeap();
  HeapFree(heap, 0, waveout->wavebuf);
  HeapFree(heap, 0, waveout);
}

struct sound_state *waveout_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                   sound_callback cbfunc, void *userptr) {
  HANDLE heap = GetProcessHeap();
  struct waveout_state *waveout = HeapAlloc(heap, HEAP_ZERO_MEMORY,
                                            sizeof(struct waveout_state));
  if (!waveout) {
    MessageBoxW(hwnd, L"cannot allocate memory for WaveOut", L"Error", MB_ICONSTOP);
    goto err;
  }
  waveout->e_waveout = CreateEventW(0, FALSE, FALSE, 0);
  if (!waveout->e_waveout) {
    MessageBoxW(hwnd, L"cannot create event for WaveOut", L"Error", MB_ICONSTOP);
    goto err_waveout;
  }
  waveout->mtx_cbproc = CreateMutexW(0, FALSE, 0);
  if (!waveout->mtx_cbproc) {
    MessageBoxW(hwnd, L"cannot create mutex for WaveOut", L"Error", MB_ICONSTOP);
    goto err_event;
  }
  waveout->wavebuf = HeapAlloc(heap, HEAP_ZERO_MEMORY, sectlen*HDRCNT);
  if (!waveout->wavebuf) {
    MessageBoxW(hwnd, L"cannot allocate buffer memory for WaveOut", L"Error", MB_ICONSTOP);
    goto err_mtx;
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
                   (DWORD_PTR)waveout->e_waveout, 0, CALLBACK_EVENT);
  if (hr != MMSYSERR_NOERROR) {
    MessageBoxW(hwnd, L"cannot WaveOutOpen", L"Error", MB_ICONSTOP);
    goto err_wavebuf;
  }
  for (int i = 0; i < HDRCNT; i++) {
    WAVEHDR *wh = &waveout->waveheaders[i];
    wh->lpData = ((char *)waveout->wavebuf) + sectlen*i;
    wh->dwBufferLength = sectlen;
    wh->dwFlags = 0;
    waveOutPrepareHeader(waveout->wo, wh, sizeof(*wh));
  }
  waveout->firstout = 1;
  waveout->paused = 1;
  waveout->terminate = 0;
  waveout->cbfunc = cbfunc;
  waveout->userptr = userptr;
  waveout->t_update = CreateThread(0, 0, bufupdatethread, waveout, 0, 0);
  if (!waveout->t_update) {
    MessageBoxW(hwnd, L"cannot create thread for Waveout", L"Error", MB_ICONSTOP);
    goto err_waveoutopen;
  }
  SetThreadPriority(waveout->t_update, THREAD_PRIORITY_HIGHEST);
  waveout->soundout.pause = waveout_pause;
  waveout->soundout.free = waveout_free;
  waveout->soundout.apiname = L"WinMM";
  return (struct sound_state *)waveout;

err_waveoutopen:
  waveOutReset(waveout->wo);
  for (int i = 0; i < HDRCNT; i++) {
    WAVEHDR *wh = &waveout->waveheaders[i];
    waveOutUnprepareHeader(waveout->wo, wh, sizeof(*wh));
  }
  waveOutClose(waveout->wo);
err_wavebuf:
  HeapFree(heap, 0, waveout->wavebuf);
err_mtx:
  CloseHandle(waveout->mtx_cbproc);
err_event:
  CloseHandle(waveout->e_waveout);
err_waveout:
  HeapFree(heap, 0, waveout);
err:
  return 0;
}
