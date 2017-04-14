#include "wavewrite.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

struct wavefile {
  HANDLE file;
  uint32_t written_frames;
};

static void write16le(uint8_t *ptr, uint16_t data) {
  ptr[0] = data;
  ptr[1] = data >> 8;
}

static void write32le(uint8_t *ptr, uint32_t data) {
  ptr[0] = data;
  ptr[1] = data >> 8;
  ptr[2] = data >> 16;
  ptr[3] = data >> 24;
}

struct wavefile *wavewrite_open_w(const wchar_t *path, uint32_t samplerate) {
  struct wavefile *wavefile = 0;
  wavefile = malloc(sizeof(*wavefile));
  if (!wavefile) goto err;
  *wavefile = (struct wavefile){
    .file = INVALID_HANDLE_VALUE
  };
  wavefile->file = CreateFile(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  if (wavefile->file == INVALID_HANDLE_VALUE) goto err;
  uint8_t waveheader[44] = {0};
  memcpy(waveheader, "RIFF", 4);
  memcpy(waveheader+8, "WAVE", 4);
  memcpy(waveheader+12, "fmt ", 4);
  write32le(waveheader+16, 16);
  write16le(waveheader+20, 1);
  write16le(waveheader+22, 2);
  write32le(waveheader+24, samplerate);
  write32le(waveheader+28, samplerate * 2 * 2);
  write16le(waveheader+32, 4);
  write16le(waveheader+34, 16);
  memcpy(waveheader+36, "data", 4);
  DWORD written;
  if (!WriteFile(wavefile->file, waveheader, sizeof(waveheader), &written, 0) || (written != sizeof(waveheader))) {
    goto err;
  }
  return wavefile;
err:
  if (wavefile) {
    if (wavefile->file != INVALID_HANDLE_VALUE) CloseHandle(wavefile->file);
    free(wavefile);
  }
  return 0;
}

size_t wavewrite_write(struct wavefile *wavefile, const int16_t *buf, size_t frames) {
  if (frames >= (1ull<<(32-2))) return 0;
  DWORD written;
  if (!WriteFile(wavefile->file, buf, frames*4, &written, 0)) {
    return 0;
  }
  uint32_t written_frames = written / 4u;
  wavefile->written_frames += written_frames;
  return written_frames;
}

void wavewrite_close(struct wavefile *wavefile) {
  uint32_t size;
  DWORD written;
  if (SetFilePointer(wavefile->file, 40, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    goto cleanup;
  }
  size = wavefile->written_frames * 4;
  if (!WriteFile(wavefile->file, &size, sizeof(size), &written, 0) || (written != sizeof(size))) {
    goto cleanup;
  }
  if (SetFilePointer(wavefile->file, 4, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    goto cleanup;
  }
  size += 4 + 8 + 16 + 8;
  if (!WriteFile(wavefile->file, &size, sizeof(size), &written, 0) || (written != sizeof(size))) {
    goto cleanup;
  }
cleanup:
  CloseHandle(wavefile->file);
  free(wavefile);
}
