#include <gtk/gtk.h>
#include <cairo.h>
#include "oscilloview.h"
#include <string.h>

struct oscilloview oscilloview_g = {
  .flag = ATOMIC_FLAG_INIT
};

enum {
  VIEW_SAMPLES = 1024,
  VIEW_SKIP = 2,
};

static struct {
  GtkWidget *win;
  struct oscillodata oscillodata[LIBOPNA_OSCILLO_TRACK_COUNT];
} g;

static void on_destroy(GtkWidget *w, gpointer ptr) {
  (void)w;
  (void)ptr;
  g.win = 0;
}

static void draw_track(cairo_t *cr,
                       double x, double y, double w, double h,
                       const struct oscillodata *data) {
  int start = OSCILLO_SAMPLE_COUNT - VIEW_SAMPLES;
  start -= (data->offset >> OSCILLO_OFFSET_SHIFT);
  if (start < 0) start = 0;
  for (int i = 0; i < (VIEW_SAMPLES / VIEW_SKIP); i++) {
    cairo_line_to(cr, x + ((i)*w)/(VIEW_SAMPLES / VIEW_SKIP), y + h/2.0 - (data[0].buf[start + i*VIEW_SKIP] / 16384.0) * h/2);
  }
  cairo_stroke(cr);
}

static gboolean draw_cb(GtkWidget *w,
                        cairo_t *cr,
                        gpointer ptr) {
  (void)w;
  (void)ptr;
  guint width = gtk_widget_get_allocated_width(w) / 3u;
  guint height = gtk_widget_get_allocated_height(w) / 3u;
  cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
  if (!atomic_flag_test_and_set_explicit(
    &oscilloview_g.flag, memory_order_acquire)) {
    memcpy(g.oscillodata,
           oscilloview_g.oscillodata,
           sizeof(oscilloview_g.oscillodata));
    atomic_flag_clear_explicit(&oscilloview_g.flag, memory_order_release);
  }
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_paint(cr);
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  for (int x = 0; x < 3; x++) {
    for (int y = 0; y < 3; y++) {
      draw_track(cr, x*width, y*height, width, height, &g.oscillodata[x*3+y]);
    }
  }
  return FALSE;
}

static gboolean tick_cb(GtkWidget *w, GdkFrameClock *clock, gpointer ptr) {
  (void)clock;
  (void)ptr;
  gtk_widget_queue_draw(w);
  return G_SOURCE_CONTINUE;
}

void show_oscilloview(void) {
  if (!g.win) {
    g.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g.win), "Oscilloscope view");
    g_signal_connect(g.win, "destroy", G_CALLBACK(on_destroy), 0);
    GtkWidget *drawarea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(g.win), drawarea);
    g_signal_connect(drawarea, "draw", G_CALLBACK(draw_cb), 0);
    gtk_widget_add_tick_callback(drawarea, tick_cb, 0, 0);
    gtk_widget_show_all(g.win);
  } else {
    gtk_window_present(GTK_WINDOW(g.win));
  }
}
