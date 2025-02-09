#pragma once

#include "../venv_analyzer.h"
#include <gtk/gtk.h>

GtkWidget* package_list_new(VenvAnalyzer* analyzer);
void package_list_update(GtkWidget* list, VenvAnalyzer* analyzer);
void show_package_details(VenvAnalyzer* analyzer, Package* pkg);
