#include "analyzer.h"
#include "package.h"
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#define COMMAND_BUFFER_SIZE 1024
#define OUTPUT_BUFFER_SIZE 4096

static char error_buffer[256] = {0};

// Helper function to execute pip commands
// In analyzer.c, update the execute_pip_command function:

static char* execute_pip_command(const char* venv_path, const char* command) {
    if (!venv_path || !venv_path[0]) {
        g_snprintf(error_buffer, sizeof(error_buffer), "No virtual environment path set");
        return NULL;
    }

    g_print("Debug: Checking venv path: %s\n", venv_path);
    if (!g_file_test(venv_path, G_FILE_TEST_IS_DIR)) {
        g_print("Debug: Venv path is not a directory\n");
        return NULL;
    }

    // Create the full command using python -m pip
    char full_command[COMMAND_BUFFER_SIZE];
    char python_path[COMMAND_BUFFER_SIZE];
    
    // Find Python executable
    const char* python_paths[] = {
        "/bin/python",
        "/bin/python3",
        "Scripts/python.exe",  // Windows support
        NULL
    };
    
    const char* python_exe = NULL;
    for (const char** path = python_paths; *path; path++) {
        g_snprintf(python_path, sizeof(python_path), "%s/%s", venv_path, *path);
        if (g_file_test(python_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE)) {
            python_exe = *path;
            break;
        }
    }
    
    if (!python_exe) {
        g_snprintf(error_buffer, sizeof(error_buffer), 
                  "Python executable not found in virtual environment");
        return NULL;
    }

    // Build the full command
    if (g_strcmp0(command, "freeze") == 0) {
        g_snprintf(full_command, sizeof(full_command),
                  "%s/%s -m pip freeze", venv_path, python_exe);
    } else {
        g_snprintf(full_command, sizeof(full_command),
                  "%s/%s -m pip show %s", venv_path, python_exe, command);
    }

    g_print("Debug: Executing command: %s\n", full_command);

    // Execute command
    FILE* pipe = popen(full_command, "r");
    if (!pipe) {
        g_snprintf(error_buffer, sizeof(error_buffer),
                  "Failed to execute command: %s", full_command);
        return NULL;
    }

    char* output = g_malloc(OUTPUT_BUFFER_SIZE);
    size_t total = 0;
    size_t bytes_read;
    char buffer[1024];

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        if (total + bytes_read >= OUTPUT_BUFFER_SIZE) {
            g_snprintf(error_buffer, sizeof(error_buffer), "Output buffer overflow");
            g_free(output);
            pclose(pipe);
            return NULL;
        }
        memcpy(output + total, buffer, bytes_read);
        total += bytes_read;
    }

    output[total] = '\0';
    int status = pclose(pipe);

    if (status != 0) {
        g_snprintf(error_buffer, sizeof(error_buffer),
                  "Command failed with status %d", status);
        g_free(output);
        return NULL;
    }

    return output;
}

// End of helper functions

VenvAnalyzer* venv_analyzer_new(const char* path) {
    VenvAnalyzer* analyzer = g_new0(VenvAnalyzer, 1);
    if (!analyzer) return NULL;

    g_strlcpy(analyzer->venv_path, path, sizeof(analyzer->venv_path));
    
    // Only validate path if one is provided
    if (path && path[0] && !g_file_test(path, G_FILE_TEST_IS_DIR)) {
        g_snprintf(error_buffer, sizeof(error_buffer),
                  "Invalid venv path: %s", path);
        g_free(analyzer);
        return NULL;
    }
    
    analyzer->packages = NULL;
    analyzer->gvc = (GVC_t*)gvContext();  // Cast to proper type
    analyzer->db = NULL;

    // Initialize GUI components
    analyzer->package_store = g_list_store_new(PACKAGE_TYPE);
    analyzer->selection_model = GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(analyzer->package_store)));
    
    
    // Create drawing area for graph
    analyzer->details_view = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(analyzer->details_view), 400);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(analyzer->details_view), 300);
    gtk_widget_set_hexpand(analyzer->details_view, TRUE);
    gtk_widget_set_vexpand(analyzer->details_view, TRUE);

    // Create status bar
    analyzer->status_bar = gtk_label_new("");
    gtk_widget_set_hexpand(analyzer->status_bar, TRUE);
    
    return analyzer;
}

void venv_analyzer_free(VenvAnalyzer* analyzer) {
    if (!analyzer) return;
    
    // Free package list
    Package* pkg = analyzer->packages;
    while (pkg) {
        Package* next = pkg->next;
        package_free(pkg);
        pkg = next;
    }
    
    // Free GTK widgets 
    if (analyzer->package_store) {
        g_object_unref(analyzer->package_store);
    }
    if (analyzer->selection_model) {
        g_object_unref(analyzer->selection_model);
    }
    if (analyzer->details_view) {
        gtk_widget_unparent(analyzer->details_view);
    }
    if (analyzer->status_bar) {
        gtk_widget_unparent(analyzer->status_bar);
    }
    
    // Free GraphViz context
    if (analyzer->gvc) {
        gvFreeContext((GVC_t*)analyzer->gvc);  // Cast to proper type
    }

    // Close database connection
    if (analyzer->db) {
        sqlite3_close(analyzer->db);
    }
    
    g_free(analyzer);
}

