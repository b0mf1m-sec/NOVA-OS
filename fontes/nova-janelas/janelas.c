// nova-janelas: Alt+Tab com miniaturas e pré-visualização das janelas ao passar o mouse no dock (GTK3 + Xlib + XComposite).
//   nova-janelas            -> fica rodando em segundo plano (iniciado por nova-sessao)
//   kill -USR1/-USR2 PID    -> Alt+Tab para frente/trás (o atalho chama o script nova-alttab)
#include "../nova-comum/nova.h"
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>
#include <glib-unix.h>
#include <gio/gdesktopappinfo.h>

typedef struct { Window id; char *title, *cls, *inst; cairo_surface_t *icon, *thumb; gboolean mini; } Win;
typedef struct { char *wmclass, *exec; } Item;           // item do dock (lançador)

static Display *dpy; static GdkDisplay *gd; static Window root; static int SW, SH;
static GPtrArray *wins;                                   // janelas mostradas no popup
static enum { M_NONE, M_ALT, M_DOCK } mode = M_NONE;
static GtkWidget *pop, *area; static int sel, hov = -1, rows_n, per_row, TW, TH;
static int pop_x, pop_y, pop_w, pop_h, dock_item = -1;
static GHashTable *icons;                                 // classe minúscula -> GIcon
static guint hide_t, show_t, refresh_t;
static gboolean grabbed;

static Atom A(const char *n) { return XInternAtom(dpy, n, False); }
static void *prop(Window w, Atom p, Atom type, gulong *n) {
    Atom t; int f; gulong b; unsigned char *d = NULL;
    gdk_x11_display_error_trap_push(gd);
    int r = XGetWindowProperty(dpy, w, p, 0, 1 << 20, False, type, &t, &f, n, &b, &d);
    if (gdk_x11_display_error_trap_pop(gd) || r != Success || !d) { *n = 0; return NULL; }
    if (!*n) { XFree(d); return NULL; }
    return d;
}
static gboolean has_atom(Window w, Atom p, Atom v) {
    gulong n; Atom *l = prop(w, p, XA_ATOM, &n); gboolean r = FALSE;
    for (gulong i = 0; l && i < n; i++) if (l[i] == v) r = TRUE;
    if (l) XFree(l); return r;
}
static Window frame_of(Window w) {
    for (int i = 0; i < 6; i++) {
        Window r, p, *c; unsigned nc;
        gdk_x11_display_error_trap_push(gd);
        int ok = XQueryTree(dpy, w, &r, &p, &c, &nc); if (ok && c) XFree(c);
        if (gdk_x11_display_error_trap_pop(gd) || !ok) return w;
        if (p == root || !p) return w; w = p;
    }
    return w;
}

