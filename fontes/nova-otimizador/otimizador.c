// Otimizador NOVA OS: aba "Otimizar" (limpeza com um clique) e aba "Monitor" (gerenciador de tarefas).
#include "../nova-comum/nova.h"
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#define NSTEP 4
#define NHEAVY 5
#define NPROC 9
#define HIST 60
static const char *STEP_NAME[NSTEP] = { "Limpando arquivos temporários", "Removendo sobras de programas",
                                        "Trazendo memória de volta do disco", "Liberando memória em cache" };
static const char *STEP_CMD[NSTEP] = {
    "rm -rf \"$HOME/.cache/thumbnails\"/* 2>/dev/null; sudo -n /usr/local/sbin/nova-admin apt-clean; sudo -n /usr/local/sbin/nova-admin journal-vacuum >/dev/null 2>&1; true",
    "command -v flatpak >/dev/null && flatpak uninstall --user --unused -y --noninteractive >/dev/null 2>&1; true",
    "u=$(awk '/SwapTotal/{t=$2}/SwapFree/{f=$2}END{print t-f}' /proc/meminfo); a=$(awk '/MemAvailable/{print $2}' /proc/meminfo);"
    " [ \"$u\" -gt 0 ] && [ \"$a\" -gt $((u*3/2)) ] && sudo -n /usr/local/sbin/nova-admin swap-reset; true",
    "sync; sudo -n /usr/local/sbin/nova-admin drop-caches 2>/dev/null; true",
};

typedef struct { char *exe; long kb; double cpu; int n; } Group;
typedef struct { GtkWidget *box, *name, *state, *cpu, *mem; char *exe; } Row;

static GtkWidget *win, *gauge, *btn, *action, *result, *step_stack[NSTEP], *step_lbl[NSTEP], *pages, *seg[2];
static Row heavy[NHEAVY], procs[NPROC];
static GtkWidget *heavy_empty, *tile_val[4], *tile_sub[4], *tile_graph[4], *sort_btn[2];
static double hist[4][HIST]; static int hist_n;
static double used_frac, shown_frac, total_gb, used_gb, cpu_frac;
static long mem_before, cache_before; static double disk_before; static int step = -1; static gint64 step_t0;
static GHashTable *prev_ticks; static gint64 prev_scan_t; static long prev_cpu_total, prev_cpu_idle;
static int sort_by_cpu = 1;

/* ---------- medidas ---------- */
static long meminfo(const char *key) {
    char *s = NULL; long v = 0;
    if (g_file_get_contents("/proc/meminfo", &s, NULL, NULL)) { char *p = strstr(s, key); if (p) v = atol(p + strlen(key) + 1); g_free(s); }
    return v;
}
static double disk_free_mb(void) { struct statvfs s; return statvfs("/", &s) ? 0 : (double)s.f_bavail * s.f_frsize / 1048576.0; }
static long read_long(const char *path) { char *s = NULL; long v = -1; if (g_file_get_contents(path, &s, NULL, NULL)) { v = atol(s); g_free(s); } return v; }

static void sample_mem(void) {
    long t = meminfo("MemTotal"), a = meminfo("MemAvailable");
    if (t > 0) { used_frac = (double)(t - a) / t; total_gb = t / 1048576.0; used_gb = (t - a) / 1048576.0; }
}
static void sample_cpu(void) {
    char *s = NULL; if (!g_file_get_contents("/proc/stat", &s, NULL, NULL)) return;
    long v[8] = { 0 }; sscanf(s, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]); g_free(s);
    long tot = 0; for (int i = 0; i < 8; i++) tot += v[i];
    long idle = v[3] + v[4];
    if (prev_cpu_total && tot > prev_cpu_total) cpu_frac = 1.0 - (double)(idle - prev_cpu_idle) / (tot - prev_cpu_total);
    prev_cpu_total = tot; prev_cpu_idle = idle;
}
static double read_temp(void) {
    double best = -1;
    for (int i = 0; i < 8; i++) {
        char p[80], *ty = NULL; g_snprintf(p, sizeof p, "/sys/class/thermal/thermal_zone%d/type", i);
        if (!g_file_get_contents(p, &ty, NULL, NULL)) break;
        g_snprintf(p, sizeof p, "/sys/class/thermal/thermal_zone%d/temp", i); double t = read_long(p) / 1000.0;
        if (g_str_has_prefix(ty, "x86_pkg_temp")) { g_free(ty); return t; }
        if (t > best) best = t;
        g_free(ty);
    }
    return best;
}

/* ---------- medidor circular ---------- */
static void mix(double t, double *r, double *g, double *b) {
    const double c[3][3] = { { .19, .78, .35 }, { 1.0, .62, .04 }, { 1.0, .27, .23 } };
    t = CLAMP((t - .45) / .45, 0, 1) * 2; int i = t >= 1; double f = t - i; if (t >= 2) { i = 1; f = 1; }
    *r = c[i][0] + (c[i + 1][0] - c[i][0]) * f; *g = c[i][1] + (c[i + 1][1] - c[i][1]) * f; *b = c[i][2] + (c[i + 1][2] - c[i][2]) * f;
}
static gboolean draw_gauge(GtkWidget *w, cairo_t *cr, gpointer d) {
    double cx = gtk_widget_get_allocated_width(w) / 2.0, cy = gtk_widget_get_allocated_height(w) / 2.0, r = 84, R, G, B;
    GdkRGBA fg; nova_color(w, "fg", &fg); mix(shown_frac, &R, &G, &B);
    cairo_set_line_width(cr, 13); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, cx, cy, r, 0, 2 * G_PI); cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, .09); cairo_stroke(cr);
    if (step >= 0) {
        double p = .5 + .5 * sin(g_get_monotonic_time() / 1e6 * 5);
        cairo_arc(cr, cx, cy, r, 0, 2 * G_PI); cairo_set_source_rgba(cr, R, G, B, .10 + .16 * p); cairo_set_line_width(cr, 24); cairo_stroke(cr);
        cairo_set_line_width(cr, 13);
    }
    cairo_arc(cr, cx, cy, r, -G_PI_2, -G_PI_2 + 2 * G_PI * MAX(shown_frac, .01)); cairo_set_source_rgb(cr, R, G, B); cairo_stroke(cr);
    char b[64]; cairo_text_extents_t e;
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 46); g_snprintf(b, sizeof b, "%d%%", (int)round(shown_frac * 100));
    cairo_text_extents(cr, b, &e); cairo_move_to(cr, cx - e.width / 2 - e.x_bearing, cy + 10); gdk_cairo_set_source_rgba(cr, &fg); cairo_show_text(cr, b);
    cairo_select_font_face(cr, "Inter", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.5); g_snprintf(b, sizeof b, "%.1f de %.1f GB em uso", used_gb, total_gb);
    cairo_text_extents(cr, b, &e); cairo_move_to(cr, cx - e.width / 2 - e.x_bearing, cy + 33);
    cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, .58); cairo_show_text(cr, b);
    return TRUE;
}
static gboolean tick_gauge(GtkWidget *w, GdkFrameClock *c, gpointer d) {
    double dlt = used_frac - shown_frac;
    if (fabs(dlt) > .0005 || step >= 0) { shown_frac += dlt * .07; gtk_widget_queue_draw(w); }
    return G_SOURCE_CONTINUE;
}

