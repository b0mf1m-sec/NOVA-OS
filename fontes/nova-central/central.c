// Central de controle do NOVA OS: Wi-Fi, tema, brilho, som, bateria, apps em segundo plano e energia.
// Abre como um painel de vidro sob a barra; clicar fora (ou Esc) fecha. Abrir de novo com ela aberta também fecha.
#include "../nova-comum/nova.h"
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#define NBG 5
typedef struct { GtkWidget *box, *name, *mem; char *exe; } BgRow;
static GtkWidget *win, *wifi_btn, *wifi_sub, *theme_btn, *theme_sub, *bt_btn, *bt_sub, *dnd_btn, *dnd_sub, *bri, *vol, *bat_img, *bat_txt, *bat_sub, *bg_empty;
static BgRow bg[NBG];
static GtkWidget *tiles[8]; static int ntiles; static gint64 t0;
static gboolean wifi_on, bt_on, dnd_on, loading = TRUE;
static guint bri_src, vol_src, armed_src; static GtkWidget *armed;

static void setc(GtkWidget *w, const char *c, gboolean on) { GtkStyleContext *s = gtk_widget_get_style_context(w); if (on) gtk_style_context_add_class(s, c); else gtk_style_context_remove_class(s, c); }

/* ---------- estado ---------- */
static void read_state(void) {
    char *o = nova_sh("LC_ALL=C nmcli radio wifi 2>/dev/null"); wifi_on = g_str_has_prefix(o, "enabled"); g_free(o);
    o = wifi_on ? nova_sh("LC_ALL=C nmcli -t -f ACTIVE,SSID dev wifi 2>/dev/null | awk -F: '$1==\"yes\"{print $2; exit}'") : g_strdup("");
    gtk_label_set_text(GTK_LABEL(wifi_sub), !wifi_on ? "Desligado" : *o ? o : "Sem conexão"); g_free(o); setc(wifi_btn, "on", wifi_on);
    gtk_label_set_text(GTK_LABEL(theme_sub), nova_dark ? "Escuro" : "Claro"); setc(theme_btn, "on", nova_dark);
    o = nova_sh("systemctl is-active bluetooth 2>/dev/null"); bt_on = !strcmp(o, "active"); g_free(o);
    gtk_label_set_text(GTK_LABEL(bt_sub), bt_on ? "Ligado" : "Desligado"); setc(bt_btn, "on", bt_on);
    o = nova_sh("dunstctl is-paused 2>/dev/null"); dnd_on = !strcmp(o, "true"); g_free(o);
    gtk_label_set_text(GTK_LABEL(dnd_sub), dnd_on ? "Ligado" : "Desligado"); setc(dnd_btn, "on", dnd_on);
    o = nova_sh("LC_ALL=C wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | awk '{printf \"%d\", $2*100+0.5}'");
    gtk_range_set_value(GTK_RANGE(vol), *o ? atof(o) : 0); g_free(o);
    o = nova_sh("b=$(ls -d /sys/class/backlight/* 2>/dev/null | head -1); [ -n \"$b\" ] && echo $(( $(cat $b/brightness) * 100 / $(cat $b/max_brightness) ))");
    gtk_range_set_value(GTK_RANGE(bri), *o ? atof(o) : 100); gtk_widget_set_sensitive(bri, *o != 0); g_free(o);
}

