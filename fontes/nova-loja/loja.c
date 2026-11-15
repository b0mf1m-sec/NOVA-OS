// Loja NOVA OS: instala os programas mais usados com um clique (GTK3; apt + Flatpak).
#include "../nova-comum/nova.h"

typedef struct {
    const char *name, *desc; int cat; char kind;      // kind: 'A' apt, 'F' flatpak
    const char *pkgs, *cmd, *icon; gboolean feat;
    // estado em execução
    gboolean installed; GtkWidget *card, *stack, *ring, *pct; double frac, shown; guint tick;
} App;

enum { C_FEAT, C_NET, C_OFFICE, C_MEDIA, C_IMG, C_GAMES, C_TOOLS, C_INST, NCAT };
static const char *CAT_NAME[] = { "Destaques", "Internet", "Escritório", "Mídia", "Imagem", "Jogos", "Ferramentas", "Instalados" };
static const char *CAT_ICON[] = { "starred-symbolic", "network-workgroup-symbolic", "x-office-document-symbolic", "folder-music-symbolic",
    "applications-graphics-symbolic", "applications-games-symbolic", "applications-system-symbolic", "emblem-ok-symbolic" };

static App apps[] = {
 { "Firefox", "Navegador rápido e privado", C_NET, 'A', "firefox-esr firefox-esr-l10n-pt-br", "firefox-esr", "firefox", TRUE },
 { "WhatsApp", "Suas conversas numa janela própria", C_NET, 'B', "nova-whatsapp", "nova-whatsapp", "whatsapp", TRUE },
 { "Google Chrome", "O navegador do Google", C_NET, 'F', "com.google.Chrome", NULL, "google-chrome" },
 { "Zoom", "Reuniões e chamadas de vídeo", C_NET, 'F', "us.zoom.Zoom", NULL, "Zoom" },
 { "Chromium", "Navegador de código aberto, base do Chrome", C_NET, 'A', "chromium chromium-l10n", "chromium", "chromium" },
 { "Thunderbird", "E-mail, calendário e contatos", C_NET, 'A', "thunderbird thunderbird-l10n-pt-br", "thunderbird", "thunderbird" },
 { "Telegram", "Mensagens rápidas e sincronizadas", C_NET, 'F', "org.telegram.desktop", NULL, "telegram" },
 { "Discord", "Voz, vídeo e texto para comunidades", C_NET, 'F', "com.discordapp.Discord", NULL, "discord", TRUE },
 { "qBittorrent", "Cliente de torrents leve", C_NET, 'A', "qbittorrent", "qbittorrent", "qbittorrent" },
 { "LibreOffice", "Textos, planilhas e apresentações", C_OFFICE, 'A', "libreoffice-writer libreoffice-calc libreoffice-impress libreoffice-gtk3 libreoffice-l10n-pt-br", "libreoffice", "libreoffice-startcenter", TRUE },
 { "Editor de texto", "Mousepad: anotações e arquivos simples", C_OFFICE, 'A', "mousepad", "mousepad", "org.xfce.mousepad" },
 { "Calculadora", "Galculator: simples e científica", C_OFFICE, 'A', "galculator", "galculator", "galculator" },
 { "KeePassXC", "Cofre de senhas seguro", C_OFFICE, 'A', "keepassxc", "keepassxc", "keepassxc" },
 { "VLC", "Toca qualquer vídeo ou música", C_MEDIA, 'A', "vlc", "vlc", "vlc", TRUE },
 { "Spotify", "Música e podcasts por streaming", C_MEDIA, 'F', "com.spotify.Client", NULL, "spotify", TRUE },
 { "Audacity", "Gravação e edição de áudio", C_MEDIA, 'A', "audacity", "audacity", "audacity" },
 { "OBS Studio", "Gravação de tela e transmissões ao vivo", C_MEDIA, 'A', "obs-studio", "obs", "obs" },
 { "Kdenlive", "Editor de vídeo completo", C_MEDIA, 'A', "kdenlive", "kdenlive", "kdenlive" },
 { "GIMP", "Edição de fotos e imagens", C_IMG, 'A', "gimp", "gimp", "gimp", TRUE },
 { "Inkscape", "Desenho vetorial", C_IMG, 'A', "inkscape", "inkscape", "inkscape" },
 { "Krita", "Pintura e ilustração digital", C_IMG, 'A', "krita", "krita", "krita" },
 { "Visualizador de imagens", "Ristretto: leve e rápido", C_IMG, 'A', "ristretto", "ristretto", "org.xfce.ristretto" },
 { "Steam", "A maior loja de jogos para PC", C_GAMES, 'F', "com.valvesoftware.Steam", NULL, "steam", TRUE },
 { "RetroArch", "Jogos clássicos: NES, Super Nintendo, Mega Drive, Game Boy, PlayStation 1 e mais. Aceita controle por cabo ou Bluetooth", C_GAMES, 'F', "org.libretro.RetroArch", NULL, "retroarch", TRUE },
 { "SuperTuxKart", "Corrida de kart em 3D", C_GAMES, 'A', "supertuxkart", "supertuxkart", "supertuxkart" },
 { "Luanti", "Mundo de blocos (antigo Minetest)", C_GAMES, 'A', "luanti", "luanti", "minetest" },
 { "Paciência", "Dezenas de jogos de cartas", C_GAMES, 'A', "aisleriot", "sol", "gnome-aisleriot" },
 { "VS Code", "Editor de código da Microsoft", C_TOOLS, 'F', "com.visualstudio.code", NULL, "visual-studio-code", TRUE },
 { "Compactador", "Xarchiver: abre zip, rar, 7z e outros", C_TOOLS, 'A', "xarchiver unzip p7zip-full", "xarchiver", "xarchiver" },
 { "htop", "Monitor de processos no terminal", C_TOOLS, 'A', "htop", "xterm -e htop", "htop" },
 { "BleachBit", "Limpeza de arquivos temporários", C_TOOLS, 'A', "bleachbit", "bleachbit", "bleachbit" },
 { "Programas do Windows", "Wine: abre programas .exe feitos para Windows", C_TOOLS, 'A', "wine", "winecfg", "wine" },
 { "Impressoras", "Adicionar e gerenciar impressoras", C_TOOLS, 'A', "cups system-config-printer", "system-config-printer", "printer" },
 { "Bluetooth", "Conectar fones, caixas de som e outros aparelhos", C_TOOLS, 'A', "blueman", "blueman-manager", "blueman" },
 { "Captura de tela", "Flameshot: recorta e anota a tela (tecla Print)", C_TOOLS, 'A', "flameshot", "flameshot gui", "flameshot" },
};
#define NAPPS (int)G_N_ELEMENTS(apps)

