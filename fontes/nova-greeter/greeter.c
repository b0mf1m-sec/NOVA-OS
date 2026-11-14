// NOVA OS Greeter: tela de login minimalista (horizonte com fio de luz, só o essencial) e animações (GTK3 + liblightdm).
// NOVA_GREETER_DEMO=1 roda sem o LightDM (senha "ok" = sucesso), para testes.
#include <gtk/gtk.h>
#include <lightdm.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#define DIR "/usr/share/nova/"
#define SESSION "nova"

static LightDMGreeter *greeter;
static gboolean demo, prompt_ready, pending, busy;
static char *username, *shown_name;
static GtkWidget *win, *content, *entry, *pill, *hint, *msg, *lbl_time, *lbl_date, *spinner, *go, *eye, *pw_cap;
static cairo_surface_t *bg, *blur;
static int W, H;
static double enter = 0;   // 0..1 animação de entrada (e saída, invertida)
static gint64 enter_t0, shake_t0;
static gboolean leaving;
static GtkWidget *parts[3];
#define DICA ""
static void dica(void) {
    gboolean caps = gdk_keymap_get_caps_lock_state(gdk_keymap_get_for_display(gdk_display_get_default()));
    gtk_label_set_text(GTK_LABEL(msg), caps ? "A tecla Caps Lock está ligada" : DICA);
}

static double ease(double t) { t = CLAMP(t, 0, 1); return 1 - pow(1 - t, 3); }

/* ---------- fundo ---------- */
static void glow(cairo_t *cr, double x, double y, double r, double R, double G, double B, double a) {
    cairo_pattern_t *g = cairo_pattern_create_radial(x, y, 0, x, y, r);
    cairo_pattern_add_color_stop_rgba(g, 0, R, G, B, a);
    cairo_pattern_add_color_stop_rgba(g, .45, R, G, B, a * .35);
    cairo_pattern_add_color_stop_rgba(g, 1, R, G, B, 0);
    cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g);
}
// Granulado bem leve: evita "degraus" nos degradês escuros.
static void grain(cairo_surface_t *s) {
    cairo_surface_flush(s);
    unsigned char *d = cairo_image_surface_get_data(s); int st = cairo_image_surface_get_stride(s);
    guint32 x = 2463534242u;
    for (int j = 0; j < H; j++) { unsigned char *p = d + j * st;
        for (int i = 0; i < W; i++, p += 4) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5; int n = (int)(x % 5) - 2;
            for (int k = 0; k < 3; k++) { int v = p[k] + n; p[k] = v < 0 ? 0 : v > 255 ? 255 : v; }
        } }
    cairo_surface_mark_dirty(s);
}
// Fundo NOVA OS: céu quase preto e, embaixo, o horizonte de um planeta com um fio de luz (a "nova" nascendo).
static cairo_surface_t *make_nova(gboolean light) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, W, H);
    cairo_t *cr = cairo_create(s);
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, H);
    cairo_pattern_add_color_stop_rgb(g, 0, .016, .018, .030);
    cairo_pattern_add_color_stop_rgb(g, 1, .030, .036, .062);
    cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g);
    double R = W * 1.25, hy = H * .83, cx = W / 2.0, cy = hy + R;
    if (light) {
        glow(cr, cx, hy + 30, H * .62, .16, .36, 1.0, .30);                 // atmosfera
        glow(cr, cx, hy, H * .20, .55, .72, 1.0, .22);                      // ponto mais claro
        cairo_pattern_t *rim = cairo_pattern_create_radial(cx, cy, R - 2, cx, cy, R + 70);
        cairo_pattern_add_color_stop_rgba(rim, 0, .85, .92, 1, .95);
        cairo_pattern_add_color_stop_rgba(rim, .05, .45, .66, 1, .50);
        cairo_pattern_add_color_stop_rgba(rim, .35, .20, .42, 1, .14);
        cairo_pattern_add_color_stop_rgba(rim, 1, .20, .42, 1, 0);
        cairo_pattern_t *m = cairo_pattern_create_radial(cx, hy, 0, cx, hy, W * .50);   // some para os lados
        cairo_pattern_add_color_stop_rgba(m, 0, 0, 0, 0, 1); cairo_pattern_add_color_stop_rgba(m, .55, 0, 0, 0, .45);
        cairo_pattern_add_color_stop_rgba(m, 1, 0, 0, 0, 0);
        cairo_set_source(cr, rim); cairo_mask(cr, m); cairo_pattern_destroy(rim); cairo_pattern_destroy(m);
    }
    cairo_arc(cr, cx, cy, R - 1, 0, 2 * G_PI); cairo_set_source_rgb(cr, .012, .014, .024); cairo_fill(cr);   // o planeta
    cairo_destroy(cr); grain(s); return s;
}

