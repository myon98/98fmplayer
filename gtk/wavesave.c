#include "wavesave.h"
#include "common/fmplayer_file.h"
#include <sndfile.h>
#include <string.h>
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "common/fmplayer_common.h"
#include "configdialog.h"

enum {
  SRATE = 55467,
};

static void msgbox_err(GtkWindow *parent, const char *msg) {
  GtkWidget *d = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                          msg);
  gtk_dialog_run(GTK_DIALOG(d));
  gtk_widget_destroy(d);
}

static sf_count_t gio_get_filelen(void *ptr) {
  (void)ptr;
  return -1;
}

static sf_count_t gio_seek(sf_count_t offset, int whence, void *ptr) {
  GFileOutputStream *os = ptr;
  GSeekType st;
  switch (whence) {
  default:
    st = G_SEEK_CUR;
    break;
  case SEEK_SET:
    st = G_SEEK_SET;
    break;
  case SEEK_END:
    st = G_SEEK_END;
    break;
  }
  if (!g_seekable_seek(G_SEEKABLE(os), offset, st, 0, 0)) return -1;
  return g_seekable_tell(G_SEEKABLE(os));
}

static sf_count_t gio_read(void *buf, sf_count_t count, void *ptr) {
  (void)buf;
  (void)count;
  (void)ptr;
  return -1;
}

static sf_count_t gio_write(const void *buf, sf_count_t count, void *ptr) {
  GFileOutputStream *os = ptr;
  return g_output_stream_write(G_OUTPUT_STREAM(os), buf, count, 0, 0);
}

static sf_count_t gio_tell(void *ptr) {
  GFileOutputStream *os = ptr;
  return g_seekable_tell(G_SEEKABLE(os));
}

static SF_VIRTUAL_IO sf_virt_gio = {
  .get_filelen = gio_get_filelen,
  .seek = gio_seek,
  .read = gio_read,
  .write = gio_write,
  .tell = gio_tell,
};

struct fadeout {
  struct opna_timer *timer;
  struct fmdriver_work *work;
  uint64_t vol;
  uint8_t loopcnt;
};

static bool fadeout_mix(
  struct fadeout *fadeout,
  int16_t *buf, unsigned frames
) {
  opna_timer_mix(fadeout->timer, buf, frames);
  for (unsigned i = 0; i < frames; i++) {
    int vol = fadeout->vol >> 16;
    buf[i*2+0] = (buf[i*2+0] * vol) >> 16;
    buf[i*2+1] = (buf[i*2+1] * vol) >> 16;
    if (fadeout->work->loop_cnt >= fadeout->loopcnt) {
      fadeout->vol = (fadeout->vol * 0xffff0000ull) >> 32;
    }
  }
  return fadeout->vol;
}

struct thread_write_data {
  struct opna opna;
  struct opna_timer timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  struct fadeout fadeout;
  SNDFILE *sndfile;
  GtkDialog *dialog;
  GtkProgressBar *pbar;
  double fraction;
  uint8_t adpcm_ram[OPNA_ADPCM_RAM_SIZE];
};

static gboolean idle_close_dialog(gpointer ptr) {
  struct thread_write_data *data = ptr;
  gtk_dialog_response(data->dialog, GTK_RESPONSE_ACCEPT);
  return G_SOURCE_REMOVE;
}

static gboolean idle_progress_fraction(gpointer ptr) {
  struct thread_write_data *data = ptr;
  gtk_progress_bar_set_fraction(data->pbar, data->fraction);
  return G_SOURCE_REMOVE;
}

static gpointer thread_write(gpointer ptr) {
  struct thread_write_data *data = ptr;
  enum {
    BUFLEN = 1024,
  };
  int16_t buf[BUFLEN*2];
  for (;;) {
    memset(buf, 0, sizeof(buf));
    bool end = !fadeout_mix(&data->fadeout, buf, BUFLEN);
    if (sf_writef_short(data->sndfile, buf, BUFLEN) != BUFLEN) {
      return 0;
    }
    double newfrac = (double)data->work.timerb_cnt / data->work.loop_timerb_cnt;
    if ((newfrac - data->fraction) > 0.005) {
      data->fraction = newfrac;
      g_idle_add(idle_progress_fraction, ptr);
    }
    if (end) break;
  }
  g_idle_add(idle_close_dialog, ptr);
  return 0;
}

