// Ajustes do NOVA OS: aparência, papel de parede, dock, tela e som, rede, segurança e informações do sistema.
#include "../nova-comum/nova.h"
#include <unistd.h>
#include <sys/statvfs.h>

static GtkWidget *win, *side, *stack, *list;
static char *self_path; static gboolean loading = TRUE;
static char *BIN;                                           // ~/.local/bin/

static const char *PAGES[] = { "Aparência", "Papel de parede", "Dock", "Tela e som", "Rede e Bluetooth", "Segurança", "Aplicativos", "Atalhos", "Atualizações", "Sobre" };
static const char *PICON[] = { "preferences-desktop-appearance-symbolic", "preferences-desktop-wallpaper-symbolic", "view-app-grid-symbolic",
    "preferences-desktop-display-symbolic", "network-wireless-symbolic", "security-high-symbolic", "application-x-executable-symbolic", "input-keyboard-symbolic", "system-software-update-symbolic", "help-about-symbolic" };
#define NPAGES 10

static void setc(GtkWidget *w, const char *c, gboolean on) { GtkStyleContext *s = gtk_widget_get_style_context(w); if (on) gtk_style_context_add_class(s, c); else gtk_style_context_remove_class(s, c); }
static char *cfg(const char *key, const char *def) { char *c = g_strdup_printf("%snova-config -g %s %s", BIN, key, def), *o = nova_sh(c); g_free(c); return o; }
static void cfg_set(const char *key, const char *val) { char *c = g_strdup_printf("%snova-config %s %s", BIN, key, val); g_free(nova_sh(c)); g_free(c); }
static char *conf(const char *key) { char *c = g_strdup_printf("sed -n 's/^%s = //p' $HOME/.config/nova/config", key), *o = nova_sh(c); g_free(c); return o; }

// Mudanças de tema recriam a paleta: aplica e reabre os Ajustes na mesma página, já com as cores novas.
static void retheme(const char *args) {
    char *c = g_strdup_printf("%snova-tema %s", BIN, args); g_free(nova_sh(c)); g_free(c);
    int pg = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(GTK_LIST_BOX(list)));
    char n[8]; g_snprintf(n, sizeof n, "%d", pg); execl(self_path, self_path, "--pagina", n, NULL);
}

/* ---------- construção de páginas ---------- */
static GtkWidget *page_new(const char *title, const char *sub) {
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(v, 30); gtk_widget_set_margin_end(v, 30); gtk_widget_set_margin_top(v, 24); gtk_widget_set_margin_bottom(v, 24);
    gtk_box_pack_start(GTK_BOX(v), lbl(title, "h1", 0), FALSE, FALSE, 0);
    GtkWidget *s = lbl(sub, "sub", 0); gtk_widget_set_margin_bottom(s, 16); gtk_label_set_line_wrap(GTK_LABEL(s), TRUE); gtk_box_pack_start(GTK_BOX(v), s, FALSE, FALSE, 0);
    return v;
}
static GtkWidget *group(GtkWidget *page, const char *caption) {
    if (caption) { GtkWidget *c = lbl(caption, "cap", 0); gtk_widget_set_margin_top(c, 8); gtk_widget_set_margin_bottom(c, 7); gtk_widget_set_margin_start(c, 4); gtk_box_pack_start(GTK_BOX(page), c, FALSE, FALSE, 0); }
    GtkWidget *g = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "group"); gtk_widget_set_margin_bottom(g, 12);
    gtk_box_pack_start(GTK_BOX(page), g, FALSE, FALSE, 0); return g;
}
static GtkWidget *row(GtkWidget *grp, const char *title, const char *sub, GtkWidget *ctl) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(grp));
    if (ch) gtk_box_pack_start(GTK_BOX(grp), cls(gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), "hair"), FALSE, FALSE, 0);
    g_list_free(ch);
    GtkWidget *r = cls(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14), "row");
    GtkWidget *t = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1); gtk_widget_set_valign(t, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(t), lbl(title, "rtitle", 0), FALSE, FALSE, 0);
    GtkWidget *s = NULL;
    if (sub) { s = lbl(sub, "rsub", 0); gtk_label_set_line_wrap(GTK_LABEL(s), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(s), 46); gtk_box_pack_start(GTK_BOX(t), s, FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(r), t, TRUE, TRUE, 0);
    if (ctl) { gtk_widget_set_valign(ctl, GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(r), ctl, FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(grp), r, FALSE, FALSE, 0);
    return s ? s : r;
}
static GtkWidget *sw_new(gboolean on, GCallback cb, gpointer data) { GtkWidget *s = gtk_switch_new(); gtk_switch_set_active(GTK_SWITCH(s), on); g_signal_connect(s, "notify::active", cb, data); return s; }
static GtkWidget *scale_new(double lo, double hi, double step, double val, GCallback cb) {
    GtkWidget *s = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, lo, hi, step); gtk_scale_set_draw_value(GTK_SCALE(s), FALSE);
    gtk_range_set_value(GTK_RANGE(s), val); gtk_widget_set_size_request(s, 220, -1); g_signal_connect(s, "value-changed", cb, NULL); return s;
}
static GtkWidget *btn_new(const char *text, const char *kind, GCallback cb, gpointer data) {
    GtkWidget *b = cls(cls(gtk_button_new_with_label(text), "pill"), kind); g_signal_connect(b, "clicked", cb, data); return b;
}
static void on_run(GtkWidget *w, gpointer cmd) { char *c = g_strdup_printf("sh -c \"%s\"", (const char *)cmd); nova_spawn(c); g_free(c); }

/* ---------- Aparência ---------- */
static gboolean draw_preview(GtkWidget *w, cairo_t *cr, gpointer dark) {
    gboolean d = GPOINTER_TO_INT(dark); int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    nova_rrect(cr, 0, 0, W, H, 10); cairo_clip(cr);
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, W, H);
    if (d) { cairo_pattern_add_color_stop_rgb(g, 0, .05, .07, .20); cairo_pattern_add_color_stop_rgb(g, 1, .30, .14, .34); }
    else { cairo_pattern_add_color_stop_rgb(g, 0, .30, .58, 1); cairo_pattern_add_color_stop_rgb(g, 1, 1, .78, .66); }
    cairo_set_source(cr, g); cairo_paint(cr); cairo_pattern_destroy(g);
    cairo_set_source_rgba(cr, d ? .11 : .97, d ? .11 : .97, d ? .12 : .98, .85); cairo_rectangle(cr, 0, 0, W, 9); cairo_fill(cr);
    nova_rrect(cr, 22, 24, W - 52, H - 46, 6); cairo_set_source_rgb(cr, d ? .16 : 1, d ? .16 : 1, d ? .17 : 1); cairo_fill(cr);
    nova_rrect(cr, 22, 24, W - 52, 11, 6); cairo_set_source_rgb(cr, d ? .22 : .91, d ? .22 : .91, d ? .23 : .93); cairo_fill(cr);
    const double tl[3][3] = { { 1, .37, .34 }, { 1, .74, .18 }, { .16, .78, .25 } };
    for (int i = 0; i < 3; i++) { cairo_arc(cr, 29 + i * 8, 29.5, 2.4, 0, 2 * G_PI); cairo_set_source_rgb(cr, tl[i][0], tl[i][1], tl[i][2]); cairo_fill(cr); }
    GdkRGBA a; nova_color(w, "accent", &a); nova_rrect(cr, 30, 44, 46, 7, 3.5); gdk_cairo_set_source_rgba(cr, &a); cairo_fill(cr);
    cairo_set_source_rgba(cr, d ? 1 : 0, d ? 1 : 0, d ? 1 : 0, .16); nova_rrect(cr, 30, 57, W - 80, 5, 2.5); cairo_fill(cr); nova_rrect(cr, 30, 67, W - 100, 5, 2.5); cairo_fill(cr);
    nova_rrect(cr, W / 2 - 34, H - 15, 68, 11, 5); cairo_set_source_rgba(cr, d ? .2 : 1, d ? .2 : 1, d ? .22 : 1, .6); cairo_fill(cr);
    return TRUE;
}
static void on_theme(GtkWidget *w, gpointer dark) { if (GPOINTER_TO_INT(dark) != nova_dark) retheme(GPOINTER_TO_INT(dark) ? "tema escuro" : "tema claro"); }
static void on_accent(GtkWidget *w, gpointer name) { char *c = g_strdup_printf("destaque %s", (const char *)name); retheme(c); }
static void on_glass(GObject *s, GParamSpec *p, gpointer d) {
    if (loading) return;
    nova_sh(gtk_switch_get_active(GTK_SWITCH(s)) ? "rm -f $HOME/.config/picom/sem-vidro; pkill -x picom; sleep .3; ($HOME/.local/bin/nova-picom >/dev/null 2>&1 &)"
                                                  : "touch $HOME/.config/picom/sem-vidro; pkill -x picom; sleep .3; ($HOME/.local/bin/nova-picom >/dev/null 2>&1 &)");
}
static GtkWidget *page_aparencia(void) {
    GtkWidget *p = page_new("Aparência", "Escolha o visual do sistema. As mudanças valem na hora.");
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16); gtk_widget_set_margin_bottom(h, 16);
    const char *tn[2] = { "Claro", "Escuro" };
    for (int i = 0; i < 2; i++) {
        GtkWidget *b = cls(gtk_button_new(), "themecard"); setc(b, "sel", nova_dark == i);
        GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8); GtkWidget *da = gtk_drawing_area_new(); gtk_widget_set_size_request(da, 190, 112);
        g_signal_connect(da, "draw", G_CALLBACK(draw_preview), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(v), da, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(v), lbl(tn[i], "rtitle", .5), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(b), v); g_signal_connect(b, "clicked", G_CALLBACK(on_theme), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(h), b, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(p), h, FALSE, FALSE, 0);
    GtkWidget *g = group(p, NULL);
    GtkWidget *dots = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    static const char *an[] = { "azul", "roxo", "rosa", "vermelho", "laranja", "verde", "grafite" };
    static const char *ac[] = { "#0a84ff", "#bf5af2", "#ff5c93", "#ff453a", "#ff9f0a", "#32d74b", "#8e8e93" };
    char *cur = conf("destaque");
    for (int i = 0; i < 7; i++) {
        GtkWidget *b = cls(gtk_button_new(), "dot"); gtk_widget_set_tooltip_text(b, an[i]); setc(b, "sel", !strcmp(cur, an[i]));
        char *css = g_strdup_printf("button { background-color: %s; }", ac[i]); GtkCssProvider *pr = gtk_css_provider_new();
        gtk_css_provider_load_from_data(pr, css, -1, NULL); g_free(css);
        gtk_style_context_add_provider(gtk_widget_get_style_context(b), GTK_STYLE_PROVIDER(pr), 950);
        g_signal_connect(b, "clicked", G_CALLBACK(on_accent), (gpointer)an[i]); gtk_box_pack_start(GTK_BOX(dots), b, FALSE, FALSE, 0);
    }
    g_free(cur);
    row(g, "Cor de destaque", "Usada em botões, seleções e interruptores", dots);
    row(g, "Efeito de vidro fosco", "Desfoca o fundo atrás da barra. Desligue se o computador ficar lento.",
        sw_new(!g_file_test(g_build_filename(g_get_home_dir(), ".config/picom/sem-vidro", NULL), G_FILE_TEST_EXISTS), G_CALLBACK(on_glass), NULL));
    return p;
}