/* ---------- ícones e classes ---------- */
static void load_icons(void) {
    icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    for (GList *l = g_app_info_get_all(); l; l = l->next) {
        GAppInfo *ai = l->data; GIcon *ic = g_app_info_get_icon(ai); if (!ic || !G_IS_DESKTOP_APP_INFO(ai)) continue;
        const char *wc = g_desktop_app_info_get_startup_wm_class(G_DESKTOP_APP_INFO(ai));
        if (wc) g_hash_table_insert(icons, g_ascii_strdown(wc, -1), g_object_ref(ic));
        const char *ex = g_app_info_get_executable(ai);
        if (ex) { char *b = g_path_get_basename(ex), *k = g_ascii_strdown(b, -1); g_free(b);
            if (!g_hash_table_contains(icons, k)) g_hash_table_insert(icons, k, g_object_ref(ic)); else g_free(k); }
    }
}
static cairo_surface_t *icon_from_net(Window w) {
    gulong n; unsigned long *d = prop(w, A("_NET_WM_ICON"), XA_CARDINAL, &n); if (!d) return NULL;
    unsigned long *best = NULL; int bw = 0, bh = 0;
    for (gulong i = 0; i + 2 < n;) {
        int iw = d[i], ih = d[i + 1]; if (iw <= 0 || ih <= 0 || i + 2 + (gulong)iw * ih > n) break;
        if (!best || (bw < 48 && iw > bw) || (iw >= 48 && iw < bw)) { best = d + i + 2; bw = iw; bh = ih; }
        i += 2 + (gulong)iw * ih;
    }
    cairo_surface_t *s = NULL;
    if (best) {
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bw, bh);
        uint32_t *o = (uint32_t *)cairo_image_surface_get_data(s); int st = cairo_image_surface_get_stride(s) / 4;
        for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) {
            uint32_t p = best[y * bw + x], a = p >> 24, r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            o[y * st + x] = (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
        }
        cairo_surface_mark_dirty(s);
    }
    XFree(d); return s;
}
static cairo_surface_t *icon_theme(Win *w, int size) {
    GtkIconTheme *th = gtk_icon_theme_get_default(); GIcon *gi = NULL; char *k;
    k = g_ascii_strdown(w->inst ? w->inst : "", -1); gi = g_hash_table_lookup(icons, k); g_free(k);
    if (!gi) { k = g_ascii_strdown(w->cls ? w->cls : "", -1); gi = g_hash_table_lookup(icons, k); g_free(k); }
    GdkPixbuf *pb = NULL;
    if (gi) { GtkIconInfo *ii = gtk_icon_theme_lookup_by_gicon(th, gi, size, GTK_ICON_LOOKUP_FORCE_SIZE); if (ii) { pb = gtk_icon_info_load_icon(ii, NULL); g_object_unref(ii); } }
    if (!pb && w->cls) { k = g_ascii_strdown(w->cls, -1); pb = gtk_icon_theme_load_icon(th, k, size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL); g_free(k); }
    if (!pb) pb = gtk_icon_theme_load_icon(th, "application-x-executable", size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    if (!pb) return NULL;
    cairo_surface_t *s = gdk_cairo_surface_create_from_pixbuf(pb, 1, NULL); g_object_unref(pb); return s;
}

/* ---------- janelas ---------- */
static void win_free(gpointer p) {
    Win *w = p; g_free(w->title); g_free(w->cls); g_free(w->inst);
    if (w->icon) cairo_surface_destroy(w->icon); if (w->thumb) cairo_surface_destroy(w->thumb); g_free(w);
}
static Win *win_new(Window id) {
    Win *w = g_new0(Win, 1); w->id = id; gulong n; char *t;
    t = prop(id, A("_NET_WM_NAME"), A("UTF8_STRING"), &n);
    if (!t) t = prop(id, XA_WM_NAME, AnyPropertyType, &n);
    w->title = t ? g_strndup(t, n) : g_strdup("(sem título)"); if (t) XFree(t);
    XClassHint ch; gdk_x11_display_error_trap_push(gd);
    if (XGetClassHint(dpy, id, &ch)) { w->inst = g_strdup(ch.res_name); w->cls = g_strdup(ch.res_class); XFree(ch.res_name); XFree(ch.res_class); }
    gdk_x11_display_error_trap_pop(gd);
    w->mini = has_atom(id, A("_NET_WM_STATE"), A("_NET_WM_STATE_HIDDEN"));
    w->icon = icon_from_net(id); if (!w->icon) w->icon = icon_theme(w, 48);
    return w;
}
static gboolean is_listed(Window id) {
    if (has_atom(id, A("_NET_WM_STATE"), A("_NET_WM_STATE_SKIP_TASKBAR"))) return FALSE;
    gulong n; Atom *ty = prop(id, A("_NET_WM_WINDOW_TYPE"), XA_ATOM, &n); gboolean ok = TRUE;
    if (ty) { ok = FALSE; for (gulong i = 0; i < n; i++)
        if (ty[i] == A("_NET_WM_WINDOW_TYPE_NORMAL") || ty[i] == A("_NET_WM_WINDOW_TYPE_DIALOG")) ok = TRUE; XFree(ty); }
    return ok;
}
// janelas em ordem de uso recente (a de cima primeiro); filtro opcional por item do dock
static GPtrArray *list_wins(Item *only) {
    GPtrArray *r = g_ptr_array_new_with_free_func(win_free); gulong n;
    Window *l = prop(root, A("_NET_CLIENT_LIST_STACKING"), XA_WINDOW, &n);
    for (long i = (long)n - 1; l && i >= 0; i--) {
        if (!is_listed(l[i])) continue;
        Win *w = win_new(l[i]);
        if (only) { char *a = g_ascii_strdown(only->wmclass ? only->wmclass : "", -1), *i1 = g_ascii_strdown(w->inst ? w->inst : "-", -1), *c1 = g_ascii_strdown(w->cls ? w->cls : "-", -1);
            gboolean m = (*a && (!strcmp(a, i1) || !strcmp(a, c1))) || (only->exec && (!strcmp(only->exec, i1) || !strcmp(only->exec, c1)));
            g_free(a); g_free(i1); g_free(c1); if (!m) { win_free(w); continue; } }
        g_ptr_array_add(r, w);
    }
    if (l) XFree(l); return r;
}
static void capture(Win *w) {
    Window fr = frame_of(w->id); XWindowAttributes at;
    gdk_x11_display_error_trap_push(gd);
    if (!XGetWindowAttributes(dpy, fr, &at) || at.map_state != IsViewable || at.width < 8 || at.height < 8) { gdk_x11_display_error_trap_pop_ignored(gd); return; }
    Pixmap pm = XCompositeNameWindowPixmap(dpy, fr);
    if (!pm) { gdk_x11_display_error_trap_pop_ignored(gd); return; }
    cairo_surface_t *src = cairo_xlib_surface_create(dpy, pm, at.visual, at.width, at.height);
    double sc = MIN(1.0, 340.0 / at.width); int ow = at.width * sc, oh = at.height * sc;
    cairo_surface_t *dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ow, oh); cairo_t *c = cairo_create(dst);
    cairo_scale(c, sc, sc); cairo_set_source_surface(c, src, 0, 0); cairo_pattern_set_filter(cairo_get_source(c), CAIRO_FILTER_GOOD); cairo_paint(c);
    cairo_destroy(c); cairo_surface_destroy(src); XFreePixmap(dpy, pm); XSync(dpy, False);
    if (gdk_x11_display_error_trap_pop(gd)) { cairo_surface_destroy(dst); return; }
    if (w->thumb) cairo_surface_destroy(w->thumb); w->thumb = dst;
}
static void activate(Window id) {
    XEvent e = { 0 }; e.xclient.type = ClientMessage; e.xclient.window = id; e.xclient.message_type = A("_NET_ACTIVE_WINDOW");
    e.xclient.format = 32; e.xclient.data.l[0] = 2; e.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &e); XFlush(dpy);
}
static void close_win(Window id) {
    XEvent e = { 0 }; e.xclient.type = ClientMessage; e.xclient.window = id; e.xclient.message_type = A("_NET_CLOSE_WINDOW");
    e.xclient.format = 32; e.xclient.data.l[0] = CurrentTime; e.xclient.data.l[1] = 2;
    XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &e); XFlush(dpy);
}

