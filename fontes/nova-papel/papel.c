// Papel de parede vivo do NOVA OS: céu fixo e quatro ondas que flutuam devagar e acompanham o mouse (paralaxe).
// Leve de propósito: camadas pré-renderizadas no servidor gráfico; quadros só quando algo muda; pausa na bateria fraca.
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#define NL 4
#define MX 60
#define MY 24
static GtkWidget *win;
static cairo_surface_t *sky, *layer[NL];
static double W, H, mx = .5, my = .5, tx = .5, ty = .5;
static const double AMP[NL] = { 8, 15, 24, 34 }, DRIFT[NL] = { 14, 20, 26, 32 }, PERIOD[NL] = { 23, 19, 16, 13 }, PHASE[NL] = { 0, 1.7, 3.1, 4.4 };
static gint64 last_move; static int frame; static char **g_argv;

static cairo_surface_t *load(const char *name, double sx, double sy) {      // carrega o PNG para uma superfície do servidor (composição acelerada)
    char *t = NULL, *p; gboolean dark = FALSE;
    p = g_build_filename(g_get_user_config_dir(), "nova", "tema", NULL); if (g_file_get_contents(p, &t, NULL, NULL)) dark = g_str_has_prefix(t, "escuro"); g_free(p); g_free(t);
    p = g_strdup_printf("%s/.local/share/nova/papeis/vivo-%s-%s.png", g_get_home_dir(), dark ? "escuro" : "claro", name);
    cairo_surface_t *img = cairo_image_surface_create_from_png(p); g_free(p);
    int iw = cairo_image_surface_get_width(img), ih = cairo_image_surface_get_height(img);
    cairo_surface_t *s = gdk_window_create_similar_surface(gtk_widget_get_window(win), CAIRO_CONTENT_COLOR_ALPHA, (int)(iw * sx + .5), (int)(ih * sy + .5));
    cairo_t *cr = cairo_create(s); cairo_scale(cr, sx, sy); cairo_set_source_surface(cr, img, 0, 0); cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_paint(cr);
    cairo_destroy(cr); cairo_surface_destroy(img); return s;
}
static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer d) {
    double t = g_get_monotonic_time() / 1e6, sx = W / 1366.0, sy = H / 768.0;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); cairo_set_source_surface(cr, sky, 0, 0); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    for (int i = 0; i < NL; i++) {
        double a = t * 2 * G_PI / PERIOD[i] + PHASE[i];
        double dx = sin(a) * DRIFT[i] + (mx - .5) * -2 * AMP[i], dy = cos(a * .8) * 5 + (my - .5) * -AMP[i] * .5;
        cairo_set_source_surface(cr, layer[i], round((-MX + dx) * sx), round((-MY + dy) * sy)); cairo_paint(cr);
    }
    return TRUE;
}
static gboolean bateria_fraca(void) {
    static gint64 chk; static gboolean low;
    if (g_get_monotonic_time() - chk > 30e6) {
        char *s = NULL, *c = NULL; chk = g_get_monotonic_time(); low = FALSE;
        if (g_file_get_contents("/sys/class/power_supply/BAT1/status", &s, NULL, NULL) && g_file_get_contents("/sys/class/power_supply/BAT1/capacity", &c, NULL, NULL))
            low = g_str_has_prefix(s, "Discharging") && atoi(c) <= 25;
        g_free(s); g_free(c);
    }
    return low;
}
static gboolean tick(gpointer d) {
    int x, y; frame++;
    gdk_device_get_position(gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default())), NULL, &x, &y);
    double nx = CLAMP(x / W, 0, 1), ny = CLAMP(y / H, 0, 1);
    if (fabs(nx - tx) > .001 || fabs(ny - ty) > .001) { tx = nx; ty = ny; last_move = g_get_monotonic_time(); }
    gboolean seguindo = fabs(tx - mx) > .002 || fabs(ty - my) > .002;
    mx += (tx - mx) * .12; my += (ty - my) * .12;                       // o fundo segue o mouse com suavidade
    if (bateria_fraca()) return G_SOURCE_CONTINUE;                      // economiza bateria quando está acabando
    if (seguindo || frame % 3 == 0) gtk_widget_queue_draw(win);         // 30 quadros/s com o mouse em movimento; 10 só flutuando
    return G_SOURCE_CONTINUE;
}
static void on_usr1(int s) { execv("/proc/self/exe", g_argv); }          // tema mudou: recarrega as camadas

int main(int argc, char **argv) {
    g_argv = argv; gtk_init(&argc, &argv); signal(SIGUSR1, on_usr1);
    GdkRectangle g; gdk_monitor_get_geometry(gdk_display_get_monitor(gdk_display_get_default(), 0), &g); W = g.width; H = g.height;
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Papel de parede"); gtk_window_set_wmclass(GTK_WINDOW(win), "nova-papel", "Nova-papel");
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DESKTOP); gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(win), g.width, g.height); gtk_window_move(GTK_WINDOW(win), g.x, g.y);
    gtk_window_set_accept_focus(GTK_WINDOW(win), FALSE); gtk_window_set_keep_below(GTK_WINDOW(win), TRUE); gtk_window_stick(GTK_WINDOW(win));
    gtk_widget_set_app_paintable(win, TRUE); gtk_widget_realize(win);
    double sx = W / 1366.0, sy = H / 768.0;
    sky = load("sky", sx, sy); const char *n[NL] = { "w0", "w1", "w2", "w3" };
    for (int i = 0; i < NL; i++) layer[i] = load(n[i], sx, sy);
    g_signal_connect(win, "draw", G_CALLBACK(on_draw), NULL);
    gtk_widget_show_all(win); gdk_window_lower(gtk_widget_get_window(win));
    g_timeout_add(33, tick, NULL);
    gtk_main(); return 0;
}
