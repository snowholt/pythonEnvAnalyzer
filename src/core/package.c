#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

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

Package* package_new(const char* name, const char* version) {
    Package* pkg = calloc(1, sizeof(Package));
    if (!pkg) {
        snprintf(error_message, sizeof(error_message), "Memory allocation failed");
        return NULL;
    }

    strncpy(pkg->name, name, MAX_PACKAGE_NAME - 1);
    strncpy(pkg->version, version, MAX_VERSION_LEN - 1);
    pkg->size = 0;
    pkg->dependencies = NULL;
    pkg->conflicts = NULL;
    pkg->next = NULL;

    return pkg;
}

void package_free(Package* package) {
    if (!package) return;

    // Free dependencies
    PackageDep* dep = package->dependencies;
    while (dep) {
        PackageDep* next = dep->next;
        free(dep);
        dep = next;
    }

    // Free conflicts
    dep = package->conflicts;
    while (dep) {
        PackageDep* next = dep->next;
        free(dep);
        dep = next;
    }

    free(package);
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

        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", location, package->name);

        // Calculate directory size
        char du_command[COMMAND_BUFFER_SIZE];
        snprintf(du_command, sizeof(du_command), "du -sb %s 2>/dev/null", path);
        
        char* size_output = execute_pip_command(du_command);
        if (size_output) {
            package->size = strtoull(size_output, NULL, 10);
            free(size_output);
        }
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