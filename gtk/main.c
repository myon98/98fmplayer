#include <gtk/gtk.h>
#include <portaudio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <cairo.h>

#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/ppz8.h"
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdsp/fmdsp.h"

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
  PaStream *pastream;
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  struct fmdsp fmdsp;
  char drum_rom[OPNA_ROM_SIZE];
  bool drum_rom_loaded;
  char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  struct driver_fmp *fmp;
  void *fmpdata;
  void *ppzbuf;
  uint8_t vram[PC98_W*PC98_H];
  struct fmdsp_font font98;
  uint8_t font98data[FONT_ROM_FILESIZE];
  void *vram32;
  int vram32_stride;
} g;

static void quit(void) {
  if (g.pastream) {
    Pa_CloseStream(g.pastream);
  }
  if (g.pa_initialized) Pa_Terminate();
  free(g.fmp);
  free(g.fmpdata);
  free(g.ppzbuf);
  gtk_main_quit();
}

static void on_destroy(GtkWidget *w, gpointer ptr) {
  quit();
}

static void on_menu_quit(GtkMenuItem *menuitem, gpointer ptr) {
  quit();
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
  opna_timer_mix(timer, buf, frames);
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

static uint8_t opna_status_libopna(struct fmdriver_work *work, bool a1) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  uint8_t status = opna_timer_status(timer);
  if (!a1) {
    status &= 0x83;
  }
  return status;
}

static FILE *pvisearch(const char *filename, const char *pvibase) {
  // TODO: not SJIS aware
  char pviname[8+3+2] = {0};
  char pviname_l[8+3+2] = {0};
  strcpy(pviname, pvibase);
  strcat(pviname, ".PVI");
  strcpy(pviname_l, pviname);
  for (char *c = pviname_l; *c; c++) {
    if (('A' <= *c) && (*c <= 'Z')) {
      *c += ('a' - 'A');
    }
  }
  FILE *pvifile = fopen(pviname, "r");
  if (pvifile) return pvifile;
  pvifile = fopen(pviname_l, "r");
  if (pvifile) return pvifile;
  char *slash = strrchr(filename, '/');
  if (!slash) return 0;
  char *pvipath = malloc((slash-filename)+1+sizeof(pviname));
  if (!pvipath) return 0;
  memcpy(pvipath, filename, slash-filename+1);
  pvipath[slash-filename+1] = 0;
  strcat(pvipath, pviname);
  pvifile = fopen(pvipath, "r");
  if (pvifile) {
    free(pvipath);
    return pvifile;
  }
  pvipath[slash-filename+1] = 0;
  strcat(pvipath, pviname_l);
  pvifile = fopen(pvipath, "r");
  if (pvifile) {
    free(pvipath);
    return pvifile;
  }
  free(pvipath);
  return 0;
}

static bool loadpvi(struct fmdriver_work *work,
                    struct driver_fmp *fmp,
                    const char *filename) {
  // no need to load, always success
  if(strlen(fmp->pvi_name) == 0) return true;
  FILE *pvifile = pvisearch(filename, fmp->pvi_name);
  if (!pvifile) goto err;
  if (fseek(pvifile, 0, SEEK_END) != 0) goto err_file;
  size_t fsize;
  {
    long size = ftell(pvifile);
    if (size < 0) goto err_file;
    fsize = size;
  }
  if (fseek(pvifile, 0, SEEK_SET) != 0) goto err_file;
  void *data = malloc(fsize);
  if (!data) goto err_file;
  if (fread(data, 1, fsize, pvifile) != fsize) goto err_memory;
  if (!fmp_adpcm_load(work, data, fsize)) goto err_memory;
  free(data);
  fclose(pvifile);
  return true;
err_memory:
  free(data);
err_file:
  fclose(pvifile);
err:
  return false;
}

