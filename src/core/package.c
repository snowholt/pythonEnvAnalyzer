#include "package.h"
#include "package-private.h"
// #include "package_conflict.h" // Add this line to include the definition of PackageConflict
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#define COMMAND_BUFFER_SIZE 2048
#define OUTPUT_BUFFER_SIZE 4096
#define MAX_NAME_LEN 256
// #define MAX_PACKAGE_NAME MAX_NAME_LEN

static char error_message[256] = {0};

// Utility function to execute pip commands and capture output
static char* execute_pip_command(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        snprintf(error_message, sizeof(error_message), "Failed to execute: %s", command);
        return NULL;
    }

    char* output = malloc(OUTPUT_BUFFER_SIZE);
    size_t total = 0;
    char buffer[128];

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        if (total + len >= OUTPUT_BUFFER_SIZE) break;
        strcpy(output + total, buffer);
        total += len;
    }

    pclose(pipe);
    return output;
}

#include "../include/venv_analyzer.h"
#include "types.h"
#include <string.h>

// Remove the duplicate struct _Package definition since it's already in types.h
struct _PackageClass {
    GObjectClass parent_class;
};

// GObject type registration
G_DEFINE_TYPE(Package, package, G_TYPE_OBJECT)

static void
package_init(Package* self)
{
    strncpy(self->name, "", MAX_PACKAGE_NAME - 1);
    strncpy(self->version, "", MAX_VERSION_LEN - 1);
    strncpy(self->description, "", sizeof(self->description) - 1);  // Initialize description
    self->size = 0;
    self->dependencies = NULL;
    self->conflicts = NULL;
    self->next = NULL;
}

static void
package_finalize(GObject* object)
{
    Package* self = PACKAGE_PACKAGE(object);
    
    // Free linked list nodes only, not the name/version since they're arrays
    PackageDep* dep = self->dependencies;
    while (dep) {
        PackageDep* next = dep->next;
        free(dep);  // Don't free name/version since they're arrays
        dep = next;
    }
    
    PackageDep* conflict = self->conflicts;
    while (conflict) {
        PackageDep* next = conflict->next;
        free(conflict);  // Don't free name/version since they're arrays
        conflict = next;
    }
    
    G_OBJECT_CLASS(package_parent_class)->finalize(object);
}

static void
package_class_init(PackageClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = package_finalize;
}

// Public API Implementation

Package* 
package_new(const char* name, const char* version)
{
    Package* pkg = g_object_new(PACKAGE_TYPE, NULL);
    strncpy(pkg->name, name, MAX_PACKAGE_NAME - 1);
    strncpy(pkg->version, version, MAX_VERSION_LEN - 1);
    return pkg;
}

// Accessors
const char* package_get_name(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), NULL);
    return pkg->name;
}

const char* package_get_version(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), NULL);
    return pkg->version;
}

const char* package_get_description(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), NULL);
    return pkg->description;
}

const PackageDep* package_get_dependencies(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), NULL);
    return pkg->dependencies;
}

const PackageDep* package_get_conflicts(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), NULL);
    return pkg->conflicts;
}

gsize package_get_size(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), 0);
    return pkg->size;
}

Package* package_get_next(Package* pkg)
{
    g_return_val_if_fail(PACKAGE_IS_PACKAGE(pkg), NULL);
    return pkg->next;
}

void package_set_next(Package* pkg, Package* next)
{
    g_return_if_fail(PACKAGE_IS_PACKAGE(pkg));
    pkg->next = next;
}

bool package_update_size_from_pip(Package* package) {
    char command[COMMAND_BUFFER_SIZE];
    snprintf(command, sizeof(command), "pip show %s", package->name);
    
    char* output = execute_pip_command(command);
    if (!output) return false;

    // Parse pip show output for location
    char* location = strstr(output, "Location: ");
    if (location) {
        location += 10; // Skip "Location: "
        char* newline = strchr(location, '\n');
        if (newline) *newline = '\0';

        // Construct path safely using g_build_filename
        char* pkg_path = g_build_filename(location, package->name, NULL);
        if (!pkg_path) {
            free(output);
            return false;
        }

        // Use g_build_filename for command construction
        char* quoted_path = g_shell_quote(pkg_path);
        char* du_command = g_strdup_printf("du -sb %s 2>/dev/null", quoted_path);
        
        char* size_output = execute_pip_command(du_command);
        if (size_output) {
            package->size = strtoull(size_output, NULL, 10);
            free(size_output);
        }

        g_free(quoted_path);
        g_free(du_command);
        g_free(pkg_path);
    }

    free(output);
    return true;
}



void package_add_dependency(Package* package, const char* name, const char* version) {
    PackageDep* dep = calloc(1, sizeof(PackageDep));
    if (!dep) return;

    strncpy(dep->name, name, MAX_PACKAGE_NAME - 1);
    strncpy(dep->version, version, MAX_VERSION_LEN - 1);
    dep->next = package->dependencies;
    package->dependencies = dep;
}

void package_add_conflict(Package* package, const char* name, const char* version) {
    PackageDep* conflict = calloc(1, sizeof(PackageDep));
    if (!conflict) return;

    strncpy(conflict->name, name, MAX_PACKAGE_NAME - 1);
    strncpy(conflict->version, version, MAX_VERSION_LEN - 1);
    conflict->next = package->conflicts;
    package->conflicts = conflict;
}

bool package_has_dependency(Package* package, const char* name) {
    PackageDep* dep = package->dependencies;
    while (dep) {
        if (strcmp(dep->name, name) == 0) return true;
        dep = dep->next;
    }
    return false;
}

bool package_version_satisfies(const char* version, const char* requirement) {
    // Handle wildcard version requirement
    if (strcmp(requirement, "*") == 0) return true;
    
    // Parse version strings into components
    int v1[3] = {0}, v2[3] = {0};
    if (sscanf(version, "%d.%d.%d", &v1[0], &v1[1], &v1[2]) < 1 ||
        sscanf(requirement, "%d.%d.%d", &v2[0], &v2[1], &v2[2]) < 1) {
        return false;
    }

    // Compare version components
    for (int i = 0; i < 3; i++) {
        if (v1[i] < v2[i]) return false;
        if (v1[i] > v2[i]) return true;
    }

    return true;  // Versions are equal
}


void package_free(Package* package) {
    if (!package) return;

    // Free dependencies
    PackageDep* dep = package->dependencies;
    while (dep) {
        PackageDep* next = dep->next;
        g_free(dep);
        dep = next;
    }

    // Free conflicts
    PackageDep* conflict = package->conflicts;
    while (conflict) {
        PackageDep* next = conflict->next;
        g_free(conflict);
        conflict = next;
    }

    // Since we're using GObject, we need to use g_object_unref
    g_object_unref(package);
}

VersionCompareResult package_compare_versions(const char* version1, const char* version2) {
    int v1[3] = {0}, v2[3] = {0};
    
    if (sscanf(version1, "%d.%d.%d", &v1[0], &v1[1], &v1[2]) < 1 ||
        sscanf(version2, "%d.%d.%d", &v2[0], &v2[1], &v2[2]) < 1) {
        return VERSION_ERROR;
    }

    for (int i = 0; i < 3; i++) {
        if (v1[i] < v2[i]) return VERSION_LESS;
        if (v1[i] > v2[i]) return VERSION_GREATER;
    }

    return VERSION_EQUAL;
}

const char* package_get_last_error(void) {
    return error_message[0] ? error_message : "No error";
}