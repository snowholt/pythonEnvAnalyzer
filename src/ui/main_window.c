#include "../include/venv_analyzer.h"
#include "../core/types.h"
#include "../core/package.h"
#include "main_window.h"
#include "graph_view.h"
#include "package_list.h"
#include <gtk/gtk.h>

// Forward declarations
static void on_package_selected(GtkListBox* box, GtkListBoxRow* row, MainWindow* window);
static void update_package_details(MainWindow* window, Package* package);
static void on_scan_clicked(GtkButton* button, MainWindow* window);
static void on_folder_selected(GObject* source, GAsyncResult* result, gpointer user_data);
static void on_choose_folder_clicked(GtkButton* button, MainWindow* window);
static MainWindow* get_main_window(GtkWidget* widget);

// CSS definitions
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

static void on_scan_clicked(GtkButton* button G_GNUC_UNUSED, MainWindow* window) {
    GError* error = NULL;
    if (!venv_analyzer_scan(window->analyzer, &error)) {
        main_window_set_status(window->window, 
            error ? error->message : "Failed to scan virtual environment");
        if (error) g_error_free(error);
        return;
    }
    
    main_window_refresh_view(window->window);
}

static void on_folder_selected(GObject* source, GAsyncResult* result, gpointer user_data) {
    GtkFileDialog* dialog = GTK_FILE_DIALOG(source);
    MainWindow* window = (MainWindow*)user_data;
    GError* error = NULL;
    
    if (!window || !window->analyzer) {
        g_warning("Invalid window or analyzer");
        g_object_unref(dialog);
        return;
    }
    
    GFile* folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);
    if (folder) {
        char* path = g_file_get_path(folder);
        if (path) {
            g_strlcpy(window->analyzer->venv_path, path, sizeof(window->analyzer->venv_path));
            
            if (!venv_analyzer_scan(window->analyzer, &error)) {
                main_window_set_status(window->window, 
                    error ? error->message : "Failed to scan virtual environment");
                if (error) g_error_free(error);
            } else {
                main_window_refresh_view(window->window);
            }
            
            g_free(path);
        }
        g_object_unref(folder);
    } else if (error) {
        g_warning("Error selecting folder: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(dialog);
}

static void on_choose_folder_clicked(GtkButton* button, MainWindow* window) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Virtual Environment");
    gtk_file_dialog_set_modal(dialog, TRUE);
    
    GtkWindow* parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    gtk_file_dialog_select_folder(dialog, parent,
                                NULL,
                                (GAsyncReadyCallback)on_folder_selected,
                                window);
}

static GtkWidget* create_toolbar(MainWindow* window) {
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(toolbar, 10);
    gtk_widget_set_margin_end(toolbar, 10);
    gtk_widget_set_margin_top(toolbar, 10);
    gtk_widget_set_margin_bottom(toolbar, 10);

    GtkWidget* choose_button = gtk_button_new_with_label("Choose Venv");
    gtk_box_append(GTK_BOX(toolbar), choose_button);
    g_signal_connect(choose_button, "clicked", G_CALLBACK(on_choose_folder_clicked), window);

    GtkWidget* scan_button = gtk_button_new_with_label("Scan Dependencies");
    gtk_box_append(GTK_BOX(toolbar), scan_button);
    g_signal_connect(scan_button, "clicked", G_CALLBACK(on_scan_clicked), window);

    return toolbar;
}

static GtkWidget* create_details_view(MainWindow* window) {
    if (!window) return NULL;

    GtkWidget* details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(details_box, 10);
    gtk_widget_set_margin_end(details_box, 10);
    gtk_widget_set_margin_bottom(details_box, 10);

    GtkWidget* text_view = gtk_text_view_new();
    GtkWidget* scrolled = gtk_scrolled_window_new();
    GtkWidget* name_label = gtk_label_new(NULL);

    window->details_view = text_view;
    
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_widget_set_vexpand(text_view, TRUE);

    gtk_label_set_markup(GTK_LABEL(name_label), "<b>Package Details</b>");
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 200);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);

    gtk_box_append(GTK_BOX(details_box), name_label);
    gtk_box_append(GTK_BOX(details_box), scrolled);

    return details_box;
}

