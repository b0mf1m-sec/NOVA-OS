// Este computador (NOVA OS): pastas pessoais e uso do armazenamento de cada disco/pendrive, como no Windows.
// Sem argumentos mostra a janela; com uma pasta/arquivo como argumento abre direto no gerenciador (pcmanfm).
#include "../nova-comum/nova.h"
#include <unistd.h>

static GtkWidget *win, *cards;
static guint refresh_id;

static const char *CSS =
"viewport, scrolledwindow { background-color: transparent; }"
"flowboxchild { padding: 0; background: transparent; }"
".glasspanel { border-radius: 16px; background-color: alpha(@card,.72); border: 1px solid alpha(@fg,.08); box-shadow: 0 1px 4px alpha(#000,.06); }"
"button.disco { background-image: none; background-color: transparent; border: none; box-shadow: none; border-radius: 16px; padding: 14px 14px; }"
"button.disco:hover { background-color: alpha(@fg,.07); }"
".dname { font-size: 15px; font-weight: 600; color: @fg; }"
".dsize { font-size: 12.5px; color: @fg2; }"
".vazio { font-size: 13px; color: @fg2; }";

static void open_path(const char *path) {
    const char *argv[] = { "pcmanfm", path, NULL };
    g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
    gtk_main_quit();
}
static void on_open(GtkWidget *w, gpointer d) { open_path(g_object_get_data(G_OBJECT(w), "caminho")); }

static void msg_box(const char *title, const char *text) {
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", title);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", text); gtk_dialog_run(GTK_DIALOG(dlg)); gtk_widget_destroy(dlg);
}

/* barra de uso */
static gboolean draw_bar(GtkWidget *w, cairo_t *cr, gpointer d) {
    int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
    double f = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "uso")) / 1000.0;
    GdkRGBA fg, c; nova_color(w, "fg", &fg); nova_color(w, f >= .92 ? "danger" : f >= .80 ? "warn" : "accent", &c);
    nova_rrect(cr, 0, 0, W, H, H / 2.0); cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, .12); cairo_fill(cr);
    if (f > 0) { double fw = MAX(H, W * f); nova_rrect(cr, 0, 0, fw, H, H / 2.0); gdk_cairo_set_source_rgba(cr, &c); cairo_fill(cr); }
    return TRUE;
}

static void done_op(GObject *src, GAsyncResult *res, gpointer ejetar) {
    GError *e = NULL; gboolean ok = ejetar ? g_mount_eject_with_operation_finish(G_MOUNT(src), res, &e) : g_mount_unmount_with_operation_finish(G_MOUNT(src), res, &e);
    if (!ok) msg_box("Não foi possível remover com segurança", "Feche os arquivos e janelas que estão usando este disco e tente de novo.");
    if (e) g_error_free(e);
}
static void on_eject(GtkWidget *w, gpointer d) {
    GMount *m = g_object_get_data(G_OBJECT(w), "mount");
    if (g_mount_can_eject(m)) g_mount_eject_with_operation(m, G_MOUNT_UNMOUNT_NONE, NULL, NULL, done_op, GINT_TO_POINTER(1));
    else g_mount_unmount_with_operation(m, G_MOUNT_UNMOUNT_NONE, NULL, NULL, done_op, NULL);
}
static void done_mount(GObject *src, GAsyncResult *res, gpointer d) {
    GError *e = NULL; if (!g_volume_mount_finish(G_VOLUME(src), res, &e)) msg_box("Não foi possível abrir o disco", "Tire e coloque o pendrive de novo. Se continuar, ele pode estar com defeito.");
    if (e) g_error_free(e);
}
static void on_mount(GtkWidget *w, gpointer d) { g_volume_mount(g_object_get_data(G_OBJECT(w), "volume"), G_MOUNT_MOUNT_NONE, NULL, NULL, done_mount, NULL); }

