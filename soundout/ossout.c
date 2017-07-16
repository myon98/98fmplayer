#include <sys/stat.h>
#include <fcntl.h>

struct ossout_state {
  struct sound_state ss;
};

struct sound_state *ossout_init(
