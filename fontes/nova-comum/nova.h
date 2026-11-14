// nova.h: base comum dos apps do NOVA OS (tema claro/escuro, vidro fosco, utilitários).
// As cores (@bg, @fg, @fg2, @card, @line, @hover, @accent, @accent_hi, @accent_lo, @danger, @ok, @glass)
// vêm de ~/.config/gtk-3.0/gtk.css, gerado pelo comando nova-tema.
#ifndef NOVA_H
#define NOVA_H
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static gboolean nova_dark, nova_leve;   // nova_leve = Modo desempenho ligado
static cairo_surface_t *nova_blur;
static GtkWidget *nova_win;

static inline GtkWidget *cls(GtkWidget *w, const char *c) { gtk_style_context_add_class(gtk_widget_get_style_context(w), c); return w; }
static inline GtkWidget *lbl(const char *text, const char *c, double xalign) {
    GtkWidget *l = gtk_label_new(text); if (c) cls(l, c); gtk_label_set_xalign(GTK_LABEL(l), xalign); return l;
}
static inline char *nova_run(const char *cmd) {
    char *o = NULL; g_spawn_command_line_sync(cmd, &o, NULL, NULL, NULL);
    if (!o) o = g_strdup("");
    g_strchomp(o); return o;
}
static inline char *nova_sh(const char *script) {
    char *argv[] = { "sh", "-c", (char *)script, NULL }, *o = NULL;
    g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &o, NULL, NULL, NULL);
    if (!o) o = g_strdup("");
    g_strchomp(o); return o;
}
static inline void nova_spawn(const char *cmd) { g_spawn_command_line_async(cmd, NULL); }
static inline double ease_out(double t) { t = CLAMP(t, 0, 1); return 1 - pow(1 - t, 3); }
static inline double ease_spring(double t) {              // passa um pouco do alvo e assenta
    t = CLAMP(t, 0, 1); double c = 1.70158, u = t - 1; return 1 + (c + 1) * u * u * u + c * u * u;
}
static inline void nova_color(GtkWidget *w, const char *name, GdkRGBA *out) {
    if (!gtk_style_context_lookup_color(gtk_widget_get_style_context(w), name, out)) gdk_rgba_parse(out, "#0a84ff");
}

static const char *NOVA_CSS =
"* { font-family: Inter, sans-serif; -gtk-icon-shadow: none; }"
".h1 { font-size: 28px; font-weight: 700; color: @fg; }"
".h2 { font-size: 15px; font-weight: 600; color: @fg; }"
".sub { font-size: 13px; color: @fg2; }"
".cap { font-size: 11.5px; font-weight: 700; letter-spacing: 1.1px; color: @fg2; }"
".panel { border-radius: 14px; background-color: @card; border: 1px solid alpha(@fg,.08); box-shadow: 0 1px 3px alpha(#000,.06); }"
"button.pill { border-radius: 999px; padding: 5px 16px; min-height: 22px; font-size: 12.5px; font-weight: 700; border: none;"
"  background-image: none; box-shadow: none; text-shadow: none; transition: all 200ms cubic-bezier(.3,1.4,.5,1); }"
"button.soft { background-color: alpha(@fg,.08); color: @accent; }"
"button.soft:hover { background-color: @accent; color: #fff; box-shadow: 0 4px 14px alpha(@accent,.40); }"
"button.soft:active { background-color: @accent_lo; padding: 5px 13px; margin: 0 3px; }"
"button.plain { background-color: alpha(@fg,.08); color: @fg; }"
"button.plain:hover { background-color: alpha(@fg,.16); }"
"button.plain:active { padding: 5px 13px; margin: 0 3px; }"
"button.accent { background-color: @accent; color: #fff; box-shadow: 0 4px 14px alpha(@accent,.35); }"
"button.accent:hover { background-color: @accent_hi; box-shadow: 0 6px 20px alpha(@accent,.50); }"
"button.accent:active { background-color: @accent_lo; padding: 5px 13px; margin: 0 3px; }";