/* um cartão de disco */
static void add_card(GIcon *icon, const char *iconname, const char *name, const char *path, gboolean removable, GMount *mount, GVolume *vol) {
    guint64 total = 0, livre = 0;
    if (path) {
        GFile *f = g_file_new_for_path(path); GFileInfo *in = g_file_query_filesystem_info(f, "filesystem::size,filesystem::free", NULL, NULL);
        if (in) { total = g_file_info_get_attribute_uint64(in, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE); livre = g_file_info_get_attribute_uint64(in, G_FILE_ATTRIBUTE_FILESYSTEM_FREE); g_object_unref(in); }
        g_object_unref(f);
    }
    GtkWidget *card = cls(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), "glasspanel");
    GtkWidget *b = cls(gtk_button_new(), "disco"); gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE); gtk_widget_set_can_focus(b, TRUE);
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *img = icon ? gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DND) : gtk_image_new_from_icon_name(iconname, GTK_ICON_SIZE_DND);
    gtk_image_set_pixel_size(GTK_IMAGE(img), 44); gtk_widget_set_valign(img, GTK_ALIGN_CENTER); gtk_box_pack_start(GTK_BOX(h), img, FALSE, FALSE, 0);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_set_valign(v, GTK_ALIGN_CENTER);
    GtkWidget *nm = lbl(name, "dname", 0); gtk_label_set_ellipsize(GTK_LABEL(nm), PANGO_ELLIPSIZE_END); gtk_box_pack_start(GTK_BOX(v), nm, FALSE, FALSE, 0);
    char *t;
    if (total > 0) {
        GtkWidget *bar = gtk_drawing_area_new(); gtk_widget_set_size_request(bar, -1, 10);
        g_object_set_data(G_OBJECT(bar), "uso", GINT_TO_POINTER((int)(1000.0 * (total - livre) / total)));
        g_signal_connect(bar, "draw", G_CALLBACK(draw_bar), NULL); gtk_box_pack_start(GTK_BOX(v), bar, FALSE, FALSE, 0);
        char *a = g_format_size(livre), *c = g_format_size(total); t = g_strdup_printf("%s livres de %s", a, c); g_free(a); g_free(c);
    } else t = g_strdup(path ? "Sem informação de espaço" : "Disco não aberto. Clique para abrir.");
    gtk_box_pack_start(GTK_BOX(v), lbl(t, "dsize", 0), FALSE, FALSE, 0); g_free(t);
    gtk_box_pack_start(GTK_BOX(h), v, TRUE, TRUE, 0); gtk_container_add(GTK_CONTAINER(b), h);
    if (path) { g_object_set_data_full(G_OBJECT(b), "caminho", g_strdup(path), g_free); g_signal_connect(b, "clicked", G_CALLBACK(on_open), NULL); }
    else { g_object_set_data(G_OBJECT(b), "volume", vol); g_signal_connect(b, "clicked", G_CALLBACK(on_mount), NULL); }
    gtk_box_pack_start(GTK_BOX(card), b, TRUE, TRUE, 0);
    if (mount && removable && (g_mount_can_eject(mount) || g_mount_can_unmount(mount))) {
        GtkWidget *e = cls(cls(gtk_button_new_with_label("Ejetar"), "pill"), "soft"); gtk_widget_set_valign(e, GTK_ALIGN_CENTER); gtk_widget_set_margin_end(e, 14);
        g_object_set_data(G_OBJECT(e), "mount", mount); g_signal_connect(e, "clicked", G_CALLBACK(on_eject), NULL); gtk_box_pack_end(GTK_BOX(card), e, FALSE, FALSE, 0);
    }
    gtk_widget_set_hexpand(card, TRUE); gtk_container_add(GTK_CONTAINER(cards), card);
}

static gboolean escondido(const char *name, const char *path) {   // partições técnicas (EFI) não interessam
    return (name && (strstr(name, "VTOYEFI") || !g_ascii_strcasecmp(name, "EFI") || !g_ascii_strcasecmp(name, "ESP"))) || (path && g_str_has_prefix(path, "/boot"));
}
static gboolean build(gpointer d) {
    refresh_id = 0;
    gtk_container_foreach(GTK_CONTAINER(cards), (GtkCallback)gtk_widget_destroy, NULL);
    add_card(NULL, "drive-harddisk", "Disco do computador", "/", FALSE, NULL, NULL);
    GVolumeMonitor *vm = g_volume_monitor_get();
    GList *l = g_volume_monitor_get_mounts(vm);
    for (GList *p = l; p; p = p->next) {
        GMount *m = p->data; GFile *r = g_mount_get_root(m); char *path = g_file_get_path(r), *name = g_mount_get_name(m);
        GDrive *dr = g_mount_get_drive(m); gboolean rem = dr ? g_drive_is_removable(dr) || g_drive_can_eject(dr) : FALSE; GIcon *ic = g_mount_get_icon(m);
        if (path && strcmp(path, "/") && !escondido(name, path)) {
            char *n = rem ? g_strdup_printf("%s (pendrive/disco externo)", name) : g_strdup(name);
            add_card(NULL, rem ? "drive-removable-media" : "drive-harddisk", n, path, rem, m, NULL); g_free(n);
        }
        if (ic) g_object_unref(ic); if (dr) g_object_unref(dr); g_free(path); g_free(name); g_object_unref(r);
    }
    g_list_free_full(l, g_object_unref);
    l = g_volume_monitor_get_volumes(vm);
    for (GList *p = l; p; p = p->next) {
        GVolume *v = p->data; GMount *m = g_volume_get_mount(v);
        if (m) { g_object_unref(m); continue; }
        char *name = g_volume_get_name(v);
        if (g_volume_can_mount(v) && !escondido(name, NULL)) { GIcon *ic = g_volume_get_icon(v); add_card(NULL, "drive-removable-media", name, NULL, TRUE, NULL, v); if (ic) g_object_unref(ic); }
        g_free(name);
    }
    g_list_free_full(l, g_object_unref); g_object_unref(vm);
    gtk_widget_show_all(cards);
    return refresh_id ? G_SOURCE_REMOVE : G_SOURCE_REMOVE;
}
static void schedule_refresh(void) { if (!refresh_id) refresh_id = g_timeout_add(350, build, NULL); }
static void on_change(GVolumeMonitor *vm, gpointer obj, gpointer d) { schedule_refresh(); }
static gboolean tick(gpointer d) { schedule_refresh(); return G_SOURCE_CONTINUE; }

