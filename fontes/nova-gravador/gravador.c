// Gravador NOVA OS: grava o microfone em WAV e reproduz. GTK3 + PulseAudio (simple API).
#include <gtk/gtk.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define RATE 44100
static const pa_sample_spec SS = { PA_SAMPLE_S16LE, RATE, 1 };

static GtkWidget *lbl_tempo, *lbl_estado, *btn_rec, *btn_stop, *btn_play;
static volatile gboolean gravando = FALSE, tocando = FALSE;
static GThread *thr = NULL;
static char arquivo[512] = "";
static guint timer_id = 0;
static gint64 t0;

static void estado(const char *s) { gtk_label_set_text(GTK_LABEL(lbl_estado), s); }

static void ajusta_botoes(void) {
    gboolean ocupado = gravando || tocando;
    gtk_widget_set_sensitive(btn_rec, !ocupado);
    gtk_widget_set_sensitive(btn_stop, ocupado);
    gtk_widget_set_sensitive(btn_play, !ocupado && arquivo[0]);
}

static void escreve_cab(FILE *f, guint32 dados) {
    guint32 v; guint16 w;
    fwrite("RIFF", 1, 4, f); v = 36 + dados; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    w = 1; fwrite(&w, 2, 1, f); w = 1; fwrite(&w, 2, 1, f);
    v = RATE; fwrite(&v, 4, 1, f); v = RATE * 2; fwrite(&v, 4, 1, f);
    w = 2; fwrite(&w, 2, 1, f); w = 16; fwrite(&w, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dados, 4, 1, f);
}

static gboolean fim_thread(gpointer msg) {
    if (thr) { g_thread_join(thr); thr = NULL; }
    gravando = tocando = FALSE;
    if (timer_id) { g_source_remove(timer_id); timer_id = 0; }
    estado(msg ? (const char *)msg : "Pronto");
    ajusta_botoes();
    return G_SOURCE_REMOVE;
}

static gpointer grava(gpointer d) {
    int err; pa_simple *s = pa_simple_new(NULL, "Gravador NOVA OS", PA_STREAM_RECORD, NULL, "gravacao", &SS, NULL, NULL, &err);
    FILE *f = fopen(arquivo, "wb");
    if (!s || !f) { if (f) fclose(f); if (s) pa_simple_free(s); g_idle_add(fim_thread, "Erro: microfone ou arquivo indisponivel"); return NULL; }
    escreve_cab(f, 0);
    guint32 total = 0; short buf[2048];
    while (gravando) {
        if (pa_simple_read(s, buf, sizeof buf, &err) < 0) break;
        fwrite(buf, 1, sizeof buf, f); total += sizeof buf;
    }
    fseek(f, 0, SEEK_SET); escreve_cab(f, total); fclose(f); pa_simple_free(s);
    g_idle_add(fim_thread, "Gravacao salva");
    return NULL;
}

static gpointer toca(gpointer d) {
    int err; FILE *f = fopen(arquivo, "rb");
    pa_simple *s = pa_simple_new(NULL, "Gravador NOVA OS", PA_STREAM_PLAYBACK, NULL, "reproducao", &SS, NULL, NULL, &err);
    if (!s || !f) { if (f) fclose(f); if (s) pa_simple_free(s); g_idle_add(fim_thread, "Erro: saida de som indisponivel"); return NULL; }
    fseek(f, 44, SEEK_SET); char buf[4096]; size_t n;
    while (tocando && (n = fread(buf, 1, sizeof buf, f)) > 0)
        if (pa_simple_write(s, buf, n, &err) < 0) break;
    if (tocando) pa_simple_drain(s, &err);
    fclose(f); pa_simple_free(s);
    g_idle_add(fim_thread, "Pronto");
    return NULL;
}

static gboolean tick(gpointer d) {
    gint64 s = (g_get_monotonic_time() - t0) / 1000000;
    char t[32]; snprintf(t, sizeof t, "%02ld:%02ld", (long)(s / 60), (long)(s % 60));
    gtk_label_set_text(GTK_LABEL(lbl_tempo), t);
    return G_SOURCE_CONTINUE;
}

static void inicia(gpointer fn) {
    t0 = g_get_monotonic_time(); gtk_label_set_text(GTK_LABEL(lbl_tempo), "00:00");
    timer_id = g_timeout_add(250, tick, NULL);
    thr = g_thread_new("audio", (GThreadFunc)fn, NULL);
    ajusta_botoes();
}

static void ao_gravar(GtkWidget *w, gpointer d) {
    time_t t = time(NULL); struct tm *tm = localtime(&t);
    char dir[256]; snprintf(dir, sizeof dir, "%s/Músicas", g_get_home_dir());
    g_mkdir_with_parents(dir, 0755);
    snprintf(arquivo, sizeof arquivo, "%s/gravacao-%04d%02d%02d-%02d%02d%02d.wav", dir, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    gravando = TRUE; estado("Gravando..."); inicia(grava);
}
static void ao_tocar(GtkWidget *w, gpointer d) { tocando = TRUE; estado("Reproduzindo..."); inicia(toca); }
static void ao_parar(GtkWidget *w, gpointer d) { gravando = FALSE; tocando = FALSE; }

static void ativa(GtkApplication *app, gpointer d) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "Gravador");
    gtk_window_set_default_size(GTK_WINDOW(win), 320, 200);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 20);
    lbl_tempo = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_tempo), "<span size='xx-large' weight='bold'>00:00</span>");
    gtk_label_set_text(GTK_LABEL(lbl_tempo), "00:00");
    PangoAttrList *at = pango_attr_list_new();
    pango_attr_list_insert(at, pango_attr_scale_new(3.0));
    gtk_label_set_attributes(GTK_LABEL(lbl_tempo), at);
    lbl_estado = gtk_label_new("Pronto");
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    btn_rec = gtk_button_new_with_label("● Gravar");
    btn_stop = gtk_button_new_with_label("■ Parar");
    btn_play = gtk_button_new_with_label("▶ Ouvir");
    g_signal_connect(btn_rec, "clicked", G_CALLBACK(ao_gravar), NULL);
    g_signal_connect(btn_stop, "clicked", G_CALLBACK(ao_parar), NULL);
    g_signal_connect(btn_play, "clicked", G_CALLBACK(ao_tocar), NULL);
    gtk_box_pack_start(GTK_BOX(row), btn_rec, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), btn_stop, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), btn_play, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), lbl_tempo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), lbl_estado, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(win), box);
    ajusta_botoes();
    gtk_widget_show_all(win);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("br.nova.gravador", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(ativa), NULL);
    int r = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app); return r;
}
