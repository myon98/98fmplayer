#include <gtk/gtk.h>
#include <portaudio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <cairo.h>
#include <stdatomic.h>

#include "common/fmplayer_file.h"
#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/fmdriver_pmd.h"
#include "fmdriver/ppz8.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdsp/fmdsp.h"
#include "toneview.h"
#include "oscillo/oscillo.h"
#include "oscilloview.h"

#define DATADIR "/.local/share/fmplayer/"
//#define FMDSP_2X

enum {
  SRATE = 55467,
  PPZ8MIX = 0xa000,
  AUDIOBUFLEN = 0,
};

static struct {
  GtkWidget *mainwin;
  bool pa_initialized;
  bool pa_paused;
  PaStream *pastream;
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  struct fmdsp fmdsp;
  char drum_rom[OPNA_ROM_SIZE];
  bool drum_rom_loaded;
  char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  struct fmplayer_file *fmfile;
  void *data;
  uint8_t vram[PC98_W*PC98_H];
  struct fmdsp_font font98;
  uint8_t font98data[FONT_ROM_FILESIZE];
  void *vram32;
  int vram32_stride;
  const char *current_uri;
  struct oscillodata oscillodata_audiothread[LIBOPNA_OSCILLO_TRACK_COUNT];
} g;

static void quit(void) {
  if (g.pastream) {
    Pa_CloseStream(g.pastream);
  }
  if (g.pa_initialized) Pa_Terminate();
  fmplayer_file_free(g.fmfile);
  gtk_main_quit();
}

static void on_destroy(GtkWidget *w, gpointer ptr) {
  quit();
}

static void on_menu_quit(GtkMenuItem *menuitem, gpointer ptr) {
  quit();
}

static void on_tone_view(GtkMenuItem *menuitem, gpointer ptr) {
  show_toneview();
}

static void on_oscillo_view(GtkMenuItem *menuitem, gpointer ptr) {
  show_oscilloview();
}

static void msgbox_err(const char *msg) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(g.mainwin), GTK_DIALOG_MODAL,
                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                          msg);
  gtk_dialog_run(GTK_DIALOG(d));
  gtk_widget_destroy(d);
}


static int pastream_cb(const void *inptr, void *outptr, unsigned long frames,
                       const PaStreamCallbackTimeInfo *timeinfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userdata) {
  struct opna_timer *timer = (struct opna_timer *)userdata;
  int16_t *buf = (int16_t *)outptr;
  memset(outptr, 0, sizeof(int16_t)*frames*2);
  opna_timer_mix_oscillo(timer, buf, frames, g.oscillodata_audiothread);

  if (!atomic_flag_test_and_set_explicit(
      &toneview_g.flag, memory_order_acquire)) {
    tonedata_from_opna(&toneview_g.tonedata, &g.opna);
    atomic_flag_clear_explicit(&toneview_g.flag, memory_order_release);
  }
  if (!atomic_flag_test_and_set_explicit(
    &oscilloview_g.flag, memory_order_acquire)) {
    memcpy(oscilloview_g.oscillodata, g.oscillodata_audiothread, sizeof(oscilloview_g.oscillodata));
    atomic_flag_clear_explicit(&oscilloview_g.flag, memory_order_release);
  }
  return paContinue;
}

static void opna_int_cb(void *userptr) {
  struct fmdriver_work *work = (struct fmdriver_work *)userptr;
  work->driver_opna_interrupt(work);
}

static void opna_mix_cb(void *userptr, int16_t *buf, unsigned samples) {
  struct ppz8 *ppz8 = (struct ppz8 *)userptr;
  ppz8_mix(ppz8, buf, samples);
}

static void opna_writereg_libopna(struct fmdriver_work *work, unsigned addr, unsigned data) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  opna_timer_writereg(timer, addr, data);
}

static unsigned opna_readreg_libopna(struct fmdriver_work *work, unsigned addr) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  return opna_readreg(&g.opna, addr);
}

static uint8_t opna_status_libopna(struct fmdriver_work *work, bool a1) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  uint8_t status = opna_timer_status(timer);
  if (!a1) {
    status &= 0x83;
  }
  return status;
}