/* ---------- gráfico de linha dos cartões ---------- */
static gboolean draw_spark(GtkWidget *w, cairo_t *cr, gpointer idx) {
    int k = GPOINTER_TO_INT(idx), W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    GdkRGBA a; nova_color(w, k == 1 ? "ok" : k == 3 ? "warn" : "accent", &a);
    if (k == 2) {                                            // disco: barra
        GdkRGBA fg; nova_color(w, "fg", &fg);
        nova_rrect(cr, 0, H - 9, W, 7, 4); cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, .10); cairo_fill(cr);
        nova_rrect(cr, 0, H - 9, MAX(8, W * hist[2][HIST - 1]), 7, 4); gdk_cairo_set_source_rgba(cr, &a); cairo_fill(cr);
        return TRUE;
    }
    int n = MIN(hist_n, HIST); if (n < 2) return TRUE;
    double dx = (double)W / (HIST - 1);
    cairo_move_to(cr, W - (n - 1) * dx, H);
    for (int i = 0; i < n; i++) { double v = CLAMP(hist[k][HIST - n + i], 0, 1); cairo_line_to(cr, W - (n - 1 - i) * dx, H - 2 - v * (H - 6)); }
    cairo_line_to(cr, W, H); cairo_close_path(cr);
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, 0, H);
    cairo_pattern_add_color_stop_rgba(g, 0, a.red, a.green, a.blue, .32); cairo_pattern_add_color_stop_rgba(g, 1, a.red, a.green, a.blue, .02);
    cairo_set_source(cr, g); cairo_fill(cr); cairo_pattern_destroy(g);
    for (int i = 0; i < n; i++) { double v = CLAMP(hist[k][HIST - n + i], 0, 1), x = W - (n - 1 - i) * dx, y = H - 2 - v * (H - 6);
        if (i) cairo_line_to(cr, x, y); else cairo_move_to(cr, x, y); }
    gdk_cairo_set_source_rgba(cr, &a); cairo_set_line_width(cr, 2); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); cairo_stroke(cr);
    return TRUE;
}

/* ---------- processos ---------- */
static const char *SKIP[] = { "openbox", "tint2", "plank", "picom", "Xorg", "systemd", "dbus-daemon", "pipewire", "pipewire-pulse", "wireplumber",
    "nova-otimizador", "Xvfb", "bamfdaemon", "nm-applet", "bash", "sh", "dash", "sshd", "sshd-session", "lightdm", "dbus-launch", "dunst",
    "nova-papel", "nova-bateria-vigia", "udiskie", "gvfsd", "sleep", "nova-central", "xdg-desktop-portal", "xdg-desktop-portal-gtk", "xdg-document-portal", "xdg-permission-store", "at-spi-bus-launcher", "at-spi2-registryd", "dconf-service", "gvfsd-fuse", "nova-apps", NULL };
static const char *NAMES[][2] = { { "firefox-esr", "Firefox" }, { "chromium", "Chromium" }, { "soffice.bin", "LibreOffice" }, { "vlc", "VLC" },
    { "mpv", "Player de vídeo" }, { "pcmanfm", "Arquivos" }, { "zathura", "Leitor de PDF" }, { "nova-loja", "Loja" }, { "gimp", "GIMP" },
    { "thunderbird", "Thunderbird" }, { "nova-gravador", "Gravador" }, { "nova-whatsapp", "WhatsApp" }, { "nova-ajustes", "Ajustes" },
    { "xterm", "Terminal" }, { "WebKitWebProcess", "WhatsApp (página)" }, { "WebKitNetworkProcess", "WhatsApp (rede)" }, { NULL, NULL } };
static char *nice_name(const char *exe) {
    const char *b = strrchr(exe, '/'); b = b ? b + 1 : exe;
    for (int i = 0; NAMES[i][0]; i++) if (g_str_has_prefix(b, NAMES[i][0])) return g_strdup(NAMES[i][1]);
    char *n = g_strdup(b); n[0] = g_ascii_toupper(n[0]); return n;
}
static int cmp_mem(gconstpointer a, gconstpointer b) { long d = ((Group *)b)->kb - ((Group *)a)->kb; return d > 0 ? 1 : d < 0 ? -1 : 0; }
static int cmp_cpu(gconstpointer a, gconstpointer b) { double d = ((Group *)b)->cpu - ((Group *)a)->cpu; return d > .05 ? 1 : d < -.05 ? -1 : cmp_mem(a, b); }
static void free_groups(GList *l) { for (GList *p = l; p; p = p->next) { g_free(((Group *)p->data)->exe); g_free(p->data); } g_list_free(l); }