/* ---------- Papel de parede ---------- */
static void on_wall(GtkWidget *w, gpointer name) { char *c = g_strdup_printf("papel %s", (const char *)name); retheme(c); }
static void on_photo(GtkWidget *w, gpointer d) {
    GtkFileChooserNative *fc = gtk_file_chooser_native_new("Escolher uma foto", GTK_WINDOW(win), GTK_FILE_CHOOSER_ACTION_OPEN, "Usar esta foto", "Cancelar");
    GtkFileFilter *f = gtk_file_filter_new(); gtk_file_filter_add_pixbuf_formats(f); gtk_file_filter_set_name(f, "Imagens"); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc)), *q = g_shell_quote(fn);
        char *c = g_strdup_printf("d=$HOME/.local/share/nova/papeis; rm -f $d/foto*.png; convert %s -auto-orient -resize 1366x768^ -gravity center -extent 1366x768 $d/foto.png && convert $d/foto.png -resize 320x180 $d/foto-mini.png", q);
        g_free(nova_sh(c)); g_free(c); g_free(q); g_free(fn); retheme("papel foto");
    }
}
static GtkWidget *page_papel(void) {
    GtkWidget *p = page_new("Papel de parede", "A versão clara ou escura acompanha o tema escolhido.");
    GtkWidget *fl = gtk_flow_box_new(); gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fl), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fl), 3); gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(fl), 3); gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fl), TRUE);
    static const char *names[] = { "ondas", "lago", "aurora", "grafite", "vivo", "foto" };
    static const char *shown[] = { "Ondas", "Lago", "Aurora", "Grafite", "Vivo (animado)", "Minha foto" };
    char *cur = conf("papel");
    for (int i = 0; i < 6; i++) {
        const char *base = !strcmp(names[i], "vivo") ? "ondas" : names[i];
        char *f = g_strdup_printf("%s/.local/share/nova/papeis/%s-%s-mini.png", g_get_home_dir(), base, nova_dark ? "escuro" : "claro");
        if (!g_file_test(f, G_FILE_TEST_EXISTS)) { g_free(f); f = g_strdup_printf("%s/.local/share/nova/papeis/%s-mini.png", g_get_home_dir(), base); }
        if (!g_file_test(f, G_FILE_TEST_EXISTS)) { g_free(f); continue; }
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(f, 176, 99, FALSE, NULL); g_free(f); if (!pb) continue;
        GtkWidget *b = cls(gtk_button_new(), "themecard"); setc(b, "sel", !strcmp(cur, names[i]));
        GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
        gtk_box_pack_start(GTK_BOX(v), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(v), lbl(shown[i], "rtitle", .5), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(b), v); g_signal_connect(b, "clicked", G_CALLBACK(on_wall), (gpointer)names[i]);
        gtk_container_add(GTK_CONTAINER(fl), b);
    }
    g_free(cur);
    gtk_box_pack_start(GTK_BOX(p), fl, FALSE, FALSE, 0);
    GtkWidget *g = group(p, NULL); gtk_widget_set_margin_top(g, 14);
    row(g, "Usar uma foto minha", "Escolha qualquer imagem do computador", btn_new("Escolher…", "soft", G_CALLBACK(on_photo), NULL));
    return p;
}

/* ---------- Dock ---------- */
static void dconf_w(const char *k, const char *v) { char *c = g_strdup_printf("dconf write /net/launchpad/plank/docks/dock1/%s \"%s\"", k, v); g_free(nova_sh(c)); g_free(c); }
static void on_dock_size(GtkRange *r, gpointer d) { if (loading) return; char v[8]; g_snprintf(v, sizeof v, "%d", (int)gtk_range_get_value(r)); dconf_w("icon-size", v);
    char *c = g_strdup_printf("sed -i 's/^dock = .*/dock = %s/' $HOME/.config/nova/config", v); g_free(nova_sh(c)); g_free(c); }
static void on_dock_zoom(GtkRange *r, gpointer d) { if (loading) return; char v[8]; g_snprintf(v, sizeof v, "%d", (int)gtk_range_get_value(r)); dconf_w("zoom-percent", v); }
static void on_dock_zoom_sw(GObject *s, GParamSpec *p, gpointer d) { if (!loading) dconf_w("zoom-enabled", gtk_switch_get_active(GTK_SWITCH(s)) ? "true" : "false"); }
static void on_dock_hide(GObject *s, GParamSpec *p, gpointer d) { if (!loading) dconf_w("hide-mode", gtk_switch_get_active(GTK_SWITCH(s)) ? "'intelligent'" : "'none'"); }
static void on_dock_pos(GtkComboBox *c, gpointer d) { if (loading) return; static const char *v[] = { "'bottom'", "'left'", "'right'" }; dconf_w("position", v[gtk_combo_box_get_active(c)]); }
static GtkWidget *page_dock(void) {
    GtkWidget *p = page_new("Dock", "A barra de ícones na parte de baixo da tela.");
    GtkWidget *g = group(p, NULL);
    char *sz = nova_sh("dconf read /net/launchpad/plank/docks/dock1/icon-size"), *zp = nova_sh("dconf read /net/launchpad/plank/docks/dock1/zoom-percent"),
         *ze = nova_sh("dconf read /net/launchpad/plank/docks/dock1/zoom-enabled"), *hm = nova_sh("dconf read /net/launchpad/plank/docks/dock1/hide-mode"),
         *ps = nova_sh("dconf read /net/launchpad/plank/docks/dock1/position");
    row(g, "Tamanho dos ícones", NULL, scale_new(32, 64, 4, *sz ? atof(sz) : 48, G_CALLBACK(on_dock_size)));
    row(g, "Ampliar ao passar o mouse", "Os ícones crescem sob o cursor", sw_new(strcmp(ze, "false") != 0, G_CALLBACK(on_dock_zoom_sw), NULL));
    row(g, "Quanto ampliar", NULL, scale_new(110, 200, 10, *zp ? atof(zp) : 150, G_CALLBACK(on_dock_zoom)));
    row(g, "Esconder automaticamente", "A dock some quando uma janela chega perto e volta ao levar o mouse à borda", sw_new(strstr(hm, "none") == NULL && *hm, G_CALLBACK(on_dock_hide), NULL));
    GtkWidget *cb = gtk_combo_box_text_new(); const char *pn[] = { "Embaixo", "À esquerda", "À direita" };
    for (int i = 0; i < 3; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cb), pn[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), strstr(ps, "left") ? 1 : strstr(ps, "right") ? 2 : 0); g_signal_connect(cb, "changed", G_CALLBACK(on_dock_pos), NULL);
    row(g, "Posição na tela", NULL, cb);
    g_free(sz); g_free(zp); g_free(ze); g_free(hm); g_free(ps);
    return p;
}

/* ---------- Tela e som ---------- */
static guint bri_src, vol_src; static GtkWidget *bri_s, *vol_s;
static gboolean apply_bri(gpointer d) { char *c = g_strdup_printf("brightnessctl -q set %d%%", MAX(2, (int)gtk_range_get_value(GTK_RANGE(bri_s)))); nova_spawn(c); g_free(c); bri_src = 0; return G_SOURCE_REMOVE; }
static gboolean apply_vol(gpointer d) { char *c = g_strdup_printf("wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", (int)gtk_range_get_value(GTK_RANGE(vol_s))); nova_spawn(c); g_free(c); vol_src = 0; return G_SOURCE_REMOVE; }
static void on_bri(GtkRange *r, gpointer d) { if (!loading && !bri_src) bri_src = g_timeout_add(50, apply_bri, NULL); }
static void on_vol(GtkRange *r, gpointer d) { if (!loading && !vol_src) vol_src = g_timeout_add(50, apply_vol, NULL); }
static void apply_session(void) { char *c = g_strdup_printf("%snova-sessao aplicar", BIN); g_free(nova_sh(c)); g_free(c); }
static void on_opt_sw(GObject *s, GParamSpec *p, gpointer key) { if (loading) return; cfg_set(key, gtk_switch_get_active(GTK_SWITCH(s)) ? "1" : "0"); apply_session(); }
static void on_sleep(GtkComboBox *c, gpointer d) { if (loading) return; static const char *v[] = { "5", "10", "15", "30", "0" }; cfg_set("tela_min", v[gtk_combo_box_get_active(c)]); apply_session(); }
static void on_dpi_reset(GtkWidget *w, gpointer d);
static GtkWidget *dpi_s, *dpi_sub; static guint dpi_src;
static void dpi_text(int v) { char *t = g_strdup_printf("%d DPI%s", v, v == 1600 ? " (padrão)" : v < 1000 ? " — mais lento e preciso" : v > 2200 ? " — mais rápido" : ""); gtk_label_set_text(GTK_LABEL(dpi_sub), t); g_free(t); }
static gboolean apply_dpi(gpointer d) { char v[8]; g_snprintf(v, sizeof v, "%d", (int)gtk_range_get_value(GTK_RANGE(dpi_s))); cfg_set("dpi", v); apply_session(); dpi_src = 0; return G_SOURCE_REMOVE; }
static void on_dpi(GtkRange *r, gpointer d) {
    int v = (int)(gtk_range_get_value(r) / 100 + .5) * 100; if (v != (int)gtk_range_get_value(r)) { gtk_range_set_value(r, v); return; }
    dpi_text(v); if (!loading) { if (dpi_src) g_source_remove(dpi_src); dpi_src = g_timeout_add(250, apply_dpi, NULL); }
}
static void on_dpi_reset(GtkWidget *w, gpointer d) { gtk_range_set_value(GTK_RANGE(dpi_s), 1600); }
static GtkWidget *page_tela(void) {
    GtkWidget *p = page_new("Tela e som", "Brilho, volume, descanso de tela e touchpad.");
    char *b = nova_sh("d=$(ls -d /sys/class/backlight/* 2>/dev/null | head -1); [ -n \"$d\" ] && echo $(( $(cat $d/brightness) * 100 / $(cat $d/max_brightness) ))");
    char *v = nova_sh("LC_ALL=C wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | awk '{printf \"%d\", $2*100+0.5}'");
    GtkWidget *g = group(p, "TELA");
    bri_s = scale_new(0, 100, 1, *b ? atof(b) : 100, G_CALLBACK(on_bri)); row(g, "Brilho", NULL, bri_s);
    GtkWidget *cb = gtk_combo_box_text_new(); const char *sn[] = { "5 minutos", "10 minutos", "15 minutos", "30 minutos", "Nunca" }; const char *sv[] = { "5", "10", "15", "30", "0" };
    char *cur = cfg("tela_min", "10"); int act = 1;
    for (int i = 0; i < 5; i++) { gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cb), sn[i]); if (!strcmp(cur, sv[i])) act = i; }
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), act); g_signal_connect(cb, "changed", G_CALLBACK(on_sleep), NULL); g_free(cur);
    row(g, "Apagar a tela sem uso após", "Economiza bateria", cb);
    g = group(p, "SOM");
    vol_s = scale_new(0, 100, 1, *v ? atof(v) : 0, G_CALLBACK(on_vol)); row(g, "Volume", NULL, vol_s);
    row(g, "Testar o som", "Toca um aviso curto nas caixas de som", btn_new("Tocar", "soft", G_CALLBACK(on_run), "paplay /usr/share/sounds/alsa/Front_Center.wav"));
    g = group(p, "MOUSE");
    char *dv = cfg("dpi", "1600"); dpi_s = scale_new(400, 3200, 100, atoi(dv), G_CALLBACK(on_dpi)); g_free(dv);
    dpi_sub = row(g, "Sensibilidade (DPI)", "…", dpi_s); dpi_text((int)gtk_range_get_value(GTK_RANGE(dpi_s)));
    row(g, "Voltar ao padrão", "Ajusta o mouse para 1600 DPI", btn_new("Padrão", "plain", G_CALLBACK(on_dpi_reset), NULL));
    g = group(p, "TOUCHPAD");
    char *tp = cfg("toque", "1"), *nt = cfg("natural", "0");
    row(g, "Tocar para clicar", "Um toque leve no touchpad vale como clique", sw_new(!strcmp(tp, "1"), G_CALLBACK(on_opt_sw), "toque"));
    row(g, "Rolagem natural", "O conteúdo acompanha o movimento dos dedos, como no celular", sw_new(!strcmp(nt, "1"), G_CALLBACK(on_opt_sw), "natural"));
    g_free(b); g_free(v); g_free(tp); g_free(nt);
    return p;
}

