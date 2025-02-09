#ifndef TYPES_H
#define TYPES_H

#include <glib.h>

#define MAX_PACKAGE_NAME 256
#define MAX_VERSION_LEN 64
#define MAX_PATH_LEN 4096  // Add this definition

typedef struct _PackageDep {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_VERSION_LEN];
    struct _PackageDep* next;
} PackageDep;

typedef struct _Package {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_VERSION_LEN];
    char description[1024];  // Add description field
    size_t size;
    PackageDep* dependencies;
    PackageDep* conflicts;  // Changed from PackageConflict to PackageDep for consistency
    struct _Package* next;
} Package;

typedef enum {
    VERSION_ERROR = -1,
    VERSION_LESS = 0,
    VERSION_EQUAL = 1,
    VERSION_GREATER = 2
} VersionCompareResult;

#endif // TYPES_H