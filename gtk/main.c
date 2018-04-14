#include <gtk/gtk.h>
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
#include "pacc/pacc.h"
#include "fmdsp/fmdsp-pacc.h"
#include "fmdsp/font.h"
#include "toneview.h"
#include "oscillo/oscillo.h"
#include "oscilloview.h"
#include "wavesave.h"
#include "configdialog.h"
#include "common/fmplayer_common.h"
#include "common/fmplayer_fontrom.h"
#include "fft/fft.h"

#include "soundout.h"

#include "fmplayer.xpm"
#include "fmplayer32.xpm"

//#define FMDSP_2X

enum {
  SRATE = 55467,
//  SRATE = 55555,
  PPZ8MIX = 0xa000,
  AUDIOBUFLEN = 0,
};

static struct {
  GtkWidget *mainwin;
  bool fmdsp_2x;
  GtkWidget *root_box_widget;
  GtkWidget *box_widget;
  GtkWidget *fmdsp_widget;
  GtkWidget *filechooser_widget;
  bool sound_paused;
  struct sound_state *ss;
  atomic_flag opna_flag;
  struct opna opna;
  struct opna_timer opna_timer;
  struct ppz8 ppz8;
  struct fmdriver_work work;
  char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
  struct fmplayer_file *fmfile;
  void *data;
  uint8_t vram[PC98_W*PC98_H];
  struct fmdsp_font font98;
  const char *current_uri;
  bool oscillo_should_update;
  struct oscillodata oscillodata_audiothread[LIBOPNA_OSCILLO_TRACK_COUNT];
  atomic_flag at_fftdata_flag;
  struct fmplayer_fft_data at_fftdata;
  struct fmplayer_fft_input_data fftdata;
  struct pacc_vtable pacc;
  struct pacc_ctx *pc;
  struct fmdsp_pacc *fp;
} g = {
  .oscillo_should_update = true,
  .opna_flag = ATOMIC_FLAG_INIT,
  .at_fftdata_flag = ATOMIC_FLAG_INIT,
};

static void quit(void) {
  if (g.ss) {
    g.ss->free(g.ss);
  }
  fmplayer_file_free(g.fmfile);
  gtk_main_quit();
}

static void on_destroy(GtkWidget *w, gpointer ptr) {
  (void)w;
  (void)ptr;
  quit();
}

static void on_menu_quit(GtkMenuItem *menuitem, gpointer ptr) {
  (void)menuitem;
  (void)ptr;
  quit();
}

static void on_menu_save(GtkMenuItem *menuitem, gpointer ptr) {
  (void)menuitem;
  (void)ptr;
  if (g.current_uri) {
    char *uri = g_strdup(g.current_uri);
    wavesave_dialog(GTK_WINDOW(g.mainwin), uri);
    g_free(uri);
  }
}

static void on_tone_view(GtkMenuItem *menuitem, gpointer ptr) {
  (void)menuitem;
  (void)ptr;
  show_toneview();
}

static void on_oscillo_view(GtkMenuItem *menuitem, gpointer ptr) {
  (void)menuitem;
  (void)ptr;
  show_oscilloview();
}

static void config_update(void *ptr) {
  (void)ptr;
  while (atomic_flag_test_and_set_explicit(&g.opna_flag, memory_order_acquire));
  opna_ssg_set_mix(&g.opna.ssg, fmplayer_config.ssg_mix);
  opna_ssg_set_ymf288(&g.opna.ssg, &g.opna.resampler, fmplayer_config.ssg_ymf288);
  ppz8_set_interpolation(&g.ppz8, fmplayer_config.ppz8_interp);
  opna_fm_set_hires_sin(&g.opna.fm, fmplayer_config.fm_hires_sin);
  opna_fm_set_hires_env(&g.opna.fm, fmplayer_config.fm_hires_env);
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);
}

static void on_config(GtkMenuItem *menuitem, gpointer ptr) {
  (void)menuitem;
  (void)ptr;
  show_configdialog(config_update, 0);
}

static void msgbox_err(const char *msg) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(g.mainwin), GTK_DIALOG_MODAL,
                          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                          "%s", msg);
  gtk_dialog_run(GTK_DIALOG(d));
  gtk_widget_destroy(d);
}

