#include "database.h"
#include "../core/types.h"
#include "../core/package.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char error_message[256];
static const char* SCHEMA_FILE = "src/db/schema.sql";

// Helper function to execute SQL from file
static DbError execute_sql_file(sqlite3* db, const char* filepath) {
    char* sql = NULL;
    char* err_msg = NULL;
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        snprintf(error_message, sizeof(error_message), 
                "Failed to open schema file: %s", filepath);
        return DB_ERROR_INIT;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    sql = malloc(size + 1);
    if (!sql) {
        fclose(file);
        snprintf(error_message, sizeof(error_message), "Memory allocation failed");
        return DB_ERROR_INIT;
    }
    
    size_t read = fread(sql, 1, size, file);
    if (read != (size_t)size) {
        free(sql);
        fclose(file);
        snprintf(error_message, sizeof(error_message), "Failed to read schema file");
        return DB_ERROR_INIT;
    }
    sql[size] = '\0';
    fclose(file);
    
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Schema execution failed: %s", err_msg);
        sqlite3_free(err_msg);
        free(sql);
        return DB_ERROR_QUERY;
    }
    
    free(sql);
    return DB_SUCCESS;
}

// Database initialization
DbError db_init(VenvAnalyzer* analyzer, const char* db_path) {
    if (!analyzer || !db_path) {
        snprintf(error_message, sizeof(error_message), 
                "Invalid parameters: analyzer or db_path is NULL");
        return DB_ERROR_INIT;
    }

    int rc = sqlite3_open(db_path, &analyzer->db);
    if (rc != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Cannot open database: %s", sqlite3_errmsg(analyzer->db));
        return DB_ERROR_INIT;
    }
    
    return execute_sql_file(analyzer->db, SCHEMA_FILE);
}

// Database cleanup
void db_close(VenvAnalyzer* analyzer) {
    if (analyzer && analyzer->db) {
        sqlite3_close(analyzer->db);
        analyzer->db = NULL;
    }
}

// Transaction management
DbError db_begin_transaction(VenvAnalyzer* analyzer) {
    char* err_msg = NULL;
    if (sqlite3_exec(analyzer->db, "BEGIN TRANSACTION", NULL, NULL, &err_msg) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Failed to begin transaction: %s", err_msg);
        sqlite3_free(err_msg);
        return DB_ERROR_QUERY;
    }
    return DB_SUCCESS;
}

DbError db_commit(VenvAnalyzer* analyzer) {
    char* err_msg = NULL;
    if (sqlite3_exec(analyzer->db, "COMMIT", NULL, NULL, &err_msg) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Failed to commit transaction: %s", err_msg);
        sqlite3_free(err_msg);
        return DB_ERROR_QUERY;
    }
    return DB_SUCCESS;
}

DbError db_rollback(VenvAnalyzer* analyzer) {
    char* err_msg = NULL;
    if (sqlite3_exec(analyzer->db, "ROLLBACK", NULL, NULL, &err_msg) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Failed to rollback transaction: %s", err_msg);
        sqlite3_free(err_msg);
        return DB_ERROR_QUERY;
    }
    return DB_SUCCESS;
}

// Package operations
DbError db_insert_package(VenvAnalyzer* analyzer, Package* package) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO packages (name, version, size) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(analyzer->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Failed to prepare package insert statement: %s",
                sqlite3_errmsg(analyzer->db));
        return DB_ERROR_QUERY;
    }
    
    sqlite3_bind_text(stmt, 1, package->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, package->version, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, package->size);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        snprintf(error_message, sizeof(error_message),
                "Failed to insert package: %s",
                sqlite3_errmsg(analyzer->db));
        sqlite3_finalize(stmt);
        return DB_ERROR_QUERY;
    }
    
    sqlite3_finalize(stmt);
    return DB_SUCCESS;
}

// Dependency operations
DbError db_add_dependency(VenvAnalyzer* analyzer, 
                         const char* package_name,
                         const char* dep_name,
                         const char* version_constraint) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO dependencies "
                     "(package_id, dependency_name, version_constraint) "
                     "VALUES ((SELECT id FROM packages WHERE name = ?), ?, ?)";
    
    if (sqlite3_prepare_v2(analyzer->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Failed to prepare dependency statement: %s",
                sqlite3_errmsg(analyzer->db));
        return DB_ERROR_QUERY;
    }
    
    sqlite3_bind_text(stmt, 1, package_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, dep_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, version_constraint, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        snprintf(error_message, sizeof(error_message),
                "Failed to insert dependency: %s",
                sqlite3_errmsg(analyzer->db));
        sqlite3_finalize(stmt);
        return DB_ERROR_QUERY;
    }
    
    sqlite3_finalize(stmt);
    return DB_SUCCESS;
}

GList* db_get_dependencies(VenvAnalyzer* analyzer, const char* package_name) {
    sqlite3_stmt* stmt;
    GList* deps = NULL;
    const char* sql = "SELECT dependency_name, version_constraint "
                     "FROM dependencies d "
                     "JOIN packages p ON d.package_id = p.id "
                     "WHERE p.name = ?";
    
    if (sqlite3_prepare_v2(analyzer->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, package_name, -1, SQLITE_STATIC);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PackageDep* dep = g_new0(PackageDep, 1);
        if (dep) {
            strncpy(dep->name, (const char*)sqlite3_column_text(stmt, 0), MAX_PACKAGE_NAME - 1);
            strncpy(dep->version, (const char*)sqlite3_column_text(stmt, 1), MAX_VERSION_LEN - 1);
            dep->next = NULL;
            deps = g_list_prepend(deps, dep);
        }
    }
    
    sqlite3_finalize(stmt);
    return deps;
}

DbError db_remove_dependency(VenvAnalyzer* analyzer,
                           const char* package_name,
                           const char* dep_name) {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM dependencies "
                     "WHERE package_id = (SELECT id FROM packages WHERE name = ?) "
                     "AND dependency_name = ?";
    
    if (sqlite3_prepare_v2(analyzer->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        snprintf(error_message, sizeof(error_message),
                "Failed to prepare remove dependency statement: %s",
                sqlite3_errmsg(analyzer->db));
        return DB_ERROR_QUERY;
    }
    
    sqlite3_bind_text(stmt, 1, package_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, dep_name, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        snprintf(error_message, sizeof(error_message),
                "Failed to remove dependency: %s",
                sqlite3_errmsg(analyzer->db));
        sqlite3_finalize(stmt);
        return DB_ERROR_QUERY;
    }
    
    sqlite3_finalize(stmt);
    return DB_SUCCESS;
}

// Error handling
const char* db_get_last_error(void) {
    return error_message[0] ? error_message : "No error";
}