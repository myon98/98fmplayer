#include "pulseout.h"

#include <stdbool.h>
#include <stdlib.h>
#include <pulse/thread-mainloop.h>
#include <pulse/stream.h>

struct pulseout_state {
  struct sound_state ss;
  bool paused;
  bool flush;
  unsigned srate;
  sound_callback cbfunc;
  void *userptr;
  pa_threaded_mainloop *pa_tm;
  pa_context *pa_c;
  pa_stream *pa_s;
  bool pa_status_changed;
};

#include <stdio.h>

static void pulseout_success_cb(pa_stream *s, int success, void *userdata) {
  (void)s;
  (void)success;
  struct pulseout_state *ps = userdata;
  pa_threaded_mainloop_signal(ps->pa_tm, 0);
}

static void pulseout_pause(struct sound_state *ss, int pause, int flush) {
  struct pulseout_state *ps = (struct pulseout_state *)ss;
  if (ps->paused != !!pause) {
    if (pause) {
      pa_threaded_mainloop_lock(ps->pa_tm);
      pa_operation *op_cork = pa_stream_cork(ps->pa_s, 1, pulseout_success_cb, ps);
      while (pa_operation_get_state(op_cork) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait(ps->pa_tm);
      pa_operation_unref(op_cork);
    }
    ps->paused = pause;
    if (!pause) {
      //if (flush) ps->flush = true;
      if (flush) {
        pa_operation *op_flush = pa_stream_flush(ps->pa_s, pulseout_success_cb, ps);
        if (op_flush) {
          while (pa_operation_get_state(op_flush) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(ps->pa_tm);
          pa_operation_unref(op_flush);
        } else {
          //fprintf(stderr, "FLUSH ERR\n");
        }
      }
      pa_operation *op_cork = pa_stream_cork(ps->pa_s, 0, pulseout_success_cb, ps);
      if (op_cork) {
        while (pa_operation_get_state(op_cork) == PA_OPERATION_RUNNING)
          pa_threaded_mainloop_wait(ps->pa_tm);
        pa_operation_unref(op_cork);
      } else {
        //fprintf(stderr, "CORK ERR\n");
      }
      pa_threaded_mainloop_unlock(ps->pa_tm);
    }
  }
}

static void pulseout_free(struct sound_state *ss) {
  struct pulseout_state *ps = (struct pulseout_state *)ss;
  if (ps) {
    if (ps->pa_tm) {
      if (ps->paused) pa_threaded_mainloop_unlock(ps->pa_tm);
      pa_threaded_mainloop_stop(ps->pa_tm);
      if (ps->pa_s) {
        pa_stream_disconnect(ps->pa_s);
        pa_stream_unref(ps->pa_s);
      }
      if (ps->pa_c) pa_context_unref(ps->pa_c);
      pa_threaded_mainloop_free(ps->pa_tm);
    }
    free(ps);
  }
}

static void pulseout_cb(pa_stream *p, size_t bytes, void *userdata) {
  struct pulseout_state *ps = userdata;
  int16_t *buf;
  pa_stream_begin_write(p, (void **)&buf, &bytes);
  size_t nframes = bytes / (2 * sizeof(int16_t));
  if (!ps->paused) {
    ps->cbfunc(ps->userptr, buf, nframes);
  } else {
    for (size_t i = 0; i < nframes; i++) {
      buf[i*2+0] = 0;
      buf[i*2+1] = 0;
    }
  }
  pa_seek_mode_t smode =
      ps->flush ? PA_SEEK_RELATIVE_ON_READ : PA_SEEK_RELATIVE;
  pa_stream_write(p, buf, nframes * 2 * sizeof(int16_t), 0, 0, smode);
  ps->flush = false;
}

static void pa_c_cb(pa_context *pa_c, void *userdata) {
  (void)pa_c;
  struct pulseout_state *ps = userdata;
  ps->pa_status_changed = true;
  pa_threaded_mainloop_signal(ps->pa_tm, 0);
}

struct sound_state *pulseout_init(
  const char *clientname, unsigned srate,
  sound_callback cbfunc, void *userptr) {
  struct pulseout_state *ps = malloc(sizeof(*ps));
  if (!ps) goto err;
  *ps = (struct pulseout_state){
    .ss = {
      .pause = pulseout_pause,
      .free = pulseout_free,
      .apiname = "PulseAudio",
    },
    .cbfunc = cbfunc,
    .userptr = userptr,
    .paused = false,
    .srate = srate,
  };
  ps->pa_tm = pa_threaded_mainloop_new();
  if (!ps->pa_tm) goto err;
  ps->pa_c = pa_context_new(
      pa_threaded_mainloop_get_api(ps->pa_tm), clientname
  );
  if (!ps->pa_c) goto err;
  if (pa_context_connect(ps->pa_c, 0, 0, 0) < 0) goto err;
  pa_context_set_state_callback(ps->pa_c, pa_c_cb, ps);
  if (pa_threaded_mainloop_start(ps->pa_tm) < 0) goto err;
  pa_threaded_mainloop_lock(ps->pa_tm);
  ps->paused = true;
  for (;;) {
    while (!ps->pa_status_changed) {
      pa_threaded_mainloop_wait(ps->pa_tm);
    }
    ps->pa_status_changed = false;
    pa_context_state_t state = pa_context_get_state(ps->pa_c);
    if (state == PA_CONTEXT_CONNECTING ||
        state == PA_CONTEXT_AUTHORIZING ||
        state == PA_CONTEXT_SETTING_NAME) continue;
    else if (state == PA_CONTEXT_READY) break;
    else goto err;
  }
  pa_sample_spec ss = {
    .format = PA_SAMPLE_S16NE,
    .rate = ps->srate,
    .channels = 2,
  };
  ps->pa_s = pa_stream_new(
      ps->pa_c,
      "stereoout", &ss, 0
  );
  if (!ps->pa_s) goto err;
  pa_stream_set_write_callback(ps->pa_s, pulseout_cb, ps);
  pa_buffer_attr battr = {
    .maxlength = -1,
    .tlength = pa_usec_to_bytes(1000*1000/30, &ss),
    .prebuf = -1,
    .minreq = -1,
    .fragsize = -1,
  };
  if (pa_stream_connect_playback(ps->pa_s, 0, &battr, PA_STREAM_ADJUST_LATENCY, 0, 0) < 0) goto err;
  return &ps->ss;
err:
  pulseout_free(&ps->ss);
  return 0;
}
