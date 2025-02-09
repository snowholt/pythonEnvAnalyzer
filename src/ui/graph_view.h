#ifndef GRAPH_VIEW_H
#define GRAPH_VIEW_H

#include "../include/venv_analyzer.h"

G_BEGIN_DECLS

#define VENV_TYPE_GRAPH_VIEW (venv_graph_view_get_type())
G_DECLARE_FINAL_TYPE(VenvGraphView, venv_graph_view, VENV, GRAPH_VIEW, GtkWidget)

// Function declarations
void venv_graph_view_snapshot(GtkWidget* widget, GtkSnapshot* snapshot);
GtkWidget* venv_graph_view_new(VenvAnalyzer* analyzer);
void venv_graph_view_update(GtkWidget* view);
void venv_graph_view_export(GtkWidget* view, const char* path, const char* format);

G_END_DECLS

#endif // GRAPH_VIEW_H