static gboolean draw_bg(GtkWidget *w, cairo_t *cr, gpointer d) {
    cairo_set_source_surface(cr, blur, 0, 0); cairo_paint(cr);            // céu sem luz
    if (enter > 0.004) { cairo_set_source_surface(cr, bg, 0, 0); cairo_paint_with_alpha(cr, enter); }   // a luz nasce
    return FALSE;
}

static gboolean draw_avatar(GtkWidget *w, cairo_t *cr, gpointer d) {
    int sz = gtk_widget_get_allocated_width(w); double c = sz / 2.0;
    cairo_arc(cr, c, c, c - 1, 0, 2 * G_PI);
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, sz);
    cairo_pattern_add_color_stop_rgba(g, 0, 1, 1, 1, .13); cairo_pattern_add_color_stop_rgba(g, 1, 1, 1, 1, .04);
    cairo_set_source(cr, g); cairo_fill_preserve(cr); cairo_pattern_destroy(g);
    cairo_set_source_rgba(cr, 1, 1, 1, .22); cairo_set_line_width(cr, 1); cairo_stroke(cr);
    char ini[8] = {0}; g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(shown_name ? shown_name : "?")), ini);
    PangoLayout *l = gtk_widget_create_pango_layout(w, ini);
    PangoFontDescription *fd = pango_font_description_from_string("Inter Medium 19"); pango_layout_set_font_description(l, fd);
    int tw, th; pango_layout_get_pixel_size(l, &tw, &th);
    cairo_move_to(cr, c - tw / 2.0, c - th / 2.0); cairo_set_source_rgba(cr, 1, 1, 1, .92); pango_cairo_show_layout(cr, l);
    pango_font_description_free(fd); g_object_unref(l);
    return TRUE;
}

// Marca NOVA OS: letra N minimalista com um ponto de brilho.
static gboolean draw_logo(GtkWidget *w, cairo_t *cr, gpointer d) {
    double s = gtk_widget_get_allocated_height(w), u = s / 64.0;
    cairo_set_line_width(cr, 8 * u); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, 14 * u, 52 * u); cairo_line_to(cr, 14 * u, 14 * u); cairo_line_to(cr, 50 * u, 52 * u); cairo_line_to(cr, 50 * u, 14 * u);
    cairo_set_source_rgba(cr, 1, 1, 1, .92); cairo_stroke(cr);
    return TRUE;
}

/* ---------- animações ---------- */
static double enter_raw = 1;                                // tempo bruto 0..1 da entrada, para a cascata
static void apply_enter(void) {
    for (int i = 0; i < 3; i++) if (parts[i]) {
        double e = leaving ? enter : ease((enter_raw * 1.45 - i * .22));
        gtk_widget_set_opacity(parts[i], e);
    }
    gtk_widget_set_margin_top(content, (int)round((1 - enter) * 18));
    gtk_widget_queue_draw(win);
}

static void start_session(void) {
    if (demo) { gtk_main_quit(); return; }
    GError *err = NULL;
    if (!lightdm_greeter_start_session_sync(greeter, SESSION, &err)) {
        g_clear_error(&err);
        if (!lightdm_greeter_start_session_sync(greeter, NULL, NULL)) gtk_main_quit();
    }
}

static gboolean tick_enter(GtkWidget *w, GdkFrameClock *c, gpointer d) {
    double t = (g_get_monotonic_time() - enter_t0) / 1e6;
    if (!leaving) { enter_raw = CLAMP(t / 1.1, 0, 1); enter = ease(t / 0.9); apply_enter(); return t < 1.1; }
    enter = 1 - ease(t / 0.40); apply_enter();
    if (t >= 0.40) { start_session(); return G_SOURCE_REMOVE; }
    return G_SOURCE_CONTINUE;
}

static gboolean tick_shake(GtkWidget *w, GdkFrameClock *c, gpointer d) {
    double t = (g_get_monotonic_time() - shake_t0) / 1e6 / 0.50;
    int off = t >= 1 ? 0 : (int)round(sin(t * G_PI * 6) * 16 * (1 - t));
    gtk_widget_set_margin_start(pill, off > 0 ? 2 * off : 0);
    gtk_widget_set_margin_end(pill, off < 0 ? -2 * off : 0);
    return t < 1;
}

