#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtk/gtk.h>
#include "../include/venv_analyzer.h"

typedef struct {
    GtkWidget* window;
    GtkWidget* details_view;
    GtkWidget* package_list;
    GtkWidget* graph_view;
    VenvAnalyzer* analyzer;
} MainWindow;

// Signal handlers
static void on_choose_folder_clicked(GtkButton* button, MainWindow* window);
static void on_scan_clicked(GtkButton* button, MainWindow* window);

// Public functions
GtkWidget* venv_main_window_new(GtkApplication* app, VenvAnalyzer* analyzer);
void main_window_set_status(GtkWidget* window, const char* message);
void main_window_refresh_view(GtkWidget* window);
void main_window_update_package_list(GtkWidget* window, VenvAnalyzer* analyzer);
void main_window_update_dependency_graph(GtkWidget* window, VenvAnalyzer* analyzer);

#endif /* MAIN_WINDOW_H */