static gboolean tick_battery(gpointer d) {
    char *cap = nova_sh("cat /sys/class/power_supply/BAT*/capacity 2>/dev/null | head -1"), *st = nova_sh("cat /sys/class/power_supply/BAT*/status 2>/dev/null | head -1");
    if (!*cap) { gtk_label_set_text(GTK_LABEL(bat_txt), "Na tomada"); gtk_label_set_text(GTK_LABEL(bat_sub), "Este computador não tem bateria"); g_free(cap); g_free(st); return G_SOURCE_CONTINUE; }
    int p = atoi(cap); gboolean chg = !strcmp(st, "Charging") || !strcmp(st, "Full") || !strcmp(st, "Not charging");
    char b[96]; g_snprintf(b, sizeof b, "%d%%", p); gtk_label_set_text(GTK_LABEL(bat_txt), b);
    char *rem = nova_sh("d=$(ls -d /sys/class/power_supply/BAT* | head -1); e=$(cat $d/energy_now 2>/dev/null || cat $d/charge_now 2>/dev/null); "
                          "w=$(cat $d/power_now 2>/dev/null || cat $d/current_now 2>/dev/null); [ \"${w:-0}\" -gt 0 ] && echo $(( e * 60 / w ))");
    int min = *rem ? atoi(rem) : 0;
    if (!strcmp(st, "Full") || (chg && p >= 99)) g_snprintf(b, sizeof b, "Carregada, ligada na tomada");
    else if (chg) g_snprintf(b, sizeof b, "Carregando");
    else if (min > 0 && min < 1200) g_snprintf(b, sizeof b, "Na bateria · resta cerca de %dh%02d", min / 60, min % 60);
    else g_snprintf(b, sizeof b, "Na bateria");
    gtk_label_set_text(GTK_LABEL(bat_sub), b);
    int l = (p + 5) / 10 * 10; char *f = g_strdup_printf("%s/.local/share/nova/barra/bat-%d%s.png", g_get_home_dir(), l > 100 ? 100 : l, chg ? "-c" : "");
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size(f, 34, 34, NULL); if (pb) { gtk_image_set_from_pixbuf(GTK_IMAGE(bat_img), pb); g_object_unref(pb); }
    g_free(f); g_free(rem); g_free(cap); g_free(st);
    return G_SOURCE_CONTINUE;
}

/* ---------- segundo plano ---------- */
static const char *SKIP[] = { "openbox", "tint2", "plank", "picom", "Xorg", "systemd", "dbus-daemon", "pipewire", "pipewire-pulse", "wireplumber", "Xvfb",
    "bamfdaemon", "nm-applet", "bash", "sh", "dash", "sshd", "sshd-session", "lightdm", "dbus-launch", "dunst", "nova-papel", "udiskie", "gvfsd", "sleep",
    "nova-central", "xdg-desktop-portal", "xdg-desktop-portal-gtk", "xdg-document-portal", "xdg-permission-store", "at-spi-bus-launcher",
    "at-spi2-registryd", "dconf-service", "gvfsd-fuse", "xsettingsd", "inotifywait", "nova-apps", "gvfs-udisks2-volume-monitor", "gvfsd-trash", NULL };
static const char *NAMES[][2] = { { "firefox-esr", "Firefox" }, { "chromium", "Chromium" }, { "soffice.bin", "LibreOffice" }, { "vlc", "VLC" },
    { "mpv", "Player de vídeo" }, { "pcmanfm", "Arquivos" }, { "zathura", "Leitor de PDF" }, { "nova-loja", "Loja" }, { "gimp", "GIMP" },
    { "thunderbird", "Thunderbird" }, { "nova-gravador", "Gravador" }, { "nova-whatsapp", "WhatsApp" }, { "nova-ajustes", "Ajustes" },
    { "nova-otimizador", "Otimizador" }, { "xterm", "Terminal" }, { "WebKitWebProcess", "WhatsApp (página)" }, { "WebKitNetworkProcess", "WhatsApp (rede)" },
    { "blueman", "Bluetooth" }, { "flameshot", "Captura de tela" }, { NULL, NULL } };