static void load_drumrom(void) {
  const char *path = "ym2608_adpcm_rom.bin";
  const char *home = getenv("HOME");
  char *dpath = 0;
  if (home) {
    const char *datadir = DATADIR;
    dpath = malloc(strlen(home)+strlen(datadir)+strlen(path) + 1);
    if (dpath) {
      strcpy(dpath, home);
      strcat(dpath, datadir);
      strcat(dpath, path);
      path = dpath;
    }
  }
  FILE *rhythm = fopen(path, "r");
  free(dpath);
  if (!rhythm) goto err;
  if (fseek(rhythm, 0, SEEK_END) != 0) goto err_file;
  long size = ftell(rhythm);
  if (size != OPNA_ROM_SIZE) goto err_file;
  if (fseek(rhythm, 0, SEEK_SET) != 0) goto err_file;
  if (fread(g.drum_rom, 1, OPNA_ROM_SIZE, rhythm) != OPNA_ROM_SIZE) goto err_file;
  fclose(rhythm);
  g.drum_rom_loaded = true;
  return;
err_file:
  fclose(rhythm);
err:
  return;
}

static void load_fontrom(void) {
  const char *path = "font.rom";
  const char *home = getenv("HOME");
  char *dpath = 0;
  fmdsp_font_from_font_rom(&g.font98, g.font98data);
  if (home) {
    const char *datadir = DATADIR;
    dpath = malloc(strlen(home)+strlen(datadir)+strlen(path) + 1);
    if (dpath) {
      strcpy(dpath, home);
      strcat(dpath, datadir);
      strcat(dpath, path);
      path = dpath;
    }
  }
  FILE *font = fopen(path, "r");
  free(dpath);
  if (!font) goto err;
  if (fseek(font, 0, SEEK_END) != 0) goto err_file;
  long size = ftell(font);
  if (size != FONT_ROM_FILESIZE) goto err_file;
  if (fseek(font, 0, SEEK_SET) != 0) goto err_file;
  if (fread(g.font98data, 1, FONT_ROM_FILESIZE, font) != FONT_ROM_FILESIZE) {
    goto err_file;
  }
  fclose(font);
  return;
err_file:
  fclose(font);
err:
  return;
}

static bool openfile(const char *uri) {
  struct fmplayer_file *fmfile = 0;
  if (!g.pa_initialized) {
    msgbox_err("Could not initialize Portaudio");
    goto err;
  }
  enum fmplayer_file_error error;
  fmfile = fmplayer_file_alloc(uri, &error);
  if (!fmfile) {
    const char *errstr = fmplayer_file_strerror(error);
    const char *errmain = "cannot load file: ";
    char *errbuf = malloc(strlen(errstr) + strlen(errmain) + 1);
    if (errbuf) {
      strcpy(errbuf, errmain);
      strcat(errbuf, errstr);
    }
    msgbox_err(errbuf ? errbuf : "cannot load file");
    free(errbuf);
    goto err;
  }
  if (!g.pastream) {
    PaError pe = Pa_OpenDefaultStream(&g.pastream, 0, 2, paInt16, SRATE, AUDIOBUFLEN,
                                      pastream_cb, &g.opna_timer);
    if (pe != paNoError) {
      msgbox_err("cannot open portaudio stream");
      goto err;
    }
  } else if (!g.pa_paused) {
    PaError pe = Pa_StopStream(g.pastream);
    if (pe != paNoError) {
      msgbox_err("Portaudio Error");
      goto err;
    }
  }
  fmplayer_file_free(g.fmfile);
  g.fmfile = fmfile;
  unsigned mask = opna_get_mask(&g.opna);
  opna_reset(&g.opna);
  opna_set_mask(&g.opna, mask);
  if (!g.drum_rom_loaded) {
    load_drumrom();
  }
  if (g.drum_rom_loaded) {
    opna_drum_set_rom(&g.opna.drum, g.drum_rom);
  }
  opna_adpcm_set_ram_256k(&g.opna.adpcm, g.adpcm_ram);
  ppz8_init(&g.ppz8, SRATE, PPZ8MIX);
  opna_timer_reset(&g.opna_timer, &g.opna);
  memset(&g.work, 0, sizeof(g.work));
  g.work.opna_writereg = opna_writereg_libopna;
  g.work.opna_readreg = opna_readreg_libopna;
  g.work.opna_status = opna_status_libopna;
  g.work.opna = &g.opna_timer;
  g.work.ppz8 = &g.ppz8;
  g.work.ppz8_functbl = &ppz8_functbl;
  char *disppath = g_filename_from_uri(uri, 0, 0);
  if (disppath) {
    strncpy(g.work.filename, disppath, sizeof(g.work.filename)-1);
    g_free(disppath);
  } else {
    strncpy(g.work.filename, uri, sizeof(g.work.filename)-1);
  }
  opna_timer_set_int_callback(&g.opna_timer, opna_int_cb, &g.work);
  opna_timer_set_mix_callback(&g.opna_timer, opna_mix_cb, &g.ppz8);
  fmplayer_file_load(&g.work, g.fmfile);
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  Pa_StartStream(g.pastream);
  g.pa_paused = false;
  {
    const char *turi = strdup(uri);
    free(g.current_uri);
    g.current_uri = turi;
  }
  return true;
err:
  fmplayer_file_free(fmfile);
  return false;
}