typedef struct { int app; gboolean remove; } Op;
static GQueue ops = G_QUEUE_INIT;
static Op *cur;
static GtkWidget *win, *side, *flow, *title, *subtitle, *hero, *page, *search, *status_rev, *status_lbl, *status_bar;
static int cat = C_FEAT;
static gint64 page_t0;
static FILE *logfile; static char last_err[200];

static gboolean draw_side(GtkWidget *w, cairo_t *cr, gpointer d) {
    nova_glass(w, cr, NULL);
    GtkAllocation a; gtk_widget_get_allocation(w, &a);
    cairo_set_source_rgba(cr, 0, 0, 0, nova_dark ? .35 : .10); cairo_rectangle(cr, a.width - 1, 0, 1, a.height); cairo_fill(cr);
    return FALSE;
}
static gboolean on_configure(GtkWidget *w, GdkEvent *e, gpointer d) { gtk_widget_queue_draw(side); return FALSE; }

/* ---------- anel de progresso ---------- */
static gboolean draw_ring(GtkWidget *w, cairo_t *cr, gpointer data) {
    App *a = data; double s = gtk_widget_get_allocated_width(w), r = s / 2 - 3;
    cairo_set_line_width(cr, 3); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, s / 2, s / 2, r, 0, 2 * G_PI); { GdkRGBA f; nova_color(w, "fg", &f); cairo_set_source_rgba(cr, f.red, f.green, f.blue, .14); } cairo_stroke(cr);
    { GdkRGBA c; nova_color(w, "accent", &c); gdk_cairo_set_source_rgba(cr, &c); }
    if (a->frac < 0) {                                   // indeterminado: arco girando
        double t = g_get_monotonic_time() / 1e6 * 4.2;
        cairo_arc(cr, s / 2, s / 2, r, t, t + 1.5 + .6 * sin(t * .7));
    } else cairo_arc(cr, s / 2, s / 2, r, -G_PI_2, -G_PI_2 + 2 * G_PI * MAX(a->shown, .02));
    cairo_stroke(cr);
    return TRUE;
}
static gboolean tick_ring(GtkWidget *w, GdkFrameClock *c, gpointer data) {
    App *a = data;
    if (a->frac >= 0) a->shown += (a->frac - a->shown) * .12;   // aproxima suavemente do valor real
    gtk_widget_queue_draw(w); return G_SOURCE_CONTINUE;
}