/* ---------- Rede ---------- */
static GtkWidget *wifi_sub, *wifi_list, *eth_sub, *eth_ctl;
static gboolean net_busy, net_scanning;

typedef void (*RunDone)(gboolean ok, const char *out, gpointer data);
typedef struct { RunDone cb; gpointer data; } RunCtx;
static void run_done(GObject *src, GAsyncResult *res, gpointer d) {
    RunCtx *c = d; char *so = NULL, *se = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &so, &se, NULL) && g_subprocess_get_successful(G_SUBPROCESS(src));
    char *msg = g_strconcat(so ? so : "", se ? se : "", NULL);
    if (c->cb) c->cb(ok, msg, c->data);
    g_free(msg); g_free(so); g_free(se); g_object_unref(src); g_free(c);
}
// Roda um programa sem passar por shell (nome de rede e senha nunca são interpretados como comando).
static void run_async(const char *const *argv, const char *input, RunDone cb, gpointer data) {
    GSubprocessLauncher *l = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE | (input ? G_SUBPROCESS_FLAGS_STDIN_PIPE : 0));
    g_subprocess_launcher_setenv(l, "LC_ALL", "C", TRUE);
    GSubprocess *p = g_subprocess_launcher_spawnv(l, argv, NULL); g_object_unref(l);
    if (!p) { if (cb) cb(FALSE, "", data); return; }
    RunCtx *c = g_new(RunCtx, 1); c->cb = cb; c->data = data;
    g_subprocess_communicate_utf8_async(p, input, NULL, run_done, c);
}
static void run_sync(const char *const *argv) { g_spawn_sync(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL); }

static gboolean wifi_on(void) { char *r = nova_sh("LC_ALL=C nmcli radio wifi 2>/dev/null"); gboolean on = g_str_has_prefix(r, "enabled"); g_free(r); return on; }
static gboolean wifi_present(void) { char *r = nova_sh("LC_ALL=C nmcli -t -f TYPE dev 2>/dev/null | grep -x wifi"); gboolean p = *r; g_free(r); return p; }

/* --- cabo --- */
static void eth_refresh(void) {
    char *o = nova_sh("LC_ALL=C nmcli -t -f DEVICE,TYPE,STATE dev 2>/dev/null | awk -F: '$2==\"ethernet\"{print $1\" \"$3; exit}'");
    char *dev = g_strdup(o), *st = strchr(dev, ' '); if (st) *st++ = 0; else st = (char *)"";
    char *t = NULL; const char *acao = NULL, *rot = NULL;
    if (!*o) t = g_strdup("Este computador não tem entrada para cabo de rede");
    else if (g_str_has_prefix(st, "connected")) {
        char *c = g_strdup_printf("LC_ALL=C nmcli -g IP4.ADDRESS dev show %s 2>/dev/null | head -1 | cut -d/ -f1", dev), *ip = nova_sh(c);
        t = g_strdup_printf("Conectado por cabo · endereço %s", *ip ? ip : "—"); acao = "disconnect"; rot = "Desconectar"; g_free(c); g_free(ip);
    } else if (g_str_has_prefix(st, "connecting")) t = g_strdup("Conectando…");
    else if (g_str_has_prefix(st, "unavailable")) t = g_strdup("Nenhum cabo ligado. Conecte o cabo de rede no computador.");
    else { t = g_strdup("Cabo ligado, mas sem conexão"); acao = "connect"; rot = "Conectar"; }
    gtk_label_set_text(GTK_LABEL(eth_sub), t);
    if (acao) { gtk_button_set_label(GTK_BUTTON(eth_ctl), rot); g_object_set_data_full(G_OBJECT(eth_ctl), "acao", g_strdup(acao), g_free); g_object_set_data_full(G_OBJECT(eth_ctl), "dev", g_strdup(dev), g_free); }
    gtk_widget_set_visible(eth_ctl, acao != NULL);
    g_free(t); g_free(dev); g_free(o);
}
static gboolean eth_later(gpointer d) { eth_refresh(); return G_SOURCE_REMOVE; }
static void on_eth(GtkWidget *w, gpointer d) {
    const char *a = g_object_get_data(G_OBJECT(w), "acao"), *dev = g_object_get_data(G_OBJECT(w), "dev");
    if (!a || !dev) return;
    const char *argv[] = { "nmcli", "dev", a, dev, NULL }; run_async(argv, NULL, NULL, NULL);
    gtk_label_set_text(GTK_LABEL(eth_sub), "Aguarde…"); g_timeout_add(3000, eth_later, NULL);
}

/* --- Wi-Fi --- */
static void wifi_refresh(void) {
    if (!wifi_present()) { gtk_label_set_text(GTK_LABEL(wifi_sub), "Este computador não tem Wi-Fi"); return; }
    if (!wifi_on()) { gtk_label_set_text(GTK_LABEL(wifi_sub), "Desligado"); return; }
    char *o = nova_sh("LC_ALL=C nmcli -t -f ACTIVE,SSID dev wifi 2>/dev/null | awk -F: '$1==\"yes\"{print $2; exit}'"), *ip = nova_sh("hostname -I | awk '{print $1}'");
    char *t = *o ? g_strdup_printf("Conectado a \"%s\" · endereço %s", o, *ip ? ip : "—") : g_strdup("Sem conexão. Escolha uma rede na lista abaixo.");
    gtk_label_set_text(GTK_LABEL(wifi_sub), t); g_free(t); g_free(o); g_free(ip);
}
static void wifi_scan(gboolean force);
static gboolean wifi_later(gpointer d) { wifi_refresh(); wifi_scan(TRUE); return G_SOURCE_REMOVE; }

typedef struct { char *ssid, *sec; int sig; gboolean use; } Net;
static void net_free(gpointer p) { Net *n = p; g_free(n->ssid); g_free(n->sec); g_free(n); }
static gint net_cmp(gconstpointer a, gconstpointer b) {
    const Net *x = *(Net *const *)a, *y = *(Net *const *)b;
    if (x->use != y->use) return y->use - x->use; return y->sig - x->sig;
}
static char *unesc(const char *s) {           // nmcli -t escreve ":" e "\" com barra invertida
    GString *g = g_string_new(NULL);
    for (; *s; s++) { if (*s == '\\' && s[1]) s++; g_string_append_c(g, *s); }
    return g_string_free(g, FALSE);
}
static gboolean net_aberta(const char *sec) { return !*sec || !strcmp(sec, "--"); }

static void clear_list(void) { gtk_container_foreach(GTK_CONTAINER(wifi_list), (GtkCallback)gtk_widget_destroy, NULL); }
static void info_row(const char *t, const char *s) { clear_list(); row(wifi_list, t, s, NULL); gtk_widget_show_all(wifi_list); }

static void msg_box(const char *title, const char *text) {
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", title);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", text); gtk_dialog_run(GTK_DIALOG(dlg)); gtk_widget_destroy(dlg);
}
static void on_pw_changed(GtkEditable *e, gpointer dlg) { gtk_dialog_set_response_sensitive(GTK_DIALOG(dlg), GTK_RESPONSE_OK, strlen(gtk_entry_get_text(GTK_ENTRY(e))) >= 8); }
static void on_pw_show(GtkToggleButton *t, gpointer entry) { gtk_entry_set_visibility(GTK_ENTRY(entry), gtk_toggle_button_get_active(t)); }
static char *ask_password(const char *ssid, const char *aviso) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons("Conectar ao Wi-Fi", GTK_WINDOW(win), GTK_DIALOG_MODAL, "Cancelar", GTK_RESPONSE_CANCEL, "Conectar", GTK_RESPONSE_OK, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg)); gtk_container_set_border_width(GTK_CONTAINER(box), 18); gtk_box_set_spacing(GTK_BOX(box), 10);
    char *t = g_strdup_printf("Digite a senha da rede \"%s\"", ssid);
    gtk_box_pack_start(GTK_BOX(box), lbl(t, "rtitle", 0), FALSE, FALSE, 0); g_free(t);
    if (aviso) gtk_box_pack_start(GTK_BOX(box), lbl(aviso, "rsub", 0), FALSE, FALSE, 0);
    GtkWidget *en = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(en), FALSE); gtk_entry_set_activates_default(GTK_ENTRY(en), TRUE); gtk_widget_set_size_request(en, 300, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(en), "Senha do Wi-Fi (mínimo 8 caracteres)");
    gtk_box_pack_start(GTK_BOX(box), en, FALSE, FALSE, 0);
    GtkWidget *ck = gtk_check_button_new_with_label("Mostrar senha"); g_signal_connect(ck, "toggled", G_CALLBACK(on_pw_show), en);
    gtk_box_pack_start(GTK_BOX(box), ck, FALSE, FALSE, 0);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK); gtk_dialog_set_response_sensitive(GTK_DIALOG(dlg), GTK_RESPONSE_OK, FALSE);
    g_signal_connect(en, "changed", G_CALLBACK(on_pw_changed), dlg);
    gtk_widget_show_all(dlg); gtk_widget_grab_focus(en);
    char *pw = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK ? g_strdup(gtk_entry_get_text(GTK_ENTRY(en))) : NULL;
    gtk_widget_destroy(dlg); return pw;
}

