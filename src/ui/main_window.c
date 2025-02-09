#include "../include/venv_analyzer.h"  // Updated include path
#include "main_window.h"
#include "graph_view.h"
#include "package_list.h"
#include <gtk/gtk.h>

static const char* CSS_DEFINITIONS = 
    ".header-bar { padding: 6px; }"
    ".sidebar { background: @theme_base_color; border-left: 1px solid @borders; }"
    ".package-list { min-width: 200px; }"
    ".graph-view { background: @theme_base_color; }"
    ".status-bar { padding: 4px; border-top: 1px solid @borders; }";

static void setup_css(void) {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, CSS_DEFINITIONS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void on_folder_selected(GObject* source, GAsyncResult* result, gpointer user_data);

static void on_open_clicked(GtkButton* button, VenvAnalyzer* analyzer) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Virtual Environment");
    gtk_file_dialog_set_modal(dialog, TRUE);
    
    GtkWindow* parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    gtk_file_dialog_select_folder(dialog, parent,
                                NULL,  // No cancellable
                                (GAsyncReadyCallback)on_folder_selected,
                                analyzer);
}

static void on_folder_selected(GObject* source, GAsyncResult* result, gpointer user_data) {
    GtkFileDialog* dialog = GTK_FILE_DIALOG(source);
    VenvAnalyzer* analyzer = (VenvAnalyzer*)user_data;
    GError* error = NULL;
    
    GFile* folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);
    if (folder) {
        char* path = g_file_get_path(folder);
        g_strlcpy(analyzer->venv_path, path, sizeof(analyzer->venv_path));
        
        if (!venv_analyzer_scan(analyzer, &error)) {
            GtkWidget* window = GTK_WIDGET(gtk_window_get_transient_for(GTK_WINDOW(dialog)));
            main_window_set_status(window, error ? error->message : "Failed to scan virtual environment");
            if (error) g_error_free(error);
        } else {
            GtkWidget* window = GTK_WIDGET(gtk_window_get_transient_for(GTK_WINDOW(dialog)));
            main_window_refresh_view(window);
        }
        
        g_free(path);
        g_object_unref(folder);
    } else if (error) {
        g_print("Error selecting folder: %s\n", error->message);
        g_error_free(error);
    }
    g_object_unref(dialog);
}

GtkWidget* venv_main_window_new(GtkApplication* app, VenvAnalyzer* analyzer) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Python Dependency Analyzer");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    setup_css();

    // Create main container
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), box);

    // Header bar with instructions
    GtkWidget* header = gtk_header_bar_new();
    gtk_widget_add_css_class(header, "header-bar");
    
    GtkWidget* label = gtk_label_new("Click folder icon to select a Python virtual environment");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), label);
    
    GtkWidget* open_button = gtk_button_new_from_icon_name("folder-open-symbolic");
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_clicked), analyzer);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), open_button);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // Main content
    GtkWidget* content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(box), content);

    // Package list (left side)
    GtkWidget* list = package_list_new(analyzer);
    gtk_widget_set_size_request(list, 200, -1);
    gtk_paned_set_start_child(GTK_PANED(content), list);
    g_object_set_data(G_OBJECT(window), "package-list", list);

    // Graph view (right side)
    GtkWidget* graph = venv_graph_view_new(analyzer);
    gtk_widget_set_hexpand(graph, TRUE);
    gtk_paned_set_end_child(GTK_PANED(content), graph);
    g_object_set_data(G_OBJECT(window), "graph-view", graph);

    // Status bar
    GtkWidget* status_bar = gtk_label_new("");
    gtk_widget_add_css_class(status_bar, "status-bar");
    gtk_box_append(GTK_BOX(box), status_bar);
    analyzer->status_bar = status_bar;

    // Store analyzer reference
    g_object_set_data(G_OBJECT(window), "analyzer", analyzer);

    return window;
}

// Remove main_window_new() function since we're consolidating to venv_main_window_new()

void main_window_set_status(GtkWidget* window, const char* message) {
    VenvAnalyzer* analyzer = g_object_get_data(G_OBJECT(window), "analyzer");
    gtk_label_set_text(GTK_LABEL(analyzer->status_bar), message);
}

void main_window_refresh_view(GtkWidget* window) {
    VenvAnalyzer* analyzer = g_object_get_data(G_OBJECT(window), "analyzer");
    
    // Update package list
    main_window_update_package_list(window, analyzer);
    
    // Update graph view
    main_window_update_dependency_graph(window, analyzer);
    
    // Update status
    int pkg_count = 0;
    int conflicts = 0;
    
    Package* pkg = analyzer->packages;
    while (pkg) {
        pkg_count++;
        if (pkg->conflicts) conflicts++;
        pkg = pkg->next;
    }
    
    // Use GString to safely handle potentially long paths
    GString* status = g_string_new(NULL);
    g_string_printf(status,
                   "Packages: %d | Conflicts: %d | Path: %s",
                   pkg_count, conflicts, analyzer->venv_path);
    
    main_window_set_status(window, status->str);
    g_string_free(status, TRUE);
}

void main_window_update_package_list(GtkWidget* window G_GNUC_UNUSED,
                                   VenvAnalyzer* analyzer)
{
    GtkWidget* list = g_object_get_data(G_OBJECT(window), "package-list");
    if (list) {
        package_list_update(list, analyzer);
    }
}

void main_window_update_dependency_graph(GtkWidget* window G_GNUC_UNUSED,
                                      VenvAnalyzer* analyzer G_GNUC_UNUSED)
{
    GtkWidget* graph = g_object_get_data(G_OBJECT(window), "graph-view");
    if (graph) {
        venv_graph_view_update(graph);
    }
}

#include "ui/main_window.h"
#include "ui/graph_view.h"
#include "ui/package_list.h"