static void on_file_activated(GtkFileChooser *chooser, gpointer ptr) {
  gchar *filename = gtk_file_chooser_get_uri(chooser);
  if (filename) {
    openfile(filename);
    g_free(filename);
  }
}

static GtkWidget *create_menubar() {
  GtkWidget *menubar = gtk_menu_bar_new();
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *file = gtk_menu_item_new_with_label("File");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);
  GtkWidget *open = gtk_menu_item_new_with_label("Open");
  //g_signal_connect(open, "activate", G_CALLBACK(on_menu_open), 0);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), open);
  GtkWidget *quit = gtk_menu_item_new_with_label("Quit");
  g_signal_connect(quit, "activate", G_CALLBACK(on_menu_quit), 0);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);

  GtkWidget *window = gtk_menu_item_new_with_label("Window");
  GtkWidget *filemenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(window), filemenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), window);
  GtkWidget *toneview = gtk_menu_item_new_with_label("Tone view");
  g_signal_connect(toneview, "activate", G_CALLBACK(on_tone_view), 0);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), toneview);
  GtkWidget *oscilloview = gtk_menu_item_new_with_label("Oscillo view");
  g_signal_connect(oscilloview, "activate", G_CALLBACK(on_oscillo_view), 0);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), oscilloview);
  return menubar;
}

