#ifndef VENV_ANALYZER_H
#define VENV_ANALYZER_H

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <graphviz/gvc.h>
#include <sqlite3.h>

G_BEGIN_DECLS

// Forward declarations
typedef struct _Package Package;
typedef struct _PackageDep PackageDep;
typedef struct _VenvAnalyzer VenvAnalyzer;

// Maximum lengths
#define MAX_PACKAGE_NAME 256
#define MAX_VERSION_LEN 64
#define MAX_PATH_LEN 1024
#define MAX_DESC_LEN 1024

// Error domains
#define VENV_ANALYZER_ERROR (venv_analyzer_error_quark())
GQuark venv_analyzer_error_quark(void);

typedef enum {
    VENV_ANALYZER_ERROR_INVALID_PATH,
    VENV_ANALYZER_ERROR_SCAN_FAILED,
    VENV_ANALYZER_ERROR_DB_FAILED,
    VENV_ANALYZER_ERROR_EXPORT_FAILED
} VenvAnalyzerError;

// Package filter flags
typedef enum {
    PACKAGE_FILTER_NONE = 0,
    PACKAGE_FILTER_DIRECT = 1 << 0,
    PACKAGE_FILTER_DEPS = 1 << 1,
    PACKAGE_FILTER_CONFLICTS = 1 << 2
} PackageFilterFlags;

// Package dependency structure
struct _PackageDep {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_VERSION_LEN];
    PackageDep* next;
};

// Package structure
struct _Package {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_VERSION_LEN];
    char description[MAX_DESC_LEN];
    size_t size;
    PackageDep* dependencies;
    PackageDep* conflicts;
    Package* next;
};

// Main analyzer structure
struct _VenvAnalyzer {
    char venv_path[MAX_PATH_LEN];
    Package* packages;
    GVC_t* gvc;          // Add GraphViz context
    sqlite3* db;         // Add SQLite database handle
    GtkWidget* status_bar;
    GtkWidget* details_view;
    GListStore* package_store;
    GtkSelectionModel* selection_model;
};

// Core functions
VenvAnalyzer*   venv_analyzer_new         (const char* venv_path);
void            venv_analyzer_free        (VenvAnalyzer* analyzer);
gboolean        venv_analyzer_scan        (VenvAnalyzer* analyzer, 
                                          GError** error);
Package*        venv_analyzer_get_package (VenvAnalyzer* analyzer, 
                                          const char* name);
gboolean        venv_analyzer_save_to_db  (VenvAnalyzer* analyzer, 
                                          GError** error);
gboolean        venv_analyzer_load_from_db(VenvAnalyzer* analyzer, 
                                          GError** error);
gboolean        venv_analyzer_export_json (VenvAnalyzer* analyzer,
                                         const char* filepath,
                                         GError** error);
gboolean        venv_analyzer_export_dot  (VenvAnalyzer* analyzer,
                                         const char* filepath,
                                         GError** error);
void            venv_analyzer_set_filter  (VenvAnalyzer* analyzer,
                                         PackageFilterFlags flags);
void            venv_analyzer_set_sort    (VenvAnalyzer* analyzer,
                                         gboolean ascending);
void            venv_analyzer_set_search  (VenvAnalyzer* analyzer,
                                         const char* term);

// Version comparison utilities
int             venv_analyzer_version_compare(const char* ver1,
                                           const char* ver2);
gboolean        venv_analyzer_version_satisfies(const char* version,
                                             const char* constraint);

G_END_DECLS

#endif // VENV_ANALYZER_H