/* ---------- autenticação ---------- */
static void set_busy(gboolean b) {
    busy = b;
    gtk_widget_set_sensitive(entry, !b);
    gtk_widget_set_visible(spinner, b); gtk_widget_set_visible(go, !b);
    if (b) gtk_spinner_start(GTK_SPINNER(spinner)); else gtk_spinner_stop(GTK_SPINNER(spinner));
}

static void begin_auth(void) {
    prompt_ready = FALSE;
    if (!demo) lightdm_greeter_authenticate(greeter, username, NULL); else prompt_ready = TRUE;
}

static void auth_result(gboolean ok) {
    if (ok) {
        gtk_label_set_text(GTK_LABEL(msg), "Entrando…");
        leaving = TRUE; enter_t0 = g_get_monotonic_time();
        gtk_widget_add_tick_callback(win, tick_enter, NULL, NULL);
        return;
    }
    set_busy(FALSE);
    gtk_entry_set_text(GTK_ENTRY(entry), "");
    gtk_label_set_text(GTK_LABEL(msg), "Senha incorreta. Tente de novo.");
    gtk_style_context_add_class(gtk_widget_get_style_context(pill), "erro");
    shake_t0 = g_get_monotonic_time();
    gtk_widget_add_tick_callback(pill, tick_shake, NULL, NULL);
    gtk_widget_grab_focus(entry);
    begin_auth();
}

static gboolean demo_result(gpointer ok) { auth_result(GPOINTER_TO_INT(ok)); return G_SOURCE_REMOVE; }

static void respond(void) {
    const char *pw = gtk_entry_get_text(GTK_ENTRY(entry));
    if (demo) { g_timeout_add(500, demo_result, GINT_TO_POINTER(!strcmp(pw, "ok"))); return; }
    lightdm_greeter_respond(greeter, pw, NULL);
}

static void on_submit(GtkWidget *w, gpointer d) {
    if (busy || !*gtk_entry_get_text(GTK_ENTRY(entry))) return;
    set_busy(TRUE);
    if (prompt_ready) respond(); else pending = TRUE;
}

static void on_prompt(LightDMGreeter *g, const char *text, LightDMPromptType type, gpointer d) {
    prompt_ready = TRUE;
    if (pending) { pending = FALSE; respond(); }
}
static void on_message(LightDMGreeter *g, const char *text, LightDMMessageType type, gpointer d) {
    gtk_label_set_text(GTK_LABEL(msg), text);
}
static void on_auth_complete(LightDMGreeter *g, gpointer d) {
    auth_result(lightdm_greeter_get_is_authenticated(g));
}

static void on_eye(GtkWidget *w, gpointer d) {
    gboolean v = !gtk_entry_get_visibility(GTK_ENTRY(entry)); gtk_entry_set_visibility(GTK_ENTRY(entry), v);
    gtk_button_set_image(GTK_BUTTON(w), gtk_image_new_from_icon_name(v ? "view-conceal-symbolic" : "view-reveal-symbolic", GTK_ICON_SIZE_MENU));
    gtk_widget_set_tooltip_text(w, v ? "Esconder a senha" : "Mostrar a senha"); gtk_widget_grab_focus(entry);
}
static void show_if(GtkWidget *w, gboolean on) {
    GtkStyleContext *c = gtk_widget_get_style_context(w);
    if (on) gtk_style_context_add_class(c, "on"); else gtk_style_context_remove_class(c, "on");
    gtk_widget_set_sensitive(w, on);
}
static void on_changed(GtkWidget *w, gpointer d) {
    gtk_style_context_remove_class(gtk_widget_get_style_context(pill), "erro");
    gboolean tem = *gtk_entry_get_text(GTK_ENTRY(entry)) != 0;
    gtk_widget_set_opacity(hint, tem ? 0 : 1);
    show_if(go, tem); show_if(eye, tem);
    if (tem) dica();
}
static gboolean on_focus(GtkWidget *w, GdkEvent *e, gpointer in) {
    GtkStyleContext *c = gtk_widget_get_style_context(pill);
    if (in) gtk_style_context_add_class(c, "foco"); else gtk_style_context_remove_class(c, "foco");
    return FALSE;
}
static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) {
    if (e->keyval == GDK_KEY_Escape) { gtk_entry_set_text(GTK_ENTRY(entry), ""); return TRUE; }
    if (!gtk_widget_has_focus(entry) && !busy) gtk_widget_grab_focus(entry);
    if (e->keyval == GDK_KEY_Caps_Lock) g_timeout_add(60, (GSourceFunc)(void (*)(void))dica, NULL);
    return FALSE;
}

