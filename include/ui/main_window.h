#pragma once

#include "../venv_analyzer.h"
#include <gtk/gtk.h>

// Window creation functions
GtkWidget* venv_main_window_new(GtkApplication* app, VenvAnalyzer* analyzer);

// Window management functions
void main_window_set_status(GtkWidget* window, const char* message);
void main_window_refresh_view(GtkWidget* window);
void main_window_update_package_list(GtkWidget* window, VenvAnalyzer* analyzer);
void main_window_update_dependency_graph(GtkWidget* window, VenvAnalyzer* analyzer);