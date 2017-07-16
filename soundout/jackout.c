#include "jackout.h"

#include <stdbool.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <soxr.h>

typedef jack_default_audio_sample_t sample_t;

enum {
  RFRAMES = 1024,
  JFRAMES = 2048,
};

struct jackout_state {
  struct sound_state ss;
  sound_callback cbfunc;
  void *userptr;
  jack_client_t *jc;
  jack_port_t *jp[2];
  soxr_t soxr;
  bool paused;
  int16_t rbuf[RFRAMES*2];
  float jbuf[JFRAMES*2];
};

static size_t soxr_cb(void *ptr, soxr_in_t *data, size_t nframes) {
  struct jackout_state *js = ptr;
  if (nframes > RFRAMES) nframes = RFRAMES;
  for (size_t i = 0; i < RFRAMES*2; i++) {
    js->rbuf[i] = 0;
  }
  js->cbfunc(js->userptr, js->rbuf, nframes);
  *data = js->rbuf;
  return nframes;
}

static int jack_cb(jack_nframes_t nframes, void *arg) {
  struct jackout_state *js = arg;
  sample_t *out[2];
  for (int i = 0; i < 2; i++) out[i] = jack_port_get_buffer(js->jp[i], nframes);
  jack_nframes_t outi = 0;
  while (nframes) {
    jack_nframes_t pframes = nframes < JFRAMES ? nframes : JFRAMES;
    pframes = soxr_output(js->soxr, js->jbuf, pframes);
    for (jack_nframes_t j = 0; j < pframes; j++) {
      out[0][outi] = js->jbuf[j*2+0];
      out[1][outi] = js->jbuf[j*2+1];
      outi++;
    }
    nframes -= pframes;
  }
  return 0;
}

static void jackout_pause(struct sound_state *ss, int pause, int flush) {
  struct jackout_state *js = (struct jackout_state *)ss;
  // TODO
  if (js->paused && !pause) {
    if (flush) {
      soxr_clear(js->soxr);
      soxr_set_input_fn(js->soxr, soxr_cb, js, RFRAMES);
    }
    jack_activate(js->jc);
    jack_connect(js->jc, jack_port_name(js->jp[0]), "system:playback_1");
    jack_connect(js->jc, jack_port_name(js->jp[1]), "system:playback_2");
  } else if (!js->paused && pause) {
    jack_deactivate(js->jc);
  }
  js->paused = pause;
}

static void jackout_free(struct sound_state *ss) {
  struct jackout_state *js = (struct jackout_state *)ss;
  if (js) {
    if (js->jc) {
      for (int i = 0; i < 2; i++) {
        if (js->jp[i]) jack_port_unregister(js->jc, js->jp[i]);
      }
      jack_client_close(js->jc);
    }
    if (js->soxr) soxr_delete(js->soxr);
    free(js);
  }
}

struct sound_state *jackout_init(
  const char *clientname, unsigned srate,
  sound_callback cbfunc, void *userptr) {
  struct jackout_state *js = malloc(sizeof(*js));
  if (!js) goto err;
  *js = (struct jackout_state){
    .ss = {
      .pause = jackout_pause,
      .free = jackout_free,
      .apiname = "JACK Audio",
    },
    .cbfunc = cbfunc,
    .userptr = userptr,
    .paused = true,
  };
  js->jc = jack_client_open(clientname, 0, 0);
  if (!js->jc) goto err;
  for (int i = 0; i < 2; i++) {
    js->jp[i] = jack_port_register(
        js->jc, i ? "right" : "left", JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput | JackPortIsTerminal, 0);
    if (!js->jp[i]) goto err;
  }
  soxr_io_spec_t iospec = {
    .itype = SOXR_INT16_I,
    .otype = SOXR_FLOAT32_I,
    .scale = 1.0,
  };
  js->soxr = soxr_create(
      srate, jack_get_sample_rate(js->jc),
      2, 0, &iospec, 0, 0
  );
  if (!js->soxr) goto err;
  soxr_set_input_fn(js->soxr, soxr_cb, js, RFRAMES);
  if (jack_set_process_callback(js->jc, jack_cb, js)) goto err;

  return &js->ss;
err:
  jackout_free(&js->ss);
  return 0;
}