typedef struct { char *exe; long kb; } G;
static int cmp(gconstpointer a, gconstpointer b) { long d = ((G *)b)->kb - ((G *)a)->kb; return d > 0 ? 1 : d < 0 ? -1 : 0; }
static GList *scan(const char *only_exe) {
    GHashTable *h = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GDir *d = g_dir_open("/proc", 0, NULL); const char *n; uid_t me = getuid();
    while (d && (n = g_dir_read_name(d))) {
        if (!g_ascii_isdigit(n[0])) continue;
        char p[64], exe[512]; struct stat st; g_snprintf(p, sizeof p, "/proc/%s", n);
        if (stat(p, &st) || st.st_uid != me) continue;
        g_snprintf(p, sizeof p, "/proc/%s/exe", n); ssize_t l = readlink(p, exe, sizeof exe - 1); if (l <= 0) continue; exe[l] = 0;
        if (strstr(exe, "/claude")) continue;
        if (only_exe) { if (!strcmp(exe, only_exe)) kill(atoi(n), SIGTERM); continue; }
        char *base = strrchr(exe, '/'); base = base ? base + 1 : exe; gboolean skip = FALSE;
        for (int i = 0; SKIP[i]; i++) if (!strcmp(base, SKIP[i])) skip = TRUE;
        if (skip) continue;
        char *s = NULL; long kb = 0; g_snprintf(p, sizeof p, "/proc/%s/smaps_rollup", n);
        if (g_file_get_contents(p, &s, NULL, NULL)) { char *q = strstr(s, "Pss:"); if (q) kb = atol(q + 4); g_free(s); }
        g_hash_table_replace(h, g_strdup(exe), GSIZE_TO_POINTER(GPOINTER_TO_SIZE(g_hash_table_lookup(h, exe)) + kb));
    }
    if (d) g_dir_close(d);
    GList *out = NULL; GHashTableIter it; gpointer k, v; g_hash_table_iter_init(&it, h);
    while (g_hash_table_iter_next(&it, &k, &v)) if (GPOINTER_TO_SIZE(v) >= 8 * 1024) { G *x = g_new(G, 1); x->exe = g_strdup(k); x->kb = GPOINTER_TO_SIZE(v); out = g_list_insert_sorted(out, x, cmp); }
    g_hash_table_destroy(h); return out;
}
static gboolean tick_bg(gpointer d) {
    GList *l = scan(NULL), *p = l; int i = 0;
    for (; i < NBG; i++) {
        if (p) { G *x = p->data; const char *b = strrchr(x->exe, '/'); b = b ? b + 1 : x->exe; char *nm = NULL, mb[32];
            for (int k = 0; NAMES[k][0]; k++) if (g_str_has_prefix(b, NAMES[k][0])) nm = g_strdup(NAMES[k][1]);
            if (!nm) { nm = g_strdup(b); nm[0] = g_ascii_toupper(nm[0]); }
            gtk_label_set_text(GTK_LABEL(bg[i].name), nm); g_free(nm);
            g_snprintf(mb, sizeof mb, "%ld MB", x->kb / 1024); gtk_label_set_text(GTK_LABEL(bg[i].mem), mb);
            g_free(bg[i].exe); bg[i].exe = g_strdup(x->exe); gtk_widget_show(bg[i].box); p = p->next;
        } else gtk_widget_hide(bg[i].box);
    }
    gtk_widget_set_visible(bg_empty, l == NULL);
    for (p = l; p; p = p->next) { g_free(((G *)p->data)->exe); g_free(p->data); } g_list_free(l);
    return G_SOURCE_CONTINUE;
}
static gboolean bg_soon(gpointer d) { tick_bg(NULL); return G_SOURCE_REMOVE; }
static void on_bg_close(GtkWidget *w, gpointer i) { BgRow *r = &bg[GPOINTER_TO_INT(i)]; if (r->exe) g_list_free(scan(r->exe)); g_timeout_add(800, bg_soon, NULL); }

/* ---------- ações ---------- */
static gboolean state_soon(gpointer d) { loading = TRUE; read_state(); loading = FALSE; return G_SOURCE_REMOVE; }
static void on_wifi(GtkWidget *w, gpointer d) { nova_sh(wifi_on ? "nmcli radio wifi off" : "nmcli radio wifi on"); g_timeout_add(1500, state_soon, NULL); read_state(); }
static void on_theme(GtkWidget *w, gpointer d) {
    char *c = g_strdup_printf("%s/.local/bin/nova-tema tema %s", g_get_home_dir(), nova_dark ? "claro" : "escuro"); nova_spawn(c); g_free(c); gtk_main_quit();
}
static void on_bt(GtkWidget *w, gpointer d) {
    nova_sh(bt_on ? "sudo -n /usr/local/sbin/nova-admin bt-stop; rfkill block bluetooth 2>/dev/null; pkill -x blueman-applet"
                    : "rfkill unblock bluetooth 2>/dev/null; sudo -n /usr/local/sbin/nova-admin bt-start; (blueman-applet >/dev/null 2>&1 &)");
    read_state();
}
static void on_dnd(GtkWidget *w, gpointer d) { nova_sh("dunstctl set-paused toggle"); read_state(); }
static gboolean apply_bri(gpointer d) { char *c = g_strdup_printf("brightnessctl -q set %d%%", MAX(2, (int)gtk_range_get_value(GTK_RANGE(bri)))); nova_spawn(c); g_free(c); bri_src = 0; return G_SOURCE_REMOVE; }
static gboolean apply_vol(gpointer d) { char *c = g_strdup_printf("wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", (int)gtk_range_get_value(GTK_RANGE(vol))); nova_spawn(c); g_free(c); vol_src = 0; return G_SOURCE_REMOVE; }
static void on_bri(GtkRange *r, gpointer d) { if (!loading && !bri_src) bri_src = g_timeout_add(50, apply_bri, NULL); }
static void on_vol(GtkRange *r, gpointer d) { if (!loading && !vol_src) vol_src = g_timeout_add(50, apply_vol, NULL); }
static void on_launch(GtkWidget *w, gpointer cmd) { char *c = g_strdup_printf("sh -c \"%s\"", (const char *)cmd); nova_spawn(c); g_free(c); gtk_main_quit(); }
static gboolean disarm(gpointer d) { if (armed) { setc(armed, "armed", FALSE); gtk_widget_set_tooltip_text(armed, g_object_get_data(G_OBJECT(armed), "tip")); armed = NULL; } armed_src = 0; return G_SOURCE_REMOVE; }
static void on_power(GtkWidget *w, gpointer cmd) {            // ações de energia pedem um segundo clique para confirmar
    if (armed == w) { on_launch(w, cmd); return; }
    if (armed_src) { g_source_remove(armed_src); disarm(NULL); }
    armed = w; setc(w, "armed", TRUE); gtk_widget_set_tooltip_text(w, "Clique de novo para confirmar");
    gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(win), "hint")), "Clique de novo para confirmar");
    armed_src = g_timeout_add(3500, disarm, NULL);
}

