#include "analyzer.h"
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>

#define COMMAND_BUFFER_SIZE 1024
#define OUTPUT_BUFFER_SIZE 4096

static char error_buffer[256] = {0};

// Helper function to execute pip commands
static char* execute_pip_command(const char* venv_path, const char* command) {
    // Construct full pip path
    char pip_path[COMMAND_BUFFER_SIZE];
    g_snprintf(pip_path, sizeof(pip_path), "%s/bin/pip", venv_path);
    
    // Verify pip exists
    if (!g_file_test(pip_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE)) {
        g_snprintf(error_buffer, sizeof(error_buffer),
                  "pip not found in virtual environment: %s", pip_path);
        return NULL;
    }
    
    // Build full command
    char full_command[COMMAND_BUFFER_SIZE];
    g_snprintf(full_command, sizeof(full_command),
              "%s %s", pip_path, command);
    
    GError* error = NULL;
    char* stdout_data = NULL;
    char* stderr_data = NULL;
    int exit_status;
    
    g_spawn_command_line_sync(full_command,
                             &stdout_data,
                             &stderr_data,
                             &exit_status,
                             &error);
    
    if (error) {
        g_snprintf(error_buffer, sizeof(error_buffer),
                  "Command failed: %s", error->message);
        g_error_free(error);
        g_free(stdout_data);
        g_free(stderr_data);
        return NULL;
    }
    
    g_free(stderr_data);
    return stdout_data;
}

VenvAnalyzer* venv_analyzer_new(const char* path) {
    VenvAnalyzer* analyzer = g_new0(VenvAnalyzer, 1);
    if (!analyzer) return NULL;

    g_strlcpy(analyzer->venv_path, path, sizeof(analyzer->venv_path));
    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
        g_snprintf(error_buffer, sizeof(error_buffer),
                  "Invalid venv path: %s", path);
        g_free(analyzer);
        return NULL;
    }
    
    analyzer->packages = NULL;
    analyzer->gvc = gvContext();
    analyzer->db = NULL;  // Initialize database handle
    analyzer->status_bar = NULL;
    analyzer->details_view = NULL;
    analyzer->package_store = NULL;
    analyzer->selection_model = NULL;
    
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
    
    // Free GraphViz context
    if (analyzer->gvc) {
        gvFreeContext(analyzer->gvc);
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
    char command[COMMAND_BUFFER_SIZE];
    g_snprintf(command, sizeof(command), "show %s", package->name);
    
    char* output = execute_pip_command(analyzer->venv_path, command);
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

gboolean
venv_analyzer_scan(VenvAnalyzer* analyzer, GError** error)
{
    char* output = execute_pip_command(analyzer->venv_path, "freeze");
    if (!output) return -1;
    
    // Clear existing packages
    Package* pkg = analyzer->packages;
    while (pkg) {
        Package* next = pkg->next;
        package_free(pkg);
        pkg = next;
    }
    analyzer->packages = NULL;
    
    // Parse pip freeze output
    parse_pip_freeze(analyzer, output);
    g_free(output);
    
    // Analyze dependencies for each package
    for (pkg = analyzer->packages; pkg; pkg = pkg->next) {
        analyze_package_dependencies(analyzer, pkg);
        package_update_size_from_pip(pkg);
    }
    
    if (!analyzer->venv_path[0]) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   "No virtual environment path set");
        return FALSE;
    }
    
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