static void add_folder(GtkWidget *fb, const char *label, const char *path) {
    if (!path || !g_file_test(path, G_FILE_TEST_IS_DIR)) return;
    GtkWidget *b = cls(cls(gtk_button_new_with_label(label), "pill"), "plain"); g_object_set_data_full(G_OBJECT(b), "caminho", g_strdup(path), g_free);
    g_signal_connect(b, "clicked", G_CALLBACK(on_open), NULL); gtk_flow_box_insert(GTK_FLOW_BOX(fb), b, -1);
}

int main(int argc, char **argv) {
    if (argc > 1) {                                   // recebeu uma pasta/arquivo: abre direto no gerenciador
        char **a = g_new0(char *, argc + 1); a[0] = (char *)"pcmanfm"; for (int i = 1; i < argc; i++) a[i] = argv[i];
        execvp("pcmanfm", a); return 1;
    }
    gtk_init(&argc, &argv);
    nova_init(CSS); nova_load_blur();
    win = nova_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Este computador"); gtk_window_set_default_size(GTK_WINDOW(win), 860, 600);
    gtk_window_set_wmclass(GTK_WINDOW(win), "pcmanfm", "Pcmanfm");     // agrupa no dock junto com o ícone de Arquivos
    gtk_widget_set_app_paintable(win, TRUE);
    g_signal_connect(win, "draw", G_CALLBACK(nova_glass), NULL);
    g_signal_connect(win, "configure-event", G_CALLBACK(nova_redraw_on_move), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC); gtk_container_add(GTK_CONTAINER(win), sw);
    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(v, 30); gtk_widget_set_margin_end(v, 30); gtk_widget_set_margin_top(v, 24); gtk_widget_set_margin_bottom(v, 26); gtk_container_add(GTK_CONTAINER(sw), v);
    gtk_box_pack_start(GTK_BOX(v), lbl("Este computador", "h1", 0), FALSE, FALSE, 0);
    GtkWidget *s = lbl("Suas pastas e o espaço usado em cada disco e pendrive.", "sub", 0); gtk_widget_set_margin_bottom(s, 18); gtk_box_pack_start(GTK_BOX(v), s, FALSE, FALSE, 0);

    GtkWidget *c = lbl("PASTAS", "cap", 0); gtk_widget_set_margin_bottom(c, 8); gtk_box_pack_start(GTK_BOX(v), c, FALSE, FALSE, 0);
    GtkWidget *fb = gtk_flow_box_new(); gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb), GTK_SELECTION_NONE); gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb), 8); gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb), 8);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb), FALSE); gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb), 8); gtk_widget_set_margin_bottom(fb, 22);
    add_folder(fb, "Pasta pessoal", g_get_home_dir());
    add_folder(fb, "Área de trabalho", g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP)); add_folder(fb, "Documentos", g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS));
    add_folder(fb, "Downloads", g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD)); add_folder(fb, "Imagens", g_get_user_special_dir(G_USER_DIRECTORY_PICTURES));
    add_folder(fb, "Músicas", g_get_user_special_dir(G_USER_DIRECTORY_MUSIC)); add_folder(fb, "Vídeos", g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS));
    gtk_box_pack_start(GTK_BOX(v), fb, FALSE, FALSE, 0);

    c = lbl("DISCOS E PENDRIVES", "cap", 0); gtk_widget_set_margin_bottom(c, 8); gtk_box_pack_start(GTK_BOX(v), c, FALSE, FALSE, 0);
    cards = gtk_flow_box_new(); gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(cards), GTK_SELECTION_NONE); gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(cards), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(cards), 1); gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(cards), 2);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(cards), 12); gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(cards), 12);
    gtk_box_pack_start(GTK_BOX(v), cards, FALSE, FALSE, 0);
    GtkWidget *tip = lbl("Ligou um pendrive e ele não apareceu? Espere alguns segundos. A lista se atualiza sozinha.", "sub", 0); gtk_widget_set_margin_top(tip, 16); gtk_box_pack_start(GTK_BOX(v), tip, FALSE, FALSE, 0);

    GVolumeMonitor *vm = g_volume_monitor_get();
    const char *sig[] = { "mount-added", "mount-removed", "mount-changed", "volume-added", "volume-removed", "volume-changed" };
    for (int i = 0; i < 6; i++) g_signal_connect(vm, sig[i], G_CALLBACK(on_change), NULL);
    build(NULL); g_timeout_add_seconds(10, tick, NULL);
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