static gboolean first_state(gpointer d) { read_state(); loading = FALSE; tick_battery(NULL); tick_bg(NULL); return G_SOURCE_REMOVE; }

/* ---------- janela ---------- */
static gboolean tick_in(GtkWidget *w, GdkFrameClock *c, gpointer d) {
    double t = (g_get_monotonic_time() - t0) / 1e6; gboolean more = FALSE;
    for (int i = 0; i < ntiles; i++) { double e = ease_out((t - i * .035) / .30); gtk_widget_set_opacity(tiles[i], e); if (e < 1) more = TRUE; }
    return more;
}
static gboolean on_focus_out(GtkWidget *w, GdkEvent *e, gpointer d) { if ((g_get_monotonic_time() - t0) > 400000) gtk_main_quit(); return FALSE; }
static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) { if (e->keyval == GDK_KEY_Escape) gtk_main_quit(); return FALSE; }

static GtkWidget *tile(GtkWidget *child) { GtkWidget *t = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "tile"); gtk_container_add(GTK_CONTAINER(t), child); tiles[ntiles++] = t; return t; }
static GtkWidget *toggle_tile(const char *icon, const char *name, GtkWidget **btn, GtkWidget **sub, GCallback cb) {
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    *btn = cls(gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_MENU), "tg"); gtk_widget_set_valign(*btn, GTK_ALIGN_CENTER); gtk_widget_set_can_focus(*btn, FALSE);
    g_signal_connect(*btn, "clicked", cb, NULL);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    *sub = lbl("", "tsub", 0); gtk_label_set_ellipsize(GTK_LABEL(*sub), PANGO_ELLIPSIZE_END); gtk_label_set_max_width_chars(GTK_LABEL(*sub), 11);
    gtk_box_pack_start(GTK_BOX(v), lbl(name, "tname", 0), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(v), *sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(h), *btn, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0);
    return tile(h);
}
static GtkWidget *slider_tile(const char *icon, const char *name, GtkWidget **scale, GCallback cb) {
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(v), lbl(name, "tname", 0), FALSE, FALSE, 0);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(h), cls(gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU), "sicon"), FALSE, FALSE, 0);
    *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1); gtk_scale_set_draw_value(GTK_SCALE(*scale), FALSE);
    g_signal_connect(*scale, "value-changed", cb, NULL);
    gtk_box_pack_start(GTK_BOX(h), *scale, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(v), h, FALSE, FALSE, 0);
    return tile(v);
}
static GtkWidget *icon_btn(const char *icon, const char *tip, GCallback cb, const char *cmd, gboolean danger) {
    GtkWidget *b = cls(gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_MENU), danger ? "ib-d" : "ib"); cls(b, "ibb");
    gtk_widget_set_tooltip_text(b, tip); g_object_set_data(G_OBJECT(b), "tip", (gpointer)tip); gtk_widget_set_can_focus(b, FALSE);
    g_signal_connect(b, "clicked", cb, (gpointer)cmd); return b;
}

