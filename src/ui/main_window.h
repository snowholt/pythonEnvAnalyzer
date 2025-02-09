#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "../include/venv_analyzer.h"
#include <gtk/gtk.h>

// Window configuration
typedef struct {
    const char* title;
    int default_width;
    int default_height;
} WindowConfig;

// Function declarations
GtkWidget* main_window_new(VenvAnalyzer* analyzer, const WindowConfig* config);
void main_window_set_status(GtkWidget* window, const char* message);
void main_window_refresh_view(GtkWidget* window);
void main_window_update_package_list(GtkWidget* window, VenvAnalyzer* analyzer);
void main_window_update_dependency_graph(GtkWidget* window, VenvAnalyzer* analyzer);

#endif // MAIN_WINDOW_H