static void parse_pip_freeze(VenvAnalyzer* analyzer, const char* output) {
    char** lines = g_strsplit(output, "\n", -1);
    for (char** line = lines; *line; line++) {
        if (strlen(*line) == 0) continue;
        
        char** parts = g_strsplit(*line, "==", 2);
        if (parts[0] && parts[1]) {
            Package* pkg = package_new(parts[0], parts[1]);
            if (pkg) {
                pkg->next = analyzer->packages;
                analyzer->packages = pkg;
            }
        }
        g_strfreev(parts);
    }
    g_strfreev(lines);
}

static void analyze_package_dependencies(VenvAnalyzer* analyzer, Package* package) {
    // Change how we build the command
    char* output = execute_pip_command(analyzer->venv_path, package->name);
    if (!output) return;
    
    char** lines = g_strsplit(output, "\n", -1);
    bool in_requires = false;
       
    for (char** line = lines; *line; line++) {
        if (g_str_has_prefix(*line, "Requires: ")) {
            in_requires = true;
            continue;
        }
        
        if (in_requires && g_str_has_prefix(*line, "  ")) {
            char* dep_name = g_strstrip(g_strdup(*line + 2));
            package_add_dependency(package, dep_name, "*");
            g_free(dep_name);
        } else if (in_requires) {
            break;
        }
    }
    
    g_strfreev(lines);
    g_free(output);
}

gboolean venv_analyzer_update_package_list(VenvAnalyzer* analyzer) {
    if (!analyzer || !analyzer->package_store) {
        g_print("Debug: Cannot update package list - invalid analyzer or store\n");
        return TRUE; // Remove source
    }

    // Clear existing items
    g_list_store_remove_all(analyzer->package_store);

    // Add packages to store
    for (Package* pkg = analyzer->packages; pkg; pkg = pkg->next) {
        g_list_store_append(analyzer->package_store, pkg);
    }

    g_print("Debug: Updated package store with %u items\n", g_list_model_get_n_items(G_LIST_MODEL(analyzer->package_store)));
    return TRUE; // Remove source after updating
}

gboolean venv_analyzer_update_graph_view(VenvAnalyzer* analyzer) {
    if (!analyzer || !analyzer->details_view) {
        g_print("Debug: Cannot update graph view - invalid analyzer or view\n");
        return TRUE; // Remove source
    }

    // Create new graph
    Agraph_t* g = agopen("venv_dependencies", Agdirected, NULL);
    
    // Add nodes and edges
    for (Package* pkg = analyzer->packages; pkg; pkg = pkg->next) {
        Agnode_t* pkg_node = agnode(g, pkg->name, TRUE);
        
        for (PackageDep* dep = pkg->dependencies; dep; dep = dep->next) {
            Agnode_t* dep_node = agnode(g, dep->name, TRUE);
            agedge(g, pkg_node, dep_node, NULL, TRUE);
        }
    }

    // Layout the graph using proper type casting
    gvLayout((GVC_t*)analyzer->gvc, g, "dot");
    
    // Update the graph view widget
    gtk_widget_queue_draw(GTK_WIDGET(analyzer->details_view));

    // Cleanup with proper type casting
    gvFreeLayout((GVC_t*)analyzer->gvc, g);
    agclose(g);
    
    return TRUE; // Remove source after updating
}

gboolean venv_analyzer_scan(VenvAnalyzer* analyzer, GError** error) {
    if (!analyzer->venv_path[0]) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   "No virtual environment path set");
        return FALSE;
    }

    char* output = execute_pip_command(analyzer->venv_path, "freeze");
    if (!output) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Failed to get package list: %s", venv_analyzer_get_last_error());
        return FALSE;
    }
    
    // Clear existing packages
    Package* pkg = analyzer->packages;
    while (pkg) {
        Package* next = pkg->next;
        package_free(pkg);
        pkg = next;
    }
    analyzer->packages = NULL;
    
    parse_pip_freeze(analyzer, output);
    g_free(output);

    for (pkg = analyzer->packages; pkg; pkg = pkg->next) {
        analyze_package_dependencies(analyzer, pkg);
        package_update_size_from_pip(pkg);
    }

    // Add these lines to update the GUI after scanning
    g_idle_add((GSourceFunc)venv_analyzer_update_package_list, analyzer);
    g_idle_add((GSourceFunc)venv_analyzer_update_graph_view, analyzer);
    
    return TRUE;
}

bool venv_analyzer_check_conflicts(VenvAnalyzer* analyzer) {
    bool has_conflicts = false;

    for (Package* pkg = analyzer->packages; pkg; pkg = pkg->next) {
        PackageDep* dep = pkg->dependencies;
        while (dep) {
            Package* dep_pkg = venv_analyzer_get_package(analyzer, dep->name);
            if (dep_pkg && !package_version_satisfies(dep_pkg->version, dep->version)) {
                package_add_conflict(pkg, dep_pkg->name, dep_pkg->version);
                has_conflicts = true;
            }
            dep = dep->next;
        }
    }
    
    return has_conflicts;
}

Package* venv_analyzer_get_package(VenvAnalyzer* analyzer, const char* name) {
    for (Package* pkg = analyzer->packages; pkg; pkg = pkg->next) {
        if (strcmp(pkg->name, name) == 0) {
            return pkg;
        }
    }
    return NULL;
}

const char* venv_analyzer_get_last_error(void) {
    return error_buffer[0] ? error_buffer : "No error";
}