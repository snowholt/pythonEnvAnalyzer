#include "../include/venv_analyzer.h"
#include "../include/ui/main_window.h"
#include <gtk/gtk.h>

static void on_activate(GtkApplication* app, gpointer user_data) {
    VenvAnalyzer* analyzer = (VenvAnalyzer*)user_data;
    GtkWidget* window = venv_main_window_new(app, analyzer);
    gtk_window_present(GTK_WINDOW(window));
}

static void on_open(GApplication* app,
                   GFile** files,
                   gint n_files,
                   const gchar* hint G_GNUC_UNUSED,
                   gpointer user_data) {
    VenvAnalyzer* analyzer = (VenvAnalyzer*)user_data;
    
    if (n_files > 0) {
        char* path = g_file_get_path(files[0]);
        if (path) {
            g_strlcpy(analyzer->venv_path, path, sizeof(analyzer->venv_path));
            g_free(path);
        }
    }
    
    on_activate(GTK_APPLICATION(app), user_data);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        g_print("Usage: %s <venv_path>\n", argv[0]);
        return 1;
    }

    GtkApplication* app = gtk_application_new("org.gtk.pythondepanalyzer", 
                                            G_APPLICATION_HANDLES_OPEN);
    
    VenvAnalyzer* analyzer = venv_analyzer_new(argv[1]);
    if (!analyzer) {
        g_print("Failed to initialize analyzer for %s\n", argv[1]);
        g_object_unref(app);
        return 1;
    }

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), analyzer);
    g_signal_connect(app, "open", G_CALLBACK(on_open), analyzer);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    venv_analyzer_free(analyzer);
    g_object_unref(app);

    return status;
}
