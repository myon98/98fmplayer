#include "dsoundout.h"
#include <windows.h>
#include <dsound.h>

struct dsound_state {
  struct sound_state soundout;
  IDirectSound8 *ds;
  DWORD sectlen;
  IDirectSoundBuffer *dsbuf;
  HANDLE e_posnotf;
  HANDLE t_update;
  DWORD terminate;
  DWORD currsect;
  DWORD playing;
  HANDLE mtx_cbproc;
  sound_callback cbfunc;
  void *userptr;
};

static DWORD WINAPI bufupdatethread(LPVOID p) {
  DWORD playcur, writecur;
  struct dsound_state *dsound = (struct dsound_state *)p;
  while (1) {
    WaitForSingleObject(dsound->e_posnotf, INFINITE);
    if (dsound->terminate) ExitThread(0);
    dsound->dsbuf->lpVtbl->GetCurrentPosition(dsound->dsbuf, &playcur, &writecur);
    DWORD playsect = playcur / dsound->sectlen;
    if (playsect < dsound->currsect) {
      LPVOID ptr;
      DWORD len;
      DWORD startptr = dsound->currsect * dsound->sectlen;
      DWORD writelen = (dsound->sectlen * (4 - dsound->currsect));
      dsound->dsbuf->lpVtbl->Lock(dsound->dsbuf, startptr, writelen, &ptr, &len, NULL, NULL, 0);
      int16_t *bufptr = (int16_t *)ptr;

      if (WaitForSingleObject(dsound->mtx_cbproc, 0) == WAIT_OBJECT_0) {
        if (dsound->playing) {
          (*dsound->cbfunc)(dsound->userptr, bufptr, writelen/4);
        } else {
          ZeroMemory(bufptr, writelen);
        }
        ReleaseMutex(dsound->mtx_cbproc);
      } else {
        ZeroMemory(bufptr, writelen);
      }

      dsound->dsbuf->lpVtbl->Unlock(dsound->dsbuf, ptr, writelen, NULL, 0);
      dsound->currsect = 0;
    } else if (playsect == dsound->currsect) continue;
    LPVOID ptr;
    DWORD len;
    DWORD startptr = dsound->currsect * dsound->sectlen;
    uint32_t writelen = (dsound->sectlen * (playsect - dsound->currsect));
    dsound->dsbuf->lpVtbl->Lock(dsound->dsbuf, startptr, writelen, &ptr, &len, NULL, NULL, 0);
    int16_t *bufptr = (int16_t *)ptr;

    if (WaitForSingleObject(dsound->mtx_cbproc, 0) == WAIT_OBJECT_0) {
      if (dsound->playing) {
        (*dsound->cbfunc)(dsound->userptr, bufptr, writelen/4);
      } else {
        ZeroMemory(bufptr, writelen);
      }
      ReleaseMutex(dsound->mtx_cbproc);
    } else {
      ZeroMemory(bufptr, writelen);
    }

    dsound->dsbuf->lpVtbl->Unlock(dsound->dsbuf, ptr, writelen, NULL, 0);
    dsound->currsect = playsect;
  }
}

static void dsound_pause(struct sound_state *state, int pause) {
  struct dsound_state *dsound = (struct dsound_state *)state;
  if (pause) {
    dsound->playing = 0;
    WaitForSingleObject(dsound->mtx_cbproc, INFINITE);
    ReleaseMutex(dsound->mtx_cbproc);
  } else {
    dsound->playing = 1;
    dsound->dsbuf->lpVtbl->Play(dsound->dsbuf, 0, 0, DSBPLAY_LOOPING);
  }
}

static void dsound_free(struct sound_state *state) {
  struct dsound_state *dsound = (struct dsound_state *)state;
  dsound_pause(state, 1);
  dsound->terminate = 1;
  SetEvent(dsound->e_posnotf);
  WaitForSingleObject(dsound->t_update, INFINITE);
  CloseHandle(dsound->mtx_cbproc);
  dsound->dsbuf->lpVtbl->Release(dsound->dsbuf);
  dsound->ds->lpVtbl->Release(dsound->ds);
  HeapFree(GetProcessHeap(), 0, dsound);
}