/* ---------- dock: itens e posição ---------- */
static GPtrArray *pinned; static gint64 pinned_t;
static void item_free(gpointer p) { Item *i = p; g_free(i->wmclass); g_free(i->exec); g_free(i); }
static void load_pinned(void) {
    if (pinned && g_get_monotonic_time() - pinned_t < 2000000) return;
    if (pinned) g_ptr_array_free(pinned, TRUE);
    pinned = g_ptr_array_new_with_free_func(item_free); pinned_t = g_get_monotonic_time();
    char *o = nova_run("dconf read /net/launchpad/plank/docks/dock1/dock-items");
    GMatchInfo *mi; GRegex *re = g_regex_new("'([^']+)'", 0, 0, NULL);
    g_regex_match(re, o, 0, &mi);
    while (g_match_info_matches(mi)) {
        char *name = g_match_info_fetch(mi, 1), *f = g_strdup_printf("%s/.config/plank/dock1/launchers/%s", g_get_home_dir(), name);
        GKeyFile *kf = g_key_file_new(); Item *it = g_new0(Item, 1);
        if (g_key_file_load_from_file(kf, f, 0, NULL)) {
            char *u = g_key_file_get_string(kf, "PlankDockItemPreferences", "Launcher", NULL);
            if (u && g_str_has_prefix(u, "file://")) {
                GDesktopAppInfo *ai = g_desktop_app_info_new_from_filename(u + 7);
                if (ai) {
                    const char *wc = g_desktop_app_info_get_startup_wm_class(ai); it->wmclass = wc ? g_strdup(wc) : NULL;
                    const char *ex = g_app_info_get_executable(G_APP_INFO(ai));
                    if (ex) { char *b = g_path_get_basename(ex); it->exec = g_ascii_strdown(b, -1); g_free(b); }
                    g_object_unref(ai);
                }
            } g_free(u);
        }
        g_ptr_array_add(pinned, it); g_key_file_free(kf); g_free(f); g_free(name); g_match_info_next(mi, NULL);
    }
    g_match_info_free(mi); g_regex_unref(re); g_free(o);
}
static gboolean item_has_win(Item *it, Win *w) {
    char *a = g_ascii_strdown(it->wmclass ? it->wmclass : "", -1), *i1 = g_ascii_strdown(w->inst ? w->inst : "-", -1), *c1 = g_ascii_strdown(w->cls ? w->cls : "-", -1);
    gboolean m = (*a && (!strcmp(a, i1) || !strcmp(a, c1))) || (it->exec && (!strcmp(it->exec, i1) || !strcmp(it->exec, c1)));
    g_free(a); g_free(i1); g_free(c1); return m;
}
static GPtrArray *items;
static void build_items(void) {
    load_pinned(); if (items) g_ptr_array_free(items, TRUE);
    items = g_ptr_array_new();                                // não é dono dos itens fixados
    for (guint i = 0; i < pinned->len; i++) g_ptr_array_add(items, pinned->pdata[i]);
    static GPtrArray *extra; if (extra) g_ptr_array_free(extra, TRUE); extra = g_ptr_array_new_with_free_func(item_free);
    gulong n; Window *l = prop(root, A("_NET_CLIENT_LIST"), XA_WINDOW, &n);
    for (gulong k = 0; l && k < n; k++) {
        if (!is_listed(l[k])) continue; Win *w = win_new(l[k]); gboolean seen = FALSE;
        for (guint i = 0; i < items->len && !seen; i++) seen = item_has_win(items->pdata[i], w);
        char *a = g_ascii_strdown(w->inst ? w->inst : "-", -1), *b = g_ascii_strdown(w->cls ? w->cls : "-", -1);
        if (!seen && (g_hash_table_contains(icons, a) || g_hash_table_contains(icons, b))) {
            Item *it = g_new0(Item, 1); it->wmclass = g_strdup(w->cls); g_ptr_array_add(extra, it); g_ptr_array_add(items, it); }
        g_free(a); g_free(b); win_free(w);
    }
    if (l) XFree(l);
}
static Window dock_win;
static gboolean find_dock(int *x, int *y, int *w, int *h) {
    Window r, p, *c; unsigned nc; int best = 0;
    if (dock_win) { XWindowAttributes at; gdk_x11_display_error_trap_push(gd);
        int ok = XGetWindowAttributes(dpy, dock_win, &at); if (gdk_x11_display_error_trap_pop(gd) || !ok || at.map_state != IsViewable) dock_win = 0;
        else { *x = at.x; *y = at.y; *w = at.width; *h = at.height; return TRUE; } }
    if (!XQueryTree(dpy, root, &r, &p, &c, &nc)) return FALSE;
    for (unsigned i = 0; i < nc; i++) {
        XClassHint ch; gdk_x11_display_error_trap_push(gd); int ok = XGetClassHint(dpy, c[i], &ch);
        if (gdk_x11_display_error_trap_pop(gd) || !ok) continue;
        if (!strcmp(ch.res_class, "Plank")) { XWindowAttributes at; XGetWindowAttributes(dpy, c[i], &at);
            if (at.map_state == IsViewable && at.width * at.height > best) { best = at.width * at.height; dock_win = c[i]; *x = at.x; *y = at.y; *w = at.width; *h = at.height; } }
        XFree(ch.res_name); XFree(ch.res_class);
    }
    if (c) XFree(c); return best > 0;
}