static void wavesave(GtkWindow *parent,
                     struct fmplayer_file *fmfile,
                     const char *saveuri,
                     bool flac,
                     int loopcnt) {
  if (loopcnt < 1) loopcnt = 1;
  if (loopcnt > 0xff) loopcnt = 0xff;
  GFile *savefile = g_file_new_for_uri(saveuri);
  GFileOutputStream *os = g_file_replace(savefile, 0, TRUE, 0, 0, 0);
  if (!os) {
    msgbox_err(parent, "Cannot open output file");
    g_object_unref(savefile);
    return;
  }
  SF_INFO sfinfo = {
    .samplerate = SRATE,
    .channels = 2,
    .format = (flac ? SF_FORMAT_FLAC : SF_FORMAT_WAV | SF_ENDIAN_CPU) | SF_FORMAT_PCM_16,
    .seekable = 1,
  };
  SNDFILE *sndfile = sf_open_virtual(&sf_virt_gio, SFM_WRITE, &sfinfo, os);
  if (!sndfile) {
    char *msg = g_strjoin(0, "SNDFILE Error: ", sf_strerror(sndfile), (char *)0);
    msgbox_err(parent, msg);
    g_free(msg);
    g_object_unref(os);
    g_object_unref(savefile);
    return;
  }
  struct thread_write_data *tdata = g_new0(struct thread_write_data, 1);
  fmplayer_init_work_opna(&tdata->work, &tdata->ppz8, &tdata->opna, &tdata->timer, tdata->adpcm_ram);
  opna_ssg_set_mix(&tdata->opna.ssg, fmplayer_config.ssg_mix);
  opna_ssg_set_ymf288(&tdata->opna.ssg, &tdata->opna.resampler, fmplayer_config.ssg_ymf288);
  ppz8_set_interpolation(&tdata->ppz8, fmplayer_config.ppz8_interp);
  opna_fm_set_hires_sin(&tdata->opna.fm, fmplayer_config.fm_hires_sin);
  opna_fm_set_hires_env(&tdata->opna.fm, fmplayer_config.fm_hires_env);
  fmplayer_file_load(&tdata->work, fmfile, loopcnt);
  tdata->fadeout.timer = &tdata->timer;
  tdata->fadeout.work = &tdata->work;
  tdata->fadeout.vol = 1ull<<32;
  tdata->fadeout.loopcnt = loopcnt;
  GtkWidget *dialog = gtk_dialog_new_with_buttons("Writing...",
                                                  parent,
                                                  GTK_DIALOG_MODAL,
                                                  "Cancel",
                                                  GTK_RESPONSE_CANCEL,
                                                  (void*)0);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_add(GTK_CONTAINER(content), gtk_label_new("Writing..."));
  GtkWidget *progress = gtk_progress_bar_new();
  gtk_container_add(GTK_CONTAINER(content), progress);
  tdata->sndfile = sndfile;
  tdata->dialog = GTK_DIALOG(dialog);
  tdata->pbar = GTK_PROGRESS_BAR(progress);
  tdata->fraction = 0.0;
  GThread *thread = g_thread_new("Write Worker", thread_write, tdata);
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  g_thread_join(thread);
  sf_close(sndfile);
  g_free(tdata);
  g_object_unref(os);
  g_object_unref(savefile);
}

enum {
  RESPONSE_IGNORE = 1,
};

void wavesave_dialog(GtkWindow *parent, const char *uri) {
  GtkWidget *fcd = gtk_file_chooser_dialog_new("Save wave file",
                                               parent,
                                               GTK_FILE_CHOOSER_ACTION_SAVE,
                                               "Cancel",
                                               GTK_RESPONSE_CANCEL,
                                               "Save",
                                               GTK_RESPONSE_ACCEPT,
                                               (void *)0
                                              );
  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  GtkWidget *typecombo = gtk_combo_box_text_new();
  gtk_widget_set_halign(typecombo, GTK_ALIGN_END);
  gtk_box_pack_start(GTK_BOX(hbox), typecombo, FALSE, FALSE, 5);
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(typecombo), 0, "RIFF WAVE (*.wav)");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(typecombo), 0, "FLAC (*.flac)");
  gtk_combo_box_set_active(GTK_COMBO_BOX(typecombo), 0);
  GtkWidget *loopbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_halign(loopbox, GTK_ALIGN_END);
  gtk_box_pack_start(GTK_BOX(hbox), loopbox, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(loopbox), gtk_label_new("Loop:"), FALSE, FALSE, 5);
  GtkWidget *loopspin = gtk_spin_button_new_with_range(1.0, 255.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(loopspin), 2.0);
  gtk_box_pack_start(GTK_BOX(loopbox), loopspin, FALSE, FALSE, 5);
  gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(fcd))),
                    hbox);
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fcd), TRUE);

  char *saveuri = 0;
  gtk_widget_show_all(fcd);
  gint res;
  while ((res = gtk_dialog_run(GTK_DIALOG(fcd))) == RESPONSE_IGNORE);
  if (res == GTK_RESPONSE_ACCEPT) {
    saveuri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(fcd));
  }
  int loopcnt = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(loopspin));
  bool flac = gtk_combo_box_get_active(GTK_COMBO_BOX(typecombo));
  gtk_widget_destroy(fcd);
  if (saveuri) {
    enum fmplayer_file_error error;
    struct fmplayer_file *fmfile = fmplayer_file_alloc(uri, &error);
    if (!fmfile) {
      char *msg = g_strjoin(0, "Cannot load file: ", fmplayer_file_strerror(error), (char *)0);
      msgbox_err(parent, msg);
      g_free(msg);
    } else {
      wavesave(parent, fmfile, saveuri, flac, loopcnt);
      fmplayer_file_free(fmfile);
    }
  }
  g_free(saveuri);
}