/* ---------- estado instalado ---------- */
static char *run_out(const char *cmd) { char *o = NULL; g_spawn_command_line_sync(cmd, &o, NULL, NULL, NULL); return o ? o : g_strdup(""); }
static char *first_pkg(App *a) { char **v = g_strsplit(a->pkgs, " ", 2); char *p = g_strdup(v[0]); g_strfreev(v); return p; }

static void set_state(App *a, const char *st) {
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), st);
    gboolean busy = !strcmp(st, "busy");
    if (busy && !a->tick) a->tick = gtk_widget_add_tick_callback(a->ring, tick_ring, a, NULL);
    if (!busy && a->tick) { gtk_widget_remove_tick_callback(a->ring, a->tick); a->tick = 0; }
}

static void refresh_installed(void) {
    char *dp = run_out("dpkg-query -W -f '${Package} ${db:Status-Abbrev}\\n'");
    char *fp = run_out("flatpak list --user --app --columns=application");
    for (int i = 0; i < NAPPS; i++) {
        App *a = &apps[i]; char *p = first_pkg(a), *needle;
        if (a->kind == 'B') { a->installed = TRUE; needle = NULL; }
        else if (a->kind == 'A') { needle = g_strdup_printf("\n%s ii", p); char *h = g_strconcat("\n", dp, NULL); a->installed = strstr(h, needle) != NULL; g_free(h); }
        else { needle = g_strdup_printf("\n%s\n", p); char *h = g_strconcat("\n", fp, "\n", NULL); a->installed = strstr(h, needle) != NULL; g_free(h); }
        g_free(needle); g_free(p);
        gboolean queued = cur && cur->app == i;
        for (GList *l = ops.head; l; l = l->next) if (((Op *)l->data)->app == i) queued = TRUE;
        if (!queued && a->stack) set_state(a, a->installed ? "open" : "get");
    }
    g_free(dp); g_free(fp);
    if (flow) gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(flow));
}

/* ---------- fila de instalação ---------- */
static void next_op(void);
static void status(const char *text, double frac) {
    gtk_label_set_text(GTK_LABEL(status_lbl), text);
    if (frac < 0) gtk_progress_bar_pulse(GTK_PROGRESS_BAR(status_bar)); else gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(status_bar), frac);
    gtk_revealer_set_reveal_child(GTK_REVEALER(status_rev), TRUE);
}
static gboolean hide_status(gpointer d) { if (!cur) gtk_revealer_set_reveal_child(GTK_REVEALER(status_rev), FALSE); return G_SOURCE_REMOVE; }

static void on_line(GObject *src, GAsyncResult *res, gpointer d) {
    char *line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(src), res, NULL, NULL);
    if (!line) { g_object_unref(src); return; }
    if (logfile) { fprintf(logfile, "%s\n", line); fflush(logfile); }
    if (*line && !g_str_has_prefix(line, "dlstatus:") && !g_str_has_prefix(line, "pmstatus:") && (strstr(line, "E:") || strstr(line, "rror") || strstr(line, "protegido") || strstr(line, "indispon") || strstr(line, "not allowed") || strstr(line, "sudo")))
        g_strlcpy(last_err, line, sizeof last_err);
    if (cur) {
        App *a = &apps[cur->app]; double f = -1;
        if (g_str_has_prefix(line, "dlstatus:")) { char **v = g_strsplit(line, ":", 4); if (v[2]) f = g_ascii_strtod(v[2], NULL) / 100 * .45; g_strfreev(v); }
        else if (g_str_has_prefix(line, "pmstatus:")) { char **v = g_strsplit(line, ":", 4); if (v[2]) f = .45 + g_ascii_strtod(v[2], NULL) / 100 * .55; g_strfreev(v); }
        if (f >= 0) {
            a->frac = MAX(a->frac, f);
            char b[16]; g_snprintf(b, sizeof b, "%d%%", (int)(a->frac * 100)); gtk_label_set_text(GTK_LABEL(a->pct), b);
            char *s = g_strdup_printf("%s %s… %d%%", cur->remove ? "Removendo" : "Instalando", a->name, (int)(a->frac * 100));
            status(s, a->frac); g_free(s);
        }
    }
    g_free(line);
    g_data_input_stream_read_line_async(G_DATA_INPUT_STREAM(src), G_PRIORITY_DEFAULT, NULL, on_line, NULL);
}

