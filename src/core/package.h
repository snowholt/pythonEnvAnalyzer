#ifndef PACKAGE_H
#define PACKAGE_H

#include "types.h"
#include <stdio.h>

typedef enum {
    VERSION_ERROR = -1,
    VERSION_LESS = 0,
    VERSION_EQUAL = 1,
    VERSION_GREATER = 2
} VersionCompareResult;

// Function declarations
Package* package_new(const char* name, const char* version);
void package_free(Package* pkg);
void package_add_dependency(Package* pkg, const char* name, const char* version);
void package_add_conflict(Package* pkg, const char* name, const char* version);
bool package_update_size_from_pip(Package* pkg);
bool package_version_satisfies(const char* version, const char* requirement);
VersionCompareResult package_compare_versions(const char* version1, const char* version2);

#endif // PACKAGE_H