/* ---------- relógio e energia ---------- */
static gboolean tick_clock(gpointer d) {
    time_t t = time(NULL); struct tm *tm = localtime(&t); char b[96];
    strftime(b, sizeof b, "%H:%M", tm); gtk_label_set_text(GTK_LABEL(lbl_time), b);
    strftime(b, sizeof b, "%a, %-d de %b", tm); gtk_label_set_text(GTK_LABEL(lbl_date), b);
    return G_SOURCE_CONTINUE;
}

static void on_power(GtkWidget *w, gpointer what) {
    if (demo) return;
    switch (GPOINTER_TO_INT(what)) {
        case 0: lightdm_suspend(NULL); break;
        case 1: lightdm_restart(NULL); break;
        case 2: lightdm_shutdown(NULL); break;
    }
}
// O nome do botão só aparece quando o mouse passa por cima.
static gboolean on_pw_hover(GtkWidget *w, GdkEventCrossing *e, gpointer name) {
    GtkStyleContext *c = gtk_widget_get_style_context(pw_cap);
    if (name) { gtk_label_set_text(GTK_LABEL(pw_cap), name); gtk_style_context_add_class(c, "on"); }
    else gtk_style_context_remove_class(c, "on");
    return FALSE;
}
static GtkWidget *power_button(const char *icon, const char *name, int what) {
    GtkWidget *b = gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "orb");
    gtk_widget_set_can_focus(b, FALSE);
    g_signal_connect(b, "clicked", G_CALLBACK(on_power), GINT_TO_POINTER(what));
    g_signal_connect(b, "enter-notify-event", G_CALLBACK(on_pw_hover), (gpointer)name);
    g_signal_connect(b, "leave-notify-event", G_CALLBACK(on_pw_hover), NULL);
    return b;
}

static const char *CSS =
"* { font-family: Inter, sans-serif; -gtk-icon-shadow: none; }"
"label { color: #fff; }"
".marca { font-size: 10.5px; font-weight: 600; letter-spacing: 3.2px; color: alpha(#fff,.72); }"
".data { font-size: 12.5px; font-weight: 400; color: alpha(#fff,.50); }"
".hora { font-size: 12.5px; font-weight: 600; color: alpha(#fff,.88); }"
".nome { font-size: 16px; font-weight: 600; letter-spacing: .2px; color: alpha(#fff,.94); }"
".msg { font-size: 12px; font-weight: 400; color: alpha(#fff,.58); }"
".pill { border-radius: 999px; border: 1px solid alpha(#fff,.14); background-color: alpha(#fff,.055);"
"  transition: border-color 240ms ease, box-shadow 240ms ease, background-color 240ms ease; }"
".pill.foco { border-color: alpha(#fff,.34); background-color: alpha(#fff,.08); box-shadow: 0 0 0 3px alpha(#6ea8ff,.14); }"
".pill.erro { border-color: alpha(#ff6b61,.85); box-shadow: 0 0 0 3px alpha(#ff453a,.18); }"
".pill entry { background: none; border: none; box-shadow: none; color: #fff; caret-color: alpha(#fff,.85);"
"  font-size: 14px; padding: 0; min-height: 38px; }"
".pill entry:disabled { color: alpha(#fff,.5); }"
".pill entry selection { background-color: alpha(#fff,.28); }"
".hint { font-size: 13px; color: alpha(#fff,.40); transition: opacity 160ms ease; }"
".go, .eye { border-radius: 999px; min-width: 26px; min-height: 26px; padding: 0; margin: 6px; border: none; background-image: none;"
"  box-shadow: none; opacity: 0; transition: opacity 200ms ease, background-color 180ms ease, color 180ms ease, -gtk-icon-transform 180ms ease; }"
".go.on, .eye.on { opacity: 1; }"
".go { background-color: alpha(#fff,.92); color: #0a0c14; }"
".go:hover { background-color: #fff; -gtk-icon-transform: translateX(1.5px); }"
".go:active { -gtk-icon-transform: scale(.85); }"
".eye { background-color: transparent; color: alpha(#fff,.45); }"
".eye:hover { color: #fff; }"
"spinner { color: alpha(#fff,.85); margin: 0 11px; min-width: 16px; min-height: 16px; }"
".orb { border-radius: 999px; min-width: 36px; min-height: 36px; padding: 0; border: none; background-image: none; box-shadow: none;"
"  background-color: transparent; color: alpha(#fff,.50);"
"  transition: background-color 200ms ease, color 200ms ease, -gtk-icon-transform 220ms cubic-bezier(.3,1.6,.5,1); }"
".orb:hover { background-color: alpha(#fff,.10); color: #fff; -gtk-icon-transform: scale(1.12); }"
".orb:active { background-color: alpha(#fff,.05); -gtk-icon-transform: scale(.9); }"
".cap { font-size: 11px; font-weight: 500; letter-spacing: .3px; color: alpha(#fff,.62); opacity: 0; transition: opacity 180ms ease; }"
".cap.on { opacity: 1; }";