// Varre /proc: agrupa por programa, soma memória (Pss) e uso de CPU. Com only_exe, envia o sinal a esse programa.
static GList *scan(const char *only_exe, int sig) {
    GHashTable *h = g_hash_table_new(g_str_hash, g_str_equal), *now = g_hash_table_new(g_direct_hash, g_direct_equal);
    GDir *d = g_dir_open("/proc", 0, NULL); const char *n; uid_t me = getuid();
    gint64 t = g_get_monotonic_time(); double el = prev_scan_t ? (t - prev_scan_t) / 1e6 : 0; int ncpu = MAX(1, (int)sysconf(_SC_NPROCESSORS_ONLN));
    while (d && (n = g_dir_read_name(d))) {
        if (!g_ascii_isdigit(n[0])) continue;
        char p[64], exe[512]; struct stat st; g_snprintf(p, sizeof p, "/proc/%s", n);
        if (stat(p, &st) || st.st_uid != me) continue;
        g_snprintf(p, sizeof p, "/proc/%s/exe", n); ssize_t l = readlink(p, exe, sizeof exe - 1); if (l <= 0) continue; exe[l] = 0;
        if (strstr(exe, "/claude")) continue;
        if (only_exe) { if (!strcmp(exe, only_exe)) kill(atoi(n), sig); continue; }
        char *base = strrchr(exe, '/'); base = base ? base + 1 : exe; gboolean skip = FALSE;
        for (int i = 0; SKIP[i]; i++) if (!strcmp(base, SKIP[i])) skip = TRUE;
        if (skip) continue;
        char *s = NULL; long kb = 0, ticks = 0; g_snprintf(p, sizeof p, "/proc/%s/smaps_rollup", n);
        if (g_file_get_contents(p, &s, NULL, NULL)) { char *q = strstr(s, "Pss:"); if (q) kb = atol(q + 4); g_free(s); }
        g_snprintf(p, sizeof p, "/proc/%s/stat", n); s = NULL;
        if (g_file_get_contents(p, &s, NULL, NULL)) { char *q = strrchr(s, ')'); long ut = 0, stt = 0;
            if (q && sscanf(q + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld", &ut, &stt) == 2) ticks = ut + stt;
            g_free(s); }
        int pid = atoi(n); g_hash_table_insert(now, GINT_TO_POINTER(pid), GSIZE_TO_POINTER(ticks + 1));
        gsize old = GPOINTER_TO_SIZE(g_hash_table_lookup(prev_ticks, GINT_TO_POINTER(pid)));
        double cpu = (old && el > .2) ? (double)(ticks + 1 - old) / sysconf(_SC_CLK_TCK) / el / ncpu * 100 : 0;
        Group *g = g_hash_table_lookup(h, exe);
        if (!g) { g = g_new0(Group, 1); g->exe = g_strdup(exe); g_hash_table_insert(h, g->exe, g); }
        g->kb += kb; g->cpu += MAX(cpu, 0); g->n++;
    }
    if (d) g_dir_close(d);
    GList *out = g_hash_table_get_values(h); g_hash_table_destroy(h);
    if (!only_exe) { g_hash_table_destroy(prev_ticks); prev_ticks = now; prev_scan_t = t; } else g_hash_table_destroy(now);
    return out;
}

static void fill_rows(Row *rows, int nrows, GList *l, long min_kb) {
    int i = 0;
    for (GList *p = l; p && i < nrows; p = p->next) {
        Group *x = p->data; if (x->kb < min_kb) continue;
        char *nm = nice_name(x->exe), b[48];
        gtk_label_set_text(GTK_LABEL(rows[i].name), nm); g_free(nm);
        g_snprintf(b, sizeof b, "%ld MB", x->kb / 1024); gtk_label_set_text(GTK_LABEL(rows[i].mem), b);
        if (rows[i].cpu) { g_snprintf(b, sizeof b, "%.0f%%", x->cpu); gtk_label_set_text(GTK_LABEL(rows[i].cpu), b); }
        if (rows[i].state) {
            gboolean act = x->cpu >= 1.5; gtk_label_set_text(GTK_LABEL(rows[i].state), act ? "Ativo" : "Em espera");
            GtkStyleContext *c = gtk_widget_get_style_context(rows[i].state);
            if (act) gtk_style_context_add_class(c, "ativo"); else gtk_style_context_remove_class(c, "ativo");
        }
        g_free(rows[i].exe); rows[i].exe = g_strdup(x->exe); gtk_widget_show(rows[i].box); i++;
    }
    for (; i < nrows; i++) gtk_widget_hide(rows[i].box);
}

/* ---------- velocidade da rede ---------- */
static GtkWidget *net_down, *net_up, *net_down_sub, *net_info;
static guint64 net_rx0, net_tx0; static gint64 net_t0; static int net_ticks;
static void read_net(guint64 *rx, guint64 *tx) {
    *rx = *tx = 0; char *s = NULL; if (!g_file_get_contents("/proc/net/dev", &s, NULL, NULL)) return;
    char **ln = g_strsplit(s, "\n", -1);
    for (char **p = ln + 2; *p && **p; p++) {
        char *c = strchr(*p, ':'); if (!c) continue; *c = 0; const char *nm = *p; while (*nm == ' ') nm++;
        if (!strcmp(nm, "lo") || g_str_has_prefix(nm, "veth") || g_str_has_prefix(nm, "docker") || g_str_has_prefix(nm, "br-")) continue;
        guint64 r = 0, t = 0; unsigned long long v[16] = { 0 };
        if (sscanf(c + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &v[0], &v[8]) == 2) { r = v[0]; t = v[8]; }
        *rx += r; *tx += t;
    }
    g_strfreev(ln); g_free(s);
}
static void fmt_rate(double bps, char *out, size_t n) {
    if (bps < 1024) g_snprintf(out, n, "%.0f B/s", bps);
    else if (bps < 1048576) g_snprintf(out, n, "%.0f KB/s", bps / 1024);
    else g_snprintf(out, n, "%.1f MB/s", bps / 1048576);
}
static void net_info_refresh(void) {
    char *o = nova_sh("d=$(ip route show default 2>/dev/null | awk '{for(i=1;i<NF;i++) if($i==\"dev\"){print $(i+1); exit}}'); [ -n \"$d\" ] || exit 0;"
        " if [ -d /sys/class/net/$d/wireless ]; then LC_ALL=C nmcli -t -f ACTIVE,SSID,RATE dev wifi 2>/dev/null | awk -F: '$1==\"yes\"{print \"wifi|\"$2\"|\"$3; exit}';"
        " else echo \"cabo||$(cat /sys/class/net/$d/speed 2>/dev/null) Mbit/s\"; fi");
    char **f = g_strsplit(o, "|", 3); char *t;
    if (!f[0] || !f[1] || !f[2]) t = g_strdup("Sem conexão com a internet");
    else if (!strcmp(f[0], "wifi")) t = g_strdup_printf("Wi-Fi · %s · conexão de até %s", f[1], f[2]);
    else t = g_strdup_printf("Cabo de rede · conexão de até %s", f[2]);
    gtk_label_set_text(GTK_LABEL(net_info), t); g_free(t); g_strfreev(f); g_free(o);
}
static void net_update(void) {
    guint64 rx, tx; read_net(&rx, &tx); gint64 now = g_get_monotonic_time();
    if (net_t0) {
        double dt = (now - net_t0) / 1e6; if (dt < .2) return;
        double d = rx >= net_rx0 ? (rx - net_rx0) / dt : 0, u = tx >= net_tx0 ? (tx - net_tx0) / dt : 0; char a[32], b[32], c[48];
        fmt_rate(d, a, sizeof a); fmt_rate(u, b, sizeof b);
        gtk_label_set_text(GTK_LABEL(net_down), a); gtk_label_set_text(GTK_LABEL(net_up), b);
        g_snprintf(c, sizeof c, "%.1f Mbps", d * 8 / 1e6); gtk_label_set_text(GTK_LABEL(net_down_sub), c);
    }
    net_rx0 = rx; net_tx0 = tx; net_t0 = now;
    if (net_ticks++ % 8 == 0) net_info_refresh();
}
/* ---------- teste de velocidade (servidor público de testes da Cloudflare) ---------- */
static GtkWidget *speed_btn, *speed_res;
static void speed_done(GObject *src, GAsyncResult *res, gpointer d) {
    char *so = NULL; g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &so, NULL, NULL);
    double lat = 0, dn = 0, up = 0; char t[160]; char **f = so ? g_strsplit(so, " ", 3) : NULL;
    if (f && f[0] && f[1] && f[2]) { lat = g_ascii_strtod(f[0], NULL); dn = g_ascii_strtod(f[1], NULL); up = g_ascii_strtod(f[2], NULL); }
    g_strfreev(f);
    if (dn > 1000) {
        double ms = lat * 1000; if (ms < 0) ms = 0;
        g_snprintf(t, sizeof t, "Resultado: ↓ %.1f Mbps  ·  ↑ %.1f Mbps  ·  resposta %.0f ms", dn * 8 / 1e6, up * 8 / 1e6, ms);
    } else g_snprintf(t, sizeof t, "Não foi possível medir. Confira se há internet e tente de novo.");
    gtk_label_set_text(GTK_LABEL(speed_res), t); gtk_button_set_label(GTK_BUTTON(speed_btn), "Testar de novo"); gtk_widget_set_sensitive(speed_btn, TRUE);
    g_free(so); g_object_unref(src);
}
static void on_speed(GtkWidget *w, gpointer d) {
    gtk_widget_set_sensitive(speed_btn, FALSE); gtk_button_set_label(GTK_BUTTON(speed_btn), "Testando…");
    gtk_label_set_text(GTK_LABEL(speed_res), "Medindo… leva uns 15 segundos. Evite baixar outras coisas agora.");
    const char *argv[] = { "sh", "-c",
        "U=https://speed.cloudflare.com; "
        "l=$(curl -s -o /dev/null -w '%{time_starttransfer} %{time_appconnect}' --max-time 5 \"$U/__down?bytes=0\" | LC_ALL=C awk '{printf \"%.3f\", $1-$2}'); "
        "d=$(curl -s -o /dev/null -w '%{speed_download}' --max-time 8 \"$U/__down?bytes=50000000\"); "
        "u=$(head -c 30000000 /dev/zero | curl -s -o /dev/null -w '%{speed_upload}' --max-time 6 -X POST --data-binary @- \"$U/__up\"); "
        "echo \"${l:-0} ${d:-0} ${u:-0}\"", NULL };
    GSubprocess *p = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    if (p) g_subprocess_communicate_utf8_async(p, NULL, NULL, speed_done, NULL); else gtk_widget_set_sensitive(speed_btn, TRUE);
}
static gboolean tick_data(gpointer d) {
    net_update();
    sample_mem(); sample_cpu();
    GList *l = scan(NULL, 0);
    l = g_list_sort(l, cmp_mem); fill_rows(heavy, NHEAVY, l, 30 * 1024);
    gtk_widget_set_visible(heavy_empty, !gtk_widget_get_visible(heavy[0].box));
    l = g_list_sort(l, sort_by_cpu ? cmp_cpu : cmp_mem); fill_rows(procs, NPROC, l, 4 * 1024);
    free_groups(l);
    struct statvfs s; double du = 0, dt = 0; if (!statvfs("/", &s)) { dt = (double)s.f_blocks * s.f_frsize / 1e9; du = dt - (double)s.f_bavail * s.f_frsize / 1e9; }
    double temp = read_temp(); long bat = read_long("/sys/class/power_supply/BAT1/capacity");
    for (int k = 0; k < 4; k++) memmove(hist[k], hist[k] + 1, sizeof(double) * (HIST - 1));
    hist[0][HIST - 1] = cpu_frac; hist[1][HIST - 1] = used_frac; hist[2][HIST - 1] = dt > 0 ? du / dt : 0; hist[3][HIST - 1] = CLAMP((temp - 30) / 60, 0, 1);
    hist_n++;
    char b[64];
    g_snprintf(b, sizeof b, "%.0f%%", cpu_frac * 100); gtk_label_set_text(GTK_LABEL(tile_val[0]), b);
    g_snprintf(b, sizeof b, "%d núcleos", (int)sysconf(_SC_NPROCESSORS_ONLN)); gtk_label_set_text(GTK_LABEL(tile_sub[0]), b);
    g_snprintf(b, sizeof b, "%.0f%%", used_frac * 100); gtk_label_set_text(GTK_LABEL(tile_val[1]), b);
    g_snprintf(b, sizeof b, "%.1f de %.1f GB", used_gb, total_gb); gtk_label_set_text(GTK_LABEL(tile_sub[1]), b);
    g_snprintf(b, sizeof b, "%.0f%%", dt > 0 ? du / dt * 100 : 0); gtk_label_set_text(GTK_LABEL(tile_val[2]), b);
    g_snprintf(b, sizeof b, "%.0f de %.0f GB", du, dt); gtk_label_set_text(GTK_LABEL(tile_sub[2]), b);
    if (temp > 0) g_snprintf(b, sizeof b, "%.0f °C", temp); else g_snprintf(b, sizeof b, "—");
    gtk_label_set_text(GTK_LABEL(tile_val[3]), b);
    if (bat >= 0) g_snprintf(b, sizeof b, "Bateria em %ld%%", bat); else g_snprintf(b, sizeof b, "Na tomada");
    gtk_label_set_text(GTK_LABEL(tile_sub[3]), b);
    for (int k = 0; k < 4; k++) gtk_widget_queue_draw(tile_graph[k]);
    return G_SOURCE_CONTINUE;
}
static gboolean refresh_soon(gpointer d) { tick_data(NULL); return G_SOURCE_REMOVE; }
static void on_end(GtkWidget *w, gpointer row) {
    Row *r = row; if (r->exe) g_list_free(scan(r->exe, SIGTERM));
    g_timeout_add(900, refresh_soon, NULL);
}
static void on_sort(GtkWidget *w, gpointer by_cpu) {
    sort_by_cpu = GPOINTER_TO_INT(by_cpu);
    for (int i = 0; i < 2; i++) { GtkStyleContext *c = gtk_widget_get_style_context(sort_btn[i]);
        if (i == (sort_by_cpu ? 0 : 1)) gtk_style_context_add_class(c, "on"); else gtk_style_context_remove_class(c, "on"); }
    tick_data(NULL);
}

/* ---------- otimização ---------- */
static void run_step(void);
static gboolean finish(gpointer d) {
    tick_data(NULL);
    double mem = (meminfo("MemAvailable") - mem_before) / 1024.0, disk = disk_free_mb() - disk_before;
    double cache = (cache_before - meminfo("\nCached") - meminfo("Buffers")) / 1024.0;
    char *part[3]; int n = 0;
    if (mem >= 8) part[n++] = g_strdup_printf("liberei %.0f MB de memória", mem);
    if (cache >= 20) part[n++] = g_strdup_printf("esvaziei %.0f MB de cache", cache);
    if (disk >= 1) part[n++] = g_strdup_printf("recuperei %.0f MB de disco", disk);
    GString *g = g_string_new("Pronto. ");
    for (int i = 0; i < n; i++) { g_string_append(g, part[i]); g_string_append(g, i == n - 1 ? "." : i == n - 2 ? " e " : ", "); g_free(part[i]); }
    if (!n) g_string_append(g, "O sistema já estava enxuto: não havia o que liberar.");
    else g->str[8] = g_ascii_toupper(g->str[8]);
    gtk_label_set_text(GTK_LABEL(result), g->str); g_string_free(g, TRUE);
    step = -1; gtk_button_set_label(GTK_BUTTON(btn), "Otimizar de novo");
    gtk_stack_set_visible_child_name(GTK_STACK(action), "idle");
    return G_SOURCE_REMOVE;
}
static gboolean step_done_cb(gpointer d) {
    gtk_stack_set_visible_child_name(GTK_STACK(step_stack[step]), "ok");
    gtk_style_context_remove_class(gtk_widget_get_style_context(step_lbl[step]), "ativo");
    gtk_style_context_add_class(gtk_widget_get_style_context(step_lbl[step]), "feito");
    if (step + 1 < NSTEP) { step++; run_step(); } else g_timeout_add(1300, finish, NULL);
    return G_SOURCE_REMOVE;
}
static void on_step_exit(GObject *src, GAsyncResult *res, gpointer d) {
    g_subprocess_wait_finish(G_SUBPROCESS(src), res, NULL); g_object_unref(src);
    double el = (g_get_monotonic_time() - step_t0) / 1000.0;
    g_timeout_add(el < 600 ? (guint)(600 - el) : 1, step_done_cb, NULL);
}
static void run_step(void) {
    gtk_stack_set_visible_child_name(GTK_STACK(step_stack[step]), "run");
    gtk_style_context_add_class(gtk_widget_get_style_context(step_lbl[step]), "ativo");
    step_t0 = g_get_monotonic_time();
    GSubprocess *p = g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, NULL, "sh", "-c", STEP_CMD[step], NULL);
    if (p) g_subprocess_wait_async(p, NULL, on_step_exit, NULL); else g_timeout_add(1, step_done_cb, NULL);
}
static void optimize(void) {
    if (step >= 0) return;
    for (int i = 0; i < NSTEP; i++) { gtk_stack_set_visible_child_name(GTK_STACK(step_stack[i]), "wait");
        gtk_style_context_remove_class(gtk_widget_get_style_context(step_lbl[i]), "feito"); }
    gtk_label_set_text(GTK_LABEL(result), "");
    mem_before = meminfo("MemAvailable"); cache_before = meminfo("\nCached") + meminfo("Buffers"); disk_before = disk_free_mb();
    gtk_stack_set_visible_child_name(GTK_STACK(action), "steps");
    step = 0; run_step();
}
static void on_btn(GtkWidget *w, gpointer d) { optimize(); }
static gboolean auto_start(gpointer d) { optimize(); return G_SOURCE_REMOVE; }