static void soundout_cb(void *userptr, int16_t *buf, unsigned frames) {
  struct opna_timer *timer = (struct opna_timer *)userptr;
  memset(buf, 0, sizeof(int16_t)*frames*2);
  while (atomic_flag_test_and_set_explicit(&g.opna_flag, memory_order_acquire));
  opna_timer_mix_oscillo(timer, buf, frames,
                         g.oscillo_should_update ?
                         g.oscillodata_audiothread : 0);
  atomic_flag_clear_explicit(&g.opna_flag, memory_order_release);
  if (!atomic_flag_test_and_set_explicit(
      &toneview_g.flag, memory_order_acquire)) {
    tonedata_from_opna(&toneview_g.tonedata, &g.opna);
    atomic_flag_clear_explicit(&toneview_g.flag, memory_order_release);
  }
  if (g.oscillo_should_update) {
    if (!atomic_flag_test_and_set_explicit(
      &oscilloview_g.flag, memory_order_acquire)) {
      memcpy(oscilloview_g.oscillodata, g.oscillodata_audiothread, sizeof(oscilloview_g.oscillodata));
      atomic_flag_clear_explicit(&oscilloview_g.flag, memory_order_release);
    }
  }
  if (!atomic_flag_test_and_set_explicit(
    &g.at_fftdata_flag, memory_order_acquire)) {
    fft_write(&g.at_fftdata, buf, frames);
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
}

static bool openfile(const char *uri) {
  struct fmplayer_file *fmfile = 0;
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
  if (!g.ss) {
    g.ss = sound_init("98FMPlayer", SRATE, soundout_cb, &g.opna_timer);
    if (!g.ss) {
      msgbox_err("cannot open audio stream");
      goto err;
    }
  } else if (!g.sound_paused) {
    g.ss->pause(g.ss, 1, 0);
  }
  fmplayer_file_free(g.fmfile);
  g.fmfile = fmfile;
  unsigned mask = opna_get_mask(&g.opna);
  g.work = (struct fmdriver_work){0};
  memset(g.adpcm_ram, 0, sizeof(g.adpcm_ram));
  fmplayer_init_work_opna(&g.work, &g.ppz8, &g.opna, &g.opna_timer, g.adpcm_ram);
  opna_set_mask(&g.opna, mask);
  config_update(0);
  char *disppath = g_filename_from_uri(uri, 0, 0);
  if (disppath) {
    strncpy(g.work.filename, disppath, sizeof(g.work.filename)-1);
    g_free(disppath);
  } else {
    strncpy(g.work.filename, uri, sizeof(g.work.filename)-1);
  }
  fmplayer_file_load(&g.work, g.fmfile, 1);
  if (g.fmfile->filename_sjis) {
    fmdsp_pacc_set_filename_sjis(g.fp, g.fmfile->filename_sjis);
  }
  fmdsp_pacc_update_file(g.fp);
  fmdsp_pacc_comment_reset(g.fp);
  g.ss->pause(g.ss, 0, 1);
  g.sound_paused = false;
  g.work.paused = false;
  {
    const char *turi = strdup(uri);
    free((void *)g.current_uri);
    g.current_uri = turi;
  }
  return true;
err:
  fmplayer_file_free(fmfile);
  return false;
}

static void on_file_activated(GtkFileChooser *chooser, gpointer ptr) {
  (void)ptr;
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
  GtkWidget *save = gtk_menu_item_new_with_label("Save wavefile");
  g_signal_connect(save, "activate", G_CALLBACK(on_menu_save), 0);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), save);
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
  GtkWidget *config = gtk_menu_item_new_with_label("Config");
  g_signal_connect(config, "activate", G_CALLBACK(on_config), 0);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), config);
  return menubar;
}