typedef struct { char *ssid, *prev, *senha; } Conn;
static void conn_free(Conn *c) { if (c->senha) { memset(c->senha, 0, strlen(c->senha)); g_free(c->senha); } g_free(c->ssid); g_free(c->prev); g_free(c); }
static void wifi_connect(const char *ssid, const char *senha);
static void conn_done(gboolean ok, const char *out, gpointer d) {
    Conn *c = d; net_busy = FALSE;
    if (!ok) {   // limpa o perfil criado com senha errada e volta para a rede que estava ligada
        const char *del[] = { "nmcli", "con", "delete", "id", c->ssid, NULL }; run_sync(del);
        if (c->prev && *c->prev) { const char *up[] = { "nmcli", "con", "up", "id", c->prev, NULL }; run_async(up, NULL, NULL, NULL); }
        if (strstr(out, "ecret") || strstr(out, "assword") || strstr(out, "802-1x")) msg_box("Senha incorreta", "A senha não foi aceita pela rede. Confira e tente de novo.");
        else if (strstr(out, "No network")) msg_box("Rede não encontrada", "A rede está fora de alcance. Chegue mais perto do roteador e tente de novo.");
        else msg_box("Não foi possível conectar", "Tente de novo em alguns instantes. Se continuar, reinicie o roteador.");
    }
    conn_free(c); wifi_refresh(); wifi_scan(TRUE);
}
static void wifi_connect(const char *ssid, const char *senha) {
    if (net_busy) return; net_busy = TRUE;
    Conn *c = g_new0(Conn, 1); c->ssid = g_strdup(ssid); c->senha = senha ? g_strdup_printf("%s\n", senha) : NULL;
    c->prev = nova_sh("LC_ALL=C nmcli -t -f NAME,TYPE con show --active 2>/dev/null | awk -F: '$2==\"802-11-wireless\"{print $1; exit}'");
    char *t = g_strdup_printf("Conectando a \"%s\"…", ssid); gtk_label_set_text(GTK_LABEL(wifi_sub), t); g_free(t);
    const char *a1[] = { "nmcli", "-w", "30", "--ask", "dev", "wifi", "connect", ssid, NULL }, *a2[] = { "nmcli", "-w", "30", "dev", "wifi", "connect", ssid, NULL };
    run_async(senha ? a1 : a2, c->senha, conn_done, c);
}
static void saved_done(gboolean ok, const char *out, gpointer d) {
    Conn *c = d; net_busy = FALSE;
    if (!ok && (strstr(out, "ecret") || strstr(out, "assword"))) {      // senha salva não vale mais: pede de novo
        char *pw = ask_password(c->ssid, "A senha salva não funcionou. Ela pode ter sido trocada.");
        if (pw) { const char *del[] = { "nmcli", "con", "delete", "id", c->ssid, NULL }; run_sync(del); wifi_connect(c->ssid, pw); memset(pw, 0, strlen(pw)); g_free(pw); }
    } else if (!ok) msg_box("Não foi possível conectar", "A rede pode estar fora de alcance. Chegue mais perto do roteador e tente de novo.");
    conn_free(c); wifi_refresh(); wifi_scan(TRUE);
}
static void on_net_click(GtkWidget *w, gpointer d) {
    if (net_busy) return;
    const char *ssid = g_object_get_data(G_OBJECT(w), "ssid"), *sec = g_object_get_data(G_OBJECT(w), "sec");
    gboolean use = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "use")), saved = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "saved"));
    if (use) {
        char *dev = nova_sh("LC_ALL=C nmcli -t -f DEVICE,TYPE dev | awk -F: '$2==\"wifi\"{print $1; exit}'");
        const char *a[] = { "nmcli", "dev", "disconnect", dev, NULL }; run_sync(a); g_free(dev);
        g_timeout_add(1500, wifi_later, NULL); return;
    }
    if (saved) {
        net_busy = TRUE; Conn *c = g_new0(Conn, 1); c->ssid = g_strdup(ssid);
        char *t = g_strdup_printf("Conectando a \"%s\"…", ssid); gtk_label_set_text(GTK_LABEL(wifi_sub), t); g_free(t);
        const char *a[] = { "nmcli", "-w", "30", "con", "up", "id", ssid, NULL }; run_async(a, NULL, saved_done, c); return;
    }
    if (net_aberta(sec)) { wifi_connect(ssid, NULL); return; }
    if (strstr(sec, "802.1X") && !strstr(sec, "WPA-PSK") && !strstr(sec, "WPA2 ") && strcmp(sec, "WPA2") && strcmp(sec, "WPA1 WPA2")) {
        msg_box("Rede corporativa", "Esta rede exige usuário e senha especiais. Vai abrir a janela de configuração avançada."); nova_spawn("nm-connection-editor"); return;
    }
    char *pw = ask_password(ssid, NULL);
    if (pw) { wifi_connect(ssid, pw); memset(pw, 0, strlen(pw)); g_free(pw); }
}
static void on_net_forget(GtkWidget *w, gpointer d) {
    const char *ssid = g_object_get_data(G_OBJECT(w), "ssid"); const char *a[] = { "nmcli", "con", "delete", "id", ssid, NULL }; run_sync(a); wifi_scan(TRUE);
}
static void net_list_fill(const char *out) {
    GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal); GPtrArray *arr = g_ptr_array_new_with_free_func(net_free);
    char **ln = g_strsplit(out, "\n", -1);
    for (char **p = ln; *p; p++) {
        char *l = *p; char *c3 = strrchr(l, ':'); if (!c3) continue; *c3 = 0;
        char *c2 = strrchr(l, ':'); if (!c2) continue; *c2 = 0;
        char *c1 = strchr(l, ':'); if (!c1) continue; *c1 = 0;
        char *ssid = unesc(c1 + 1); if (!*ssid) { g_free(ssid); continue; }
        int sig = atoi(c2 + 1); gboolean use = l[0] == '*'; Net *n = g_hash_table_lookup(seen, ssid);
        if (n) { if (sig > n->sig) { n->sig = sig; g_free(n->sec); n->sec = g_strdup(c3 + 1); } n->use |= use; g_free(ssid); continue; }
        n = g_new0(Net, 1); n->ssid = ssid; n->sig = sig; n->use = use; n->sec = g_strdup(c3 + 1); g_ptr_array_add(arr, n); g_hash_table_insert(seen, n->ssid, n);
    }
    g_strfreev(ln); g_hash_table_destroy(seen); g_ptr_array_sort(arr, net_cmp);
    char *sv = nova_sh("LC_ALL=C nmcli -t -f NAME,TYPE con show 2>/dev/null | awk -F: '$2==\"802-11-wireless\"{print $1}'"); char **saved = g_strsplit(sv, "\n", -1); g_free(sv);
    clear_list();
    if (!arr->len) row(wifi_list, "Nenhuma rede encontrada", "Chegue mais perto do roteador e toque em Atualizar.", NULL);
    for (guint i = 0; i < arr->len && i < 15; i++) {
        Net *n = arr->pdata[i]; gboolean sv_ = g_strv_contains((const gchar *const *)saved, n->ssid);
        const char *sinal = n->sig >= 70 ? "sinal forte" : n->sig >= 40 ? "sinal médio" : "sinal fraco";
        char *s = g_strdup_printf("%s%s · %s%s", n->use ? "Conectado · " : "", sinal, net_aberta(n->sec) ? "aberta (sem senha)" : "protegida por senha", sv_ && !n->use ? " · salva" : "");
        GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        if (sv_ && !n->use) { GtkWidget *f = btn_new("Esquecer", "soft", G_CALLBACK(on_net_forget), NULL); g_object_set_data_full(G_OBJECT(f), "ssid", g_strdup(n->ssid), g_free); gtk_box_pack_start(GTK_BOX(hb), f, FALSE, FALSE, 0); }
        GtkWidget *b = btn_new(n->use ? "Desconectar" : "Conectar", n->use ? "soft" : "accent", G_CALLBACK(on_net_click), NULL);
        g_object_set_data_full(G_OBJECT(b), "ssid", g_strdup(n->ssid), g_free); g_object_set_data_full(G_OBJECT(b), "sec", g_strdup(n->sec), g_free);
        g_object_set_data(G_OBJECT(b), "use", GINT_TO_POINTER(n->use)); g_object_set_data(G_OBJECT(b), "saved", GINT_TO_POINTER(sv_));
        gtk_box_pack_start(GTK_BOX(hb), b, FALSE, FALSE, 0);
        row(wifi_list, n->ssid, s, hb); g_free(s);
    }
    g_strfreev(saved); g_ptr_array_free(arr, TRUE); gtk_widget_show_all(wifi_list);
}
static void rede_scan_done(gboolean ok, const char *out, gpointer d) { net_scanning = FALSE; if (!net_busy) net_list_fill(out); }
static void wifi_scan(gboolean force) {
    if (net_scanning || net_busy || !wifi_list) return;
    if (!wifi_present()) { info_row("Sem Wi-Fi", "Este computador não tem placa de Wi-Fi."); return; }
    if (!wifi_on()) { info_row("O Wi-Fi está desligado", "Ligue o interruptor “Wi-Fi” acima para ver as redes."); return; }
    net_scanning = TRUE;
    const char *a[] = { "nmcli", "-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "dev", "wifi", "list", "--rescan", force ? "yes" : "auto", NULL }; run_async(a, NULL, rede_scan_done, NULL);
}
static void on_rescan(GtkWidget *w, gpointer d) { wifi_scan(TRUE); }
static void on_wifi(GObject *s, GParamSpec *p, gpointer d) {
    if (loading) return;
    nova_sh(gtk_switch_get_active(GTK_SWITCH(s)) ? "nmcli radio wifi on" : "nmcli radio wifi off");
    wifi_refresh(); if (!gtk_switch_get_active(GTK_SWITCH(s))) wifi_scan(FALSE); else info_row("Procurando redes…", NULL);
    g_timeout_add(3500, wifi_later, NULL);
}
static gboolean net_tick(gpointer d) {
    const char *v = gtk_stack_get_visible_child_name(GTK_STACK(stack));
    if (v && !strcmp(v, PAGES[4]) && gtk_window_is_active(GTK_WINDOW(win))) { eth_refresh(); wifi_refresh(); wifi_scan(FALSE); }
    return G_SOURCE_CONTINUE;
}
static void net_page_shown(GObject *o, GParamSpec *p, gpointer d) {
    const char *v = gtk_stack_get_visible_child_name(GTK_STACK(stack));
    if (v && !strcmp(v, PAGES[4])) { eth_refresh(); wifi_refresh(); wifi_scan(TRUE); }
}
static void on_bt(GObject *s, GParamSpec *p, gpointer d) {
    if (loading) return;
    nova_sh(gtk_switch_get_active(GTK_SWITCH(s)) ? "rfkill unblock bluetooth 2>/dev/null; sudo -n /usr/local/sbin/nova-admin bt-on; (blueman-applet >/dev/null 2>&1 &)"
                                                  : "sudo -n /usr/local/sbin/nova-admin bt-off; rfkill block bluetooth 2>/dev/null; pkill -x blueman-applet");
}
static GtkWidget *page_rede(void) {
    GtkWidget *p = page_new("Rede e Bluetooth", "Escolha como se conectar à internet: por cabo de rede ou por Wi-Fi.");
    GtkWidget *g = group(p, "CABO DE REDE");
    eth_ctl = btn_new("Conectar", "soft", G_CALLBACK(on_eth), NULL); gtk_widget_set_no_show_all(eth_ctl, TRUE);
    eth_sub = row(g, "Cabo (Ethernet)", "…", eth_ctl); eth_refresh();
    g = group(p, "WI-FI");
    wifi_sub = row(g, "Wi-Fi", "…", sw_new(wifi_on(), G_CALLBACK(on_wifi), NULL)); wifi_refresh();
    row(g, "Redes ao redor", "Procura de novo as redes disponíveis", btn_new("Atualizar", "soft", G_CALLBACK(on_rescan), NULL));
    wifi_list = group(p, "REDES DISPONÍVEIS"); row(wifi_list, "Procurando redes…", NULL, NULL);
    g = group(p, "BLUETOOTH");
    char *b = nova_sh("systemctl is-active bluetooth 2>/dev/null");
    row(g, "Bluetooth", "Fones, caixas de som, mouse e teclado sem fio", sw_new(!strcmp(b, "active"), G_CALLBACK(on_bt), NULL)); g_free(b);
    row(g, "Conectar um aparelho", "Ligue o Bluetooth antes", btn_new("Abrir…", "soft", G_CALLBACK(on_run), "blueman-manager"));
    return p;
}

/* ---------- Segurança ---------- */
static GtkWidget *scan_sub, *scan_btn, *quar_sub;
static gboolean confirm_off(const char *title, const char *msg) {
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s", title);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", msg);
    gtk_dialog_add_button(GTK_DIALOG(dlg), "Manter ligado", GTK_RESPONSE_CANCEL); gtk_dialog_add_button(GTK_DIALOG(dlg), "Desativar mesmo assim", GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL);
    gboolean ok = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK; gtk_widget_destroy(dlg); return ok;
}
static void on_av(GObject *s, GParamSpec *p, gpointer d) {
    if (loading) return;
    gboolean on = gtk_switch_get_active(GTK_SWITCH(s));
    if (!on && !confirm_off("Desativar a proteção em tempo real?", "Sem ela, vírus em arquivos novos (downloads, pendrives, documentos) não serão bloqueados na hora. O computador ficará mais exposto. Só desative se tiver certeza.")) {
        loading = TRUE; gtk_switch_set_active(GTK_SWITCH(s), TRUE); loading = FALSE; return; }
    cfg_set("antivirus", on ? "1" : "0");
    nova_sh(on ? "pgrep -x inotifywait >/dev/null || ($HOME/.local/bin/nova-vigia-downloads >/dev/null 2>&1 &)" : "pkill -x inotifywait");
}
static void on_fw(GObject *s, GParamSpec *p, gpointer d) { if (!loading) nova_sh(gtk_switch_get_active(GTK_SWITCH(s)) ? "sudo -n /usr/local/sbin/nova-admin ufw-on" : "sudo -n /usr/local/sbin/nova-admin ufw-off"); }
static void on_autologin(GObject *s, GParamSpec *p, gpointer d) {
    if (loading) return;
    nova_sh(gtk_switch_get_active(GTK_SWITCH(s))
        ? "sudo -n /usr/local/sbin/nova-admin autologin-on"
        : "sudo -n /usr/local/sbin/nova-admin autologin-off");
}
static void quar_refresh(void) { char *n = nova_sh("ls $HOME/.local/share/nova/quarentena 2>/dev/null | wc -l"); char *t = atoi(n) ? g_strdup_printf("%s arquivo(s) bloqueado(s) e isolado(s)", n) : g_strdup("Nenhum arquivo bloqueado");
    gtk_label_set_text(GTK_LABEL(quar_sub), t); g_free(t); g_free(n); }
