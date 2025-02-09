#ifndef DB_DATABASE_H
#define DB_DATABASE_H

#include "../include/venv_analyzer.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <glib.h>

// Database error codes
typedef enum {
    DB_SUCCESS = 0,
    DB_ERROR_INIT = -1,
    DB_ERROR_QUERY = -2,
    DB_ERROR_CONSTRAINT = -3,
    DB_ERROR_NOT_FOUND = -4
} DbError;

// Database initialization and cleanup
DbError db_init(VenvAnalyzer* analyzer, const char* db_path);
void db_close(VenvAnalyzer* analyzer);

// Transaction management
DbError db_begin_transaction(VenvAnalyzer* analyzer);
DbError db_commit(VenvAnalyzer* analyzer);
DbError db_rollback(VenvAnalyzer* analyzer);

// Package CRUD operations
DbError db_insert_package(VenvAnalyzer* analyzer, Package* package);
DbError db_update_package(VenvAnalyzer* analyzer, Package* package);
DbError db_delete_package(VenvAnalyzer* analyzer, const char* name);
Package* db_get_package(VenvAnalyzer* analyzer, const char* name);
GList* db_get_all_packages(VenvAnalyzer* analyzer);

// Dependency operations
DbError db_add_dependency(VenvAnalyzer* analyzer, 
                         const char* package_name,
                         const char* dep_name,
                         const char* version_constraint);
DbError db_remove_dependency(VenvAnalyzer* analyzer,
                           const char* package_name,
                           const char* dep_name);
GList* db_get_dependencies(VenvAnalyzer* analyzer, const char* package_name);

// Settings management
DbError db_save_setting(VenvAnalyzer* analyzer, 
                       const char* key, 
                       const char* value);
char* db_get_setting(VenvAnalyzer* analyzer, const char* key);

// State management
DbError db_save_state(VenvAnalyzer* analyzer);
DbError db_load_state(VenvAnalyzer* analyzer);

// Error handling
const char* db_get_last_error(void);

#endif // DB_DATABASE_H