static bool loadppzpvi(struct fmdriver_work *work,
                    struct driver_fmp *fmp,
                    const char *filename) {
  // no need to load, always success
  if(strlen(fmp->ppz_name) == 0) return true;
  FILE *pvifile = pvisearch(filename, fmp->ppz_name);
  if (!pvifile) goto err;
  if (fseek(pvifile, 0, SEEK_END) != 0) goto err_file;
  size_t fsize;
  {
    long size = ftell(pvifile);
    if (size < 0) goto err_file;
    fsize = size;
  }
  if (fseek(pvifile, 0, SEEK_SET) != 0) goto err_file;
  void *data = malloc(fsize);
  if (!data) goto err_file;
  if (fread(data, 1, fsize, pvifile) != fsize) goto err_memory;
  int16_t *decbuf = calloc(ppz8_pvi_decodebuf_samples(fsize), sizeof(int16_t));
  if (!decbuf) goto err_memory;
  if (!ppz8_pvi_load(work->ppz8, 0, data, fsize, decbuf)) goto err_decbuf;
  free(g.ppzbuf);
  g.ppzbuf = decbuf;
  free(data);
  fclose(pvifile);
  return true;
err_decbuf:
  free(decbuf);
err_memory:
  free(data);
err_file:
  fclose(pvifile);
err:
  return false;
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

static bool openfile(const char *path) {
  if (!g.pa_initialized) {
    msgbox_err("Could not initialize Portaudio");
    goto err;
  }
  FILE *fmfile = fopen(path, "r");
  if (!fmfile) {
    msgbox_err("Cannot open file");
    goto err;
  }
  if (fseek(fmfile, 0, SEEK_END) != 0) {
    msgbox_err("cannot seek file to end");
    goto err_file;
  }
  size_t filelen;
  {
    long tfilelen = ftell(fmfile);
    if ((tfilelen < 0) || (tfilelen > 0xffff)) {
      msgbox_err("invalid file length");
      goto err_file;
    }
    filelen = tfilelen;
  }
  if (fseek(fmfile, 0, SEEK_SET) != 0) {
    msgbox_err("cannot seek file to beginning");
    goto err_file;
  }
  void *fmbuf = malloc(filelen);
  if (!fmbuf) {
    msgbox_err("cannot allocate memory for file");
    goto err_file;
  }
  if (fread(fmbuf, 1, filelen, fmfile) != filelen) {
    msgbox_err("cannot read file");
    goto err_fmbuf;
  }
  struct driver_fmp *fmp = calloc(1, sizeof(struct driver_fmp));
  if (!fmp) {
    msgbox_err("cannot allocate memory for fmp");
    goto err_fmbuf;
  }
  if (!fmp_load(fmp, fmbuf, filelen)) {
    msgbox_err("invalid FMP file");
   goto err_fmp;
  }
  if (!g.pastream) {
    PaError pe = Pa_OpenDefaultStream(&g.pastream, 0, 2, paInt16, SRATE, AUDIOBUFLEN,
                                      pastream_cb, &g.opna_timer);
    if (pe != paNoError) {
      msgbox_err("cannot open portaudio stream");
      goto err_fmp;
    }
  } else {
    PaError pe = Pa_StopStream(g.pastream);
    if (pe != paNoError) {
      msgbox_err("Portaudio Error");
      goto err_fmp;
    }
  }
  free(g.fmp);
  g.fmp = fmp;
  free(g.fmpdata);
  g.fmpdata = fmbuf;
  opna_reset(&g.opna);
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
  g.work.opna_status = opna_status_libopna;
  g.work.opna = &g.opna_timer;
  g.work.ppz8 = &g.ppz8;
  g.work.ppz8_functbl = &ppz8_functbl;
  opna_timer_set_int_callback(&g.opna_timer, opna_int_cb, &g.work);
  opna_timer_set_mix_callback(&g.opna_timer, opna_mix_cb, &g.ppz8);
  fmp_init(&g.work, g.fmp);
  fmdsp_vram_init(&g.fmdsp, &g.work, g.vram);
  loadpvi(&g.work, g.fmp, path);
  loadppzpvi(&g.work, g.fmp, path);
  fclose(fmfile);
  Pa_StartStream(g.pastream);
  return true;
err_fmp:
  free(fmp);
err_fmbuf:
  free(fmbuf);
err_file:
  fclose(fmfile);
err:
  return false;
}

static void on_file_activated(GtkFileChooser *chooser, gpointer ptr) {
  gchar *filename = gtk_file_chooser_get_filename(chooser);
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
  return menubar;
}

static gboolean draw_cb(GtkWidget *w,
                 cairo_t *cr,
                 gpointer p) {
  fmdsp_update(&g.fmdsp, &g.work, g.vram);
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
  return FALSE;
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
  gtk_widget_add_events(w, GDK_KEY_PRESS_MASK);
  gtk_widget_show_all(w);
  gtk_widget_add_tick_callback(w, tick_cb, drawarea, destroynothing);
  gtk_main();
  return 0;
}
