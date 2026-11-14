// NOVA OS Apps: grade em tela cheia com todos os programas (estilo Launchpad). Busca ao digitar; Esc ou clique fora fecha. Clique direito: abrir como administrador / desinstalar.
#include "../nova-comum/nova.h"
#include <gio/gdesktopappinfo.h>

typedef struct { GAppInfo *info; GtkWidget *child; char *key; } Item;
static GtkWidget *win, *flow, *search, *empty;
static GPtrArray *items; static gint64 t0; static gboolean menu_open;

static gboolean draw_bg(GtkWidget *w, cairo_t *cr, gpointer d) {
    if (nova_blur) { cairo_set_source_surface(cr, nova_blur, 0, 0); cairo_paint(cr); }
    if (nova_dark) cairo_set_source_rgba(cr, .05, .05, .08, .50); else cairo_set_source_rgba(cr, .96, .96, 1, .30);
    cairo_paint(cr); return FALSE;
}
static void launch(Item *it) {
    g_app_info_launch(it->info, NULL, NULL, NULL); gtk_main_quit();
}
static void on_click(GtkWidget *b, gpointer it) { launch(it); }

/* ---------- clique direito: abrir, abrir como administrador, desinstalar ---------- */
static void act(Item *it, const char *what) {
    const char *f = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(it->info)); if (!f) return;
    char *q = g_shell_quote(f), *c = g_strdup_printf("nova-programa %s %s", what, q);
    nova_spawn(c); g_free(c); g_free(q); gtk_main_quit();
}
static void on_m_open(GtkWidget *w, gpointer it) { launch(it); }
static void on_m_admin(GtkWidget *w, gpointer it) { act(it, "admin"); }
static void on_m_del(GtkWidget *w, gpointer it) { act(it, "desinstalar"); }
static void on_m_done(GtkWidget *w, gpointer d) { menu_open = FALSE; t0 = g_get_monotonic_time(); }
static gboolean on_app_press(GtkWidget *b, GdkEventButton *e, gpointer data) {
    if (e->button != 3) return FALSE;
    Item *it = data; const char *f = G_IS_DESKTOP_APP_INFO(it->info) ? g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(it->info)) : NULL;
    char *kind = NULL;
    if (f) { char *q = g_shell_quote(f), *c = g_strdup_printf("nova-programa info %s", q); kind = nova_run(c); g_free(c); g_free(q); }
    gboolean removable = kind && (g_str_has_prefix(kind, "apt ") || g_str_has_prefix(kind, "flatpak "));
    gboolean admin = f && !(kind && g_str_has_prefix(kind, "flatpak ")) && !strstr(f, "nova-whatsapp") && !strstr(f, "navegador");
    g_free(kind);
    GtkWidget *m = cls(gtk_menu_new(), "appmenu"), *i;
    i = gtk_menu_item_new_with_label("Abrir"); g_signal_connect(i, "activate", G_CALLBACK(on_m_open), it); gtk_menu_shell_append(GTK_MENU_SHELL(m), i);
    if (admin) { i = gtk_menu_item_new_with_label("Executar como administrador"); g_signal_connect(i, "activate", G_CALLBACK(on_m_admin), it); gtk_menu_shell_append(GTK_MENU_SHELL(m), i); }
    if (removable) { gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
        i = cls(gtk_menu_item_new_with_label("Desinstalar"), "perigo"); g_signal_connect(i, "activate", G_CALLBACK(on_m_del), it); gtk_menu_shell_append(GTK_MENU_SHELL(m), i); }
    g_signal_connect(m, "deactivate", G_CALLBACK(on_m_done), NULL);
    gtk_widget_show_all(m); menu_open = TRUE;
    gtk_menu_popup_at_pointer(GTK_MENU(m), (GdkEvent *)e);
    return TRUE;
}
static gboolean filter(GtkFlowBoxChild *c, gpointer d) {
    const char *q = gtk_entry_get_text(GTK_ENTRY(search)); if (!*q) return TRUE;
    Item *it = g_object_get_data(G_OBJECT(c), "item"); char *n = g_utf8_casefold(q, -1);
    gboolean m = strstr(it->key, n) != NULL; g_free(n); return m;
}
static Item *first_visible(void) {
    for (guint i = 0; i < items->len; i++) { Item *it = g_ptr_array_index(items, i); if (filter(GTK_FLOW_BOX_CHILD(it->child), NULL)) return it; }
    return NULL;
}
static void on_search(GtkWidget *w, gpointer d) { gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(flow)); gtk_widget_set_visible(empty, first_visible() == NULL); }
static void on_activate(GtkWidget *w, gpointer d) { Item *it = first_visible(); if (it) launch(it); }
static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) {
    if (e->keyval == GDK_KEY_Escape) { if (*gtk_entry_get_text(GTK_ENTRY(search))) gtk_entry_set_text(GTK_ENTRY(search), ""); else gtk_main_quit(); return TRUE; }
    if (!gtk_widget_has_focus(search)) { gtk_widget_grab_focus(search); gtk_editable_set_position(GTK_EDITABLE(search), -1); }
    return FALSE;
}
static gboolean on_press(GtkWidget *w, GdkEventButton *e, gpointer d) { gtk_main_quit(); return TRUE; }   // clique no vazio fecha
static gboolean on_focus_out(GtkWidget *w, GdkEvent *e, gpointer d) { if (!menu_open && g_get_monotonic_time() - t0 > 500000) gtk_main_quit(); return FALSE; }

