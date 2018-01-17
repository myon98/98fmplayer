#include "configdialog.h"
#include <math.h>

static double mix_to_db(uint32_t mix) {
  return 20.0 * log10((double)mix / 0x10000);
}

static uint32_t db_to_mix(double db) {
  return round(pow(10.0, db / 20.0) * 0x10000);
}

static struct {
  GtkWidget *configwin;
  config_update_func *func;
  void *ptr;
  GtkWidget *radio_ppz8_none;
  GtkWidget *radio_ppz8_linear;
  GtkWidget *radio_ppz8_sinc;
} g;

struct fmplayer_config fmplayer_config = {
  .ssg_mix = 0x10000,
  .ppz8_interp = PPZ8_INTERP_SINC,
};

static void on_destroy(GtkWidget *w, gpointer ptr) {
  (void)w;
  (void)ptr;
  g.configwin = 0;
}

static void on_toggled_ssg(GtkToggleButton *radio_ssg_opna, gpointer ptr) {
  GtkWidget *spin_ssg_mix = ptr;
  fmplayer_config.ssg_ymf288 = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_ssg_opna));
  gtk_widget_set_sensitive(spin_ssg_mix, !fmplayer_config.ssg_ymf288);
  g.func(g.ptr);
}

static void on_changed_ssg_mix(GtkSpinButton *spin_ssg_mix, gpointer ptr) {
  (void)ptr;
  fmplayer_config.ssg_mix = db_to_mix(gtk_spin_button_get_value(spin_ssg_mix));
  g.func(g.ptr);
}

static void on_changed_ppz8_interp(GtkToggleButton *b, gpointer ptr) {
  (void)b;
  (void)ptr;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g.radio_ppz8_none))) {
    fmplayer_config.ppz8_interp = PPZ8_INTERP_NONE;
  } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g.radio_ppz8_linear))) {
    fmplayer_config.ppz8_interp = PPZ8_INTERP_LINEAR;
  } else {
    fmplayer_config.ppz8_interp = PPZ8_INTERP_SINC;
  }
  g.func(g.ptr);
}

static void on_toggled_fm_hires_sin(GtkToggleButton *b, gpointer ptr) {
  (void)ptr;
  fmplayer_config.fm_hires_sin = gtk_toggle_button_get_active(b);
  g.func(g.ptr);
}

static void on_toggled_fm_hires_env(GtkToggleButton *b, gpointer ptr) {
  (void)ptr;
  fmplayer_config.fm_hires_env = gtk_toggle_button_get_active(b);
  g.func(g.ptr);
}