static void on_quar_clear(GtkWidget *w, gpointer d) { nova_sh("chmod -R u+rw $HOME/.local/share/nova/quarentena 2>/dev/null; rm -rf $HOME/.local/share/nova/quarentena/*"); quar_refresh(); }
static void scan_done(GObject *src, GAsyncResult *res, gpointer d) {
    char *out = NULL; g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, NULL, NULL);
    int inf = 0, tot = 0; char *q;
    if (out && (q = strstr(out, "Infected files:"))) inf = atoi(q + 15);
    if (out && (q = strstr(out, "Scanned files:"))) tot = atoi(q + 14);
    char *t = inf ? g_strdup_printf("Atenção: %d arquivo(s) com vírus, movido(s) para a quarentena", inf) : g_strdup_printf("Tudo certo: %d arquivo(s) verificado(s), nenhuma ameaça", tot);
    gtk_label_set_text(GTK_LABEL(scan_sub), t); g_free(t); g_free(out); gtk_widget_set_sensitive(scan_btn, TRUE); gtk_button_set_label(GTK_BUTTON(scan_btn), "Verificar agora"); quar_refresh(); g_object_unref(src);
}
static void on_scan(GtkWidget *w, gpointer d) {
    gtk_widget_set_sensitive(scan_btn, FALSE); gtk_button_set_label(GTK_BUTTON(scan_btn), "Verificando…"); gtk_label_set_text(GTK_LABEL(scan_sub), "Verificando a pasta Downloads. Pode levar um minuto.");
    GSubprocess *p = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL, "sh", "-c",
        "mkdir -p $HOME/.local/share/nova/quarentena; nice -n 10 clamscan -r --infected --move=$HOME/.local/share/nova/quarentena $HOME/Downloads", NULL);
    if (p) g_subprocess_communicate_utf8_async(p, NULL, NULL, scan_done, NULL);
}
static GtkWidget *full_sub, *full_btn; static gboolean full_run;
static char *full_status(gboolean *running) {
    char *c = NULL; *running = FALSE;
    if (!g_file_get_contents((char *)g_strdup_printf("%s/.local/share/nova/varredura.estado", g_get_home_dir()), &c, NULL, NULL) || !*c) { g_free(c); return g_strdup("Verifica o computador inteiro em busca de vírus e programas maliciosos. Nunca foi feita."); }
    char w[16] = ""; long t = 0; int inf = 0, tot = 0; sscanf(c, "%15s %ld %d %d", w, &t, &inf, &tot); g_free(c);
    char *r; char when[40] = "";
    if (t) { time_t tt = t; strftime(when, sizeof when, "%d/%m/%Y %H:%M", localtime(&tt)); }
    if (!strcmp(w, "rodando")) { *running = nova_sh("pgrep -x clamscan >/dev/null && echo 1")[0] == '1'; r = *running ? g_strdup("Verificando o computador inteiro… pode levar bastante tempo, e você pode continuar usando o PC.") : g_strdup("A última verificação foi interrompida."); }
    else if (!strcmp(w, "fim")) r = inf ? g_strdup_printf("Última: %s — %d arquivo(s) com vírus isolado(s) na quarentena (%d verificados).", when, inf, tot) : g_strdup_printf("Última: %s — tudo certo, %d arquivos verificados.", when, tot);
    else r = g_strdup("A última verificação foi interrompida.");
    return r;
}
static void full_refresh(void) {
    gboolean run; char *t = full_status(&run); gtk_label_set_text(GTK_LABEL(full_sub), t); g_free(t); full_run = run;
    gtk_button_set_label(GTK_BUTTON(full_btn), run ? "Parar" : "Verificar tudo"); quar_refresh();
}
static gboolean full_tick(gpointer d) { full_refresh(); return G_SOURCE_CONTINUE; }
static void on_full(GtkWidget *w, gpointer d) {
    char *c = g_strdup_printf("%snova-varredura%s", BIN, full_run ? " parar" : ""); if (full_run) g_free(nova_sh(c)); else nova_spawn(c);
    g_free(c); if (!full_run) { nova_sh("sleep 1"); } full_refresh();
}
static int full_prev = 3;
static void on_full_freq(GtkComboBox *c, gpointer d) {
    if (loading) return;
    static const char *v[] = { "0", "1", "3", "7", "15", "30" };
    int a = gtk_combo_box_get_active(c);
    if (a == 0 && !confirm_off("Desativar a varredura automática?", "O computador deixará de ser verificado por inteiro de tempos em tempos. Vírus que já estejam em arquivos antigos podem passar despercebidos.")) {
        loading = TRUE; gtk_combo_box_set_active(c, full_prev); loading = FALSE; return; }
    full_prev = a; cfg_set("varredura_dias", v[gtk_combo_box_get_active(c)]); }
static void on_ssh(GObject *s, GParamSpec *p, gpointer d) { if (!loading) nova_sh(gtk_switch_get_active(GTK_SWITCH(s)) ? "sudo -n /usr/local/sbin/nova-admin ssh-on" : "sudo -n /usr/local/sbin/nova-admin ssh-off"); }
static void on_lock(GObject *s, GParamSpec *p, gpointer d) { if (loading) return; gboolean on = gtk_switch_get_active(GTK_SWITCH(s)); cfg_set("bloqueio", on ? "1" : "0");
    nova_sh(on ? "pgrep -x xss-lock >/dev/null || (setsid xss-lock --transfer-sleep-lock -- dm-tool lock >/dev/null 2>&1 &)" : "pkill -x xss-lock"); }