// Entrada em cascata: cada ícone surge com um pequeno atraso em relação ao anterior.
static gboolean tick_in(GtkWidget *w, GdkFrameClock *c, gpointer d) {
    double t = (g_get_monotonic_time() - t0) / 1e6; gboolean more = FALSE;
    for (guint i = 0; i < items->len; i++) {
        Item *it = g_ptr_array_index(items, i); double e = ease_out((t - .05 - MIN(i, 40) * .012) / .34);
        gtk_widget_set_opacity(it->child, e); if (e < 1) more = TRUE;
    }
    return more;
}
static int cmp(gconstpointer a, gconstpointer b) { return g_utf8_collate(g_app_info_get_display_name(*(GAppInfo **)a), g_app_info_get_display_name(*(GAppInfo **)b)); }

static const char *CSS =
"entry.busca { border-radius: 999px; min-height: 40px; padding: 0 16px; font-size: 15px; border: 1px solid alpha(#fff,.35); color: #fff; caret-color: #fff;"
"  background-image: none; background-color: alpha(#fff,.20); box-shadow: 0 8px 28px alpha(#000,.18), inset 0 1px 0 alpha(#fff,.30);"
"  transition: all 260ms cubic-bezier(.3,1.4,.5,1); }"
"entry.busca:focus { background-color: alpha(#fff,.30); border-color: alpha(#fff,.75); box-shadow: 0 8px 28px alpha(#000,.18), 0 0 0 4px alpha(#fff,.18); }"
"entry.busca image { color: alpha(#fff,.85); }"
"flowbox, flowboxchild { background: none; outline: none; padding: 0; }"
"button.app { border-radius: 20px; padding: 12px 6px 8px 6px; border: none; box-shadow: none; background: none;"
"  transition: background-color 220ms ease, -gtk-icon-transform 300ms cubic-bezier(.3,1.9,.5,1); -gtk-icon-shadow: 0 6px 12px alpha(#000,.28); }"
"button.app:hover { background-color: alpha(#fff,.20); -gtk-icon-transform: scale(1.16) translateY(-3px); }"
"button.app:active { -gtk-icon-transform: scale(.90); background-color: alpha(#fff,.30); }"
".appname { font-size: 13px; font-weight: 600; color: #fff; text-shadow: 0 1px 6px alpha(#000,.55); }"
"menu.appmenu { border-radius: 12px; padding: 6px; font-size: 13.5px; }"
"menu.appmenu menuitem { border-radius: 8px; padding: 7px 14px; min-width: 210px; }"
"menu.appmenu menuitem.perigo { color: @danger; }"
".vazio { font-size: 15px; color: #fff; text-shadow: 0 1px 6px alpha(#000,.5); }";

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    nova_init(CSS); nova_load_blur();
    GdkRectangle g; gdk_monitor_get_geometry(gdk_display_get_monitor(gdk_display_get_default(), 0), &g);
    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Apps"); gtk_window_set_wmclass(GTK_WINDOW(win), "nova-apps", "Nova-apps");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE); gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE); gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(win), g.width, g.height); gtk_window_move(GTK_WINDOW(win), g.x, g.y);
    gtk_window_fullscreen(GTK_WINDOW(win)); gtk_widget_set_app_paintable(win, TRUE);
    gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(win, "draw", G_CALLBACK(draw_bg), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(win, "button-press-event", G_CALLBACK(on_press), NULL);
    g_signal_connect(win, "focus-out-event", G_CALLBACK(on_focus_out), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_container_add(GTK_CONTAINER(win), v);
    search = cls(gtk_search_entry_new(), "busca"); gtk_entry_set_placeholder_text(GTK_ENTRY(search), "Buscar aplicativos");
    gtk_widget_set_size_request(search, 340, -1); gtk_widget_set_halign(search, GTK_ALIGN_CENTER); gtk_widget_set_margin_top(search, (int)(g.height * .075));
    g_signal_connect(search, "search-changed", G_CALLBACK(on_search), NULL); g_signal_connect(search, "activate", G_CALLBACK(on_activate), NULL);
    gtk_box_pack_start(GTK_BOX(v), search, FALSE, FALSE, 0);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_top(sw, 30); gtk_widget_set_margin_bottom(sw, 40);
    flow = gtk_flow_box_new(); gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE); gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 7); gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 7);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 14); gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 6);
    gtk_widget_set_valign(flow, GTK_ALIGN_START); gtk_widget_set_halign(flow, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(flow, (int)(g.width * .80), -1);
    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(flow), filter, NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw), flow); gtk_box_pack_start(GTK_BOX(v), sw, TRUE, TRUE, 0);
    empty = lbl("Nenhum aplicativo com esse nome.", "vazio", .5); gtk_widget_set_no_show_all(empty, TRUE);
    gtk_box_pack_start(GTK_BOX(v), empty, FALSE, FALSE, 0); gtk_box_reorder_child(GTK_BOX(v), empty, 1); gtk_widget_set_margin_top(empty, 40);

    items = g_ptr_array_new();
    GList *all = g_app_info_get_all(); GPtrArray *arr = g_ptr_array_new(); GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
    for (GList *l = all; l; l = l->next) {
        GAppInfo *a = l->data; const char *n = g_app_info_get_display_name(a);
        if (!g_app_info_should_show(a) || !n || g_hash_table_contains(seen, n)) continue;
        const char *id = g_app_info_get_id(a);
        static const char *HIDE[] = { "claude", "url-handler", "nova-apps", "plank", "tint2", "picom", "xterm", "uxterm", "display-im", "openbox", "obconf",
            "nm-connection-editor", "zathura", "mpv", "pcmanfm", "firefox-esr", "blueman-adapters", "lstopo", "htop", "feh", "xsettingsd", "dunst", "clamtk",
            "org.flameshot", "compton", "x11", "debian-", "vim", "nvim", "python", "rofi", "yelp", "im-config", "wine", "winetricks", "udiskie", "gcr-", "pavucontrol-qt", NULL };
        gboolean hide = FALSE; for (int k = 0; id && HIDE[k]; k++) if (g_str_has_prefix(id, HIDE[k])) hide = TRUE;
        if (hide) continue;
        g_hash_table_add(seen, (gpointer)n); g_ptr_array_add(arr, a);
    }
    g_ptr_array_sort(arr, cmp);
    for (guint i = 0; i < arr->len; i++) {
        GAppInfo *a = g_ptr_array_index(arr, i); Item *it = g_new0(Item, 1); it->info = a;
        char *k = g_strconcat(g_app_info_get_display_name(a), " ", g_app_info_get_description(a) ? g_app_info_get_description(a) : "", NULL);
        it->key = g_utf8_casefold(k, -1); g_free(k);
        GtkWidget *b = cls(gtk_button_new(), "app"); gtk_widget_set_can_focus(b, FALSE);
        GtkWidget *bv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        GIcon *ic = g_app_info_get_icon(a);
        GtkWidget *img = ic ? gtk_image_new_from_gicon(ic, GTK_ICON_SIZE_DIALOG) : gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 72);
        GtkWidget *nm = lbl(g_app_info_get_display_name(a), "appname", .5); gtk_label_set_ellipsize(GTK_LABEL(nm), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(nm), 14);
        gtk_box_pack_start(GTK_BOX(bv), img, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(bv), nm, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(b), bv);
        g_signal_connect(b, "clicked", G_CALLBACK(on_click), it);
        g_signal_connect(b, "button-press-event", G_CALLBACK(on_app_press), it);
        gtk_container_add(GTK_CONTAINER(flow), b);
        it->child = gtk_widget_get_parent(b); g_object_set_data(G_OBJECT(it->child), "item", it);
        gtk_widget_set_opacity(it->child, 0); g_ptr_array_add(items, it);
    }
    gtk_widget_show_all(win); gtk_widget_grab_focus(search);
    t0 = g_get_monotonic_time(); gtk_widget_add_tick_callback(win, tick_in, NULL, NULL);
    gtk_window_present(GTK_WINDOW(win));
    gtk_main();
    return 0;
}
