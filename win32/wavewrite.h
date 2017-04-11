#ifndef MYON_FMPLAYER_WIN32_WAVEWRITE_H_INCLUDED
#define MYON_FMPLAYER_WIN32_WAVEWRITE_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

struct wavefile;

struct wavefile *wavewrite_open_w(const wchar_t *path, uint32_t samplerate);

size_t wavewrite_write(struct wavefile *wavefile, const int16_t *buf, size_t frames);

void wavewrite_close(struct wavefile *wavefile);

#endif // MYON_FMPLAYER_WIN32_WAVEWRITE_H_INCLUDED