static gboolean pulse(gpointer d) {
    if (!cur) return G_SOURCE_REMOVE;
    if (apps[cur->app].frac < 0) gtk_progress_bar_pulse(GTK_PROGRESS_BAR(status_bar));
    return G_SOURCE_CONTINUE;
}

static void on_done(GObject *src, GAsyncResult *res, gpointer d) {
    GSubprocess *p = G_SUBPROCESS(src); g_subprocess_wait_finish(p, res, NULL);
    gboolean ok = g_subprocess_get_if_exited(p) && g_subprocess_get_exit_status(p) == 0;
    App *a = &apps[cur->app]; gboolean rem = cur->remove;
    g_free(cur); cur = NULL; g_object_unref(p);
    refresh_installed();
    char *s = ok ? g_strdup_printf("%s %s", a->name, rem ? "foi removido" : "está pronto para usar")
                 : g_strdup_printf("Não foi possível %s %s%s%s", rem ? "remover" : "instalar", a->name, *last_err ? " — " : "", last_err);
    status(s, ok ? 1 : 0); g_free(s);
    if (ops.length) next_op(); else g_timeout_add_seconds(4, hide_status, NULL);
}

static void next_op(void) {
    if (cur || !ops.length) return;
    cur = g_queue_pop_head(&ops);
    App *a = &apps[cur->app]; last_err[0] = 0;
    a->frac = a->kind == 'A' ? 0 : -1; a->shown = 0;
    gtk_label_set_text(GTK_LABEL(a->pct), a->kind == 'A' ? "0%" : "");
    set_state(a, "busy");
    char *s = g_strdup_printf("%s %s…", cur->remove ? "Removendo" : "Instalando", a->name); status(s, a->frac); g_free(s);
    char *sh;
    if (a->kind == 'A')
        sh = g_strdup_printf("%s/.local/bin/nova-apt %s %s 2>&1", g_get_home_dir(), cur->remove ? "remove" : "install", a->pkgs);
    else
        sh = cur->remove ? g_strdup_printf("flatpak uninstall --user -y --noninteractive %s 2>&1", a->pkgs)
                         : !strcmp(a->pkgs, "org.libretro.RetroArch")      // RetroArch já sai pronto: emuladores, pasta de jogos e controles
                           ? g_strdup_printf("flatpak install --user -y --noninteractive flathub %s 2>&1 && %s/.local/bin/nova-jogos-preparar 2>&1", a->pkgs, g_get_home_dir())
                           : g_strdup_printf("flatpak install --user -y --noninteractive flathub %s 2>&1", a->pkgs);
    if (logfile) { fprintf(logfile, "\n$ %s\n", sh); fflush(logfile); }
    GSubprocess *p = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, NULL, "sh", "-c", sh, NULL); g_free(sh);
    if (!p) { g_free(cur); cur = NULL; refresh_installed(); return; }
    GDataInputStream *in = g_data_input_stream_new(g_subprocess_get_stdout_pipe(p));
    g_data_input_stream_read_line_async(in, G_PRIORITY_DEFAULT, NULL, on_line, NULL);
    g_subprocess_wait_async(p, NULL, on_done, NULL);
    g_timeout_add(120, pulse, NULL);
}

static void enqueue(App *a, gboolean remove) {
    Op *o = g_new0(Op, 1); o->app = a - apps; o->remove = remove;
    g_queue_push_tail(&ops, o);
    a->frac = -1; gtk_label_set_text(GTK_LABEL(a->pct), cur ? "Na fila" : ""); set_state(a, "busy");
    next_op();
}