void show_configdialog(config_update_func *func, void *ptr) {
  g.func = func;
  g.ptr = ptr;
  if (!g.configwin) {
    g.configwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(g.configwin), 5);
    gtk_window_set_title(GTK_WINDOW(g.configwin), "FMPlayer config");
    g_signal_connect(g.configwin, "destroy", G_CALLBACK(on_destroy), 0);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(g.configwin), box);
    GtkWidget *frame_fm = gtk_frame_new("FM");
    gtk_container_add(GTK_CONTAINER(box), frame_fm);
    GtkWidget *fmbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(frame_fm), fmbox);
    GtkWidget *check_fm_hires_sin = gtk_check_button_new_with_label("Enable higher resolution sine table");
    g_signal_connect(check_fm_hires_sin, "toggled",
                     G_CALLBACK(on_toggled_fm_hires_sin), 0);
    gtk_container_add(GTK_CONTAINER(fmbox), check_fm_hires_sin);
    GtkWidget *check_fm_hires_env = gtk_check_button_new_with_label("Enable higher resolution envelope");
    g_signal_connect(check_fm_hires_env, "toggled",
                     G_CALLBACK(on_toggled_fm_hires_env), 0);
    gtk_container_add(GTK_CONTAINER(fmbox), check_fm_hires_env);

    GtkWidget *frame_ssg = gtk_frame_new("SSG (249600 Hz to 55467 Hz resampling + DC output)");
    gtk_container_add(GTK_CONTAINER(box), frame_ssg);
    GtkWidget *ssgbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(frame_ssg), ssgbox);
    GtkWidget *radio_ssg_opna = gtk_radio_button_new_with_label(0,
        "OPNA analog circuit simulation (sinc + HPF)");
    GSList *radio_ssg_list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_ssg_opna));
    gtk_container_add(GTK_CONTAINER(ssgbox), radio_ssg_opna);
    GtkWidget *ssgspinbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(ssgbox), ssgspinbox);
    gtk_container_add(GTK_CONTAINER(ssgspinbox), gtk_label_new("Volume offset (dB):"));
    GtkWidget *spin_ssg_mix = gtk_spin_button_new_with_range(-18.0, 18.0, 0.01);
    g_signal_connect(radio_ssg_opna, "toggled", G_CALLBACK(on_toggled_ssg), spin_ssg_mix);
    g_signal_connect(spin_ssg_mix, "value-changed", G_CALLBACK(on_changed_ssg_mix), 0);
    gtk_container_add(GTK_CONTAINER(ssgspinbox), spin_ssg_mix);
    gtk_container_add(GTK_CONTAINER(ssgbox), gtk_label_new("PC-9801-86 / YMF288: 0.0dB (reference)"));
    gtk_container_add(GTK_CONTAINER(ssgbox), gtk_label_new("PC-9801-26 / Speakboard: 1.6dB (not verified)"));
    GtkWidget *radio_ssg_ymf288 = gtk_radio_button_new_with_label(
        radio_ssg_list,
        "Bit perfect with OPN3-L aka YMF288 (average of nearest 4.5 samples)");
    gtk_container_add(GTK_CONTAINER(ssgbox), radio_ssg_ymf288);
    
    GtkWidget *frame_ppz8 = gtk_frame_new("PPZ8 interpolation");
    gtk_container_add(GTK_CONTAINER(box), frame_ppz8);
    GtkWidget *ppz8box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(frame_ppz8), ppz8box);
    g.radio_ppz8_none = gtk_radio_button_new_with_label(
        0, "Nearest neighbor (ppz8.com equivalent)");
    GSList *radio_ppz8_list =
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(g.radio_ppz8_none));
    gtk_container_add(GTK_CONTAINER(ppz8box), g.radio_ppz8_none);
    g.radio_ppz8_linear = gtk_radio_button_new_with_label(
      radio_ppz8_list, "Linear");
    gtk_container_add(GTK_CONTAINER(ppz8box), g.radio_ppz8_linear);
    radio_ppz8_list =
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(g.radio_ppz8_linear));
    g.radio_ppz8_sinc = gtk_radio_button_new_with_label(
      radio_ppz8_list, "Sinc (best quality)");
    gtk_container_add(GTK_CONTAINER(ppz8box), g.radio_ppz8_sinc);
    g_signal_connect(g.radio_ppz8_none, "toggled",
                     G_CALLBACK(on_changed_ppz8_interp), 0);
    g_signal_connect(g.radio_ppz8_linear, "toggled",
                     G_CALLBACK(on_changed_ppz8_interp), 0);
    g_signal_connect(g.radio_ppz8_sinc, "toggled",
                     G_CALLBACK(on_changed_ppz8_interp), 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_fm_hires_sin), fmplayer_config.fm_hires_sin);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_fm_hires_env), fmplayer_config.fm_hires_env);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_ssg_mix), mix_to_db(fmplayer_config.ssg_mix));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_ssg_ymf288), fmplayer_config.ssg_ymf288);
    switch (fmplayer_config.ppz8_interp) {
    case PPZ8_INTERP_NONE:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g.radio_ppz8_none), true);
      break;
    case PPZ8_INTERP_LINEAR:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g.radio_ppz8_linear), true);
      break;
    default:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g.radio_ppz8_sinc), true);
      break;
    }
    gtk_widget_show_all(g.configwin);
  } else {
    gtk_window_present(GTK_WINDOW(g.configwin));
  }
}