static GtkWidget *page_seguranca(void) {
    GtkWidget *p = page_new("Segurança", "Proteção contra vírus, acessos de fora e uso por outras pessoas.");
    GtkWidget *g = group(p, "ANTIVÍRUS");
    char *av = cfg("antivirus", "1"), *defs = nova_sh("date -r /var/lib/clamav/daily.cvd '+%d/%m/%Y' 2>/dev/null || date -r /var/lib/clamav/daily.cld '+%d/%m/%Y' 2>/dev/null");
    char *ds = *defs ? g_strdup_printf("Arquivos novos em Downloads, Documentos, Imagens, Vídeos, Música, pendrives e /tmp são checados na hora. Lista de vírus atualizada em %s.", defs) : g_strdup("Arquivos novos em Downloads, Documentos, Imagens, Vídeos, Música, pendrives e /tmp são checados na hora.");
    row(g, "Proteção em tempo real", ds, sw_new(!strcmp(av, "1"), G_CALLBACK(on_av), NULL)); g_free(ds); g_free(av); g_free(defs);
    scan_btn = btn_new("Verificar agora", "soft", G_CALLBACK(on_scan), NULL);
    scan_sub = row(g, "Verificação da pasta Downloads", "Confere todos os arquivos já baixados", scan_btn);
    quar_sub = row(g, "Quarentena", "…", btn_new("Esvaziar", "plain", G_CALLBACK(on_quar_clear), NULL)); quar_refresh();
    full_btn = btn_new("Verificar tudo", "accent", G_CALLBACK(on_full), NULL);
    full_sub = row(g, "Varredura completa do computador", "…", full_btn);
    GtkWidget *fc = gtk_combo_box_text_new(); const char *fn[] = { "Nunca", "Todo dia", "A cada 3 dias", "Toda semana", "A cada 15 dias", "Todo mês" }, *fv[] = { "0", "1", "3", "7", "15", "30" };
    char *fcur = cfg("varredura_dias", "7"); int fa = 3;
    for (int i = 0; i < 6; i++) { gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(fc), fn[i]); if (!strcmp(fcur, fv[i])) fa = i; }
    gtk_combo_box_set_active(GTK_COMBO_BOX(fc), fa); full_prev = fa; g_signal_connect(fc, "changed", G_CALLBACK(on_full_freq), NULL); g_free(fcur);
    row(g, "Varredura automática", "Roda sozinha em segundo plano, só com o notebook na tomada. Se estava desligado na hora, roda quando ligar.", fc);
    full_refresh(); g_timeout_add_seconds(3, full_tick, NULL);
    g = group(p, "PROTEÇÃO");
    char *fw = nova_sh("sudo -n /usr/local/sbin/nova-admin ufw-status 2>/dev/null | head -1");
    row(g, "Firewall", "Bloqueia tentativas de acesso vindas da rede", sw_new(strstr(fw, "active") && !strstr(fw, "inactive"), G_CALLBACK(on_fw), NULL)); g_free(fw);
    row(g, "Navegação protegida", "O navegador bloqueia sites perigosos e anúncios maliciosos, e prefere conexões seguras (HTTPS)", lbl("Ativa", "badge", .5));
    g = group(p, "ACESSO");
    char *al = nova_sh("grep -c '^autologin-user=' /etc/lightdm/lightdm.conf.d/50-nova.conf 2>/dev/null");
    row(g, "Entrar sem pedir senha", "Ao ligar, o computador abre direto na área de trabalho. Qualquer pessoa com o notebook em mãos terá acesso.", sw_new(atoi(al) > 0, G_CALLBACK(on_autologin), NULL)); g_free(al);
    char *bl = cfg("bloqueio", "1"), *sh = nova_sh("systemctl is-active ssh");
    row(g, "Pedir a senha ao voltar do descanso", "Ao fechar a tampa, suspender ou depois do descanso de tela, o computador pede a senha de novo.", sw_new(!strcmp(bl, "1"), G_CALLBACK(on_lock), NULL));
    row(g, "Acesso remoto (SSH)", "Permite que outro computador da sua casa controle este por comandos. Deixe desligado se não usa.", sw_new(!strcmp(sh, "active"), G_CALLBACK(on_ssh), NULL)); g_free(bl); g_free(sh);
    row(g, "Bloquear a tela agora", "Atalho: tecla Windows + L", btn_new("Bloquear", "soft", G_CALLBACK(on_run), "dm-tool lock"));
    return p;
}

/* ---------- Aplicativos ---------- */
static char **nav_ids;
static void on_nav(GtkComboBox *c, gpointer d) {
    if (loading) return; int a = gtk_combo_box_get_active(c); if (a < 0) return;
    cfg_set("navegador", nav_ids[a]);
    g_free(nova_sh("xdg-settings set default-web-browser nova-navegador.desktop 2>/dev/null"));
}
static GtkWidget *page_apps(void) {
    GtkWidget *p = page_new("Aplicativos", "Escolha qual programa abre cada tipo de coisa.");
    GtkWidget *g = group(p, "NAVEGADOR DA INTERNET");
    char *lst = nova_sh("for f in /usr/share/applications/*.desktop $HOME/.local/share/applications/*.desktop; do "
        "case \"$f\" in */nova-*) continue;; esac; grep -q '^Categories=.*WebBrowser' \"$f\" 2>/dev/null || continue; "
        "n=$(sed -n 's/^Name=//p' \"$f\" | head -1); echo \"$(basename \"$f\")|$n\"; done");
    char *cur = cfg("navegador", "firefox-esr.desktop");
    gchar **ls = g_strsplit(lst, "\n", -1); int n = g_strv_length(ls);
    nav_ids = g_new0(char *, n + 1); GtkWidget *c = gtk_combo_box_text_new(); int k = 0, act = 0;
    for (int i = 0; i < n; i++) { if (!*ls[i]) continue; gchar **kv = g_strsplit(ls[i], "|", 2); if (!kv[1]) { g_strfreev(kv); continue; }
        nav_ids[k] = g_strdup(kv[0]); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c), kv[1]); if (!strcmp(cur, kv[0])) act = k; k++; g_strfreev(kv); }
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), act); g_signal_connect(c, "changed", G_CALLBACK(on_nav), NULL);
    row(g, "Navegador padrão", "É o que abre ao clicar no ícone Navegador do dock e em links de outros programas. Se instalar outro navegador pela Loja, ele aparece aqui.", c);
    g_strfreev(ls); g_free(lst); g_free(cur);
    return p;
}

/* ---------- Atalhos ---------- */
static void keyrow(GtkWidget *g, const char *what, const char *keys) { row(g, what, NULL, lbl(keys, "keys", .5)); }
static GtkWidget *page_atalhos(void) {
    GtkWidget *p = page_new("Atalhos de teclado", "Todos os comandos de teclado do NOVA OS. A tecla Windows é a que fica entre Ctrl e Alt (⊞).");
    GtkWidget *g = group(p, "PROGRAMAS E SISTEMA");
    keyrow(g, "Abrir a lista de Apps", "⊞ (tocar sozinha)  ou  ⊞ + Espaço  ou  ⊞ + R");
    keyrow(g, "Central de controle (Wi-Fi, volume, brilho)", "⊞ + A   ou   ⊞ + C   ou   ⊞ + X");
    keyrow(g, "Abrir os Ajustes", "⊞ + I");
    keyrow(g, "Abrir os Arquivos", "⊞ + E");
    keyrow(g, "Abrir o terminal", "⊞ + T");
    keyrow(g, "Histórico do que foi copiado", "⊞ + V");
    keyrow(g, "Bloquear a tela", "⊞ + L");
    keyrow(g, "Monitor do sistema (programa travado)", "Ctrl + Alt + Delete   ou   Ctrl + Shift + Esc");
    g = group(p, "JANELAS");
    keyrow(g, "Trocar de janela", "Alt + Tab   (Shift junto volta)");
    keyrow(g, "Trocar de janela (alternativa)", "⊞ + Tab");
    keyrow(g, "Fechar a janela", "Alt + F4");
    keyrow(g, "Maximizar", "⊞ + ↑");
    keyrow(g, "Restaurar ou minimizar", "⊞ + ↓");
    keyrow(g, "Encostar na metade esquerda ou direita", "⊞ + ←   /   ⊞ + →");
    keyrow(g, "Tela cheia (vídeos, navegador)", "F11");
    keyrow(g, "Mostrar a área de trabalho", "⊞ + D   ou   ⊞ + M");
    keyrow(g, "Menu da janela", "Alt + Espaço");
    keyrow(g, "Focar a janela ao lado", "⊞ + Shift + setas");
    g = group(p, "ÁREAS DE TRABALHO (TELAS VIRTUAIS)");
    keyrow(g, "Ir para a área 1, 2, 3 ou 4", "⊞ + F1 … F4");
    keyrow(g, "Mudar de área", "Ctrl + Alt + setas");
    keyrow(g, "Levar a janela para outra área", "Shift + Alt + setas");
    g = group(p, "CAPTURA DE TELA");
    keyrow(g, "Capturar uma parte da tela", "Print Screen   ou   ⊞ + Shift + S   ou   Alt + Print");
    keyrow(g, "Capturar a tela inteira (salva em Imagens)", "⊞ + Print");
    g = group(p, "TECLAS DO NOTEBOOK");
    keyrow(g, "Brilho da tela", "Teclas de brilho (Fn)");
    keyrow(g, "Volume, mudo e microfone", "Teclas de volume e mudo (Fn)");
    g = group(p, "EM QUALQUER PROGRAMA");
    keyrow(g, "Copiar / Recortar / Colar", "Ctrl + C   /   Ctrl + X   /   Ctrl + V");
    keyrow(g, "Desfazer / Refazer", "Ctrl + Z   /   Ctrl + Y");
    keyrow(g, "Selecionar tudo", "Ctrl + A");
    keyrow(g, "Salvar", "Ctrl + S");
    keyrow(g, "Procurar", "Ctrl + F");
    keyrow(g, "Nova aba / fechar aba (navegador)", "Ctrl + T   /   Ctrl + W");
    return p;
}

/* ---------- Atualizações (compara com o repositório do NOVA OS no GitHub) ---------- */
static GtkWidget *up_status, *up_check, *up_ver, *up_inst_grp, *up_wrap, *up_changes, *up_step, *up_bar;
static gboolean up_busy; static GSubprocess *up_proc; static GDataInputStream *up_in; static char *up_erro; static guint up_pulse;

