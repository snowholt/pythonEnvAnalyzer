#ifndef VENV_ANALYZER_H
#define VENV_ANALYZER_H

#include <gtk/gtk.h>
#include <graphviz/gvc.h>
#include <sqlite3.h>
#include "../src/core/types.h"  // Add this line

// Constants
#define MAX_PATH_LEN 4096
#define MAX_VERSION_LEN 64
#define MAX_PACKAGE_NAME 256
#define MAX_DESC_LEN 1024

G_BEGIN_DECLS

// Forward declarations
typedef struct _Package Package;
typedef struct _PackageDep PackageDep;
typedef struct _PackageConflict PackageConflict;

// Main analyzer structure
typedef struct {
    char venv_path[MAX_PATH_LEN];
    Package* packages;
    GVC_t* gvc;  // Changed from GvContext to GVC_t
    sqlite3* db;
    
    // GUI components
    GtkWidget* status_bar;
    GtkWidget* details_view;
    GListStore* package_store;
    GtkSelectionModel* selection_model;
} VenvAnalyzer;

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