static void on_get(GtkWidget *w, gpointer a) { enqueue(a, FALSE); }
static void on_open(GtkWidget *w, gpointer data) {
    App *a = data; char *c = a->kind == 'F' ? g_strdup_printf("flatpak run %s", a->pkgs)
                         : a->kind == 'B' ? g_strdup_printf("%s/.local/bin/%s", g_get_home_dir(), a->cmd) : g_strdup(a->cmd);
    g_spawn_command_line_async(c, NULL); g_free(c);
}
static void on_remove(GtkWidget *w, gpointer data) {
    App *a = data;
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "Remover %s?", a->name);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "O programa será desinstalado. Seus arquivos pessoais não são apagados.");
    gtk_dialog_add_button(GTK_DIALOG(d), "Cancelar", GTK_RESPONSE_CANCEL);
    GtkWidget *b = gtk_dialog_add_button(GTK_DIALOG(d), "Remover", GTK_RESPONSE_OK);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "destructive-action");
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) enqueue(a, TRUE);
    gtk_widget_destroy(d);
}

/* ---------- cartões ---------- */

static gboolean on_cross(GtkWidget *w, GdkEventCrossing *e, gpointer in) {
    if (e->detail == GDK_NOTIFY_INFERIOR) return FALSE;
    if (in) gtk_widget_set_state_flags(w, GTK_STATE_FLAG_PRELIGHT, FALSE); else gtk_widget_unset_state_flags(w, GTK_STATE_FLAG_PRELIGHT);
    return FALSE;
}

static GtkWidget *make_card(App *a) {
    GtkWidget *ev = cls(gtk_event_box_new(), "card");
    gtk_widget_add_events(ev, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(ev, "enter-notify-event", G_CALLBACK(on_cross), GINT_TO_POINTER(1));
    g_signal_connect(ev, "leave-notify-event", G_CALLBACK(on_cross), GINT_TO_POINTER(0));
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkIconTheme *th = gtk_icon_theme_get_default();
    GtkWidget *img = gtk_image_new_from_icon_name(gtk_icon_theme_has_icon(th, a->icon) ? a->icon : "application-x-executable", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 56); cls(img, "app-icon");
    GtkWidget *txt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3); gtk_widget_set_valign(txt, GTK_ALIGN_CENTER);
    GtkWidget *n = cls(gtk_label_new(a->name), "app-name"); gtk_label_set_xalign(GTK_LABEL(n), 0);
    gtk_label_set_ellipsize(GTK_LABEL(n), PANGO_ELLIPSIZE_END);
    GtkWidget *ds = cls(gtk_label_new(a->desc), "app-desc"); gtk_label_set_xalign(GTK_LABEL(ds), 0);
    gtk_label_set_line_wrap(GTK_LABEL(ds), TRUE); gtk_label_set_lines(GTK_LABEL(ds), 2);
    gtk_label_set_ellipsize(GTK_LABEL(ds), PANGO_ELLIPSIZE_END); gtk_label_set_max_width_chars(GTK_LABEL(ds), 22);
    gtk_box_pack_start(GTK_BOX(txt), n, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(txt), ds, FALSE, FALSE, 0);

    a->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(a->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(a->stack), 220);
    gtk_widget_set_valign(a->stack, GTK_ALIGN_CENTER); gtk_stack_set_hhomogeneous(GTK_STACK(a->stack), FALSE);
    GtkWidget *get = cls(cls(gtk_button_new_with_label("Obter"), "pill"), "soft");
    g_signal_connect(get, "clicked", G_CALLBACK(on_get), a);
    gtk_widget_set_halign(get, GTK_ALIGN_END); gtk_widget_set_valign(get, GTK_ALIGN_CENTER);
    GtkWidget *busy = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_halign(busy, GTK_ALIGN_END);
    a->pct = cls(gtk_label_new(""), "pct");
    a->ring = gtk_drawing_area_new(); gtk_widget_set_size_request(a->ring, 30, 30); gtk_widget_set_valign(a->ring, GTK_ALIGN_CENTER);
    g_signal_connect(a->ring, "draw", G_CALLBACK(draw_ring), a);
    gtk_box_pack_start(GTK_BOX(busy), a->pct, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(busy), a->ring, FALSE, FALSE, 0);
    GtkWidget *open = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6); gtk_widget_set_halign(open, GTK_ALIGN_END); gtk_widget_set_valign(open, GTK_ALIGN_CENTER);
    GtkWidget *rm = cls(gtk_button_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_MENU), "trash");
    gtk_widget_set_tooltip_text(rm, "Remover"); g_signal_connect(rm, "clicked", G_CALLBACK(on_remove), a);
    GtkWidget *ob = cls(cls(gtk_button_new_with_label("Abrir"), "pill"), "plain");
    g_signal_connect(ob, "clicked", G_CALLBACK(on_open), a);
    if (a->kind != 'B' && strcmp(a->pkgs, "firefox-esr firefox-esr-l10n-pt-br")) gtk_box_pack_start(GTK_BOX(open), rm, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(open), ob, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(a->stack), get, "get");
    gtk_stack_add_named(GTK_STACK(a->stack), busy, "busy");
    gtk_stack_add_named(GTK_STACK(a->stack), open, "open");

    gtk_box_pack_start(GTK_BOX(row), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), txt, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(row), a->stack, FALSE, FALSE, 0);
    gtk_widget_set_margin_start(row, 14); gtk_widget_set_margin_end(row, 14); gtk_widget_set_margin_top(row, 13); gtk_widget_set_margin_bottom(row, 13);
    gtk_container_add(GTK_CONTAINER(ev), row);
    a->card = ev;
    return ev;
}