struct sound_state *dsound_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                 sound_callback cbfunc, void *userptr) {
  HANDLE heap = GetProcessHeap();
  struct dsound_state *dsound = HeapAlloc(heap, HEAP_ZERO_MEMORY,
                                          sizeof(struct dsound_state));
  if (!dsound) {
    MessageBoxW(hwnd, L"cannot allocate memory for DirectSound", L"Error", MB_ICONSTOP);
    goto err;
  }
  CoInitializeEx(0, COINIT_MULTITHREADED);
  HRESULT hr;
  hr = CoCreateInstance(
    &CLSID_DirectSound8,
    0,
    CLSCTX_INPROC_SERVER,
    &IID_IDirectSound8,
    (void **)&dsound->ds);
  if (hr != S_OK) {
    goto err_dsound;
  }
  hr = dsound->ds->lpVtbl->Initialize(dsound->ds, 0);
  if (hr != S_OK) {
    MessageBoxW(hwnd, L"cannot initialize DirectSound8", L"Error", MB_ICONSTOP);
    goto err_dsound;
  }
  dsound->ds->lpVtbl->SetCooperativeLevel(dsound->ds, hwnd, DSSCL_NORMAL);
  
  DSBUFFERDESC bufdesc = {0};
  bufdesc.dwSize = sizeof(bufdesc);
  bufdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 |
      DSBCAPS_GLOBALFOCUS |
      DSBCAPS_CTRLPOSITIONNOTIFY;
  bufdesc.dwBufferBytes = sectlen * 4;
  WAVEFORMATEX format;
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = srate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  format.cbSize = 0;
  bufdesc.lpwfxFormat = &format;
  bufdesc.guid3DAlgorithm = DS3DALG_DEFAULT;
  bufdesc.dwReserved = 0;

  if (dsound->ds->lpVtbl->CreateSoundBuffer(dsound->ds, &bufdesc, &dsound->dsbuf, 0) != S_OK) {
    MessageBoxW(hwnd, L"cannot initialize DirectSoundBuffer", L"Error", MB_ICONSTOP);
    goto err_ds;
  }

  IDirectSoundNotify *dsnotf;
  if (dsound->dsbuf->lpVtbl->QueryInterface(dsound->dsbuf,
                                          &IID_IDirectSoundNotify,
                                          (void **)&dsnotf) != S_OK) {
    goto err_dsbuf;
  }

  dsound->mtx_cbproc = CreateMutexW(0, FALSE, 0);
  if (!dsound->mtx_cbproc) {
    goto err_dsnotf;
  }
  dsound->e_posnotf = CreateEventW(NULL, FALSE, FALSE, 0);
  if (!dsound->e_posnotf) {
    goto err_mtx;
  }
  dsound->t_update = CreateThread(NULL, 0, bufupdatethread, dsound, 0, NULL);
  if (!dsound->t_update) {
    goto err_event;
  }
  SetThreadPriority(dsound->t_update, THREAD_PRIORITY_HIGHEST);
  dsound->sectlen = sectlen;
  dsound->playing = 0;
  dsound->terminate = 0;
  dsound->cbfunc = cbfunc;
  dsound->userptr = userptr;
  
  
  DSBPOSITIONNOTIFY posnotf[4] = {
    {(DWORD)dsound->sectlen*0, dsound->e_posnotf},
    {(DWORD)dsound->sectlen*1, dsound->e_posnotf},
    {(DWORD)dsound->sectlen*2, dsound->e_posnotf},
    {(DWORD)dsound->sectlen*3, dsound->e_posnotf},
  };
  dsnotf->lpVtbl->SetNotificationPositions(dsnotf, 4, posnotf);
  dsnotf->lpVtbl->Release(dsnotf);
  dsound->soundout.pause = dsound_pause;
  dsound->soundout.free = dsound_free;
  dsound->soundout.apiname = L"DirectSound";
  return (struct sound_state *)dsound;

err_event:
  CloseHandle(dsound->e_posnotf);
err_mtx:
  CloseHandle(dsound->mtx_cbproc);
err_dsnotf:
  dsnotf->lpVtbl->Release(dsnotf);
err_dsbuf:
  dsound->dsbuf->lpVtbl->Release(dsound->dsbuf);
err_ds:
  dsound->ds->lpVtbl->Release(dsound->ds);
err_dsound:
  HeapFree(heap, 0, dsound);
err:
  return 0;
}