/* ---------- popup ---------- */
static void rrect(cairo_t *c, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(c); cairo_arc(c, x + w - r, y + r, r, -M_PI / 2, 0); cairo_arc(c, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(c, x + r, y + h - r, r, M_PI / 2, M_PI); cairo_arc(c, x + r, y + r, r, M_PI, 3 * M_PI / 2); cairo_close_path(c);
}
#define PAD 14
#define GAP 10
#define LABEL 26
static int TILE_TH(void) { return TW * 62 / 100; }
static void tile_pos(int i, int *x, int *y) { *x = PAD + (i % per_row) * (TW + GAP); *y = PAD + (i / per_row) * (TILE_TH() + LABEL + GAP); }
static gboolean on_draw(GtkWidget *w, cairo_t *c, gpointer d) {
    GdkRGBA bg, fg, ac; nova_color(w, "bg", &bg); nova_color(w, "fg", &fg); nova_color(w, "accent", &ac);
    cairo_set_operator(c, CAIRO_OPERATOR_SOURCE); cairo_set_source_rgba(c, 0, 0, 0, 0); cairo_paint(c); cairo_set_operator(c, CAIRO_OPERATOR_OVER);
    rrect(c, .5, .5, pop_w - 1, pop_h - 1, 22); cairo_set_source_rgba(c, bg.red, bg.green, bg.blue, nova_leve ? .96 : .86); cairo_fill_preserve(c);
    cairo_set_source_rgba(c, fg.red, fg.green, fg.blue, .16); cairo_set_line_width(c, 1); cairo_stroke(c);
    int th = TILE_TH();
    for (guint i = 0; i < wins->len; i++) {
        Win *wn = wins->pdata[i]; int x, y; tile_pos(i, &x, &y);
        gboolean on = (int)i == sel || (int)i == hov;
        rrect(c, x, y, TW, th + LABEL, 14);
        if ((int)i == sel) cairo_set_source_rgba(c, ac.red, ac.green, ac.blue, .30); else if (on) cairo_set_source_rgba(c, fg.red, fg.green, fg.blue, .12); else cairo_set_source_rgba(c, fg.red, fg.green, fg.blue, .05);
        cairo_fill(c);
        if ((int)i == sel) { rrect(c, x + .5, y + .5, TW - 1, th + LABEL - 1, 14); cairo_set_source_rgba(c, ac.red, ac.green, ac.blue, .95); cairo_set_line_width(c, 2); cairo_stroke(c); }
        // miniatura (ou ícone grande se minimizada/sem imagem)
        int bx = x + 8, by = y + 8, bw = TW - 16, bh = th - 8;
        cairo_save(c); rrect(c, bx, by, bw, bh, 8); cairo_clip(c);
        cairo_set_source_rgba(c, 0, 0, 0, .28); cairo_paint(c);
        if (wn->thumb) {
            double iw = cairo_image_surface_get_width(wn->thumb), ih = cairo_image_surface_get_height(wn->thumb), s = MIN(bw / iw, bh / ih);
            cairo_translate(c, bx + (bw - iw * s) / 2, by + (bh - ih * s) / 2); cairo_scale(c, s, s);
            cairo_set_source_surface(c, wn->thumb, 0, 0); cairo_pattern_set_filter(cairo_get_source(c), CAIRO_FILTER_GOOD); cairo_paint(c);
        } else if (wn->icon) {
            double iw = cairo_image_surface_get_width(wn->icon), ih = cairo_image_surface_get_height(wn->icon), s = MIN(56.0 / iw, 56.0 / ih);
            cairo_translate(c, bx + (bw - iw * s) / 2, by + (bh - ih * s) / 2); cairo_scale(c, s, s);
            cairo_set_source_surface(c, wn->icon, 0, 0); cairo_paint_with_alpha(c, wn->mini ? .6 : 1);
        }
        cairo_restore(c);
        // título com o ícone pequeno
        int tx = x + 10;
        if (wn->icon && wn->thumb) {
            cairo_save(c); double iw = cairo_image_surface_get_width(wn->icon), s = 16.0 / iw;
            cairo_translate(c, tx, y + th + 5); cairo_scale(c, s, s); cairo_set_source_surface(c, wn->icon, 0, 0); cairo_paint(c); cairo_restore(c); tx += 22;
        }
        PangoLayout *pl = pango_cairo_create_layout(c); PangoFontDescription *fd = pango_font_description_from_string("Inter 9.5");
        pango_layout_set_font_description(pl, fd); pango_layout_set_width(pl, (x + TW - 10 - tx) * PANGO_SCALE); pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_END);
        pango_layout_set_text(pl, wn->title, -1); cairo_move_to(c, tx, y + th + 6); cairo_set_source_rgba(c, fg.red, fg.green, fg.blue, .95);
        pango_cairo_show_layout(c, pl); pango_font_description_free(fd); g_object_unref(pl);
    }
    return TRUE;
}
static void popup_hide(void) {
    if (show_t) { g_source_remove(show_t); show_t = 0; }
    if (hide_t) { g_source_remove(hide_t); hide_t = 0; }
    if (refresh_t) { g_source_remove(refresh_t); refresh_t = 0; }
    if (grabbed) { gdk_seat_ungrab(gdk_display_get_default_seat(gd)); grabbed = FALSE; }
    if (pop) gtk_widget_hide(pop);
    mode = M_NONE; sel = 0; hov = -1; dock_item = -1;
    if (wins) { g_ptr_array_free(wins, TRUE); wins = NULL; }
}
static void layout(int anchor_x, int bottom_y) {
    int n = wins->len;
    if (mode == M_ALT) { TW = 200; per_row = MAX(1, (SW - 100 + GAP) / (TW + GAP)); if (n < per_row) { per_row = n; }
        if (n > per_row) { /* várias linhas */ } else { int avail = (SW - 100 - (n - 1) * GAP) / n; TW = CLAMP(avail, 120, 200); } }
    else { TW = 190; per_row = MAX(1, MIN(n, (SW - 60 + GAP) / (TW + GAP))); }
    rows_n = (n + per_row - 1) / per_row; int th = TILE_TH();
    pop_w = per_row * TW + (per_row - 1) * GAP + 2 * PAD; pop_h = rows_n * (th + LABEL) + (rows_n - 1) * GAP + 2 * PAD;
    if (mode == M_ALT) { pop_x = (SW - pop_w) / 2; pop_y = (SH - pop_h) / 2; }
    else { pop_x = CLAMP(anchor_x - pop_w / 2, 8, SW - pop_w - 8); pop_y = bottom_y - pop_h; }
}
static void build_popup(void) {
    if (!pop) {
        pop = gtk_window_new(GTK_WINDOW_POPUP); area = gtk_drawing_area_new(); gtk_container_add(GTK_CONTAINER(pop), area);
        GdkScreen *sc = gdk_screen_get_default(); GdkVisual *v = gdk_screen_get_rgba_visual(sc); if (v) gtk_widget_set_visual(pop, v);
        gtk_widget_set_app_paintable(pop, TRUE);
        gtk_widget_add_events(pop, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(area, "draw", G_CALLBACK(on_draw), NULL);
    }
    gtk_widget_set_size_request(area, pop_w, pop_h); gtk_window_resize(GTK_WINDOW(pop), pop_w, pop_h); gtk_window_move(GTK_WINDOW(pop), pop_x, pop_y);
    gtk_widget_show_all(pop); gtk_widget_queue_draw(area);
}
static int tile_at(double mx, double my) {
    for (guint i = 0; i < wins->len; i++) { int x, y; tile_pos(i, &x, &y);
        if (mx >= x && mx < x + TW && my >= y && my < y + TILE_TH() + LABEL) return i; }
    return -1;
}

/* ---------- Alt+Tab ---------- */
static gboolean alt_held(void) {
    Window r, c; int a, b, d, e; unsigned m; XQueryPointer(dpy, root, &r, &c, &a, &b, &d, &e, &m); return (m & Mod1Mask) != 0;
}
static void alt_commit(gboolean ok) {
    Window id = (ok && wins && sel >= 0 && sel < (int)wins->len) ? ((Win *)wins->pdata[sel])->id : 0;
    popup_hide(); if (id) activate(id);
}
static gboolean alt_check(gpointer d) { if (mode == M_ALT && !alt_held()) alt_commit(TRUE); return mode == M_ALT; }
static gboolean on_key(GtkWidget *w, GdkEventKey *e, gpointer d) {
    if (mode != M_ALT) return FALSE;
    int n = wins->len;
    if (e->type == GDK_KEY_RELEASE) { if (e->keyval == GDK_KEY_Alt_L || e->keyval == GDK_KEY_Alt_R || e->keyval == GDK_KEY_Meta_L) alt_commit(TRUE); return TRUE; }
    switch (e->keyval) {
        case GDK_KEY_Tab: sel = (e->state & GDK_SHIFT_MASK) ? (sel + n - 1) % n : (sel + 1) % n; break;
        case GDK_KEY_ISO_Left_Tab: case GDK_KEY_Left: case GDK_KEY_Up: sel = (sel + n - 1) % n; break;
        case GDK_KEY_Right: case GDK_KEY_Down: sel = (sel + 1) % n; break;
        case GDK_KEY_Escape: alt_commit(FALSE); return TRUE;
        case GDK_KEY_Return: case GDK_KEY_space: alt_commit(TRUE); return TRUE;
        case GDK_KEY_q: case GDK_KEY_Delete: { Win *wn = wins->pdata[sel]; close_win(wn->id); g_ptr_array_remove_index(wins, sel);
            if (!wins->len) { popup_hide(); return TRUE; } sel = MIN(sel, (int)wins->len - 1); layout(0, 0); build_popup(); return TRUE; }
        default: return TRUE;
    }
    gtk_widget_queue_draw(area); return TRUE;
}
static gboolean on_click(GtkWidget *w, GdkEventButton *e, gpointer d) {
    if (!wins) return FALSE; int i = tile_at(e->x, e->y); if (i < 0) return TRUE;
    Win *wn = wins->pdata[i];
    if (e->button == 2) { close_win(wn->id); g_ptr_array_remove_index(wins, i);
        if (!wins->len) { popup_hide(); return TRUE; } sel = MIN(sel, (int)wins->len - 1); hov = -1;
        if (mode == M_DOCK) { int cx = pop_x + pop_w / 2, by = pop_y + pop_h; layout(cx, by); build_popup(); } else { layout(0, 0); build_popup(); }
        return TRUE; }
    sel = i; if (mode == M_ALT) alt_commit(TRUE); else { Window id = wn->id; popup_hide(); activate(id); }
    return TRUE;
}
static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer d) {
    if (!wins) return FALSE; int i = tile_at(e->x, e->y); if (i != hov) { hov = i; if (mode == M_ALT && i >= 0) sel = i; gtk_widget_queue_draw(area); } return TRUE;
}
static void alt_step(int dir) {
    if (mode == M_DOCK) popup_hide();
    if (mode == M_NONE) {
        wins = list_wins(NULL); int n = wins->len;
        if (n < 2) { if (n == 1) activate(((Win *)wins->pdata[0])->id); g_ptr_array_free(wins, TRUE); wins = NULL; return; }
        for (int i = 0; i < n; i++) capture(wins->pdata[i]);
        mode = M_ALT; sel = dir > 0 ? 1 : n - 1; hov = -1; layout(0, 0); build_popup();
        GdkSeat *seat = gdk_display_get_default_seat(gd);
        grabbed = gdk_seat_grab(seat, gtk_widget_get_window(pop), GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_POINTER, TRUE, NULL, NULL, NULL, NULL) == GDK_GRAB_SUCCESS;
        if (!alt_held()) { alt_commit(TRUE); return; }
        hide_t = g_timeout_add(150, alt_check, NULL);
    } else { int n = wins->len; sel = (sel + n + dir) % n; gtk_widget_queue_draw(area); }
}
static gboolean sig_next(gpointer d) { alt_step(1); return G_SOURCE_CONTINUE; }
static gboolean sig_prev(gpointer d) { alt_step(-1); return G_SOURCE_CONTINUE; }