/* ---------- navegação ---------- */
static gboolean filter(GtkFlowBoxChild *c, gpointer d) {
    App *a = &apps[gtk_flow_box_child_get_index(c)];
    const char *q = gtk_entry_get_text(GTK_ENTRY(search));
    if (*q) {
        char *h = g_utf8_casefold(g_strconcat(a->name, " ", a->desc, NULL), -1), *n = g_utf8_casefold(q, -1);
        gboolean m = strstr(h, n) != NULL; g_free(h); g_free(n); return m;
    }
    return cat == C_FEAT ? a->feat : cat == C_INST ? a->installed : a->cat == cat;
}

static gboolean tick_page(GtkWidget *w, GdkFrameClock *c, gpointer d) {
    double t = (g_get_monotonic_time() - page_t0) / 1e6 / 0.32, e = t >= 1 ? 1 : 1 - pow(1 - t, 3);
    gtk_widget_set_opacity(page, e); gtk_widget_set_margin_top(page, (int)round((1 - e) * 14));
    return t < 1;
}

static void show_page(void) {
    const char *q = gtk_entry_get_text(GTK_ENTRY(search));
    gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(flow));
    int n = 0; for (int i = 0; i < NAPPS; i++) if (filter(gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(flow), i), NULL)) n++;
    gtk_label_set_text(GTK_LABEL(title), *q ? "Resultados" : CAT_NAME[cat]);
    char *s = n == 0 ? g_strdup(*q ? "Nada encontrado. Tente outro nome." : "Nenhum programa aqui ainda.")
                     : g_strdup_printf(n == 1 ? "%d programa" : "%d programas", n);
    gtk_label_set_text(GTK_LABEL(subtitle), s); g_free(s);
    gtk_widget_set_visible(hero, cat == C_FEAT && !*q);
    page_t0 = g_get_monotonic_time(); gtk_widget_set_opacity(page, 0);
    gtk_widget_add_tick_callback(page, tick_page, NULL, NULL);
}
static void on_cat(GtkListBox *b, GtkListBoxRow *r, gpointer d) {
    if (!r) return;
    cat = gtk_list_box_row_get_index(r);
    if (*gtk_entry_get_text(GTK_ENTRY(search))) gtk_entry_set_text(GTK_ENTRY(search), ""); else show_page();
}
static void on_search(GtkWidget *w, gpointer d) { show_page(); }

