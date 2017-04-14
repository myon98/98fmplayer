#include "fmdsp_platform_info.h"
#include <sys/times.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>

static struct {
  clock_t lastall;
  clock_t lastcpu;
  struct timespec lasttimespec;
} g;

int fmdsp_cpu_usage(void) {
  struct tms tmsbuf;
  clock_t all = times(&tmsbuf);
  clock_t cpu = tmsbuf.tms_utime + tmsbuf.tms_stime;
  clock_t percentage = 0;
  clock_t alld = all - g.lastall;
  clock_t cpud = cpu - g.lastcpu;
  if (alld) percentage = cpud * 100 / alld;
  g.lastall = all;
  g.lastcpu = cpu;
  if (!g.lastall) percentage = 0;
  if (percentage > INT_MAX) percentage = INT_MAX;
  if (percentage < 0) percentage = 0;
  return percentage;
}

int fmdsp_fps_30(void) {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  uint64_t fps = 0;
  if (g.lasttimespec.tv_sec || g.lasttimespec.tv_nsec) {
    uint64_t diffns = time.tv_sec - g.lasttimespec.tv_sec;
    diffns *= 1000000000ull;
    diffns += time.tv_nsec - g.lasttimespec.tv_nsec;
    if (diffns) {
      fps = 30ull * 1000000000ull / diffns;
    }
  }
  g.lasttimespec = time;
  if (fps > INT_MAX) fps = INT_MAX;
  return fps;
}