static gboolean draw_cb(GtkWidget *w,
                 cairo_t *cr,
                 gpointer p) {
  fmdsp_update(&g.fmdsp, &g.work, &g.opna, g.vram);
  fmdsp_vrampalette(&g.fmdsp, g.vram, g.vram32, g.vram32_stride);
  cairo_surface_t *s = cairo_image_surface_create_for_data(
    g.vram32, CAIRO_FORMAT_RGB24, PC98_W, PC98_H, g.vram32_stride);
#ifdef FMDSP_2X
  cairo_scale(cr, 2.0, 2.0);
#endif
  cairo_set_source_surface(cr, s, 0.0, 0.0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
  cairo_paint(cr);
  cairo_surface_destroy(s);
  return FALSE;
}

static gboolean tick_cb(GtkWidget *w,
                        GdkFrameClock *frame_clock,
                        gpointer p) {
  (void)frame_clock;
  gtk_widget_queue_draw(GTK_WIDGET(p));
  return G_SOURCE_CONTINUE;
}

static void destroynothing(gpointer p) {
  (void)p;
}

static void mask_set(unsigned mask, bool shift) {
  if (shift) {
    opna_set_mask(&g.opna, ~mask);
  } else {
    opna_set_mask(&g.opna, opna_get_mask(&g.opna) ^ mask);
  }
}

static gboolean key_press_cb(GtkWidget *w,
                             GdkEvent *e,
                             gpointer ptr) {
  (void)w;
  (void)ptr;
  if (GDK_KEY_F1 <= e->key.keyval && e->key.keyval <= GDK_KEY_F10) {
    if (e->key.state & GDK_CONTROL_MASK) {
      fmdsp_palette_set(&g.fmdsp, e->key.keyval - GDK_KEY_F1);
      return TRUE;
    }
  }
  bool shift = e->key.state & GDK_CONTROL_MASK;
  switch (e->key.keyval) {
  case GDK_KEY_F6:
    if (g.current_uri) {
      openfile(g.current_uri);
    }
    break;
  case GDK_KEY_F7:
    if (g.pa_paused) {
      Pa_StartStream(g.pastream);
      g.pa_paused = false;
    } else {
      Pa_StopStream(g.pastream);
      g.pa_paused = true;
    }
    break;
  case GDK_KEY_F11:
    fmdsp_dispstyle_set(&g.fmdsp, (g.fmdsp.style+1) % FMDSP_DISPSTYLE_CNT);
    break;
  case GDK_KEY_1:
    mask_set(LIBOPNA_CHAN_FM_1, shift);
    break;
  case GDK_KEY_2:
    mask_set(LIBOPNA_CHAN_FM_2, shift);
    break;
  case GDK_KEY_3:
    mask_set(LIBOPNA_CHAN_FM_3, shift);
    break;
  case GDK_KEY_4:
    mask_set(LIBOPNA_CHAN_FM_4, shift);
    break;
  case GDK_KEY_5:
    mask_set(LIBOPNA_CHAN_FM_5, shift);
    break;
  case GDK_KEY_6:
    mask_set(LIBOPNA_CHAN_FM_6, shift);
    break;
  case GDK_KEY_7:
    mask_set(LIBOPNA_CHAN_SSG_1, shift);
    break;
  case GDK_KEY_8:
    mask_set(LIBOPNA_CHAN_SSG_2, shift);
    break;
  case GDK_KEY_9:
    mask_set(LIBOPNA_CHAN_SSG_3, shift);
    break;
  case GDK_KEY_0:
    mask_set(LIBOPNA_CHAN_DRUM_ALL, shift);
    break;
  case GDK_KEY_minus:
    mask_set(LIBOPNA_CHAN_ADPCM, shift);
    break;
  // jp106 / pc98
  case GDK_KEY_asciicircum:
  // us
  case GDK_KEY_equal:
    opna_set_mask(&g.opna, ~opna_get_mask(&g.opna));
    break;
  case GDK_KEY_backslash:
    opna_set_mask(&g.opna, 0);
    break;
  default:
    return FALSE;
  }
  return TRUE;
}

static void drag_data_recv_cb(
  GtkWidget *w,
  GdkDragContext *ctx,
  gint x, gint y,
  GtkSelectionData *data,
  guint info, guint time, gpointer ptr) {
  (void)x;
  (void)y;
  (void)info;
  (void)ptr;
  gchar **uris = gtk_selection_data_get_uris(data);
  if (uris && uris[0]) {
    openfile(uris[0]);
  }
  g_strfreev(uris);
  gtk_drag_finish(ctx, TRUE, FALSE, time);
}

int main(int argc, char **argv) {
  load_fontrom();
  gtk_init(&argc, &argv);
  GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g.mainwin = w;
  //gtk_window_set_resizable(GTK_WINDOW(w), FALSE);
  gtk_window_set_title(GTK_WINDOW(w), "FMPlayer");
  g_signal_connect(w, "destroy", G_CALLBACK(on_destroy), 0);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(w), box);

  GtkWidget *menubar = create_menubar();
  gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, TRUE, 0);

  GtkWidget *hbox;
  {
    GtkOrientation o = GTK_ORIENTATION_VERTICAL;
#ifdef FMDSP_2X
    o = GTK_ORIENTATION_HORIZONTAL;
#endif
    hbox = gtk_box_new(o, 0);
  }
  gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, TRUE, 0);
  
  GtkWidget *drawarea = gtk_drawing_area_new();
  {
    gint ww = PC98_W;
    gint wh = PC98_H;
#ifdef FMDSP_2X
    ww *= 2;
    wh *= 2;
#endif
    gtk_widget_set_size_request(drawarea, ww, wh);
  }
  g_signal_connect(drawarea, "draw", G_CALLBACK(draw_cb), 0);
  gtk_box_pack_start(GTK_BOX(hbox), drawarea, FALSE, TRUE, 0);

  GtkWidget *filechooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
  g_signal_connect(filechooser, "file-activated", G_CALLBACK(on_file_activated), 0);
  gtk_box_pack_start(GTK_BOX(hbox), filechooser, TRUE, TRUE, 0);

  
  g.pa_initialized = (Pa_Initialize() == paNoError);
  fmdsp_init(&g.fmdsp, &g.font98);
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  g.vram32_stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, PC98_W);
  g.vram32 = malloc((g.vram32_stride*PC98_H)*4);

  g_signal_connect(w, "key-press-event", G_CALLBACK(key_press_cb), 0);
  gtk_drag_dest_set(
    w, GTK_DEST_DEFAULT_MOTION|GTK_DEST_DEFAULT_HIGHLIGHT|GTK_DEST_DEFAULT_DROP,
    0, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets(w);
  g_signal_connect(w, "drag-data-received", G_CALLBACK(drag_data_recv_cb), 0);
  gtk_widget_add_events(w, GDK_KEY_PRESS_MASK);
  gtk_widget_show_all(w);
  gtk_widget_add_tick_callback(w, tick_cb, drawarea, destroynothing);
  gtk_main();
  return 0;
}