static const char *CSS =
"window { background-color: @bg; }"
".side { padding: 14px 10px; }"
".brand { font-size: 17px; font-weight: 700; color: @fg; margin: 4px 8px 12px 8px; }"
".side entry { border-radius: 999px; background-color: alpha(@fg,.07); background-image: none; border: 1px solid alpha(@fg,.08);"
"  color: @fg; min-height: 30px; padding: 0 10px; box-shadow: none; transition: all 220ms ease; }"
".side entry:focus { background-color: @base; border-color: @accent; box-shadow: 0 0 0 3px alpha(@accent,.25); }"
".side list { background: none; margin-top: 12px; }"
".side row { border-radius: 9px; padding: 7px 10px; margin: 1px 0; color: @fg; transition: background-color 200ms ease, color 200ms ease; }"
".side row:hover { background-color: alpha(@fg,.06); }"
".side row:selected { background-color: @accent; color: #fff; box-shadow: 0 3px 10px alpha(@accent,.35); }"
".side row image { color: @accent; margin-right: 10px; transition: -gtk-icon-transform 260ms cubic-bezier(.3,1.8,.5,1), color 200ms ease; }"
".side row:hover image { -gtk-icon-transform: scale(1.22); }"
".side row:selected image { color: #fff; -gtk-icon-transform: scale(1.1); }"
".side row label { font-size: 13.5px; font-weight: 500; }"
".status { font-size: 11.5px; color: @fg; margin: 0 6px 6px 6px; }"
".side progressbar trough { min-height: 4px; border-radius: 4px; background-color: alpha(@fg,.12); border: none; }"
".side progressbar progress { min-height: 4px; border-radius: 4px; background-color: @accent; background-image: none; border: none; }"
".title { font-size: 28px; font-weight: 700; color: @fg; }"
".subtitle { font-size: 13px; color: @fg2; }"
".hero { border-radius: 18px; padding: 26px 28px; margin-bottom: 18px;"
"  background-image: radial-gradient(circle at 88% 15%, alpha(#fff,.45), transparent 50%), linear-gradient(120deg, #0a84ff, #5e5ce6 55%, #bf5af2);"
"  box-shadow: 0 12px 32px alpha(#5e5ce6,.30), inset 0 1px 0 alpha(#fff,.30); }"
".hero-k { font-size: 11px; font-weight: 700; letter-spacing: 1.5px; color: alpha(#fff,.80); }"
".hero-t { font-size: 25px; font-weight: 700; color: #fff; }"
".hero-s { font-size: 13.5px; color: alpha(#fff,.90); }"
"flowbox { background: none; } flowboxchild { padding: 6px; background: none; outline: none; }"
".card { border-radius: 16px; background-color: @card; border: 1px solid alpha(@fg,.07); box-shadow: 0 1px 3px alpha(#000,.05);"
"  transition: background-color 240ms ease, border-color 240ms ease, box-shadow 240ms ease; }"
".card:hover { border-color: alpha(@accent,.45); box-shadow: 0 12px 28px alpha(#000,.16), 0 0 0 3px alpha(@accent,.12); }"
".app-icon { transition: -gtk-icon-transform 320ms cubic-bezier(.3,1.9,.5,1); }"
".card:hover .app-icon { -gtk-icon-transform: scale(1.16) rotate(-3deg); }"
".app-name { font-size: 14.5px; font-weight: 600; color: @fg; }"
".app-desc { font-size: 12px; color: @fg2; }"
".pct { font-size: 11.5px; font-weight: 600; color: @fg2; }"
"button.trash { border-radius: 999px; min-width: 28px; min-height: 28px; padding: 0; border: none; background: none; box-shadow: none;"
"  color: alpha(@fg,0); transition: color 200ms ease, background-color 200ms ease; }"
".card:hover button.trash { color: @fg2; }"
".card:hover button.trash:hover { color: @danger; background-color: alpha(@danger,.14); }"
"scrolledwindow undershoot, scrolledwindow overshoot { background: none; }";

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    nova_init(CSS);
    char *lp = g_build_filename(g_get_user_cache_dir(), "nova-loja.log", NULL); logfile = fopen(lp, "w"); g_free(lp);
    nova_load_blur();

    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Loja"); gtk_window_set_default_size(GTK_WINDOW(win), 1000, 650);
    gtk_window_set_icon_name(GTK_WINDOW(win), "nova-loja"); gtk_window_set_wmclass(GTK_WINDOW(win), "nova-loja", "Nova-loja");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "configure-event", G_CALLBACK(on_configure), NULL);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_container_add(GTK_CONTAINER(win), root);

    // barra lateral
    side = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), "side"); gtk_widget_set_size_request(side, 216, -1);
    g_signal_connect(side, "draw", G_CALLBACK(draw_side), NULL);
    GtkWidget *brand = cls(gtk_label_new("Loja"), "brand"); gtk_label_set_xalign(GTK_LABEL(brand), 0);
    search = gtk_search_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(search), "Buscar");
    g_signal_connect(search, "search-changed", G_CALLBACK(on_search), NULL);
    GtkWidget *list = gtk_list_box_new();
    for (int i = 0; i < NCAT; i++) {
        GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(r), gtk_image_new_from_icon_name(CAT_ICON[i], GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
        GtkWidget *l = gtk_label_new(CAT_NAME[i]); gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_pack_start(GTK_BOX(r), l, TRUE, TRUE, 0);
        gtk_list_box_insert(GTK_LIST_BOX(list), r, -1);
    }
    status_rev = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(status_rev), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(status_rev), 280);
    GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    status_lbl = cls(gtk_label_new(""), "status"); gtk_label_set_xalign(GTK_LABEL(status_lbl), 0);
    gtk_label_set_line_wrap(GTK_LABEL(status_lbl), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(status_lbl), 24);
    status_bar = gtk_progress_bar_new(); gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(status_bar), .06);
    gtk_widget_set_margin_start(status_bar, 6); gtk_widget_set_margin_end(status_bar, 6); gtk_widget_set_margin_bottom(status_bar, 4);
    gtk_box_pack_start(GTK_BOX(sb), status_lbl, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(sb), status_bar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(status_rev), sb);
    gtk_box_pack_start(GTK_BOX(side), brand, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), search, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), list, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(side), status_rev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), side, FALSE, FALSE, 0);

    // conteúdo
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);   // margem fixa; "page" anima dentro
    gtk_widget_set_margin_start(wrap, 26); gtk_widget_set_margin_end(wrap, 26); gtk_widget_set_margin_top(wrap, 22); gtk_widget_set_margin_bottom(wrap, 22);
    title = cls(gtk_label_new(""), "title"); gtk_label_set_xalign(GTK_LABEL(title), 0);
    subtitle = cls(gtk_label_new(""), "subtitle"); gtk_label_set_xalign(GTK_LABEL(subtitle), 0); gtk_widget_set_margin_bottom(subtitle, 16);
    hero = cls(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5), "hero");
    GtkWidget *hk = cls(gtk_label_new("BEM-VINDO"), "hero-k"), *ht = cls(gtk_label_new("Tudo o que você usa, a um clique"), "hero-t"),
              *hs = cls(gtk_label_new("Escolha um programa e toque em Obter. A loja baixa, instala e deixa pronto para abrir."), "hero-s");
    gtk_label_set_xalign(GTK_LABEL(hk), 0); gtk_label_set_xalign(GTK_LABEL(ht), 0); gtk_label_set_xalign(GTK_LABEL(hs), 0);
    gtk_label_set_line_wrap(GTK_LABEL(hs), TRUE);
    gtk_box_pack_start(GTK_BOX(hero), hk, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(hero), ht, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(hero), hs, FALSE, FALSE, 0);
    gtk_widget_show_all(hero); gtk_widget_set_no_show_all(hero, TRUE); gtk_widget_hide(hero);
    flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 2); gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 3);
    gtk_widget_set_valign(flow, GTK_ALIGN_START);
    for (int i = 0; i < NAPPS; i++) gtk_container_add(GTK_CONTAINER(flow), make_card(&apps[i]));
    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(flow), filter, NULL, NULL);
    gtk_box_pack_start(GTK_BOX(page), hero, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), flow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wrap), page, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(sw), wrap);
    gtk_box_pack_start(GTK_BOX(root), sw, TRUE, TRUE, 0);

    g_signal_connect(list, "row-selected", G_CALLBACK(on_cat), NULL);
    gtk_widget_show_all(win);
    refresh_installed();
    gtk_list_box_select_row(GTK_LIST_BOX(list), gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), 0));
    gtk_widget_grab_focus(list);
    gtk_main();
    return 0;
}