static GtkWidget *label(const char *text, const char *cls) {
    GtkWidget *l = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), cls);
    return l;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    gtk_init(&argc, &argv);
    demo = g_getenv("NOVA_GREETER_DEMO") != NULL;

    if (!demo) {
        greeter = lightdm_greeter_new();
        g_signal_connect(greeter, "show-prompt", G_CALLBACK(on_prompt), NULL);
        g_signal_connect(greeter, "show-message", G_CALLBACK(on_message), NULL);
        g_signal_connect(greeter, "authentication-complete", G_CALLBACK(on_auth_complete), NULL);
        if (!lightdm_greeter_connect_to_daemon_sync(greeter, NULL)) return 1;
        const char *hint = lightdm_greeter_get_select_user_hint(greeter);
        GList *us = lightdm_user_list_get_users(lightdm_user_list_get_instance());
        LightDMUser *u = NULL;
        if (hint) u = lightdm_user_list_get_user_by_name(lightdm_user_list_get_instance(), hint);
        if (!u && us) u = us->data;
        if (!u) return 1;
        username = g_strdup(lightdm_user_get_name(u));
        shown_name = g_strdup(lightdm_user_get_display_name(u));
    } else { username = g_strdup(g_get_user_name()); shown_name = g_strdup(username); }
    if (shown_name && g_ascii_islower(shown_name[0])) shown_name[0] = g_ascii_toupper(shown_name[0]);

    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE,
                 "gtk-icon-theme-name", "Adwaita", "gtk-font-name", "Inter 10", "gtk-xft-antialias", 1,
                 "gtk-xft-hinting", 1, "gtk-xft-hintstyle", "hintslight", "gtk-enable-animations", TRUE, NULL);
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), 900);

    GdkDisplay *dpy = gdk_display_get_default();
    GdkMonitor *mon = gdk_display_get_primary_monitor(dpy);
    if (!mon) mon = gdk_display_get_monitor(dpy, 0);
    GdkRectangle geo; gdk_monitor_get_geometry(mon, &geo);
    W = geo.width; H = geo.height;
    bg = make_nova(TRUE); blur = make_nova(FALSE);

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(win), W, H);
    gtk_window_move(GTK_WINDOW(win), geo.x, geo.y);
    gtk_widget_set_app_paintable(win, TRUE);
    g_signal_connect(win, "draw", G_CALLBACK(draw_bg), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), content);

    // topo: marca pequena à esquerda, data e hora à direita
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(top, 22); gtk_widget_set_margin_start(top, 30); gtk_widget_set_margin_end(top, 30);
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *logo = gtk_drawing_area_new(); gtk_widget_set_size_request(logo, 13, 13); gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
    g_signal_connect(logo, "draw", G_CALLBACK(draw_logo), NULL);
    gtk_box_pack_start(GTK_BOX(brand), logo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand), label("NOVA OS", "marca"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top), brand, FALSE, FALSE, 0);
    GtkWidget *clock = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    lbl_date = label("", "data"); lbl_time = label("", "hora");
    gtk_box_pack_start(GTK_BOX(clock), lbl_date, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(clock), lbl_time, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), clock, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), top, FALSE, FALSE, 0); parts[0] = top;
    tick_clock(NULL); g_timeout_add_seconds(1, tick_clock, NULL);

    // centro: avatar, nome, senha
    GtkWidget *mid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(mid, GTK_ALIGN_CENTER); gtk_widget_set_margin_bottom(mid, (int)(H * 0.07));
    GtkWidget *av = gtk_drawing_area_new();
    gtk_widget_set_size_request(av, 60, 60); gtk_widget_set_halign(av, GTK_ALIGN_CENTER);
    g_signal_connect(av, "draw", G_CALLBACK(draw_avatar), NULL);
    GtkWidget *nome = label(shown_name, "nome");
    gtk_widget_set_margin_top(nome, 14); gtk_widget_set_margin_bottom(nome, 18);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);   // segura o campo no centro durante a tremida
    gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
    pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(pill), "pill");
    gtk_widget_set_size_request(pill, 252, 40);
    entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(entry), 0x2022);
    gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_alignment(GTK_ENTRY(entry), .5); gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_signal_connect(entry, "activate", G_CALLBACK(on_submit), NULL);
    g_signal_connect(entry, "changed", G_CALLBACK(on_changed), NULL);
    g_signal_connect(entry, "focus-in-event", G_CALLBACK(on_focus), GINT_TO_POINTER(1));
    g_signal_connect(entry, "focus-out-event", G_CALLBACK(on_focus), GINT_TO_POINTER(0));
    eye = gtk_button_new_from_icon_name("view-reveal-symbolic", GTK_ICON_SIZE_MENU);
    gtk_style_context_add_class(gtk_widget_get_style_context(eye), "eye"); gtk_widget_set_can_focus(eye, FALSE);
    gtk_widget_set_valign(eye, GTK_ALIGN_CENTER); gtk_widget_set_tooltip_text(eye, "Mostrar a senha");
    g_signal_connect(eye, "clicked", G_CALLBACK(on_eye), NULL);
    go = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_MENU);
    gtk_style_context_add_class(gtk_widget_get_style_context(go), "go");
    gtk_widget_set_can_focus(go, FALSE); gtk_widget_set_valign(go, GTK_ALIGN_CENTER);
    g_signal_connect(go, "clicked", G_CALLBACK(on_submit), NULL);
    spinner = gtk_spinner_new(); gtk_widget_set_no_show_all(spinner, TRUE);
    GtkWidget *ov = gtk_overlay_new();                     // "Senha" visível mesmo com o campo em foco
    gtk_container_add(GTK_CONTAINER(ov), entry);
    hint = label("Senha", "hint");
    gtk_widget_set_halign(hint, GTK_ALIGN_CENTER); gtk_widget_set_valign(hint, GTK_ALIGN_CENTER);
    gtk_overlay_add_overlay(GTK_OVERLAY(ov), hint);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(ov), hint, TRUE);
    gtk_box_pack_start(GTK_BOX(pill), eye, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pill), ov, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pill), spinner, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pill), go, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), pill, FALSE, FALSE, 0);
    msg = label(DICA, "msg"); gtk_widget_set_margin_top(msg, 12);
    gtk_widget_set_size_request(msg, -1, 16);
    gtk_box_pack_start(GTK_BOX(mid), av, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mid), nome, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mid), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mid), msg, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), mid, TRUE, TRUE, 0); parts[1] = mid;

    // base: energia (só ícones; o nome aparece ao passar o mouse)
    GtkWidget *bot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(bot, GTK_ALIGN_CENTER); gtk_widget_set_margin_bottom(bot, 18);
    pw_cap = label("Desligar", "cap");
    GtkWidget *orbs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_widget_set_halign(orbs, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(orbs), power_button("weather-clear-night-symbolic", "Suspender", 0), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(orbs), power_button("view-refresh-symbolic", "Reiniciar", 1), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(orbs), power_button("system-shutdown-symbolic", "Desligar", 2), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bot), pw_cap, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bot), orbs, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(content), bot, FALSE, FALSE, 0); parts[2] = bot;
    show_if(go, FALSE); show_if(eye, FALSE);

    gtk_widget_show_all(win);
    gdk_window_set_cursor(gdk_get_default_root_window(), gdk_cursor_new_from_name(dpy, "default"));
    gdk_window_focus(gtk_widget_get_window(win), GDK_CURRENT_TIME);
    gtk_widget_grab_focus(entry);
    enter = 0; enter_raw = 0; apply_enter();
    enter_t0 = g_get_monotonic_time();
    gtk_widget_add_tick_callback(win, tick_enter, NULL, NULL);
    begin_auth();
    gtk_main();
    return 0;
}
