#include "alsaout.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <asoundlib.h>

enum {
  BUF_FRAMES = 1024,
};

struct alsaout_state {
  struct sound_state ss;
  sound_callback cbfunc;
  void *userptr;
  snd_pcm_t *apcm;
  int fd_event;
  int fds_space;
  struct pollfd *fds;
  bool paused;
  bool terminate;
  atomic_flag cb_flag;
  bool thread_valid;
  pthread_t alsa_thread;
  int16_t buf[BUF_FRAMES*2];
};

static void *alsaout_thread(void *ptr) {
  struct alsaout_state *as = ptr;
  for (;;) {
    as->fds[0] = (struct pollfd) {
      .fd = as->fd_event,
      .events = POLLIN,
    };
    int fd_cnt = 1;
    fd_cnt += snd_pcm_poll_descriptors(
        as->apcm, as->fds + 1, as->fds_space - 1
    );
    int err = poll(as->fds, fd_cnt, -1);
    if (err <= 0) {
      continue;
    }
    if (as->terminate) return 0;
    if (as->fds[0].revents) {
      uint64_t eventdata;
      ssize_t t = read(as->fd_event, &eventdata, sizeof(eventdata));
      (void)t;
    }
    unsigned short event;
    if (snd_pcm_poll_descriptors_revents(
        as->apcm, as->fds + 1, fd_cnt - 1, &event) < 0) continue;
    if (!event) continue;
    snd_pcm_sframes_t frames = snd_pcm_avail_update(as->apcm);
    if (frames <= 0) continue;
    while (frames) {
      snd_pcm_sframes_t genframes = frames;
      if (genframes > BUF_FRAMES) genframes = BUF_FRAMES;
      while (atomic_flag_test_and_set_explicit(
          &as->cb_flag, memory_order_acquire));
      if (as->paused) {
        atomic_flag_clear_explicit(&as->cb_flag, memory_order_release);
        break;
      }
      as->cbfunc(as->userptr, as->buf, genframes);
      atomic_flag_clear_explicit(&as->cb_flag, memory_order_release);
      frames -= genframes;
      if (snd_pcm_state(as->apcm) == SND_PCM_STATE_XRUN) {
        err = snd_pcm_prepare(as->apcm);
      }
      snd_pcm_sframes_t written = snd_pcm_writei(as->apcm, as->buf, genframes);
      if (written < 0) {
        snd_pcm_prepare(as->apcm);
      }
    }
  }
}

static void alsaout_pause(struct sound_state *ss, int pause, int flush) {
  struct alsaout_state *as = (struct alsaout_state *)ss;
  
  as->paused = pause;
  if (!pause && flush) {
    snd_pcm_drop(as->apcm);
    snd_pcm_prepare(as->apcm);
    snd_pcm_start(as->apcm);
  } else {
    snd_pcm_pause(as->apcm, pause);
  }
  while (atomic_flag_test_and_set_explicit(
      &as->cb_flag, memory_order_acquire));
  atomic_flag_clear_explicit(&as->cb_flag, memory_order_release);
  uint64_t event = 1;
  ssize_t t = write(as->fd_event, &event, sizeof(event));
  (void)t;
}

static void alsaout_free(struct sound_state *ss) {
  if (!ss) return;
  struct alsaout_state *as = (struct alsaout_state *)ss;
  if (as->thread_valid) {
    as->terminate = true;
    uint64_t event = 1;
    ssize_t t = write(as->fd_event, &event, sizeof(event));
    (void)t;
    pthread_join(as->alsa_thread, 0);
  }
  if (as->fd_event != -1) {
    close(as->fd_event);
  }
  if (as->fds) free(as->fds);
  if (as->apcm) {
    snd_pcm_close(as->apcm);
  }
  free(as);
}

struct sound_state *alsaout_init(
  const char *clientname, unsigned srate,
  sound_callback cbfunc, void *userptr) {
  (void)clientname;
  struct alsaout_state *as = malloc(sizeof(*as));
  if (!as) goto err;
  *as = (struct alsaout_state) {
    .ss = {
      .pause = alsaout_pause,
      .free = alsaout_free,
      .apiname = "ALSA",
    },
    .cbfunc = cbfunc,
    .userptr = userptr,
    .fd_event = -1,
    .cb_flag = ATOMIC_FLAG_INIT,
    .paused = true,
  };
  if (snd_pcm_open(&as->apcm, "default", SND_PCM_STREAM_PLAYBACK, 0)) {
    goto err;
  }
  as->fd_event = eventfd(0, EFD_CLOEXEC);
  if (as->fd_event < 0) goto err;
  as->fds_space = snd_pcm_poll_descriptors_count(as->apcm) + 1;
  as->fds = malloc(sizeof(*as->fds) * as->fds_space);
  if (!as->fds) goto err;
  if (snd_pcm_set_params(
      as->apcm,
      SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, srate, 1,
      1000*1000/60)) {
    goto err;
  }
  if (pthread_create(&as->alsa_thread, 0, alsaout_thread, as)) {
    goto err;
  }
  as->thread_valid = true;
  return &as->ss;

err:
  alsaout_free(&as->ss);
  return 0;
}