static inline void nova_init(const char *app_css) {
    char *p = g_build_filename(g_get_user_config_dir(), "nova", "tema", NULL), *t = NULL;
    nova_dark = g_file_get_contents(p, &t, NULL, NULL) && g_str_has_prefix(t, "escuro");
    g_free(p); g_free(t); t = NULL;
    p = g_build_filename(g_get_user_config_dir(), "nova", "opcoes", NULL);
    nova_leve = g_file_get_contents(p, &t, NULL, NULL) && (g_str_has_prefix(t, "desempenho=1") || strstr(t, "\ndesempenho=1"));
    g_free(p); g_free(t);
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", nova_dark, "gtk-enable-animations", !nova_leve, NULL);
    char *css = g_strconcat(NOVA_CSS, app_css, NULL);
    GtkCssProvider *pr = gtk_css_provider_new(); gtk_css_provider_load_from_data(pr, css, -1, NULL); g_free(css);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(pr), 900);
}

// Carrega o papel de parede desfocado, na escala da tela (igual ao "feh --bg-fill").
static inline void nova_load_blur(void) {
    GdkRectangle g; gdk_monitor_get_geometry(gdk_display_get_monitor(gdk_display_get_default(), 0), &g);
    char *p = g_build_filename(g_get_home_dir(), ".local/share/nova/wallpaper-blur.png", NULL);
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(p, NULL); g_free(p);
    if (!pb) pb = gdk_pixbuf_new_from_file("/usr/share/nova/login-blur.png", NULL);
    nova_blur = cairo_image_surface_create(CAIRO_FORMAT_RGB24, g.width, g.height);
    cairo_t *cr = cairo_create(nova_blur);
    cairo_set_source_rgb(cr, nova_dark ? .12 : .95, nova_dark ? .12 : .95, nova_dark ? .13 : .96); cairo_paint(cr);
    if (pb) {
        double sc = MAX((double)g.width / gdk_pixbuf_get_width(pb), (double)g.height / gdk_pixbuf_get_height(pb));
        cairo_translate(cr, (g.width - gdk_pixbuf_get_width(pb) * sc) / 2, (g.height - gdk_pixbuf_get_height(pb) * sc) / 2);
        cairo_scale(cr, sc, sc); gdk_cairo_set_source_pixbuf(cr, pb, 0, 0); cairo_paint(cr); g_object_unref(pb);
    }
    cairo_destroy(cr);
}

static inline void nova_rrect(cairo_t *cr, double x, double y, double w, double h, double r) {
    r = MIN(r, MIN(w, h) / 2); cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI_2, 0); cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI_2);
    cairo_arc(cr, x + r, y + h - r, r, G_PI_2, G_PI); cairo_arc(cr, x + r, y + r, r, G_PI, 1.5 * G_PI); cairo_close_path(cr);
}

// Vidro fosco: pinta no widget o trecho do papel de parede desfocado que fica atrás dele, mais a película do tema.
// Conecte no sinal "draw"; o dado é o raio dos cantos (0 = reto). Requer nova_win e nova_load_blur().
static inline gboolean nova_glass(GtkWidget *w, cairo_t *cr, gpointer radius) {
    int ox = 0, oy = 0, x = 0, y = 0; GtkAllocation a; gtk_widget_get_allocation(w, &a);
    if (!nova_blur || !nova_win || !gtk_widget_get_window(nova_win)) return FALSE;
    gdk_window_get_origin(gtk_widget_get_window(nova_win), &ox, &oy);
    if (w != nova_win) gtk_widget_translate_coordinates(w, nova_win, 0, 0, &x, &y);
    int ww = w == nova_win ? gtk_widget_get_allocated_width(w) : a.width, hh = w == nova_win ? gtk_widget_get_allocated_height(w) : a.height;
    cairo_save(cr);
    if (GPOINTER_TO_INT(radius) > 0) { nova_rrect(cr, 0, 0, ww, hh, GPOINTER_TO_INT(radius)); cairo_clip(cr); }
    cairo_set_source_surface(cr, nova_blur, -(ox + x), -(oy + y)); cairo_paint(cr);
    if (nova_dark) cairo_set_source_rgba(cr, .11, .11, .12, .62); else cairo_set_source_rgba(cr, .965, .965, .975, .68);
    cairo_paint(cr);
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, hh);
    cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, nova_dark ? .08 : .35); cairo_pattern_add_color_stop_rgba(g, .3, 1, 1, 1, 0);
    cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g);
    cairo_restore(cr);
    return FALSE;
}
static inline gboolean nova_redraw_on_move(GtkWidget *w, GdkEvent *e, gpointer target) { gtk_widget_queue_draw(target ? GTK_WIDGET(target) : w); return FALSE; }
#endif