static const char *CSS =
".root { padding: 12px; }"
".tile { border-radius: 14px; background-color: alpha(@card,.66); border: 1px solid alpha(@fg,.07); box-shadow: 0 1px 3px alpha(#000,.07); padding: 10px 12px; }"
".tname { font-size: 13px; font-weight: 600; color: @fg; } .tsub { font-size: 11.5px; color: @fg2; }"
"button.tg { border-radius: 999px; min-width: 34px; min-height: 34px; padding: 0; border: none; box-shadow: none; background-image: none;"
"  background-color: alpha(@fg,.10); color: @fg; transition: all 260ms cubic-bezier(.3,1.7,.5,1); }"
"button.tg:hover { -gtk-icon-transform: scale(1.2); }"
"button.tg:active { -gtk-icon-transform: scale(.8); }"
"button.tg.on { background-color: @accent; color: #fff; box-shadow: 0 4px 12px alpha(@accent,.45); }"
".sicon { color: @fg2; }"
".batv { font-size: 22px; font-weight: 700; color: @fg; }"
".bgname { font-size: 12.5px; font-weight: 600; color: @fg; } .bgmem { font-size: 11.5px; color: @fg2; }"
"button.x { border-radius: 999px; min-width: 22px; min-height: 22px; padding: 0; border: none; box-shadow: none; background-image: none;"
"  background-color: alpha(@fg,.08); color: @fg2; transition: all 200ms ease; }"
"button.x:hover { background-color: @danger; color: #fff; -gtk-icon-transform: scale(1.1); }"
"button.ibb { border-radius: 12px; min-width: 40px; min-height: 36px; padding: 0; border: none; box-shadow: none; background-image: none;"
"  background-color: alpha(@card,.66); color: @fg; transition: all 240ms cubic-bezier(.3,1.7,.5,1); }"
"button.ib:hover { background-color: @accent; color: #fff; -gtk-icon-transform: scale(1.18); }"
"button.ib-d:hover, button.ib-d.armed { background-color: @danger; color: #fff; -gtk-icon-transform: scale(1.18); }"
".hint { font-size: 11px; color: @fg2; }";

