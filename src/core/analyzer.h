#ifndef CORE_ANALYZER_H
#define CORE_ANALYZER_H

#include "../include/venv_analyzer.h"
#include "package.h"
#include <glib.h>
#include <stdbool.h>

// Error codes
typedef enum {
    ANALYZER_SUCCESS = 0,
    ANALYZER_ERROR_INVALID_PATH = -1,
    ANALYZER_ERROR_SCAN_FAILED = -2,
    ANALYZER_ERROR_NO_MEMORY = -3,
    ANALYZER_ERROR_DB_FAILED = -4
} AnalyzerError;

// Scan options
typedef struct {
    bool include_dev_packages;
    bool follow_global_packages;
    bool calculate_sizes;
    int max_depth;
} ScanOptions;

// Function declarations
/**
 * Creates a new analyzer instance
 * @param venv_path Path to Python virtual environment
 * @return New analyzer instance or NULL on error
 */
VenvAnalyzer* venv_analyzer_new(const char* venv_path);

/**
 * Frees analyzer instance and all resources
 */
void venv_analyzer_free(VenvAnalyzer* analyzer);

/**
 * Scans virtual environment with given options
 * @return Error code
 */
AnalyzerError venv_analyzer_scan_with_options(VenvAnalyzer* analyzer, const ScanOptions* options);

/**
 * Gets package dependencies as GList
 * @return List of Package* or NULL on error
 */
GList* venv_analyzer_get_dependencies(VenvAnalyzer* analyzer, const char* package_name);

/**
 * Checks for package conflicts
 * @return true if conflicts found
 */
bool venv_analyzer_check_conflicts(VenvAnalyzer* analyzer);

/**
 * Updates specified package
 * @return Error code
 */
AnalyzerError venv_analyzer_update_package(VenvAnalyzer* analyzer, 
                                         const char* package_name,
                                         const char* version);

/**
 * Gets last error message
 */
const char* venv_analyzer_get_last_error(void);

/**
 * Updates the package list in the GUI
 * @return TRUE to remove source, FALSE to keep it
 */
gboolean venv_analyzer_update_package_list(VenvAnalyzer* analyzer);

/**
 * Updates the graph view in the GUI
 * @return TRUE to remove source, FALSE to keep it
 */
gboolean venv_analyzer_update_graph_view(VenvAnalyzer* analyzer);

#endif // CORE_ANALYZER_H