static void realize_cb(GtkWidget *w, gpointer ptr) {
  GtkGLArea *glarea = GTK_GL_AREA(w);
  (void)ptr;
  gtk_gl_area_make_current(glarea);
  if (gtk_gl_area_get_error(glarea)) {
    msgbox_err("cannot make OpenGL context current");
    quit();
  }
  g.pc = pacc_init_gl(PC98_W, PC98_H, &g.pacc);
  if (!g.pc) {
    msgbox_err("cannot initialize OpenGL context");
    quit();
  }
  if (!fmdsp_pacc_init(g.fp, g.pc, &g.pacc)) {
    msgbox_err("cannot initialize fmdsp");
    quit();
  }
  fmdsp_pacc_set_font16(g.fp, &g.font98);
  fmdsp_pacc_set(g.fp, &g.work, &g.opna, &g.fftdata);
}

static void unrealize_cb(GtkWidget *w, gpointer ptr) {
  GtkGLArea *glarea = GTK_GL_AREA(w);
  (void)ptr;
  gtk_gl_area_make_current(glarea);
  fmdsp_pacc_deinit(g.fp);
  g.pacc.pacc_delete(g.pc);
  g.pc = 0;
}

static gboolean render_cb(GtkGLArea *glarea, GdkGLContext *glctx, gpointer ptr) {
  (void)glarea;
  (void)glctx;
  (void)ptr;
  if (!atomic_flag_test_and_set_explicit(
    &g.at_fftdata_flag, memory_order_acquire)) {
    memcpy(&g.fftdata.fdata, &g.at_fftdata, sizeof(g.fftdata.fdata));
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
  fmdsp_pacc_render(g.fp);
  return TRUE;
}

/*
static gboolean draw_cb(GtkWidget *w,
                 cairo_t *cr,
                 gpointer p) {
  (void)w;
  (void)p;
  if (!atomic_flag_test_and_set_explicit(
    &g.at_fftdata_flag, memory_order_acquire)) {
    memcpy(&g.fftdata.fdata, &g.at_fftdata, sizeof(g.fftdata));
    atomic_flag_clear_explicit(&g.at_fftdata_flag, memory_order_release);
  }
  //fmdsp_update(&g.fmdsp, &g.work, &g.opna, g.vram, &g.fftdata);
  fmdsp_vrampalette(&g.fmdsp, g.vram, g.vram32, g.vram32_stride);
  cairo_surface_t *s = cairo_image_surface_create_for_data(
    g.vram32, CAIRO_FORMAT_RGB24, PC98_W, PC98_H, g.vram32_stride);
  if (g.fmdsp_2x) cairo_scale(cr, 2.0, 2.0);
  cairo_set_source_surface(cr, s, 0.0, 0.0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
  cairo_paint(cr);
  cairo_surface_destroy(s);
  return FALSE;
}

*/

static gboolean tick_cb(GtkWidget *w,
                        GdkFrameClock *frame_clock,
                        gpointer p) {
  (void)w;
  (void)frame_clock;
  (void)p;
  gtk_widget_queue_draw(g.fmdsp_widget);
  return G_SOURCE_CONTINUE;
}

/*
static void destroynothing(gpointer p) {
  (void)p;
}
*/

static void mask_set(int p, bool shift, bool control) {
  if (!control) {
    if (p >= 11) return;
    static const unsigned masktbl[11] = {
      LIBOPNA_CHAN_FM_1,
      LIBOPNA_CHAN_FM_2,
      LIBOPNA_CHAN_FM_3,
      LIBOPNA_CHAN_FM_4,
      LIBOPNA_CHAN_FM_5,
      LIBOPNA_CHAN_FM_6,
      LIBOPNA_CHAN_SSG_1,
      LIBOPNA_CHAN_SSG_2,
      LIBOPNA_CHAN_SSG_3,
      LIBOPNA_CHAN_DRUM_ALL,
      LIBOPNA_CHAN_ADPCM,
    };
    unsigned mask = masktbl[p];
    if (shift) {
      opna_set_mask(&g.opna, ~mask);
      ppz8_set_mask(&g.ppz8, -1);
    } else {
      opna_set_mask(&g.opna, opna_get_mask(&g.opna) ^ mask);
    }
  } else {
    if (p >= 8) return;
    unsigned mask = 1u<<p;
    if (shift) {
      ppz8_set_mask(&g.ppz8, ~mask);
      opna_set_mask(&g.opna, -1);
    } else {
      ppz8_set_mask(&g.ppz8, ppz8_get_mask(&g.ppz8) ^ mask);
    }
  }
}

static void create_box(void) {
  if (g.box_widget) {
    g_object_ref(G_OBJECT(g.fmdsp_widget));
    gtk_container_remove(GTK_CONTAINER(g.box_widget), g.fmdsp_widget);
    g_object_ref(G_OBJECT(g.filechooser_widget));
    gtk_container_remove(GTK_CONTAINER(g.box_widget), g.filechooser_widget);
    gtk_container_remove(GTK_CONTAINER(g.root_box_widget), g.box_widget);
  }
  gtk_widget_set_size_request(g.fmdsp_widget,
                              PC98_W * (g.fmdsp_2x + 1),
                              PC98_H * (g.fmdsp_2x + 1));
  GtkOrientation o = g.fmdsp_2x ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
  g.box_widget = gtk_box_new(o, 0);
  gtk_box_pack_start(GTK_BOX(g.root_box_widget), g.box_widget, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(g.box_widget), g.fmdsp_widget, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(g.box_widget), g.filechooser_widget, TRUE, TRUE, 0);
  gtk_widget_show_all(g.root_box_widget);
}

static gboolean key_press_cb(GtkWidget *w,
                             GdkEvent *e,
                             gpointer ptr) {
  (void)w;
  (void)ptr;
  const GdkModifierType ALLACCELS = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK;
  if (GDK_KEY_F1 <= e->key.keyval && e->key.keyval <= GDK_KEY_F10) {
    if ((e->key.state & ALLACCELS) == GDK_CONTROL_MASK) {
      fmdsp_pacc_palette(g.fp, e->key.keyval - GDK_KEY_F1);
      return TRUE;
    }
  }
  guint keyval;
  gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(
                                        gtk_widget_get_display(w)),
                                      e->key.hardware_keycode,
                                      0,
                                      e->key.group,
                                      &keyval, 0, 0, 0);
  bool shift = e->key.state & GDK_SHIFT_MASK;
  bool ctrl = e->key.state & GDK_CONTROL_MASK;
  switch (keyval) {
  case GDK_KEY_F6:
    if (g.current_uri) {
      openfile(g.current_uri);
    }
    break;
  case GDK_KEY_F7:
    if (g.ss) {
      g.sound_paused ^= 1;
      g.work.paused = g.sound_paused;
      g.ss->pause(g.ss, g.sound_paused, 0);
    }
    break;
  case GDK_KEY_F11:
    if (shift) {
      fmdsp_pacc_set_right_mode(
          g.fp,
          (fmdsp_pacc_right_mode(g.fp)+1) % FMDSP_RIGHT_MODE_CNT);
    } else {
      fmdsp_pacc_set_left_mode(
          g.fp,
          (fmdsp_pacc_left_mode(g.fp)+1) % FMDSP_LEFT_MODE_CNT);
    }
    break;
  case GDK_KEY_F12:
    g.fmdsp_2x ^= 1;
    create_box();
    break;
  case GDK_KEY_1:
    mask_set(0, shift, ctrl);
    break;
  case GDK_KEY_2:
    mask_set(1, shift, ctrl);
    break;
  case GDK_KEY_3:
    mask_set(2, shift, ctrl);
    break;
  case GDK_KEY_4:
    mask_set(3, shift, ctrl);
    break;
  case GDK_KEY_5:
    mask_set(4, shift, ctrl);
    break;
  case GDK_KEY_6:
    mask_set(5, shift, ctrl);
    break;
  case GDK_KEY_7:
    mask_set(6, shift, ctrl);
    break;
  case GDK_KEY_8:
    mask_set(7, shift, ctrl);
    break;
  case GDK_KEY_9:
    mask_set(8, shift, ctrl);
    break;
  case GDK_KEY_0:
    mask_set(9, shift, ctrl);
    break;
  case GDK_KEY_minus:
    mask_set(10, shift, ctrl);
    break;
  // jp106 / pc98
  case GDK_KEY_asciicircum:
  // us
  case GDK_KEY_equal:
    opna_set_mask(&g.opna, ~opna_get_mask(&g.opna));
    ppz8_set_mask(&g.ppz8, ~ppz8_get_mask(&g.ppz8));
    break;
  case GDK_KEY_backslash:
    opna_set_mask(&g.opna, 0);
    ppz8_set_mask(&g.ppz8, 0);
    break;
  case GDK_KEY_Up:
  case GDK_KEY_Down:
    if (shift) {
      fmdsp_pacc_comment_scroll(g.fp, keyval == GDK_KEY_Down);
    } else {
      return FALSE;
    }
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
  (void)w;
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
#ifdef ENABLE_NEON
  opna_ssg_sinc_calc_func = opna_ssg_sinc_calc_neon;
#endif
#ifdef ENABLE_SSE
  if (__builtin_cpu_supports("sse2")) opna_ssg_sinc_calc_func = opna_ssg_sinc_calc_sse2;
#endif
  fft_init_table();
  fmplayer_font_rom_load(&g.font98);
  gtk_init(&argc, &argv);
  {
    GList *iconlist = 0;
    iconlist = g_list_append(iconlist, gdk_pixbuf_new_from_xpm_data(fmplayer_xpm_16));
    iconlist = g_list_append(iconlist, gdk_pixbuf_new_from_xpm_data(fmplayer_xpm_32));
    gtk_window_set_default_icon_list(iconlist);
    g_list_free(iconlist);
  }
  GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g.mainwin = w;
  gtk_window_set_resizable(GTK_WINDOW(w), FALSE);
  gtk_window_set_title(GTK_WINDOW(w), "98FMPlayer");
  g_signal_connect(w, "destroy", G_CALLBACK(on_destroy), 0);
  g.root_box_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(w), g.root_box_widget);

  GtkWidget *menubar = create_menubar();
  gtk_box_pack_start(GTK_BOX(g.root_box_widget), menubar, FALSE, TRUE, 0);

  g.fmdsp_widget = gtk_gl_area_new();
  g.fp = fmdsp_pacc_alloc();
  if (!g.fp) {
    msgbox_err("cannot alloc fmdsp");
    quit();
  }
#ifdef PACC_GL_ES
  gtk_gl_area_set_use_es(GTK_GL_AREA(g.fmdsp_widget), TRUE);
#ifdef PACC_GL_3
  gtk_gl_area_set_required_version(GTK_GL_AREA(g.fmdsp_widget), 2, 0);
#else
  gtk_gl_area_set_required_version(GTK_GL_AREA(g.fmdsp_widget), 2, 0);
#endif
#else // PACC_GL_ES
#ifdef PACC_GL_3
  gtk_gl_area_set_required_version(GTK_GL_AREA(g.fmdsp_widget), 3, 2);
#else
  gtk_gl_area_set_required_version(GTK_GL_AREA(g.fmdsp_widget), 2, 0);
#endif
#endif // PACC_GL_ES
  gtk_widget_add_tick_callback(g.fmdsp_widget, tick_cb, 0, 0);
  g_signal_connect(g.fmdsp_widget, "render", G_CALLBACK(render_cb), 0);
  g_signal_connect(g.fmdsp_widget, "realize", G_CALLBACK(realize_cb), 0);
  g_signal_connect(g.fmdsp_widget, "unrealize", G_CALLBACK(unrealize_cb), 0);

  g.filechooser_widget = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
  g_signal_connect(g.filechooser_widget, "file-activated", G_CALLBACK(on_file_activated), 0);

  create_box();

  g_signal_connect(w, "key-press-event", G_CALLBACK(key_press_cb), 0);
  gtk_drag_dest_set(
    w, GTK_DEST_DEFAULT_MOTION|GTK_DEST_DEFAULT_HIGHLIGHT|GTK_DEST_DEFAULT_DROP,
    0, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets(w);
  g_signal_connect(w, "drag-data-received", G_CALLBACK(drag_data_recv_cb), 0);
  gtk_widget_add_events(w, GDK_KEY_PRESS_MASK);
  gtk_widget_show_all(w);
  gtk_main();
  return 0;
}
