// WhatsApp para o NOVA OS: abre o WhatsApp Web numa janela própria, com sessão salva, notificações e downloads.
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <string.h>

static GtkWidget *win;

static gboolean on_permission(WebKitWebView *v, WebKitPermissionRequest *r, gpointer d) {
    // notificações, microfone e câmera (mensagens de voz e chamadas) são permitidos; o resto, não
    if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(r) || WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(r)) webkit_permission_request_allow(r);
    else webkit_permission_request_deny(r);
    return TRUE;
}
static gboolean on_notify(WebKitWebView *v, WebKitNotification *n, gpointer d) {
    if (gtk_window_is_active(GTK_WINDOW(win))) return TRUE;
    char *argv[] = { "notify-send", "-a", "WhatsApp", "-i", "/usr/share/icons/Papirus/64x64/apps/whatsapp.svg",
                     (char *)webkit_notification_get_title(n), (char *)(webkit_notification_get_body(n) ? webkit_notification_get_body(n) : ""), NULL };
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    gtk_window_set_urgency_hint(GTK_WINDOW(win), TRUE);
    return TRUE;
}
static gboolean on_policy(WebKitWebView *v, WebKitPolicyDecision *dec, WebKitPolicyDecisionType type, gpointer d) {
    if (type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION && type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;
    WebKitNavigationAction *a = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(dec));
    const char *uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(a));
    gboolean interno = uri && (strstr(uri, "whatsapp.com") || strstr(uri, "whatsapp.net") || g_str_has_prefix(uri, "blob:") || g_str_has_prefix(uri, "about:"));
    if (interno && type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;
    if (!interno && uri) {                                   // links de fora abrem no navegador
        gtk_show_uri_on_window(GTK_WINDOW(win), uri, GDK_CURRENT_TIME, NULL); webkit_policy_decision_ignore(dec); return TRUE;
    }
    return FALSE;
}
static gboolean on_dest(WebKitDownload *dl, const char *name, gpointer d) {
    char *dir = g_build_filename(g_get_home_dir(), "Downloads", NULL), *path = g_build_filename(dir, name, NULL);
    g_mkdir_with_parents(dir, 0755);
    for (int i = 1; g_file_test(path, G_FILE_TEST_EXISTS) && i < 100; i++) { g_free(path); char *n = g_strdup_printf("%d-%s", i, name); path = g_build_filename(dir, n, NULL); g_free(n); }
    char *uri = g_filename_to_uri(path, NULL, NULL); webkit_download_set_destination(dl, uri);
    g_free(uri); g_free(path); g_free(dir); return TRUE;
}
static void on_download(WebKitWebContext *c, WebKitDownload *dl, gpointer d) { g_signal_connect(dl, "decide-destination", G_CALLBACK(on_dest), NULL); }
static void on_focus(GtkWidget *w, GdkEvent *e, gpointer d) { gtk_window_set_urgency_hint(GTK_WINDOW(win), FALSE); }

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    char *base = g_build_filename(g_get_user_data_dir(), "nova", "whatsapp", NULL), *cache = g_build_filename(g_get_user_cache_dir(), "nova-whatsapp", NULL);
    WebKitWebsiteDataManager *dm = webkit_website_data_manager_new("base-data-directory", base, "base-cache-directory", cache, NULL);
    WebKitWebContext *ctx = webkit_web_context_new_with_website_data_manager(dm);
    char *cookies = g_build_filename(base, "cookies.sqlite", NULL);
    webkit_cookie_manager_set_persistent_storage(webkit_web_context_get_cookie_manager(ctx), cookies, WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    g_signal_connect(ctx, "download-started", G_CALLBACK(on_download), NULL);

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "WhatsApp"); gtk_window_set_default_size(GTK_WINDOW(win), 1080, 660);
    gtk_window_set_wmclass(GTK_WINDOW(win), "nova-whatsapp", "Nova-whatsapp");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "focus-in-event", G_CALLBACK(on_focus), NULL);

    WebKitWebView *view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(ctx));
    WebKitSettings *st = webkit_web_view_get_settings(view);
    webkit_settings_set_user_agent(st, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36");
    webkit_settings_set_enable_media_stream(st, TRUE);
    webkit_settings_set_enable_smooth_scrolling(st, TRUE);
    webkit_settings_set_hardware_acceleration_policy(st, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    g_signal_connect(view, "permission-request", G_CALLBACK(on_permission), NULL);
    g_signal_connect(view, "show-notification", G_CALLBACK(on_notify), NULL);
    g_signal_connect(view, "decide-policy", G_CALLBACK(on_policy), NULL);
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(view));
    webkit_web_view_load_uri(view, "https://web.whatsapp.com/");
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
