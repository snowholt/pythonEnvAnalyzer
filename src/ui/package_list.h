#ifndef PACKAGE_LIST_H
#define PACKAGE_LIST_H

#include "../include/venv_analyzer.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

// Forward declaration to avoid circular dependencies
void show_package_details(VenvAnalyzer* analyzer, Package* pkg);

// Column definitions for package list store
enum {
    PACKAGE_COL_NAME,
    PACKAGE_COL_VERSION,
    PACKAGE_COL_DESCRIPTION,
    PACKAGE_COL_PACKAGE_PTR,  // Hidden column for Package* pointer
    PACKAGE_N_COLUMNS
};

// Package list widget creation and management
GtkWidget*      package_list_new                    (VenvAnalyzer* analyzer);
void            package_list_update                  (GtkWidget* list, 
                                                     VenvAnalyzer* analyzer);
Package*        package_list_get_selected_package    (GtkWidget* list);

// Signal handlers
void            package_list_on_row_activated       (GtkListView* list_view,
                                                    guint position,
                                                    gpointer user_data);
void            package_list_on_selection_changed   (GtkSelectionModel* selection_model,
                                                    guint position,
                                                    guint n_items,
                                                    gpointer user_data);

G_END_DECLS

#endif // PACKAGE_LIST_H
