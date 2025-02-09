#ifndef PACKAGE_H
#define PACKAGE_H

#include <glib-object.h>
#include <stdbool.h>
#include "types.h"

G_BEGIN_DECLS

#define PACKAGE_TYPE (package_get_type())
G_DECLARE_FINAL_TYPE(Package, package, PACKAGE, PACKAGE, GObject)

// Constructor
Package* package_new(const char* name, const char* version);
void package_free(Package* package);

// Accessors
const char* package_get_name(Package* pkg);
const char* package_get_version(Package* pkg);
const char* package_get_description(Package* pkg);
const PackageDep* package_get_dependencies(Package* pkg);
const PackageDep* package_get_conflicts(Package* pkg);  // Changed from PackageConflict to PackageDep
gsize package_get_size(Package* pkg);
Package* package_get_next(Package* pkg);
void package_set_next(Package* pkg, Package* next);

// Operations
void package_add_dependency(Package* pkg, const char* name, const char* version);
void package_add_conflict(Package* pkg, const char* name, const char* version);
bool package_has_dependency(Package* pkg, const char* name);
bool package_update_size_from_pip(Package* pkg);
void package_set_size(Package* package, size_t size);

// Version comparison
bool package_version_satisfies(const char* version, const char* requirement);
VersionCompareResult package_compare_versions(const char* ver1, const char* ver2);

G_END_DECLS

#endif // PACKAGE_H