static char *up_version_text(void) {
    char *f = g_build_filename(g_get_home_dir(), ".local/share/nova/versao", NULL), *s = NULL, *t = NULL;
    if (g_file_get_contents(f, &s, NULL, NULL)) { char **l = g_strsplit(s, "\n", 3); if (l[0] && strlen(l[0]) >= 7) t = g_strdup_printf("%.7s · %s", l[0], l[1] ? l[1] : ""); g_strfreev(l); }
    g_free(f); g_free(s); return t ? t : g_strdup("não registrada");
}
static void showw(GtkWidget *w) { gtk_widget_set_no_show_all(w, FALSE); gtk_widget_show_all(w); }
static void up_clear_changes(void) { gtk_container_foreach(GTK_CONTAINER(up_changes), (GtkCallback)gtk_widget_destroy, NULL); }
static void up_check_done(gboolean ok, const char *out, gpointer d) {
    char **ln = g_strsplit(out, "\n", -1), *estado = NULL, *disp = NULL, *data = NULL; up_clear_changes(); int n = 0;
    for (char **p = ln; *p; p++) {
        if (g_str_has_prefix(*p, "estado=")) estado = *p + 7; else if (g_str_has_prefix(*p, "disponivel=")) disp = *p + 11; else if (g_str_has_prefix(*p, "data_disponivel=")) data = *p + 16;
        else if (g_str_has_prefix(*p, "mudanca=")) {
            char *t = g_strdup_printf("•  %s", *p + 8); GtkWidget *l = lbl(t, "rsub", 0); g_free(t);
            gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(l), 62); gtk_widget_set_margin_start(l, 14); gtk_widget_set_margin_end(l, 14); gtk_widget_set_margin_top(l, 7); gtk_widget_set_margin_bottom(l, 7);
            gtk_box_pack_start(GTK_BOX(up_changes), l, FALSE, FALSE, 0); n++;
        }
    }
    char *t;
    if (estado && !strcmp(estado, "atual")) t = g_strdup("Seu sistema está atualizado ✓");
    else if (estado && (!strcmp(estado, "nova") || !strcmp(estado, "desconhecida")))
        t = g_strdup_printf("Há uma atualização disponível (versão %s, de %s). Veja as novidades abaixo e toque em Instalar.", disp ? disp : "?", data ? data : "?");
    else t = g_strdup("Não foi possível verificar. Confira se o computador está com internet e tente de novo.");
    gtk_label_set_text(GTK_LABEL(up_status), t); g_free(t);
    gboolean nova = estado && (!strcmp(estado, "nova") || !strcmp(estado, "desconhecida"));
    if (nova) { showw(up_inst_grp); if (n) showw(up_wrap); else gtk_widget_hide(up_wrap); } else { gtk_widget_hide(up_inst_grp); gtk_widget_hide(up_wrap); }
    { char *vt = up_version_text(); gtk_label_set_text(GTK_LABEL(up_ver), vt); g_free(vt); }
    gtk_button_set_label(GTK_BUTTON(up_check), "Verificar de novo"); gtk_widget_set_sensitive(up_check, TRUE); up_busy = FALSE;
    g_strfreev(ln);
}
static void on_up_check(GtkWidget *w, gpointer d) {
    if (up_busy) return; up_busy = TRUE; gtk_widget_set_sensitive(up_check, FALSE); gtk_button_set_label(GTK_BUTTON(up_check), "Verificando…");
    gtk_label_set_text(GTK_LABEL(up_status), "Consultando o repositório do NOVA OS na internet…");
    char *c = g_strdup_printf("%snova-atualizar", BIN); const char *a[] = { c, "verificar", NULL }; run_async(a, NULL, up_check_done, NULL); g_free(c);
}

static void up_finish(GObject *src, GAsyncResult *res, gpointer d) {
    g_subprocess_wait_finish(G_SUBPROCESS(src), res, NULL); gboolean ok = g_subprocess_get_successful(G_SUBPROCESS(src));
    if (up_pulse) { g_source_remove(up_pulse); up_pulse = 0; }
    gtk_widget_hide(up_bar); gtk_widget_hide(up_step); up_busy = FALSE;
    g_clear_object(&up_in); g_clear_object(&up_proc);
    char *v = up_version_text(); gtk_label_set_text(GTK_LABEL(up_ver), v); g_free(v);
    if (ok) {
        gtk_label_set_text(GTK_LABEL(up_status), "Atualização instalada ✓");
        gtk_widget_hide(up_inst_grp); gtk_widget_hide(up_wrap);
        GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, "Atualização instalada!");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "Para aplicar todas as novidades, reinicie o computador. Pode ser agora ou mais tarde.");
        gtk_dialog_add_button(GTK_DIALOG(dlg), "Mais tarde", GTK_RESPONSE_CANCEL); gtk_dialog_add_button(GTK_DIALOG(dlg), "Reiniciar agora", GTK_RESPONSE_OK);
        gboolean rb = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK; gtk_widget_destroy(dlg); if (rb) nova_spawn("systemctl reboot");
    } else {
        gtk_widget_set_sensitive(up_check, TRUE); showw(up_inst_grp);
        gboolean pw = up_erro && strstr(up_erro, "senha");
        gtk_label_set_text(GTK_LABEL(up_status), pw ? "Senha incorreta. Toque em Instalar para tentar de novo." : "A atualização não foi concluída. Toque em Instalar para tentar de novo.");
        msg_box(pw ? "Senha incorreta" : "A atualização não terminou",
                pw ? "A senha digitada não confere. Use a mesma senha com que você entra no computador." : up_erro ? up_erro : "Tente de novo. Se continuar, os detalhes ficam em ~/.local/share/nova/instalacao.log");
    }
    g_free(up_erro); up_erro = NULL;
}
static void up_line(GObject *src, GAsyncResult *res, gpointer d) {
    char *ln = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(src), res, NULL, NULL);
    if (!ln) { g_subprocess_wait_async(up_proc, NULL, up_finish, NULL); return; }
    const char *txt = NULL;
    if (g_str_has_prefix(ln, "ETAPA: ")) txt = ln + 7; else if (g_str_has_prefix(ln, "==> ")) txt = ln + 4;
    else if (g_str_has_prefix(ln, "ERRO: ")) { g_free(up_erro); up_erro = g_strdup(ln + 6); }
    if (txt) gtk_label_set_text(GTK_LABEL(up_step), txt);
    g_free(ln); g_data_input_stream_read_line_async(G_DATA_INPUT_STREAM(src), G_PRIORITY_DEFAULT, NULL, up_line, NULL);
}
static gboolean up_pulse_cb(gpointer d) { gtk_progress_bar_pulse(GTK_PROGRESS_BAR(up_bar)); return G_SOURCE_CONTINUE; }
static char *ask_admin(void) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons("Senha do administrador", GTK_WINDOW(win), GTK_DIALOG_MODAL, "Cancelar", GTK_RESPONSE_CANCEL, "Continuar", GTK_RESPONSE_OK, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg)); gtk_container_set_border_width(GTK_CONTAINER(box), 18); gtk_box_set_spacing(GTK_BOX(box), 10);
    gtk_box_pack_start(GTK_BOX(box), lbl("Digite a senha que você usa para entrar no computador", "rtitle", 0), FALSE, FALSE, 0);
    GtkWidget *en = gtk_entry_new(); gtk_entry_set_visibility(GTK_ENTRY(en), FALSE); gtk_entry_set_activates_default(GTK_ENTRY(en), TRUE); gtk_widget_set_size_request(en, 300, -1);
    gtk_box_pack_start(GTK_BOX(box), en, FALSE, FALSE, 0);
    GtkWidget *ck = gtk_check_button_new_with_label("Mostrar senha"); g_signal_connect(ck, "toggled", G_CALLBACK(on_pw_show), en); gtk_box_pack_start(GTK_BOX(box), ck, FALSE, FALSE, 0);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK); gtk_widget_show_all(dlg); gtk_widget_grab_focus(en);
    char *pw = NULL; if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK && *gtk_entry_get_text(GTK_ENTRY(en))) pw = g_strdup(gtk_entry_get_text(GTK_ENTRY(en)));
    gtk_widget_destroy(dlg); return pw;
}
static void on_up_install(GtkWidget *w, gpointer d) {
    if (up_busy) return;
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "Instalar a atualização?");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "O computador vai baixar as novidades e instalá-las. Pode levar alguns minutos; não desligue durante o processo. Suas configurações e arquivos continuam como estão.");
    gtk_dialog_add_button(GTK_DIALOG(dlg), "Agora não", GTK_RESPONSE_CANCEL); gtk_dialog_add_button(GTK_DIALOG(dlg), "Instalar", GTK_RESPONSE_OK); gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gboolean go = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK; gtk_widget_destroy(dlg); if (!go) return;
    char *pw = ask_admin(); if (!pw) return;
    char *c = g_strdup_printf("%snova-atualizar", BIN); const char *a[] = { c, "instalar", NULL };
    up_proc = g_subprocess_newv(a, G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE, NULL); g_free(c);
    if (!up_proc) { memset(pw, 0, strlen(pw)); g_free(pw); msg_box("Não foi possível iniciar", "O programa de atualização não foi encontrado."); return; }
    GOutputStream *o = g_subprocess_get_stdin_pipe(up_proc); char *l = g_strdup_printf("%s\n", pw);
    g_output_stream_write_all(o, l, strlen(l), NULL, NULL, NULL); g_output_stream_close(o, NULL, NULL);
    memset(l, 0, strlen(l)); g_free(l); memset(pw, 0, strlen(pw)); g_free(pw);
    up_busy = TRUE; gtk_widget_set_sensitive(up_check, FALSE); gtk_widget_hide(up_inst_grp);
    gtk_label_set_text(GTK_LABEL(up_status), "Instalando a atualização. Não desligue o computador."); gtk_label_set_text(GTK_LABEL(up_step), "Começando…");
    gtk_widget_show(up_step); gtk_widget_show(up_bar); up_pulse = g_timeout_add(120, up_pulse_cb, NULL);
    up_in = g_data_input_stream_new(g_subprocess_get_stdout_pipe(up_proc)); g_data_input_stream_read_line_async(up_in, G_PRIORITY_DEFAULT, NULL, up_line, NULL);
}
static void on_up_aviso(GObject *s, GParamSpec *p, gpointer d) { if (!loading) cfg_set("atualiza_aviso", gtk_switch_get_active(GTK_SWITCH(s)) ? "1" : "0"); }
static GtkWidget *page_atualiza(void) {
    GtkWidget *p = page_new("Atualizações", "Veja se o NOVA OS tem novidades e instale com um clique.");
    GtkWidget *g = group(p, "SEU SISTEMA");
    char *v = up_version_text(); up_ver = lbl(v, "rval", 1); g_free(v);
    row(g, "Versão instalada", NULL, up_ver);
    up_check = btn_new("Verificar atualização", "accent", G_CALLBACK(on_up_check), NULL);
    up_status = row(g, "Atualização do sistema", "Toque em Verificar para procurar novidades. É preciso internet.", up_check);
    up_inst_grp = group(p, NULL); row(up_inst_grp, "Instalar a atualização", "Baixa e instala as novidades. Suas escolhas e arquivos são mantidos.", btn_new("Instalar", "accent", G_CALLBACK(on_up_install), NULL));
    gtk_widget_set_no_show_all(up_inst_grp, TRUE); gtk_widget_hide(up_inst_grp);
    up_wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); GtkWidget *cap = lbl("O QUE MUDA", "cap", 0); gtk_widget_set_margin_top(cap, 8); gtk_widget_set_margin_bottom(cap, 7); gtk_widget_set_margin_start(cap, 4);
    gtk_box_pack_start(GTK_BOX(up_wrap), cap, FALSE, FALSE, 0);
    up_changes = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "group"); gtk_widget_set_margin_bottom(up_changes, 12); gtk_box_pack_start(GTK_BOX(up_wrap), up_changes, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p), up_wrap, FALSE, FALSE, 0); gtk_widget_set_no_show_all(up_wrap, TRUE); gtk_widget_hide(up_wrap);
    up_step = lbl("", "rsub", 0); gtk_widget_set_margin_top(up_step, 6); gtk_widget_set_no_show_all(up_step, TRUE); gtk_box_pack_start(GTK_BOX(p), up_step, FALSE, FALSE, 0);
    up_bar = gtk_progress_bar_new(); gtk_widget_set_margin_top(up_bar, 6); gtk_widget_set_margin_bottom(up_bar, 12); gtk_widget_set_no_show_all(up_bar, TRUE); gtk_box_pack_start(GTK_BOX(p), up_bar, FALSE, FALSE, 0);
    g = group(p, "AVISOS");
    char *a = cfg("atualiza_aviso", "1");
    row(g, "Avisar sobre novidades", "Ao ligar o computador, verifica (no máximo uma vez por dia) e mostra um aviso se houver atualização. Nunca instala sozinho.", sw_new(!strcmp(a, "1"), G_CALLBACK(on_up_aviso), NULL)); g_free(a);
    return p;
}

