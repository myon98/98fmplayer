#include "wasapiout.h"
//#include "srcloader.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

enum {
  SRCBUFFRAMES = 256,
};

struct wasapi_state {
  struct sound_state soundout;
  IAudioClient *ac;
  IAudioRenderClient *rc;
  HANDLE t_update;
  HANDLE e_update;
  HANDLE mtx_cbproc;
  sound_callback cbfunc;
  void *userptr;
  int16_t *buf_i;
  float *buf_f;
//  SRC_STATE *src;
  DWORD terminate;
  DWORD paused;
  unsigned buf_frames;
  unsigned buf_used_frames;
  UINT32 wasapi_buf_frames;
//  unsigned srate;
//  double src_ratio;
};

static DWORD CALLBACK bufupdatethread(void *p) {
  struct wasapi_state *wasapiout = (struct wasapi_state *)p;
  for (;;) {
    WaitForSingleObject(wasapiout->e_update, INFINITE);
    if (wasapiout->terminate) ExitThread(0);
    int16_t *outbuf;
    while (SUCCEEDED(wasapiout->rc->lpVtbl->GetBuffer(wasapiout->rc,
      wasapiout->wasapi_buf_frames, (BYTE **)&outbuf))) {
      int silence = 1;
      if (WaitForSingleObject(wasapiout->mtx_cbproc, 0) == WAIT_OBJECT_0) {
        if (!wasapiout->paused) {
          silence = 0;
/*
          ZeroMemory(wasapiout->buf_i, wasapiout->wasapi_buf_frames*4);
          wasapiout->cbfunc(
            wasapiout->userptr,
            wasapiout->buf_i,
            wasapiout->wasapi_buf_frames);
*/
          ZeroMemory(wasapiout->buf_i, wasapiout->wasapi_buf_frames*4);
          wasapiout->cbfunc(
            wasapiout->userptr,
            outbuf,
            wasapiout->wasapi_buf_frames);
        }
        ReleaseMutex(wasapiout->mtx_cbproc);
      }
      if (!silence) {
        //g_src.src_short_to_float_array(wasapiout->buf_i, outbuf, wasapiout->wasapi_buf_frames*2);
      }
      wasapiout->rc->lpVtbl->ReleaseBuffer(wasapiout->rc,
        wasapiout->wasapi_buf_frames,
        silence ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
    }
  }
}

static void wasapi_pause(struct sound_state *state, int pause) {
  struct wasapi_state *wasapiout = (struct wasapi_state *)state;
  if (pause) {
    //wasapiout->ac->lpVtbl->Stop(wasapiout->ac);
    WaitForSingleObject(wasapiout->mtx_cbproc, INFINITE);
    wasapiout->paused = 1;
    ReleaseMutex(wasapiout->mtx_cbproc);
  } else {
    wasapiout->paused = 0;
    //wasapiout->ac->lpVtbl->Start(wasapiout->ac);
  }
}

static void wasapi_free(struct sound_state *state) {
  struct wasapi_state *wasapiout = (struct wasapi_state *)state;
  HANDLE heap = GetProcessHeap();
  wasapiout->terminate = 1;
  SetEvent(wasapiout->e_update);
  WaitForSingleObject(wasapiout->t_update, INFINITE);
  wasapiout->ac->lpVtbl->Stop(wasapiout->ac);
  wasapiout->rc->lpVtbl->Release(wasapiout->rc);
  wasapiout->ac->lpVtbl->Release(wasapiout->ac);
  HeapFree(heap, 0, wasapiout->buf_f);
  HeapFree(heap, 0, wasapiout->buf_i);
  CloseHandle(wasapiout->mtx_cbproc);
  CloseHandle(wasapiout->e_update);
//  g_src.src_delete(wasapiout->src);
  HeapFree(heap, 0, wasapiout);
}

struct sound_state *wasapi_init(HWND hwnd, unsigned srate, unsigned sectlen,
                                   sound_callback cbfunc, void *userptr) {
  if (sectlen % (sizeof(int16_t)*2)) goto err;
  unsigned sectframes = sectlen / (sizeof(int16_t)*2);
/*
  src_load();
  if (!g_src.dll) goto err;
*/
  HANDLE heap = GetProcessHeap();
  if (!heap) {
    MessageBoxW(hwnd, L"Cannot get process heap for wasapi", L"Error", MB_ICONSTOP);
    goto err;
  }
  struct wasapi_state *wasapiout = HeapAlloc(heap, HEAP_ZERO_MEMORY,
                                             sizeof(struct wasapi_state));
  if (!wasapiout) {
    MessageBoxW(hwnd, L"Cannot allocate memory for wasapi", L"Error", MB_ICONSTOP);
    goto err;
  }
/*
  {
    int e;
    wasapiout->src = g_src.src_new(SRC_SINC_BEST_QUALITY, 2, &e);
  }
  if (!wasapiout->src) {
    MessageBoxW(hwnd, L"WASAPI failed to create samplerate converter", L"Error",
                MB_ICONSTOP);
    goto err_wasapiout;
  }
*/
  CoInitializeEx(0, COINIT_MULTITHREADED);
  HRESULT hr;
  IMMDeviceEnumerator *mmde;
  hr = CoCreateInstance(
    &CLSID_MMDeviceEnumerator,
    0,
    CLSCTX_ALL,
    &IID_IMMDeviceEnumerator,
    (void **)&mmde);
  if (FAILED(hr)) {
    goto err_src;
  }
  IMMDevice *mmdev;
  hr = mmde->lpVtbl->GetDefaultAudioEndpoint(mmde, eRender, eMultimedia, &mmdev);
  if (FAILED(hr)) {
    MessageBoxW(hwnd, L"WASAPI GetDefaultEndpoint failed", L"Error", MB_ICONSTOP);
    goto err_mmde;
  }
  hr = mmdev->lpVtbl->Activate(
    mmdev, &IID_IAudioClient, CLSCTX_ALL, 0, (void **)&wasapiout->ac);
  if (FAILED(hr)) {
    MessageBoxW(hwnd, L"WASAPI cannot get IAudioClient", L"Error",
                MB_ICONSTOP);
    goto err_mmdev;
  }
  wasapiout->e_update = CreateEventW(0, FALSE, FALSE, 0);
  if (!wasapiout->e_update) {
    MessageBoxW(hwnd, L"WASAPI CreateEvent failed", L"Error", MB_ICONSTOP);
    goto err_ac;
  }
  wasapiout->mtx_cbproc = CreateMutexW(0, FALSE, 0);
  if (!wasapiout->mtx_cbproc) {
    MessageBoxW(hwnd, L"WASAPI CreateMutex failed", L"Error", MB_ICONSTOP);
    goto err_event;
  }
  DWORD mixsrate;
  {
    WAVEFORMATEX *mixfmt;
    hr = wasapiout->ac->lpVtbl->GetMixFormat(wasapiout->ac, &mixfmt);
    if (FAILED(hr)) {
      MessageBoxW(hwnd, L"WASAPI GetMixFormat failed", L"Error", MB_ICONSTOP);
      goto err_mtx;
    }
    mixsrate = mixfmt->nSamplesPerSec;
    CoTaskMemFree(mixfmt);
  }
  WAVEFORMATEXTENSIBLE format = {0};
  format.Format.wFormatTag = WAVE_FORMAT_PCM;
  format.Format.nChannels = 2;
  format.Format.nSamplesPerSec = srate;
  format.Format.wBitsPerSample = 16;
  format.Format.nBlockAlign =
    format.Format.nChannels * format.Format.wBitsPerSample / 8;
  format.Format.nAvgBytesPerSec =
    format.Format.nSamplesPerSec * format.Format.nBlockAlign;
  format.Format.cbSize = 0;//sizeof(format)-sizeof(format.Format);
  format.Samples.wValidBitsPerSample = format.Format.wBitsPerSample;
  format.dwChannelMask = (1<<format.Format.nChannels)-1;
  format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  
  hr = wasapiout->ac->lpVtbl->Initialize(
    wasapiout->ac, AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_EVENTCALLBACK | 0x80000000,
    0, 0, &format.Format, 0);
  if (FAILED(hr)) {
    wchar_t str[] = L"0x        : WASAPI IAudioClient Initialize failed";
    for (int i = 0; i < 8; i++) {
      wchar_t c = (hr >> ((7-i)*4)) & 0xf;
      c += L'0';
      if (c > L'9') c += (L'A' - L'0');
      str[2+i] = c;
    }
    MessageBoxW(hwnd, str, L"Error",
                MB_ICONSTOP);
    goto err_mtx;
  }
  hr = wasapiout->ac->lpVtbl->SetEventHandle(
    wasapiout->ac, wasapiout->e_update);
  if (FAILED(hr)) {
    MessageBoxW(hwnd, L"WASAPI cannot set event handle", L"Error", MB_ICONSTOP);
    goto err_mtx;
  }
  hr = wasapiout->ac->lpVtbl->GetBufferSize(
    wasapiout->ac, &wasapiout->wasapi_buf_frames);
  if (FAILED(hr)) {
    MessageBoxW(hwnd, L"WASAPI GetBufferSize failed", L"Error", MB_ICONSTOP);
    goto err_mtx;
  }
  wasapiout->wasapi_buf_frames /= 4;
  wasapiout->buf_i = HeapAlloc(heap, HEAP_ZERO_MEMORY,
                               sizeof(int16_t)*wasapiout->wasapi_buf_frames*2);
  if (!wasapiout->buf_i) {
    MessageBoxW(hwnd, L"WASAPI buffer allocation failed", L"Error", MB_ICONSTOP);
    goto err_mtx;
  }
  wasapiout->buf_f = HeapAlloc(heap, HEAP_ZERO_MEMORY,
                               sizeof(float)*sectframes*2);
  if (!wasapiout->buf_f) {
    MessageBoxW(hwnd, L"WASAPI buffer allocation failed", L"Error", MB_ICONSTOP);
    goto err_buf_i;
  }
  hr = wasapiout->ac->lpVtbl->GetService(
    wasapiout->ac, &IID_IAudioRenderClient, (void **)&wasapiout->rc);
  if (FAILED(hr)) {
    MessageBoxW(hwnd, L"WASAPI cannot get IAudioRenderClient", L"Error",
                MB_ICONSTOP);
    goto err_buf_f;
  }
  wasapiout->terminate = 0;
  wasapiout->paused = 1;
  wasapiout->cbfunc = cbfunc;
  wasapiout->userptr = userptr;
  wasapiout->buf_frames = sectframes;
  wasapiout->buf_used_frames = 0;
  wasapiout->t_update = CreateThread(0, 0, bufupdatethread, wasapiout, 0, 0);
  if (!wasapiout->t_update) {
    MessageBoxW(hwnd, L"WASAPI CreateThread error", L"Error", MB_ICONSTOP);
    goto err_rc;
  }
  SetThreadPriority(wasapiout->t_update, THREAD_PRIORITY_HIGHEST);
  wasapiout->soundout.pause = wasapi_pause;
  wasapiout->soundout.free = wasapi_free;
  wasapiout->soundout.apiname = L"WASAPI";
  mmdev->lpVtbl->Release(mmdev);
  mmde->lpVtbl->Release(mmde);
  wasapiout->ac->lpVtbl->Start(wasapiout->ac);
  return &wasapiout->soundout;

err_rc:
  wasapiout->rc->lpVtbl->Release(wasapiout->rc);
err_buf_f:
  HeapFree(heap, 0, wasapiout->buf_f);
err_buf_i:
  HeapFree(heap, 0, wasapiout->buf_i);
err_mtx:
  CloseHandle(wasapiout->mtx_cbproc);
err_event:
  CloseHandle(wasapiout->e_update);
err_ac:
  wasapiout->ac->lpVtbl->Release(wasapiout->ac);
err_mmdev:
  mmdev->lpVtbl->Release(mmdev);
err_mmde:
  mmde->lpVtbl->Release(mmde);
err_src:
//  g_src.src_delete(wasapiout->src);
err_wasapiout:
  HeapFree(heap, 0, wasapiout);
err:
  return 0;
}
