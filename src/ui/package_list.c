#include "../../include/venv_analyzer.h"  // Fix include path
#include "../core/package.h"
#include "package_list.h"
#include <gtk/gtk.h>

#define PACKAGE_TYPE_ITEM (package_item_get_type())
G_DECLARE_FINAL_TYPE(PackageItem, package_item, PACKAGE, ITEM, GObject)

// Package item structure for the list model
struct _PackageItem {
    GObject parent_instance;
    Package* package;
};

G_DEFINE_TYPE(PackageItem, package_item, G_TYPE_OBJECT)

static void
package_item_init(PackageItem* self)
{
    self->package = NULL;
}

static void
package_item_class_init(PackageItemClass* class)
{
    // Remove unused variable
    // No operations needed in class init for now
}

static void
setup_list_factory(GtkListItemFactory* factory G_GNUC_UNUSED,
                  GtkListItem* list_item,
                  gpointer user_data G_GNUC_UNUSED)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* name_label = gtk_label_new(NULL);
    GtkWidget* version_label = gtk_label_new(NULL);
    
    gtk_widget_set_margin_start(box, 6);
    gtk_widget_set_margin_end(box, 6);
    gtk_widget_set_margin_top(box, 3);
    gtk_widget_set_margin_bottom(box, 3);
    
    gtk_label_set_xalign(GTK_LABEL(name_label), 0);
    gtk_widget_set_hexpand(name_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(version_label), 1);
    
    gtk_box_append(GTK_BOX(box), name_label);
    gtk_box_append(GTK_BOX(box), version_label);
    
    gtk_list_item_set_child(list_item, box);
}

static void
bind_list_factory(GtkListItemFactory* factory G_GNUC_UNUSED,
                 GtkListItem* list_item,
                 gpointer user_data G_GNUC_UNUSED)
{
    GtkWidget* box = gtk_list_item_get_child(list_item);
    GtkWidget* name_label = gtk_widget_get_first_child(box);
    GtkWidget* version_label = gtk_widget_get_next_sibling(name_label);
    
    PackageItem* item = gtk_list_item_get_item(list_item);
    if (!item || !item->package) return;
    
    gtk_label_set_text(GTK_LABEL(name_label), item->package->name);
    gtk_label_set_text(GTK_LABEL(version_label), item->package->version);
    
    if (item->package->conflicts) {
        gtk_widget_add_css_class(box, "warning");
    } else {
        gtk_widget_remove_css_class(box, "warning");
    }
}

GtkWidget*
package_list_new(VenvAnalyzer* analyzer)  // Changed from venv_package_list_new
{
    GListStore* store = g_list_store_new(PACKAGE_TYPE_ITEM);
    
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(
        gtk_single_selection_new(G_LIST_MODEL(store)));
    
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_list_factory), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_list_factory), NULL);
    
    GtkWidget* list = gtk_list_view_new(selection, factory);
    gtk_widget_set_vexpand(list, TRUE);
    
    // Store references
    g_object_set_data(G_OBJECT(list), "store", store);
    g_object_set_data(G_OBJECT(list), "analyzer", analyzer);
    
    package_list_update(list, analyzer);  // Changed from venv_package_list_update
    return list;
}

void
package_list_update(GtkWidget* list, VenvAnalyzer* analyzer)  // Changed from venv_package_list_update
{
    g_return_if_fail(GTK_IS_LIST_VIEW(list));
    
    GListStore* store = g_object_get_data(G_OBJECT(list), "store");
    g_return_if_fail(G_IS_LIST_STORE(store));
    
    g_list_store_remove_all(store);
    
    for (Package* pkg = analyzer->packages; pkg; pkg = pkg->next) {
        PackageItem* item = g_object_new(PACKAGE_TYPE_ITEM, NULL);
        item->package = pkg;
        g_list_store_append(store, item);
        g_object_unref(item);
    }
}

void
show_package_details(VenvAnalyzer* analyzer, Package* pkg)
{
    g_return_if_fail(analyzer != NULL);
    g_return_if_fail(pkg != NULL);
    g_return_if_fail(analyzer->details_view != NULL);

    GString* details = g_string_new(NULL);
    
    // Basic info
    g_string_append_printf(details, "<b>Package:</b> %s\n", pkg->name);
    g_string_append_printf(details, "<b>Version:</b> %s\n", pkg->version);
    
    // Size info
    if (pkg->size > 0) {
        if (pkg->size < 1024) {
            g_string_append_printf(details, "<b>Size:</b> %zu bytes\n", pkg->size);
        } else if (pkg->size < 1024*1024) {
            g_string_append_printf(details, "<b>Size:</b> %.1f KB\n", pkg->size/1024.0);
        } else {
            g_string_append_printf(details, "<b>Size:</b> %.1f MB\n", pkg->size/(1024.0*1024.0));
        }
    }
    
    // Dependencies
    g_string_append(details, "\n<b>Dependencies:</b>\n");
    PackageDep* dep = pkg->dependencies;
    if (!dep) {
        g_string_append(details, "  None\n");
    }
    while (dep) {
        g_string_append_printf(details, "  • %s (%s)\n", 
            dep->name, dep->version);
        dep = dep->next;
    }
    
    // Conflicts
    if (pkg->conflicts) {
        g_string_append(details, "\n<b>Conflicts:</b>\n");
        PackageDep* conflict = pkg->conflicts;
        while (conflict) {
            g_string_append_printf(details, "  ⚠ %s (%s)\n",
                conflict->name, conflict->version);
            conflict = conflict->next;
        }
    }
    
    gtk_label_set_markup(GTK_LABEL(analyzer->details_view), details->str);
    g_string_free(details, TRUE);
}