static gboolean on_leve(GtkSwitch *sw, gboolean on, gpointer d) {
    nova_spawn(on ? "nova-desempenho on" : "nova-desempenho off");
    return FALSE;
}

static void on_seg(GtkWidget *w, gpointer idx) {
    int i = GPOINTER_TO_INT(idx);
    gtk_stack_set_transition_type(GTK_STACK(pages), i ? GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT : GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_stack_set_visible_child_name(GTK_STACK(pages), i ? "monitor" : "otimizar");
    for (int k = 0; k < 2; k++) { GtkStyleContext *c = gtk_widget_get_style_context(seg[k]);
        if (k == i) gtk_style_context_add_class(c, "on"); else gtk_style_context_remove_class(c, "on"); }
}

static const char *CSS =
".segbar { border-radius: 10px; background-color: alpha(@fg,.08); padding: 3px; }"
".segbar button { border-radius: 8px; border: none; background: none; box-shadow: none; padding: 4px 26px; font-size: 13px; font-weight: 600; color: @fg2;"
"  transition: all 240ms cubic-bezier(.3,1.3,.5,1); }"
".segbar button.on { background-color: @card; color: @fg; box-shadow: 0 2px 6px alpha(#000,.16); }"
".result { font-size: 13.5px; color: @fg; }"
"button.go { border-radius: 999px; padding: 10px 34px; font-size: 14.5px; font-weight: 700; border: none; color: #fff; text-shadow: none;"
"  background-image: linear-gradient(180deg, @accent_hi, @accent); box-shadow: 0 8px 22px alpha(@accent,.40), inset 0 1px 0 alpha(#fff,.30);"
"  transition: all 240ms cubic-bezier(.3,1.5,.5,1); }"
"button.go:hover { box-shadow: 0 12px 30px alpha(@accent,.55), inset 0 1px 0 alpha(#fff,.35); padding: 10px 38px; }"
"button.go:active { background-image: linear-gradient(180deg, @accent, @accent_lo); padding: 10px 30px; box-shadow: 0 3px 10px alpha(@accent,.3); }"
".glasspanel { border-radius: 16px; background-color: alpha(@card,.72); border: 1px solid alpha(@fg,.08); box-shadow: 0 1px 4px alpha(#000,.06); padding: 6px 14px; }"
".step { font-size: 13px; color: alpha(@fg,.40); transition: color 250ms ease; }"
".step.ativo { color: @fg; font-weight: 600; } .step.feito { color: alpha(@fg,.75); }"
".dot { color: alpha(@fg,.22); } .okc { color: @ok; }"
".pname { font-size: 13.5px; font-weight: 600; color: @fg; } .pnum { font-size: 12.5px; color: @fg2; }"
".pstate { font-size: 11px; font-weight: 600; color: @fg2; border-radius: 999px; padding: 1px 8px; background-color: alpha(@fg,.07); }"
".pstate.ativo { color: @ok; background-color: alpha(@ok,.14); }"
".vazio { font-size: 12.5px; color: @fg2; }"
".prow { border-radius: 10px; padding: 5px 8px; transition: background-color 180ms ease; }"
".prow:hover { background-color: alpha(@fg,.05); }"
".prow button.pill { padding: 2px 13px; min-height: 18px; font-size: 12px; }"
".tile { border-radius: 16px; background-color: alpha(@card,.78); border: 1px solid alpha(@fg,.08); box-shadow: 0 1px 4px alpha(#000,.06); padding: 12px 14px; }"
".tval { font-size: 26px; font-weight: 700; color: @fg; } .tsub { font-size: 11.5px; color: @fg2; }"
".colh { font-size: 11px; font-weight: 700; letter-spacing: .8px; color: @fg2; }"
"button.sort { border: none; background: none; box-shadow: none; padding: 2px 6px; border-radius: 6px; font-size: 11px; font-weight: 700; letter-spacing: .8px; color: @fg2; }"
"button.sort.on { color: @accent; background-color: alpha(@accent,.12); }";

static Row make_row(gboolean full) {
    Row r = { 0 };
    r.box = cls(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10), "prow");
    r.name = lbl("", "pname", 0); gtk_label_set_ellipsize(GTK_LABEL(r.name), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(r.box), r.name, TRUE, TRUE, 0);
    if (full) {
        r.state = lbl("", "pstate", .5); gtk_widget_set_valign(r.state, GTK_ALIGN_CENTER);
        GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_widget_set_size_request(sb, 92, -1);
        gtk_box_pack_start(GTK_BOX(sb), r.state, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(r.box), sb, FALSE, FALSE, 0);
        r.cpu = lbl("", "pnum", 1); gtk_widget_set_size_request(r.cpu, 52, -1); gtk_box_pack_start(GTK_BOX(r.box), r.cpu, FALSE, FALSE, 0);
    }
    r.mem = lbl("", "pnum", 1); gtk_widget_set_size_request(r.mem, 70, -1); gtk_box_pack_start(GTK_BOX(r.box), r.mem, FALSE, FALSE, 0);
    return r;
}
static void add_end_button(Row *r, const char *text) {
    GtkWidget *b = cls(cls(gtk_button_new_with_label(text), "pill"), "soft"); gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
    g_signal_connect(b, "clicked", G_CALLBACK(on_end), r); gtk_box_pack_start(GTK_BOX(r->box), b, FALSE, FALSE, 0);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    nova_init(CSS); nova_load_blur();
    prev_ticks = g_hash_table_new(g_direct_hash, g_direct_equal);
    sample_mem(); sample_cpu();

    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Otimizador"); gtk_window_set_default_size(GTK_WINDOW(win), 800, 590);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE); gtk_window_set_wmclass(GTK_WINDOW(win), "nova-otimizador", "Nova-otimizador");
    gtk_widget_set_app_paintable(win, TRUE);
    g_signal_connect(win, "draw", G_CALLBACK(nova_glass), NULL);
    g_signal_connect(win, "configure-event", G_CALLBACK(nova_redraw_on_move), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(v, 24); gtk_widget_set_margin_end(v, 24); gtk_widget_set_margin_top(v, 14); gtk_widget_set_margin_bottom(v, 20);
    gtk_container_add(GTK_CONTAINER(win), v);

    GtkWidget *sbar = cls(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2), "segbar"); gtk_widget_set_halign(sbar, GTK_ALIGN_CENTER);
    const char *sn[2] = { "Otimizar", "Monitor" };
    for (int i = 0; i < 2; i++) { seg[i] = gtk_button_new_with_label(sn[i]); gtk_widget_set_can_focus(seg[i], FALSE);
        g_signal_connect(seg[i], "clicked", G_CALLBACK(on_seg), GINT_TO_POINTER(i)); gtk_box_pack_start(GTK_BOX(sbar), seg[i], FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(v), sbar, FALSE, FALSE, 0);
    pages = gtk_stack_new(); gtk_stack_set_transition_duration(GTK_STACK(pages), 380); gtk_widget_set_margin_top(pages, 14);
    gtk_box_pack_start(GTK_BOX(v), pages, TRUE, TRUE, 0);

    /* --- aba Otimizar --- */
    GtkWidget *po = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 26);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_size_request(left, 330, -1);
    gauge = gtk_drawing_area_new(); gtk_widget_set_size_request(gauge, 220, 216);
    g_signal_connect(gauge, "draw", G_CALLBACK(draw_gauge), NULL); gtk_widget_add_tick_callback(gauge, tick_gauge, NULL, NULL);
    gtk_box_pack_start(GTK_BOX(left), gauge, FALSE, FALSE, 6);
    action = gtk_stack_new(); gtk_stack_set_transition_type(GTK_STACK(action), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(action), 280); gtk_widget_set_size_request(action, -1, 170);
    GtkWidget *idle = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16); gtk_widget_set_valign(idle, GTK_ALIGN_CENTER);
    result = lbl("", "result", .5); gtk_label_set_line_wrap(GTK_LABEL(result), TRUE); gtk_label_set_justify(GTK_LABEL(result), GTK_JUSTIFY_CENTER);
    gtk_label_set_max_width_chars(GTK_LABEL(result), 36);
    btn = cls(gtk_button_new_with_label("Otimizar agora"), "go"); gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_btn), NULL);
    gtk_box_pack_start(GTK_BOX(idle), result, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(idle), btn, FALSE, FALSE, 0);
    GtkWidget *steps = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "glasspanel"); gtk_widget_set_valign(steps, GTK_ALIGN_CENTER);
    for (int i = 0; i < NSTEP; i++) {
        GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_widget_set_margin_top(r, 6); gtk_widget_set_margin_bottom(r, 6);
        step_stack[i] = gtk_stack_new(); gtk_stack_set_transition_type(GTK_STACK(step_stack[i]), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
        gtk_stack_set_transition_duration(GTK_STACK(step_stack[i]), 220);
        GtkWidget *sp = gtk_spinner_new(); gtk_spinner_start(GTK_SPINNER(sp));
        gtk_stack_add_named(GTK_STACK(step_stack[i]), cls(gtk_image_new_from_icon_name("media-record-symbolic", GTK_ICON_SIZE_MENU), "dot"), "wait");
        gtk_stack_add_named(GTK_STACK(step_stack[i]), sp, "run");
        gtk_stack_add_named(GTK_STACK(step_stack[i]), cls(gtk_image_new_from_icon_name("emblem-ok-symbolic", GTK_ICON_SIZE_MENU), "okc"), "ok");
        step_lbl[i] = lbl(STEP_NAME[i], "step", 0);
        gtk_box_pack_start(GTK_BOX(r), step_stack[i], FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(r), step_lbl[i], TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(steps), r, FALSE, FALSE, 0);
    }
    gtk_stack_add_named(GTK_STACK(action), idle, "idle"); gtk_stack_add_named(GTK_STACK(action), steps, "steps");
    gtk_box_pack_start(GTK_BOX(left), action, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(po), left, FALSE, FALSE, 0);
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *h = lbl("O QUE MAIS PESA AGORA", "cap", 0); gtk_widget_set_margin_top(h, 10); gtk_widget_set_margin_bottom(h, 8);
    gtk_box_pack_start(GTK_BOX(right), h, FALSE, FALSE, 0);
    GtkWidget *hp = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "glasspanel");
    for (int i = 0; i < NHEAVY; i++) { heavy[i] = make_row(FALSE); add_end_button(&heavy[i], "Fechar"); gtk_box_pack_start(GTK_BOX(hp), heavy[i].box, FALSE, FALSE, 2); }
    heavy_empty = lbl("Nenhum programa pesado aberto.", "vazio", .5); gtk_widget_set_margin_top(heavy_empty, 10); gtk_widget_set_margin_bottom(heavy_empty, 10);
    gtk_box_pack_start(GTK_BOX(hp), heavy_empty, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right), hp, FALSE, FALSE, 0);
    GtkWidget *np = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 4), "glasspanel"); gtk_widget_set_margin_top(np, 12);
    gtk_box_pack_start(GTK_BOX(np), lbl("REDE AGORA", "colh", 0), FALSE, FALSE, 2);
    GtkWidget *nr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_box_set_homogeneous(GTK_BOX(nr), TRUE);
    GtkWidget *nd = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), *nu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    net_down = lbl("—", "tval", 0); net_up = lbl("—", "tval", 0); net_down_sub = lbl("", "tsub", 0); net_info = lbl("…", "tsub", 0);
    gtk_box_pack_start(GTK_BOX(nd), lbl("↓ BAIXANDO", "colh", 0), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(nd), net_down, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(nd), net_down_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nu), lbl("↑ ENVIANDO", "colh", 0), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(nu), net_up, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nr), nd, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(nr), nu, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(np), nr, FALSE, FALSE, 0);
    gtk_label_set_ellipsize(GTK_LABEL(net_info), PANGO_ELLIPSIZE_END); gtk_widget_set_margin_top(net_info, 4);
    gtk_box_pack_start(GTK_BOX(np), net_info, FALSE, FALSE, 0);
    speed_btn = cls(cls(gtk_button_new_with_label("Testar velocidade"), "pill"), "soft"); gtk_widget_set_halign(speed_btn, GTK_ALIGN_START); gtk_widget_set_margin_top(speed_btn, 6);
    g_signal_connect(speed_btn, "clicked", G_CALLBACK(on_speed), NULL); gtk_box_pack_start(GTK_BOX(np), speed_btn, FALSE, FALSE, 0);
    speed_res = lbl("Mede o máximo da sua internet. Gasta até uns 60 MB de dados.", "tsub", 0); gtk_label_set_line_wrap(GTK_LABEL(speed_res), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(speed_res), 44);
    gtk_box_pack_start(GTK_BOX(np), speed_res, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right), np, FALSE, FALSE, 0);
    GtkWidget *lv = cls(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14), "glasspanel"); gtk_widget_set_margin_top(lv, 12);
    GtkWidget *lt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_set_margin_top(lt, 6); gtk_widget_set_margin_bottom(lt, 6);
    GtkWidget *ls = lbl("Desliga animações, efeitos e tarefas de fundo. O computador fica mais leve.", "sub", 0);
    gtk_label_set_line_wrap(GTK_LABEL(ls), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(ls), 34);
    gtk_box_pack_start(GTK_BOX(lt), lbl("Modo desempenho", "h2", 0), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(lt), ls, FALSE, FALSE, 0);
    GtkWidget *sw = gtk_switch_new(); gtk_widget_set_valign(sw, GTK_ALIGN_CENTER); gtk_widget_set_can_focus(sw, FALSE);
    gtk_switch_set_active(GTK_SWITCH(sw), nova_leve); g_signal_connect(sw, "state-set", G_CALLBACK(on_leve), NULL);
    gtk_box_pack_start(GTK_BOX(lv), lt, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(lv), sw, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right), lv, FALSE, FALSE, 0);
    GtkWidget *tip = lbl("Dica: fechar o programa que mais pesa é o que mais destrava o computador. A limpeza ajuda sobretudo a recuperar espaço em disco.", "sub", 0);
    gtk_label_set_line_wrap(GTK_LABEL(tip), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(tip), 44); gtk_widget_set_margin_top(tip, 14);
    gtk_box_pack_start(GTK_BOX(right), tip, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(po), right, TRUE, TRUE, 0);
    gtk_stack_add_named(GTK_STACK(pages), po, "otimizar");

    /* --- aba Monitor --- */
    GtkWidget *pm = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *tiles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); gtk_box_set_homogeneous(GTK_BOX(tiles), TRUE);
    const char *tn[4] = { "PROCESSADOR", "MEMÓRIA", "DISCO", "TEMPERATURA" };
    for (int k = 0; k < 4; k++) {
        GtkWidget *t = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 1), "tile");
        tile_val[k] = lbl("—", "tval", 0); tile_sub[k] = lbl("", "tsub", 0);
        tile_graph[k] = gtk_drawing_area_new(); gtk_widget_set_size_request(tile_graph[k], -1, 40); gtk_widget_set_margin_top(tile_graph[k], 6);
        g_signal_connect(tile_graph[k], "draw", G_CALLBACK(draw_spark), GINT_TO_POINTER(k));
        gtk_box_pack_start(GTK_BOX(t), lbl(tn[k], "colh", 0), FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(t), tile_val[k], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(t), tile_sub[k], FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(t), tile_graph[k], FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(tiles), t, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(pm), tiles, FALSE, FALSE, 0);
    GtkWidget *tp = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "glasspanel");
    GtkWidget *hd = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_widget_set_margin_start(hd, 8); gtk_widget_set_margin_end(hd, 8);
    gtk_widget_set_margin_top(hd, 6); gtk_widget_set_margin_bottom(hd, 4);
    gtk_box_pack_start(GTK_BOX(hd), lbl("PROGRAMA", "colh", 0), TRUE, TRUE, 0);
    GtkWidget *c1 = lbl("ESTADO", "colh", 0); gtk_widget_set_size_request(c1, 92, -1); gtk_box_pack_start(GTK_BOX(hd), c1, FALSE, FALSE, 0);
    sort_btn[0] = cls(gtk_button_new_with_label("CPU ▾"), "sort"); gtk_widget_set_size_request(sort_btn[0], 52, -1);
    sort_btn[1] = cls(gtk_button_new_with_label("MEMÓRIA ▾"), "sort"); gtk_widget_set_size_request(sort_btn[1], 70, -1);
    g_signal_connect(sort_btn[0], "clicked", G_CALLBACK(on_sort), GINT_TO_POINTER(1)); g_signal_connect(sort_btn[1], "clicked", G_CALLBACK(on_sort), GINT_TO_POINTER(0));
    gtk_box_pack_start(GTK_BOX(hd), sort_btn[0], FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(hd), sort_btn[1], FALSE, FALSE, 0);
    GtkWidget *sp2 = gtk_label_new(""); gtk_widget_set_size_request(sp2, 78, -1); gtk_box_pack_start(GTK_BOX(hd), sp2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tp), hd, FALSE, FALSE, 0);
    for (int i = 0; i < NPROC; i++) { procs[i] = make_row(TRUE); add_end_button(&procs[i], "Encerrar"); gtk_box_pack_start(GTK_BOX(tp), procs[i].box, FALSE, FALSE, 0); }
    gtk_box_pack_start(GTK_BOX(pm), tp, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(pages), pm, "monitor");

    gtk_widget_show_all(win);
    for (int i = 0; i < NHEAVY; i++) { gtk_widget_set_no_show_all(heavy[i].box, TRUE); gtk_widget_hide(heavy[i].box); }
    for (int i = 0; i < NPROC; i++) { gtk_widget_set_no_show_all(procs[i].box, TRUE); gtk_widget_hide(procs[i].box); }
    gboolean mon = argc > 1 && !strcmp(argv[1], "--monitor");
    on_seg(NULL, GINT_TO_POINTER(mon ? 1 : 0)); on_sort(NULL, GINT_TO_POINTER(1));
    net_update(); g_timeout_add(2000, tick_data, NULL);
    if (!mon && !g_getenv("NOVA_OTIMIZADOR_MANUAL")) g_timeout_add(700, auto_start, NULL);
    gtk_main();
    return 0;
}