static void update_package_details(MainWindow* window, Package* package) {
    if (!window || !window->details_view) {
        g_warning("Invalid window or details view");
        return;
    }

    GtkTextView* text_view = GTK_TEXT_VIEW(window->details_view);
    if (!GTK_IS_TEXT_VIEW(text_view)) {
        g_warning("Invalid text view widget");
        return;
    }

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(text_view);
    if (!buffer) {
        g_warning("Failed to get text buffer");
        return;
    }

    if (!window->analyzer->venv_path[0]) {
        gtk_text_buffer_set_text(buffer,
            "Welcome to Python Dependency Analyzer!\n\n"
            "To begin:\n"
            "1. Click 'Choose Venv' to select your Python virtual environment folder\n"
            "2. Click 'Scan Dependencies' to analyze the packages\n"
            "3. Select a package from the list to view its details", -1);
        return;
    }

    GString* details = g_string_new("");

    if (package) {
        g_string_append_printf(details, "Package: %s\nVersion: %s\n\n", 
                             package->name, package->version);
        
        g_string_append(details, "Dependencies:\n");
        PackageDep* dep = package->dependencies;
        if (!dep) {
            g_string_append(details, "  None\n");
        }
        while (dep) {
            Package* installed_dep = venv_analyzer_get_package(window->analyzer, dep->name);
            if (installed_dep) {
                g_string_append_printf(details, "  • %s (required: %s, installed: %s)\n",
                                     dep->name, dep->version, installed_dep->version);
            } else {
                g_string_append_printf(details, "  • %s (required: %s, not installed)\n",
                                     dep->name, dep->version);
            }
            dep = dep->next;
        }

        g_string_append(details, "\nConflicts:\n");
        PackageDep* conflict = package->conflicts;
        if (!conflict) {
            g_string_append(details, "  None\n");
        }
        while (conflict) {
            g_string_append_printf(details, "  • %s (version %s conflicts)\n",
                                 conflict->name, conflict->version);
            conflict = conflict->next;
        }
    } else {
        g_string_append(details, "No package selected");
    }

    gtk_text_buffer_set_text(buffer, details->str, -1);
    g_string_free(details, TRUE);
}

static GtkWidget* create_package_list(MainWindow* window) {
    GtkWidget* list = gtk_list_box_new();
    gtk_widget_add_css_class(list, "package-list");
    g_signal_connect(list, "row-selected", G_CALLBACK(on_package_selected), window);
    window->package_list = list;
    return list;
}

static void on_package_selected(GtkListBox* box G_GNUC_UNUSED, 
                              GtkListBoxRow* row,
                              MainWindow* window) {
    if (!row) return;
    Package* package = g_object_get_data(G_OBJECT(row), "package");
    update_package_details(window, package);
}

GtkWidget* venv_main_window_new(GtkApplication* app, VenvAnalyzer* analyzer) {
    MainWindow* win = g_new0(MainWindow, 1);
    if (!win) return NULL;
    
    win->analyzer = analyzer;
    win->window = gtk_application_window_new(app);
    if (!win->window) {
        g_free(win);
        return NULL;
    }

    GtkWidget* details = create_details_view(win);
    GtkWidget* package_list = create_package_list(win);
    
    if (!details || !package_list || !win->details_view) {
        g_free(win);
        return NULL;
    }

    gtk_window_set_title(GTK_WINDOW(win->window), "Python Dependency Analyzer");
    gtk_window_set_default_size(GTK_WINDOW(win->window), 800, 600);

    setup_css();

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(win->window), box);

    gtk_box_append(GTK_BOX(box), create_toolbar(win));

    GtkWidget* content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(box), content);

    win->package_list = create_package_list(win);
    gtk_widget_set_size_request(win->package_list, 200, -1);
    gtk_paned_set_start_child(GTK_PANED(content), win->package_list);

    gtk_paned_set_end_child(GTK_PANED(content), details);

    g_object_set_data_full(G_OBJECT(win->window), "window-data", win, g_free);

    update_package_details(win, NULL);

    return win->window;
}

static MainWindow* get_main_window(GtkWidget* widget) {
    return g_object_get_data(G_OBJECT(widget), "window-data");
}

void main_window_set_status(GtkWidget* window, const char* message) {
    MainWindow* win = get_main_window(window);
    if (win && win->analyzer->status_bar) {
        gtk_label_set_text(GTK_LABEL(win->analyzer->status_bar), message);
    }
}

void main_window_refresh_view(GtkWidget* window) {
    MainWindow* win = get_main_window(window);
    if (!win) return;
    
    main_window_update_package_list(window, win->analyzer);
    main_window_update_dependency_graph(window, win->analyzer);
    
    int pkg_count = 0;
    int conflicts = 0;
    
    for (Package* pkg = win->analyzer->packages; pkg; pkg = pkg->next) {
        pkg_count++;
        if (pkg->conflicts) conflicts++;
    }
    
    GString* status = g_string_new(NULL);
    g_string_printf(status,
                   "Packages: %d | Conflicts: %d | Path: %s",
                   pkg_count, conflicts, win->analyzer->venv_path);
    
    main_window_set_status(window, status->str);
    g_string_free(status, TRUE);
}

void main_window_update_package_list(GtkWidget* window, VenvAnalyzer* analyzer) {
    if (!GTK_IS_WIDGET(window)) {
        g_warning("Invalid window widget");
        return;
    }

    MainWindow* win = get_main_window(window);
    if (!win) {
        g_warning("Could not get main window data");
        return;
    }

    if (!win->package_list || !GTK_IS_WIDGET(win->package_list)) {
        g_warning("Invalid package list widget");
        return;
    }

    package_list_update(win->package_list, analyzer);
}

void main_window_update_dependency_graph(GtkWidget* window G_GNUC_UNUSED,
                                      VenvAnalyzer* analyzer G_GNUC_UNUSED)
{
    MainWindow* win = get_main_window(window);
    if (win && win->details_view) {
        venv_graph_view_update(win->details_view);
    }
}