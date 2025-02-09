#include "../include/venv_analyzer.h"
#include "../include/ui/main_window.h"
#include <gtk/gtk.h>

static void on_activate(GtkApplication* app G_GNUC_UNUSED, 
                       gpointer user_data G_GNUC_UNUSED) {
    VenvAnalyzer* analyzer = venv_analyzer_new("");  // Start with empty path
    GtkWidget* window = venv_main_window_new(app, analyzer);
    gtk_window_present(GTK_WINDOW(window));
}

int main(void) {
    GtkApplication* app = gtk_application_new("org.gtk.pythondepanalyzer",
                                            G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
    return status;
}
