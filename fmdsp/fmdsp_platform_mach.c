#include "fmdsp_platform_info.h"
#include <mach/mach.h>
#include <mach/clock.h>
#include <sys/times.h>
#include <stdbool.h>
#include <limits.h>

static struct {
  clock_t lastall;
  clock_t lastcpu;
  mach_timespec_t lasttimespec;
  bool clock_initialized;
  clock_serv_t clock;
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
  if (!g.clock_initialized) {
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &g.clock);
    g.clock_initialized = true;
  }
  mach_timespec_t time;
  clock_get_time(g.clock, &time);
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
