// Jogos retrô grátis (NOVA OS): atalhos para catálogos LEGAIS de jogos clássicos (homebrew, freeware, jogos de criadores independentes).
#include "../nova-comum/nova.h"

static GtkWidget *win, *entry, *combo;
static const char *CONSOLES[] = { "Todos os consoles", "NES", "Super Nintendo", "Mega Drive", "Master System", "Game Boy", "Game Boy Advance", "PlayStation", "Atari 2600" };
static const char *CSS =
".glasspanel { border-radius: 16px; background-color: alpha(@card,.72); border: 1px solid alpha(@fg,.08); padding: 14px 16px; }"
"entry { border-radius: 10px; padding: 8px 10px; }";

static void abrir(const char *url) { const char *a[] = { "xdg-open", url, NULL }; g_spawn_async(NULL, (char **)a, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL); }
static char *consulta(void) {
    const char *t = gtk_entry_get_text(GTK_ENTRY(entry)); int c = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    char *q = g_strdup_printf("%s%s%s", t, (*t && c > 0) ? " " : "", c > 0 ? CONSOLES[c] : ""); char *e = g_uri_escape_string(q, NULL, FALSE); g_free(q); return e;
}
static void on_itch(GtkWidget *w, gpointer d) { char *q = consulta(), *u = g_strdup_printf("https://itch.io/search?q=%s", q); abrir(*q ? u : "https://itch.io/games/tag-retro"); g_free(u); g_free(q); }
static void on_hh(GtkWidget *w, gpointer d) { abrir("https://hh.gbdev.io/"); }
static void on_ra(GtkWidget *w, gpointer d) { const char *a[] = { "flatpak", "run", "org.libretro.RetroArch", "--menu", NULL }; g_spawn_async(NULL, (char **)a, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL); }
static void on_pasta(GtkWidget *w, gpointer d) { char *p = g_build_filename(g_get_home_dir(), "Jogos", NULL); const char *a[] = { "pcmanfm", p, NULL }; g_spawn_async(NULL, (char **)a, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL); g_free(p); }
static void on_enter(GtkWidget *w, gpointer d) { on_itch(w, d); }

static GtkWidget *card(GtkWidget *box, const char *titulo, const char *texto, const char *botao, const char *tipo, GCallback cb) {
    GtkWidget *c = cls(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14), "glasspanel"); gtk_widget_set_margin_bottom(c, 10);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(v), lbl(titulo, "h2", 0), FALSE, FALSE, 0);
    GtkWidget *s = lbl(texto, "sub", 0); gtk_label_set_line_wrap(GTK_LABEL(s), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(s), 48); gtk_box_pack_start(GTK_BOX(v), s, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(c), v, TRUE, TRUE, 0);
    GtkWidget *b = cls(cls(gtk_button_new_with_label(botao), "pill"), tipo); gtk_widget_set_valign(b, GTK_ALIGN_CENTER); g_signal_connect(b, "clicked", cb, NULL); gtk_box_pack_end(GTK_BOX(c), b, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0); return c;
}
int main(int argc, char **argv) {
    gtk_init(&argc, &argv); nova_init(CSS); nova_load_blur();
    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL); gtk_window_set_title(GTK_WINDOW(win), "Jogos retrô grátis"); gtk_window_set_default_size(GTK_WINDOW(win), 700, 540);
    gtk_window_set_wmclass(GTK_WINDOW(win), "nova-jogos", "Nova-jogos"); gtk_widget_set_app_paintable(win, TRUE);
    g_signal_connect(win, "draw", G_CALLBACK(nova_glass), NULL); g_signal_connect(win, "configure-event", G_CALLBACK(nova_redraw_on_move), NULL); g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_container_set_border_width(GTK_CONTAINER(v), 28); gtk_container_add(GTK_CONTAINER(win), v);
    gtk_box_pack_start(GTK_BOX(v), lbl("Jogos retrô grátis", "h1", 0), FALSE, FALSE, 0);
    GtkWidget *s = lbl("Catálogos legais de jogos clássicos: criados por fãs e desenvolvedores independentes (homebrew e freeware).", "sub", 0); gtk_label_set_line_wrap(GTK_LABEL(s), TRUE); gtk_widget_set_margin_bottom(s, 16); gtk_box_pack_start(GTK_BOX(v), s, FALSE, FALSE, 0);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_margin_bottom(h, 14);
    entry = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nome do jogo (opcional)"); gtk_widget_set_hexpand(entry, TRUE); g_signal_connect(entry, "activate", G_CALLBACK(on_enter), NULL);
    combo = gtk_combo_box_text_new(); for (int i = 0; i < 9; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), CONSOLES[i]); gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_box_pack_start(GTK_BOX(h), entry, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(h), combo, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(v), h, FALSE, FALSE, 0);
    card(v, "itch.io", "Milhares de jogos retrô gratuitos e pagos feitos por criadores independentes. Busca pelo nome e console acima.", "Buscar", "accent", G_CALLBACK(on_itch));
    card(v, "Homebrew Hub", "Jogos novos para Game Boy, Game Boy Advance, NES e outros, feitos pela comunidade.", "Abrir", "soft", G_CALLBACK(on_hh));
    card(v, "Baixador do RetroArch", "Dentro do RetroArch: Menu principal → Atualizador Online → Baixador de conteúdo. Traz homebrew e freeware prontos.", "Abrir RetroArch", "soft", G_CALLBACK(on_ra));
    card(v, "Minha pasta de jogos", "Depois de baixar, ponha o arquivo na pasta do console em ~/Jogos e escaneie no RetroArch.", "Abrir pasta", "soft", G_CALLBACK(on_pasta));
    gtk_widget_show_all(win); gtk_main(); return 0;
}