/* ---------- pré-visualização no dock ---------- */
static gboolean dock_refresh(gpointer d) {
    if (mode != M_DOCK || !wins) return G_SOURCE_REMOVE;
    for (guint i = 0; i < wins->len; i++) capture(wins->pdata[i]); gtk_widget_queue_draw(area); return G_SOURCE_CONTINUE;
}
static int show_item = -2;
static gboolean dock_show(gpointer d) {
    show_t = 0; if (show_item < 0 || mode == M_ALT) return G_SOURCE_REMOVE;
    build_items(); if (show_item >= (int)items->len) return G_SOURCE_REMOVE; Item *it = g_ptr_array_index(items, show_item);
    GPtrArray *l = list_wins(it);
    if (!l->len) { g_ptr_array_free(l, TRUE); return G_SOURCE_REMOVE; }
    int dx, dy, dw, dh; if (!find_dock(&dx, &dy, &dw, &dh)) { g_ptr_array_free(l, TRUE); return G_SOURCE_REMOVE; }
    if (wins) g_ptr_array_free(wins, TRUE); wins = l;
    for (guint i = 0; i < wins->len; i++) capture(wins->pdata[i]);
    mode = M_DOCK; dock_item = show_item; sel = -1; hov = -1;
    double icon = 48, pitch = icon * 1.2417; int N = items->len;
    int ax = SW / 2 - (int)(N * pitch / 2) + (int)((show_item + .5) * pitch);
    layout(ax, SH - 76); build_popup();
    if (!refresh_t) refresh_t = g_timeout_add(1000, dock_refresh, NULL);
    return G_SOURCE_REMOVE;
}
static gboolean hide_later(gpointer d) { hide_t = 0; if (mode == M_DOCK) popup_hide(); return G_SOURCE_REMOVE; }
static gboolean poll(gpointer d) {
    if (mode == M_ALT) return G_SOURCE_CONTINUE;
    Window r, c; int px, py, a, b; unsigned m; XQueryPointer(dpy, root, &r, &c, &px, &py, &a, &b, &m);
    double icon = 48, pitch = icon * 1.2417; int N = items ? items->len : 0;
    if (py >= SH - 62 && mode == M_NONE) { build_items(); N = items->len; }
    // item sob o mouse, se o mouse estiver sobre o dock
    int item = -1; double left = SW / 2.0 - N * pitch / 2;
    if (py >= SH - 62 && px >= left - 4 && px < left + N * pitch + 4) item = CLAMP((int)((px - left) / pitch), 0, N - 1);
    // zona segura: o popup e o espaço entre ele e o dock
    gboolean in_pop = mode == M_DOCK && px >= pop_x - 4 && px < pop_x + pop_w + 4 && py >= pop_y - 4;
    if (item >= 0) {
        if (hide_t) { g_source_remove(hide_t); hide_t = 0; }
        if (mode == M_DOCK && item != dock_item) { popup_hide(); }
        if (mode == M_NONE && item != show_item) { show_item = item; if (show_t) g_source_remove(show_t); show_t = g_timeout_add(350, dock_show, NULL); }
        else if (mode == M_NONE && !show_t) { show_item = item; show_t = g_timeout_add(350, dock_show, NULL); }
    } else if (in_pop) { if (hide_t) { g_source_remove(hide_t); hide_t = 0; } }
    else {
        if (show_t) { g_source_remove(show_t); show_t = 0; } show_item = -2;
        if (mode == M_DOCK && !hide_t) hide_t = g_timeout_add(300, hide_later, NULL);
    }
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    gd = gdk_display_get_default(); dpy = gdk_x11_get_default_xdisplay(); root = DefaultRootWindow(dpy);
    SW = DisplayWidth(dpy, 0); SH = DisplayHeight(dpy, 0);
    nova_leve = g_file_test(g_strdup_printf("%s/.cache/nova-leve", g_get_home_dir()), G_FILE_TEST_EXISTS);
    load_icons();
    pop = NULL; wins = NULL; pop_w = pop_h = 10; per_row = 1; TW = 190; wins = g_ptr_array_new();
    build_popup(); gtk_widget_hide(pop); g_ptr_array_free(wins, TRUE); wins = NULL;
    g_signal_connect(pop, "key-press-event", G_CALLBACK(on_key), NULL); g_signal_connect(pop, "key-release-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(pop, "button-press-event", G_CALLBACK(on_click), NULL); g_signal_connect(pop, "motion-notify-event", G_CALLBACK(on_motion), NULL);
    g_unix_signal_add(SIGUSR1, sig_next, NULL); g_unix_signal_add(SIGUSR2, sig_prev, NULL);
    g_timeout_add(110, poll, NULL);
    gtk_main(); return 0;
}
