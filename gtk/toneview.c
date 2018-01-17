#include <gtk/gtk.h>
#include "toneview.h"
#include <stdatomic.h>
#include <stdbool.h>

struct toneview_g toneview_g = {
  .flag = ATOMIC_FLAG_INIT
};

static struct {
  GtkWidget *tonewin;
  GtkWidget *label[6];
  struct fmplayer_tonedata tonedata;
  struct fmplayer_tonedata tonedata_n;
  struct fmplayer_tonedata tonedata_n_disp;
  char strbuf[FMPLAYER_TONEDATA_STR_SIZE];
  enum fmplayer_tonedata_format format, format_disp;
  bool normalize;
  GtkClipboard *clipboard;
} g = {
  .normalize = true
};

static void on_destroy(GtkWidget *w, gpointer ptr) {
  (void)w;
  (void)ptr;
  g.tonewin = 0;
}

gboolean tick_cb(GtkWidget *widget, GdkFrameClock *clock, gpointer ptr) {
  (void)widget;
  (void)clock;
  (void)ptr;
  if (!atomic_flag_test_and_set_explicit(
      &toneview_g.flag, memory_order_acquire)) {
    g.tonedata = toneview_g.tonedata;
    atomic_flag_clear_explicit(&toneview_g.flag, memory_order_release);
  }
  g.tonedata_n = g.tonedata;
  for (int c = 0; c < 6; c++) {
    if (g.normalize) {
      tonedata_ch_normalize_tl(&g.tonedata_n.ch[c]);
    }
    if (g.format != g.format_disp ||
        !fmplayer_tonedata_channel_isequal(&g.tonedata_n.ch[c], &g.tonedata_n_disp.ch[c])) {
      g.tonedata_n_disp.ch[c] = g.tonedata_n.ch[c];
      tonedata_ch_string(g.format, g.strbuf, &g.tonedata_n.ch[c], 0);
      gtk_label_set_text(GTK_LABEL(g.label[c]), g.strbuf);
    }
  }
  g.format_disp = g.format;
  return G_SOURCE_CONTINUE;
}

static void on_format_changed(GtkComboBox *widget, gpointer ptr) {
  (void)ptr;
  g.format = gtk_combo_box_get_active(widget);
}

static void on_normalize_toggled(GtkToggleButton *widget, gpointer ptr) {
  (void)ptr;
  g.normalize = gtk_toggle_button_get_active(widget);
}

static void on_copy_clicked(GtkButton *button, gpointer ptr) {
  (void)button;
  int c = (intptr_t)ptr;
  if (!g.clipboard) {
#if GTK_MINOR_VERSION < 16
    GdkAtom selection = gdk_atom_intern("CLIPBOARD", TRUE);
    if (selection) {
      g.clipboard = gtk_clipboard_get(selection);
    }
#else
    GdkDisplay *disp = gdk_display_get_default();
    if (disp) {
      g.clipboard = gtk_clipboard_get_default(disp);
    }
#endif
  }
  if (g.clipboard) {
    tonedata_ch_string(g.format, g.strbuf, &g.tonedata_n.ch[c], 0);
    gtk_clipboard_set_text(g.clipboard, g.strbuf, -1);
  }
}

void show_toneview(void) {
  if (!g.tonewin) {
    g.format_disp = -1;
    g.tonewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(g.tonewin), 5);
    gtk_window_set_title(GTK_WINDOW(g.tonewin), "FM Tone Viewer");
    g_signal_connect(g.tonewin, "destroy", G_CALLBACK(on_destroy), 0);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(g.tonewin), box);
    GtkWidget *ctrlbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(box), ctrlbox);
    GtkWidget *format = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(ctrlbox), format, FALSE, TRUE, 0);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format), "PMD");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format), "FMP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format), "VOPM");
    gtk_combo_box_set_active(GTK_COMBO_BOX(format), g.format);
    g_signal_connect(format, "changed", G_CALLBACK(on_format_changed), 0);
    GtkWidget *normalizecheck = gtk_check_button_new_with_label("Normalize");
    gtk_box_pack_start(GTK_BOX(ctrlbox), normalizecheck, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(normalizecheck), g.normalize);
    g_signal_connect(normalizecheck, "toggled", G_CALLBACK(on_normalize_toggled), 0);
    for (int c = 0; c < 6; c++) {
      GtkWidget *cbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_box_pack_start(GTK_BOX(box), cbox, TRUE, TRUE, 0);
      GtkWidget *tonetext = gtk_label_new(0);
      PangoAttrList *pattrl = pango_attr_list_new();
      PangoAttribute *pattr = pango_attr_family_new("monospace");
      pango_attr_list_insert(pattrl, pattr);
      gtk_label_set_attributes(GTK_LABEL(tonetext), pattrl);
      pango_attr_list_unref(pattrl);
      gtk_box_pack_start(GTK_BOX(cbox), tonetext, TRUE, TRUE, 0);
      GtkWidget *copybutton = gtk_button_new_with_label("Copy");
      g_signal_connect(copybutton, "clicked", G_CALLBACK(on_copy_clicked), (gpointer)((intptr_t)c));
      gtk_box_pack_start(GTK_BOX(cbox), copybutton, FALSE, TRUE, 0);
      g.label[c] = tonetext;
    }
    gtk_widget_add_tick_callback(g.tonewin, tick_cb, 0, 0);
    gtk_widget_show_all(g.tonewin);
  } else {
    gtk_window_present(GTK_WINDOW(g.tonewin));
  }
}