/* ---------- Sobre ---------- */
static GtkWidget *page_sobre(void) {
    GtkWidget *p = page_new("Sobre", "Informações deste computador.");
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18); gtk_widget_set_margin_bottom(h, 18);
    char *f = g_build_filename(g_get_home_dir(), ".local/share/nova/icons/marca.png", NULL); GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size(f, 84, 84, NULL); g_free(f);
    if (pb) gtk_box_pack_start(GTK_BOX(h), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(v), lbl("NOVA OS", "h1", 0), FALSE, FALSE, 0);
    char *deb = nova_sh(". /etc/os-release; echo \"Versão 1.0 · feito sobre o $PRETTY_NAME\""); gtk_box_pack_start(GTK_BOX(v), lbl(deb, "sub", 0), FALSE, FALSE, 0); g_free(deb);
    gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(p), h, FALSE, FALSE, 0);
    GtkWidget *g = group(p, NULL);
    char *model = nova_sh("echo \"$(cat /sys/class/dmi/id/sys_vendor 2>/dev/null) $(cat /sys/class/dmi/id/product_name 2>/dev/null)\""), *cpu = nova_sh("sed -n 's/^model name\\s*: //p' /proc/cpuinfo | head -1 | sed 's/(R)//g; s/(TM)//g; s/ CPU//; s/  */ /g'");
    char *mem = nova_sh("awk '/MemTotal/{printf \"%.1f GB\", $2/1048576}' /proc/meminfo"), *host = nova_sh("hostname");
    struct statvfs s; char disk[64] = "—"; if (!statvfs("/", &s)) g_snprintf(disk, sizeof disk, "%.0f GB livres de %.0f GB", (double)s.f_bavail * s.f_frsize / 1e9, (double)s.f_blocks * s.f_frsize / 1e9);
    row(g, "Computador", NULL, lbl(model, "rval", 1)); row(g, "Nome na rede", NULL, lbl(host, "rval", 1)); row(g, "Processador", NULL, lbl(cpu, "rval", 1));
    row(g, "Memória", NULL, lbl(mem, "rval", 1)); row(g, "Armazenamento", NULL, lbl(disk, "rval", 1));
    g_free(model); g_free(cpu); g_free(mem); g_free(host);
    g = group(p, NULL);
    row(g, "Atualizações do sistema", "Baixa e instala correções de segurança. Abre uma janela que mostra o andamento.",
        btn_new("Atualizar", "accent", G_CALLBACK(on_run), "xterm -T 'Atualizando o sistema' -e sh -c 'sudo apt-get update && sudo apt-get -y full-upgrade; echo; echo Pronto. Pode fechar esta janela.; read x'"));
    row(g, "Liberar espaço e memória", NULL, btn_new("Abrir o Otimizador", "soft", G_CALLBACK(on_run), "$HOME/.local/bin/nova-otimizador"));
    return p;
}

/* ---------- janela ---------- */
static gboolean draw_side(GtkWidget *w, cairo_t *cr, gpointer d) {
    nova_glass(w, cr, NULL); GtkAllocation a; gtk_widget_get_allocation(w, &a);
    cairo_set_source_rgba(cr, 0, 0, 0, nova_dark ? .35 : .10); cairo_rectangle(cr, a.width - 1, 0, 1, a.height); cairo_fill(cr); return FALSE;
}
static void on_page(GtkListBox *b, GtkListBoxRow *r, gpointer d) { if (r) gtk_stack_set_visible_child_name(GTK_STACK(stack), PAGES[gtk_list_box_row_get_index(r)]); }

static const char *CSS =
"window { background-color: @bg; }"
".side { padding: 14px 10px; }"
".brand { font-size: 17px; font-weight: 700; color: @fg; margin: 4px 8px 12px 8px; }"
".side list { background: none; }"
".side row { border-radius: 9px; padding: 7px 10px; margin: 1px 0; color: @fg; transition: background-color 200ms ease, color 200ms ease; }"
".side row:hover { background-color: alpha(@fg,.06); }"
".side row:selected { background-color: @accent; color: #fff; box-shadow: 0 3px 10px alpha(@accent,.35); }"
".side row image { color: @accent; margin-right: 10px; transition: -gtk-icon-transform 260ms cubic-bezier(.3,1.8,.5,1), color 200ms ease; }"
".side row:hover image { -gtk-icon-transform: scale(1.22); } .side row:selected image { color: #fff; }"
".side row label { font-size: 13.5px; font-weight: 500; }"
".group { border-radius: 12px; background-color: @card; border: 1px solid alpha(@fg,.08); box-shadow: 0 1px 3px alpha(#000,.05); }"
".row { padding: 11px 14px; }"
".hair { background-color: alpha(@fg,.08); min-height: 1px; margin: 0 14px; }"
".rtitle { font-size: 13.5px; font-weight: 500; color: @fg; } .rsub { font-size: 12px; color: @fg2; } .rval { font-size: 13px; color: @fg2; }"
".keys { font-size: 12px; font-weight: 600; color: @fg; background-color: alpha(@fg,.08); border-radius: 7px; padding: 3px 9px; }"
".badge { font-size: 11.5px; font-weight: 700; color: @ok; background-color: alpha(@ok,.14); border-radius: 999px; padding: 2px 10px; }"
"button.themecard { border-radius: 14px; padding: 8px; background-image: none; background-color: alpha(@fg,.04); border: 2px solid transparent; box-shadow: none;"
"  transition: all 240ms cubic-bezier(.3,1.4,.5,1); }"
"button.themecard:hover { background-color: alpha(@fg,.08); padding: 6px; margin: 2px; }"
"button.themecard.sel { border-color: @accent; background-color: alpha(@accent,.10); box-shadow: 0 6px 18px alpha(@accent,.22); }"
"button.dot { border-radius: 999px; min-width: 22px; min-height: 22px; padding: 0; background-image: none; border: 2px solid alpha(#fff,.0); box-shadow: 0 1px 3px alpha(#000,.25);"
"  transition: all 240ms cubic-bezier(.3,1.8,.5,1); }"
"button.dot:hover { margin: -3px; min-width: 28px; min-height: 28px; }"
"button.dot.sel { border-color: @base; box-shadow: 0 0 0 2px @fg; }"
"combobox button { border-radius: 8px; padding: 3px 10px; }"
"flowbox, flowboxchild { background: none; outline: none; padding: 3px; }"
"scrolledwindow undershoot, scrolledwindow overshoot { background: none; }";

int main(int argc, char **argv) {
    self_path = g_file_read_link("/proc/self/exe", NULL); BIN = g_strdup_printf("%s/.local/bin/", g_get_home_dir());
    gtk_init(&argc, &argv);
    nova_init(CSS); nova_load_blur();
    int start = (argc > 2 && !strcmp(argv[1], "--pagina")) ? CLAMP(atoi(argv[2]), 0, NPAGES - 1) : 0;
    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Ajustes"); gtk_window_set_default_size(GTK_WINDOW(win), 900, 620);
    gtk_window_set_wmclass(GTK_WINDOW(win), "nova-ajustes", "Nova-ajustes");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_container_add(GTK_CONTAINER(win), root);
    side = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "side"); gtk_widget_set_size_request(side, 216, -1);
    g_signal_connect(side, "draw", G_CALLBACK(draw_side), NULL); g_signal_connect(win, "configure-event", G_CALLBACK(nova_redraw_on_move), side);
    gtk_box_pack_start(GTK_BOX(side), lbl("Ajustes", "brand", 0), FALSE, FALSE, 0);
    list = gtk_list_box_new();
    for (int i = 0; i < NPAGES; i++) {
        GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(r), gtk_image_new_from_icon_name(PICON[i], GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(r), lbl(PAGES[i], NULL, 0), TRUE, TRUE, 0); gtk_list_box_insert(GTK_LIST_BOX(list), r, -1);
    }
    gtk_box_pack_start(GTK_BOX(side), list, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(root), side, FALSE, FALSE, 0);
    stack = gtk_stack_new(); gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN); gtk_stack_set_transition_duration(GTK_STACK(stack), 340);
    GtkWidget *(*mk[NPAGES])(void) = { page_aparencia, page_papel, page_dock, page_tela, page_rede, page_seguranca, page_apps, page_atalhos, page_atualiza, page_sobre };
    for (int i = 0; i < NPAGES; i++) {
        GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(sw), mk[i]()); gtk_stack_add_named(GTK_STACK(stack), sw, PAGES[i]);
    }
    gtk_box_pack_start(GTK_BOX(root), stack, TRUE, TRUE, 0);
    g_signal_connect(stack, "notify::visible-child-name", G_CALLBACK(net_page_shown), NULL); g_timeout_add_seconds(15, net_tick, NULL);
    g_signal_connect(list, "row-selected", G_CALLBACK(on_page), NULL);
    gtk_widget_show_all(win);
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_list_box_select_row(GTK_LIST_BOX(list), gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), start));
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
    loading = FALSE;
    gtk_main();
    return 0;
}
