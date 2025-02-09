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

static void on_choose_folder_clicked(GtkButton* button, MainWindow* window) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Virtual Environment");
    gtk_file_dialog_set_modal(dialog, TRUE);
    
    GtkWindow* parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    gtk_file_dialog_select_folder(dialog, parent,
                                NULL,
                                (GAsyncReadyCallback)on_folder_selected,
                                window->analyzer);
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

static GtkWidget* create_toolbar(MainWindow* window) {
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(toolbar, 10);
    gtk_widget_set_margin_end(toolbar, 10);
    gtk_widget_set_margin_top(toolbar, 10);
    gtk_widget_set_margin_bottom(toolbar, 10);

    // Create folder chooser button
    GtkWidget* choose_button = gtk_button_new_with_label("Choose Venv");
    gtk_box_append(GTK_BOX(toolbar), choose_button);
    g_signal_connect(choose_button, "clicked", G_CALLBACK(on_choose_folder_clicked), window);

    // Create scan button
    GtkWidget* scan_button = gtk_button_new_with_label("Scan Dependencies");
    gtk_box_append(GTK_BOX(toolbar), scan_button);
    g_signal_connect(scan_button, "clicked", G_CALLBACK(on_scan_clicked), window);

    return toolbar;
}

static GtkWidget* create_details_view(MainWindow* window) {
    GtkWidget* details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(details_box, 10);
    gtk_widget_set_margin_end(details_box, 10);
    gtk_widget_set_margin_bottom(details_box, 10);

    // Package name label
    GtkWidget* name_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(name_label), "<b>Package Details</b>");
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(details_box), name_label);

    // Create scrolled window for details
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 200);
    gtk_widget_set_vexpand(scrolled, TRUE);

    // Text view for package details
    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);
    gtk_box_append(GTK_BOX(details_box), scrolled);

    window->details_view = text_view;
    return details_box;
}

static void update_package_details(MainWindow* window, Package* package) {
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(window->details_view));
    
    if (!window->analyzer->venv_path[0]) {
        // Show welcome message if no venv selected
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
        
        // Add dependencies
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

        // Add conflicts
        g_string_append(details, "\nConflicts:\n");
        PackageDep* conflict = package->conflicts; // Changed type to PackageDep
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

static void setup_list_item(GtkListItemFactory* factory,
                           GtkListItem* list_item,
                           gpointer user_data) {
    Package* package = g_object_get_data(G_OBJECT(list_item), "package");
    GtkWidget* label = gtk_label_new(package ? package->name : "");
    gtk_list_item_set_child(list_item, label);
}

static void on_selection_changed(GtkSelectionModel* selection_model G_GNUC_UNUSED, 
                               guint position G_GNUC_UNUSED,
                               guint n_items G_GNUC_UNUSED,
                               MainWindow* window) {
    GtkSingleSelection* single = GTK_SINGLE_SELECTION(selection_model);
    GtkListItem* item = gtk_single_selection_get_selected_item(single);
    
    if (item) {
        Package* package = g_object_get_data(G_OBJECT(item), "package");
        update_package_details(window, package);
    }
}

static GtkWidget* create_package_list(MainWindow* window) {
    // Create list model
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_list_item), NULL);
    
    // Create list with empty model initially
    GtkNoSelection* model = gtk_no_selection_new(NULL);
    GtkWidget* list = gtk_list_view_new(GTK_SELECTION_MODEL(model), factory);
    
    // Connect selection changed signal
    g_signal_connect(model, "selection-changed",
                    G_CALLBACK(on_selection_changed), window);
    
    return list;
}

GtkWidget* venv_main_window_new(GtkApplication* app, VenvAnalyzer* analyzer) {
    MainWindow* win = g_new0(MainWindow, 1);
    win->analyzer = analyzer;
    
    win->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win->window), "Python Dependency Analyzer");
    gtk_window_set_default_size(GTK_WINDOW(win->window), 800, 600);

    setup_css();

    // Create main container
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(win->window), box);

    // Add toolbar
    gtk_box_append(GTK_BOX(box), create_toolbar(win));

    // Main content
    GtkWidget* content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(box), content);

    // Package list (left side)
    win->package_list = create_package_list(win);
    gtk_widget_set_size_request(win->package_list, 200, -1);
    gtk_paned_set_start_child(GTK_PANED(content), win->package_list);

    // Details view (right side)
    win->details_view = create_details_view(win);
    gtk_paned_set_end_child(GTK_PANED(content), win->details_view);

    // Store window data
    g_object_set_data_full(G_OBJECT(win->window), "window-data", win, g_free);

    // Show initial welcome message
    update_package_details(win, NULL);

    return win->window;
}

// Helper to get MainWindow struct from GtkWidget
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
    
    // Update package list
    main_window_update_package_list(window, win->analyzer);
    
    // Update graph view
    main_window_update_dependency_graph(window, win->analyzer);
    
    // Update status
    int pkg_count = 0;
    int conflicts = 0;
    
    Package* pkg = win->analyzer->packages;
    while (pkg) {
        pkg_count++;
        if (pkg->conflicts) conflicts++;
        pkg = pkg->next;
    }
    
    // Use GString to safely handle potentially long paths
    GString* status = g_string_new(NULL);
    g_string_printf(status,
                   "Packages: %d | Conflicts: %d | Path: %s",
                   pkg_count, conflicts, win->analyzer->venv_path);
    
    main_window_set_status(window, status->str);
    g_string_free(status, TRUE);
}

void main_window_update_package_list(GtkWidget* window G_GNUC_UNUSED,
                                   VenvAnalyzer* analyzer)
{
    MainWindow* win = get_main_window(window);
    if (win && win->package_list) {
        package_list_update(win->package_list, analyzer);
    }
}

void main_window_update_dependency_graph(GtkWidget* window G_GNUC_UNUSED,
                                      VenvAnalyzer* analyzer G_GNUC_UNUSED)
{
    MainWindow* win = get_main_window(window);
    if (win && win->details_view) {
        venv_graph_view_update(win->details_view);
    }
}

#include "ui/main_window.h"
#include "ui/graph_view.h"
#include "ui/package_list.h"