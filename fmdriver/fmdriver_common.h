#ifndef MYON_FMDRIVER_COMMON_H_INCLUDED
#define MYON_FMDRIVER_COMMON_H_INCLUDED

static inline uint16_t read16le(const uint8_t *ptr) {
  return (unsigned)ptr[0] | (((unsigned)ptr[1])<<8);
}

static inline int8_t u8s8(uint8_t v) {
  return (v & 0x80) ? ((int16_t)v)-0x100 : v;
}

static inline int16_t u16s16(uint16_t v) {
  return (v & 0x8000u) ? ((int32_t)v)-0x10000l : v;
}

#if 0
#include <stdio.h>
#define FMDRIVER_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define FMDRIVER_DEBUG(...)
#endif
#endif // MYON_FMDRIVER_COMMON_H_INCLUDED
