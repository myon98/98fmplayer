#include "fmdsp_platform_info.h"
#include <stdint.h>
#include <limits.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static struct {
  HANDLE currproc;
  uint64_t lastall;
  uint64_t lastcpu;
  uint64_t lastfpstime;
} g;

int fmdsp_cpu_usage(void) {
  if (!g.currproc) g.currproc = GetCurrentProcess();
  FILETIME ft_sys, ft_creat, ft_exit, ft_kern, ft_user;
  GetSystemTimeAsFileTime(&ft_sys);
  GetProcessTimes(g.currproc, &ft_creat, &ft_exit, &ft_kern, &ft_user);
  uint64_t all = ft_sys.dwHighDateTime;
  all <<= 32;
  all |= ft_sys.dwLowDateTime;
  uint64_t kern = ft_kern.dwHighDateTime;
  kern <<= 32;
  kern |= ft_kern.dwLowDateTime;
  uint64_t user = ft_user.dwHighDateTime;
  user <<= 32;
  user |= ft_user.dwLowDateTime;
  uint64_t cpu = kern + user;
  uint64_t alld = all - g.lastall;
  uint64_t cpud = cpu - g.lastcpu;
  int percentage = 0;
  if (alld) percentage = cpud * 100 / alld;
  g.lastall = all;
  g.lastcpu = cpu;
  if (!g.lastall) return 0;
  if (percentage > INT_MAX) percentage = INT_MAX;
  if (percentage < 0) percentage = 0;
  return percentage;
}

int fmdsp_fps_30(void) {
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t time = ft.dwHighDateTime;
  time <<= 32;
  time |= ft.dwLowDateTime;
  uint64_t fps = 0;
  if (g.lastfpstime) {
    uint64_t diff = time - g.lastfpstime;
    if (diff) {
      fps = 30ull * 10000000ull / diff;
    }
  }
  g.lastfpstime = time;
  if (fps > INT_MAX) fps = INT_MAX;
  return fps;
}