static void activate(GtkApplication *app, gpointer d) {
    if (win) { gtk_main_quit(); return; }
    nova_init(CSS); nova_load_blur();
    GdkRectangle g; gdk_monitor_get_geometry(gdk_display_get_monitor(gdk_display_get_default(), 0), &g);
    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Central de controle"); gtk_window_set_wmclass(GTK_WINDOW(win), "nova-central", "Nova-central");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE); gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE); gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE); gtk_window_set_keep_above(GTK_WINDOW(win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_widget_set_app_paintable(win, TRUE);
    g_signal_connect(win, "draw", G_CALLBACK(nova_glass), NULL);
    g_signal_connect(win, "focus-out-event", G_CALLBACK(on_focus_out), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *v = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10), "root"); gtk_widget_set_size_request(v, 336, -1);
    gtk_container_add(GTK_CONTAINER(win), v);

    GtkWidget *r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_box_set_homogeneous(GTK_BOX(r1), TRUE);
    gtk_box_pack_start(GTK_BOX(r1), toggle_tile("network-wireless-symbolic", "Wi-Fi", &wifi_btn, &wifi_sub, G_CALLBACK(on_wifi)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(r1), toggle_tile("weather-clear-night-symbolic", "Modo escuro", &theme_btn, &theme_sub, G_CALLBACK(on_theme)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(v), r1, FALSE, FALSE, 0);
    GtkWidget *r2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_box_set_homogeneous(GTK_BOX(r2), TRUE);
    gtk_box_pack_start(GTK_BOX(r2), toggle_tile("bluetooth-active-symbolic", "Bluetooth", &bt_btn, &bt_sub, G_CALLBACK(on_bt)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(r2), toggle_tile("notifications-disabled-symbolic", "Não perturbe", &dnd_btn, &dnd_sub, G_CALLBACK(on_dnd)), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(v), r2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), slider_tile("display-brightness-symbolic", "Brilho da tela", &bri, G_CALLBACK(on_bri)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), slider_tile("audio-volume-high-symbolic", "Som", &vol, G_CALLBACK(on_vol)), FALSE, FALSE, 0);

    GtkWidget *bh = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    bat_img = gtk_image_new(); bat_txt = lbl("", "batv", 0); bat_sub = lbl("", "tsub", 0);
    GtkWidget *bv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_valign(bv, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(bv), lbl("Bateria", "tname", 0), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(bv), bat_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bh), bat_img, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(bh), bv, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(bh), bat_txt, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), tile(bh), FALSE, FALSE, 0);

    GtkWidget *bgv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_box_pack_start(GTK_BOX(bgv), lbl("Em segundo plano", "tname", 0), FALSE, FALSE, 2);
    for (int i = 0; i < NBG; i++) {
        bg[i].box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        bg[i].name = lbl("", "bgname", 0); gtk_label_set_ellipsize(GTK_LABEL(bg[i].name), PANGO_ELLIPSIZE_END); bg[i].mem = lbl("", "bgmem", 1);
        GtkWidget *x = cls(gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU), "x"); gtk_widget_set_tooltip_text(x, "Fechar"); gtk_widget_set_can_focus(x, FALSE);
        g_signal_connect(x, "clicked", G_CALLBACK(on_bg_close), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(bg[i].box), bg[i].name, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(bg[i].box), bg[i].mem, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(bg[i].box), x, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bgv), bg[i].box, FALSE, FALSE, 1);
    }
    bg_empty = lbl("Nenhum programa aberto.", "tsub", 0); gtk_box_pack_start(GTK_BOX(bgv), bg_empty, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(v), tile(bgv), FALSE, FALSE, 0);

    GtkWidget *fr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_box_set_homogeneous(GTK_BOX(fr), TRUE);
    gtk_box_pack_start(GTK_BOX(fr), icon_btn("emblem-system-symbolic", "Ajustes", G_CALLBACK(on_launch), "$HOME/.local/bin/nova-ajustes", FALSE), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fr), icon_btn("utilities-system-monitor-symbolic", "Monitor do sistema", G_CALLBACK(on_launch), "$HOME/.local/bin/nova-otimizador --monitor", FALSE), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fr), icon_btn("system-lock-screen-symbolic", "Bloquear a tela", G_CALLBACK(on_launch), "dm-tool lock", FALSE), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fr), icon_btn("weather-clear-night-symbolic", "Suspender", G_CALLBACK(on_launch), "systemctl suspend || sudo -n /usr/local/sbin/nova-admin suspend", FALSE), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fr), icon_btn("system-reboot-symbolic", "Reiniciar", G_CALLBACK(on_power), "systemctl reboot || sudo -n /usr/local/sbin/nova-admin reboot", TRUE), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fr), icon_btn("system-shutdown-symbolic", "Desligar", G_CALLBACK(on_power), "systemctl poweroff || sudo -n /usr/local/sbin/nova-admin poweroff", TRUE), TRUE, TRUE, 0);
    tiles[ntiles++] = fr; gtk_box_pack_start(GTK_BOX(v), fr, FALSE, FALSE, 0);
    GtkWidget *hint = lbl("", "hint", .5); g_object_set_data(G_OBJECT(win), "hint", hint); gtk_box_pack_start(GTK_BOX(v), hint, FALSE, FALSE, 0);

    for (int i = 0; i < ntiles; i++) gtk_widget_set_opacity(tiles[i], 0);
    gtk_widget_show_all(win);
    for (int i = 0; i < NBG; i++) { gtk_widget_set_no_show_all(bg[i].box, TRUE); gtk_widget_hide(bg[i].box); }
    gtk_widget_hide(bg_empty);
    int ww, hh; gtk_window_get_size(GTK_WINDOW(win), &ww, &hh);
    gtk_window_move(GTK_WINDOW(win), g.x + g.width - ww - 8, g.y + 34);
    g_timeout_add(30, first_state, NULL);
    t0 = g_get_monotonic_time(); gtk_widget_add_tick_callback(win, tick_in, NULL, NULL);
    g_timeout_add_seconds(3, tick_bg, NULL); g_timeout_add_seconds(15, tick_battery, NULL);
    gtk_window_present(GTK_WINDOW(win));
    gtk_main();
    g_application_quit(G_APPLICATION(app));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("br.nova.central", 0);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
