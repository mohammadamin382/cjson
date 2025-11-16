#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <sqlite3.h>

static char* sanitize_name(const char* name) {
    if (!name) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Table/column name is NULL", 0, 0);
        return NULL;
    }
    
    size_t len = strlen(name);
    if (len == 0) {
        json_set_error(JSON_ERROR_INVALID_SYNTAX, "Table/column name is empty", 0, 0);
        return NULL;
    }
    
    if (len > 255) {
        json_set_error(JSON_ERROR_SQLITE_ERROR, "Table/column name too long (>255 chars)", 0, 0);
        return NULL;
    }
    
    // Check for SQL keywords to prevent injection
    const char* sql_keywords[] = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "DROP", "CREATE", "ALTER", 
        "UNION", "WHERE", "FROM", "JOIN", "EXEC", "EXECUTE", NULL
    };
    
    for (int i = 0; sql_keywords[i]; i++) {
        if (strcasecmp(name, sql_keywords[i]) == 0) {
            json_set_error(JSON_ERROR_INVALID_SYNTAX, "Name cannot be SQL keyword", 0, 0);
            return NULL;
        }
    }
    
    char* result = malloc(len + 1);
    if (!result) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate sanitized name", 0, 0);
        return NULL;
    }
    
    size_t j = 0;
    
    // First character must be letter or underscore
    if (!isalpha(name[0]) && name[0] != '_') {
        free(result);
        json_set_error(JSON_ERROR_INVALID_SYNTAX, "Name must start with letter or underscore", 0, 0);
        return NULL;
    }
    
    for (size_t i = 0; i < len; i++) {
        if (isalnum(name[i]) || name[i] == '_') {
            result[j++] = name[i];
        }
    }
    
    if (j == 0) {
        free(result);
        json_set_error(JSON_ERROR_INVALID_SYNTAX, "Name contains no valid characters", 0, 0);
        return NULL;
    }
    
    result[j] = '\0';
    return result;
}

static bool create_table_from_json(sqlite3* db, const char* table_name, const JsonValue* sample) {
    if (!db || !table_name || !sample) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Invalid arguments to create_table_from_json", 0, 0);
        return false;
    }
    
    if (sample->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Sample must be a JSON object", 0, 0);
        return false;
    }
    
    char* safe_table = sanitize_name(table_name);
    if (!safe_table) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize table name", 0, 0);
        return false;
    }
    
    char query[4096];
    int written = snprintf(query, sizeof(query), "CREATE TABLE IF NOT EXISTS %s (id INTEGER PRIMARY KEY AUTOINCREMENT", safe_table);
    
    if (written >= sizeof(query) - 100) {
        free(safe_table);
        json_set_error(JSON_ERROR_SQLITE_ERROR, "Query too long for buffer", 0, 0);
        return false;
    }
    
    JsonObject* obj = sample->data.object_value;
    for (size_t i = 0; i < obj->count; i++) {
        char* safe_col = sanitize_name(obj->pairs[i].key);
        if (!safe_col) continue;
        
        if (strlen(query) + strlen(safe_col) + 50 >= sizeof(query)) {
            free(safe_col);
            free(safe_table);
            json_set_error(JSON_ERROR_SQLITE_ERROR, "Query buffer overflow", 0, 0);
            return false;
        }
        
        strcat(query, ", ");
        strcat(query, safe_col);
        
        switch (obj->pairs[i].value->type) {
            case JSON_NUMBER:
                strcat(query, " REAL");
                break;
            case JSON_BOOL:
                strcat(query, " INTEGER");
                break;
            default:
                strcat(query, " TEXT");
                break;
        }
        free(safe_col);
    }
    strcat(query, ")");
    
    char* err_msg = NULL;
    int rc = sqlite3_exec(db, query, NULL, NULL, &err_msg);
    free(safe_table);
    
    if (rc != SQLITE_OK) {
        char error_buf[512];
        snprintf(error_buf, sizeof(error_buf), "Failed to create table: %s", err_msg ? err_msg : "unknown error");
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_buf, 0, 0);
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    
    return true;
}

JsonSqliteDB* json_to_sqlite(const JsonValue* value, const char* db_path) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Input JSON value is NULL", 0, 0);
        return NULL;
    }
    
    if (!db_path) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Database path is NULL", 0, 0);
        return NULL;
    }
    
    sqlite3* db;
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to open database: %s", sqlite3_errmsg(db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        sqlite3_close(db);
        return NULL;
    }
    
    char* err_msg = NULL;
    
    // Advanced optimizations with error checking
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA synchronous=NORMAL", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA cache_size=100000", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA page_size=32768", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA temp_store=MEMORY", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA mmap_size=268435456", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    if (sqlite3_exec(db, "PRAGMA auto_vacuum=INCREMENTAL", NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        err_msg = NULL;
    }
    
    if (value->type == JSON_ARRAY) {
        JsonArray* arr = value->data.array_value;
        if (arr->count > 0 && arr->values[0]->type == JSON_OBJECT) {
            create_table_from_json(db, "data", arr->values[0]);
            
            sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
            
            for (size_t i = 0; i < arr->count; i++) {
                if (arr->values[i]->type != JSON_OBJECT) continue;
                
                JsonObject* obj = arr->values[i]->data.object_value;
                char query[4096] = "INSERT INTO data (";
                char values[4096] = " VALUES (";
                
                for (size_t j = 0; j < obj->count; j++) {
                    char* safe_col = sanitize_name(obj->pairs[j].key);
                    strcat(query, safe_col);
                    free(safe_col);
                    
                    if (j < obj->count - 1) strcat(query, ", ");
                }
                strcat(query, ")");
                
                for (size_t j = 0; j < obj->count; j++) {
                    JsonValue* val = obj->pairs[j].value;
                    
                    if (val->type == JSON_STRING) {
                        strcat(values, "'");
                        strcat(values, val->data.string_value);
                        strcat(values, "'");
                    } else if (val->type == JSON_NUMBER) {
                        char num[64];
                        snprintf(num, sizeof(num), "%g", val->data.number_value);
                        strcat(values, num);
                    } else if (val->type == JSON_BOOL) {
                        strcat(values, val->data.bool_value ? "1" : "0");
                    } else {
                        strcat(values, "NULL");
                    }
                    
                    if (j < obj->count - 1) strcat(values, ", ");
                }
                strcat(values, ")");
                strcat(query, values);
                
                sqlite3_exec(db, query, NULL, NULL, NULL);
            }
            
            sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        }
    } else if (value->type == JSON_OBJECT) {
        JsonObject* root = value->data.object_value;
        
        for (size_t i = 0; i < root->count; i++) {
            if (root->pairs[i].value->type == JSON_ARRAY) {
                JsonArray* arr = root->pairs[i].value->data.array_value;
                if (arr->count > 0 && arr->values[0]->type == JSON_OBJECT) {
                    create_table_from_json(db, root->pairs[i].key, arr->values[0]);
                }
            }
        }
    }
    
    JsonSqliteDB* result = malloc(sizeof(JsonSqliteDB));
    result->db = db;
    result->path = malloc(strlen(db_path) + 1);
    strcpy(result->path, db_path);
    result->auto_optimize = true;
    
    return result;
}

JsonValue* sqlite_to_json(const char* db_path, const char* table_name) {
    if (!db_path) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Database path is NULL", 0, 0);
        return NULL;
    }
    
    if (!table_name) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Table name is NULL", 0, 0);
        return NULL;
    }
    
    sqlite3* db;
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Cannot open database: %s", sqlite3_errmsg(db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        sqlite3_close(db);
        return NULL;
    }
    
    char query[256];
    char* safe_table = sanitize_name(table_name);
    
    if (!safe_table) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize table name", 0, 0);
        sqlite3_close(db);
        return NULL;
    }
    
    int written = snprintf(query, sizeof(query), "SELECT * FROM %s", safe_table);
    free(safe_table);
    
    if (written >= sizeof(query)) {
        json_set_error(JSON_ERROR_SQLITE_ERROR, "Query too long", 0, 0);
        sqlite3_close(db);
        return NULL;
    }
    
    sqlite3_stmt* stmt;
    JsonValue* array = json_create_array();
    
    if (!array) {
        sqlite3_close(db);
        return NULL;
    }
    
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to prepare query: %s", sqlite3_errmsg(db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        json_free(array);
        sqlite3_close(db);
        return NULL;
    }
    
    int col_count = sqlite3_column_count(stmt);
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        JsonValue* obj = json_create_object();
        if (!obj) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            json_free(array);
            return NULL;
        }
        
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            if (!col_name) continue;
            
            int col_type = sqlite3_column_type(stmt, i);
            
            JsonValue* value = NULL;
            switch (col_type) {
                case SQLITE_INTEGER:
                    value = json_create_number(sqlite3_column_int(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    value = json_create_number(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT: {
                    const char* text = (const char*)sqlite3_column_text(stmt, i);
                    value = json_create_string(text ? text : "");
                    break;
                }
                case SQLITE_NULL:
                default:
                    value = json_create_null();
                    break;
            }
            
            if (value) {
                json_object_set(obj, col_name, value);
            }
        }
        
        json_array_append(array, obj);
    }
    
    if (rc != SQLITE_DONE) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error reading results: %s", sqlite3_errmsg(db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return array;
}

bool sqlite_insert(JsonSqliteDB* db, const char* table, const JsonValue* value) {
    if (!db) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Database handle is NULL", 0, 0);
        return false;
    }
    
    if (!table) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Table name is NULL", 0, 0);
        return false;
    }
    
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Value to insert is NULL", 0, 0);
        return false;
    }
    
    if (value->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Value must be a JSON object", 0, 0);
        return false;
    }
    
    JsonObject* obj = value->data.object_value;
    
    if (obj->count == 0) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Cannot insert empty object", 0, 0);
        return false;
    }
    
    char query[4096] = "INSERT INTO ";
    char* safe_table = sanitize_name(table);
    
    if (!safe_table) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize table name", 0, 0);
        return false;
    }
    
    if (strlen(query) + strlen(safe_table) + 10 >= sizeof(query)) {
        free(safe_table);
        json_set_error(JSON_ERROR_SQLITE_ERROR, "Query buffer too small", 0, 0);
        return false;
    }
    
    strcat(query, safe_table);
    strcat(query, " (");
    free(safe_table);
    
    for (size_t i = 0; i < obj->count; i++) {
        char* safe_col = sanitize_name(obj->pairs[i].key);
        if (!safe_col) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize column name", 0, 0);
            return false;
        }
        
        if (strlen(query) + strlen(safe_col) + 10 >= sizeof(query)) {
            free(safe_col);
            json_set_error(JSON_ERROR_SQLITE_ERROR, "Query buffer overflow", 0, 0);
            return false;
        }
        
        strcat(query, safe_col);
        free(safe_col);
        if (i < obj->count - 1) strcat(query, ", ");
    }
    strcat(query, ") VALUES (");
    
    for (size_t i = 0; i < obj->count; i++) {
        strcat(query, "?");
        if (i < obj->count - 1) strcat(query, ", ");
    }
    strcat(query, ")");
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to prepare statement: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        return false;
    }
    
    for (size_t i = 0; i < obj->count; i++) {
        JsonValue* val = obj->pairs[i].value;
        int bind_rc = SQLITE_OK;
        
        switch (val->type) {
            case JSON_NUMBER:
                bind_rc = sqlite3_bind_double(stmt, i + 1, val->data.number_value);
                break;
            case JSON_STRING:
                bind_rc = sqlite3_bind_text(stmt, i + 1, val->data.string_value, -1, SQLITE_TRANSIENT);
                break;
            case JSON_BOOL:
                bind_rc = sqlite3_bind_int(stmt, i + 1, val->data.bool_value ? 1 : 0);
                break;
            default:
                bind_rc = sqlite3_bind_null(stmt, i + 1);
                break;
        }
        
        if (bind_rc != SQLITE_OK) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Failed to bind parameter %zu: %s", i, sqlite3_errmsg(db->db));
            json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
            sqlite3_finalize(stmt);
            return false;
        }
    }
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Insert failed: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
    }
    
    sqlite3_finalize(stmt);
    
    if (success && db->auto_optimize) {
        static int insert_count = 0;
        if (++insert_count % 1000 == 0) {
            sqlite_optimize(db);
        }
    }
    
    return success;
}

JsonValue* sqlite_query(JsonSqliteDB* db, const char* table, const char* key, const char* value) {
    if (!db) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Database handle is NULL", 0, 0);
        return NULL;
    }
    
    if (!table || !key || !value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Table, key or value is NULL", 0, 0);
        return NULL;
    }
    
    char query[512];
    char* safe_table = sanitize_name(table);
    char* safe_key = sanitize_name(key);
    
    if (!safe_table || !safe_key) {
        if (safe_table) free(safe_table);
        if (safe_key) free(safe_key);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize names", 0, 0);
        return NULL;
    }
    
    int written = snprintf(query, sizeof(query), "SELECT * FROM %s WHERE %s = ?", safe_table, safe_key);
    free(safe_table);
    free(safe_key);
    
    if (written >= sizeof(query)) {
        json_set_error(JSON_ERROR_SQLITE_ERROR, "Query too long", 0, 0);
        return NULL;
    }
    
    sqlite3_stmt* stmt;
    JsonValue* array = json_create_array();
    
    if (!array) {
        return NULL;
    }
    
    int rc = sqlite3_prepare_v2(db->db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to prepare query: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        json_free(array);
        return NULL;
    }
    
    rc = sqlite3_bind_text(stmt, 1, value, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to bind value: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        sqlite3_finalize(stmt);
        json_free(array);
        return NULL;
    }
    
    int col_count = sqlite3_column_count(stmt);
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        JsonValue* obj = json_create_object();
        if (!obj) {
            sqlite3_finalize(stmt);
            json_free(array);
            return NULL;
        }
        
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            if (!col_name) continue;
            
            int col_type = sqlite3_column_type(stmt, i);
            
            JsonValue* val = NULL;
            switch (col_type) {
                case SQLITE_INTEGER:
                    val = json_create_number(sqlite3_column_int(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    val = json_create_number(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT: {
                    const char* text = (const char*)sqlite3_column_text(stmt, i);
                    val = json_create_string(text ? text : "");
                    break;
                }
                default:
                    val = json_create_null();
                    break;
            }
            
            if (val) {
                json_object_set(obj, col_name, val);
            }
        }
        
        json_array_append(array, obj);
    }
    
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Query execution error: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
    }
    
    sqlite3_finalize(stmt);
    return array;
}

JsonValue* sqlite_get_all(JsonSqliteDB* db, const char* table) {
    return sqlite_to_json(db->path, table);
}

bool sqlite_update(JsonSqliteDB* db, const char* table, const char* key, const char* key_value, const JsonValue* new_data) {
    if (!db || !table || !key || !key_value || !new_data) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Invalid NULL argument to sqlite_update", 0, 0);
        return false;
    }
    
    if (new_data->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Update data must be a JSON object", 0, 0);
        return false;
    }
    
    JsonObject* obj = new_data->data.object_value;
    
    if (obj->count == 0) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Cannot update with empty object", 0, 0);
        return false;
    }
    
    char query[4096] = "UPDATE ";
    char* safe_table = sanitize_name(table);
    
    if (!safe_table) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize table name", 0, 0);
        return false;
    }
    
    strcat(query, safe_table);
    strcat(query, " SET ");
    free(safe_table);
    
    for (size_t i = 0; i < obj->count; i++) {
        char* safe_col = sanitize_name(obj->pairs[i].key);
        if (!safe_col) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize column name", 0, 0);
            return false;
        }
        
        if (strlen(query) + strlen(safe_col) + 20 >= sizeof(query)) {
            free(safe_col);
            json_set_error(JSON_ERROR_SQLITE_ERROR, "Update query too long", 0, 0);
            return false;
        }
        
        strcat(query, safe_col);
        strcat(query, " = ?");
        free(safe_col);
        if (i < obj->count - 1) strcat(query, ", ");
    }
    
    char* safe_key = sanitize_name(key);
    if (!safe_key) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize key name", 0, 0);
        return false;
    }
    
    strcat(query, " WHERE ");
    strcat(query, safe_key);
    strcat(query, " = ?");
    free(safe_key);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to prepare update: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        return false;
    }
    
    for (size_t i = 0; i < obj->count; i++) {
        JsonValue* val = obj->pairs[i].value;
        int bind_rc = SQLITE_OK;
        
        switch (val->type) {
            case JSON_NUMBER:
                bind_rc = sqlite3_bind_double(stmt, i + 1, val->data.number_value);
                break;
            case JSON_STRING:
                bind_rc = sqlite3_bind_text(stmt, i + 1, val->data.string_value, -1, SQLITE_TRANSIENT);
                break;
            case JSON_BOOL:
                bind_rc = sqlite3_bind_int(stmt, i + 1, val->data.bool_value ? 1 : 0);
                break;
            default:
                bind_rc = sqlite3_bind_null(stmt, i + 1);
                break;
        }
        
        if (bind_rc != SQLITE_OK) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Failed to bind parameter: %s", sqlite3_errmsg(db->db));
            json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
            sqlite3_finalize(stmt);
            return false;
        }
    }
    
    rc = sqlite3_bind_text(stmt, obj->count + 1, key_value, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to bind WHERE value: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        sqlite3_finalize(stmt);
        return false;
    }
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Update failed: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool sqlite_delete(JsonSqliteDB* db, const char* table, const char* key, const char* value) {
    if (!db || !table || !key || !value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Invalid NULL argument to sqlite_delete", 0, 0);
        return false;
    }
    
    char query[512];
    char* safe_table = sanitize_name(table);
    char* safe_key = sanitize_name(key);
    
    if (!safe_table || !safe_key) {
        if (safe_table) free(safe_table);
        if (safe_key) free(safe_key);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to sanitize names", 0, 0);
        return false;
    }
    
    int written = snprintf(query, sizeof(query), "DELETE FROM %s WHERE %s = ?", safe_table, safe_key);
    free(safe_table);
    free(safe_key);
    
    if (written >= sizeof(query)) {
        json_set_error(JSON_ERROR_SQLITE_ERROR, "Delete query too long", 0, 0);
        return false;
    }
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to prepare delete: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        return false;
    }
    
    rc = sqlite3_bind_text(stmt, 1, value, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to bind value: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
        sqlite3_finalize(stmt);
        return false;
    }
    
    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Delete failed: %s", sqlite3_errmsg(db->db));
        json_set_error(JSON_ERROR_SQLITE_ERROR, error_msg, 0, 0);
    }
    
    sqlite3_finalize(stmt);
    return success;
}

void sqlite_optimize(JsonSqliteDB* db) {
    if (!db || !db->db) return;
    
    // Comprehensive analysis
    sqlite3_exec(db->db, "ANALYZE", NULL, NULL, NULL);
    sqlite3_exec(db->db, "PRAGMA optimize", NULL, NULL, NULL);
    
    // Get all tables
    sqlite3_stmt* stmt;
    const char* query = "SELECT name FROM sqlite_master WHERE type='table'";
    
    if (sqlite3_prepare_v2(db->db, query, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* table_name = (const char*)sqlite3_column_text(stmt, 0);
            
            // Create index on id column
            char idx_query[512];
            snprintf(idx_query, sizeof(idx_query), 
                    "CREATE INDEX IF NOT EXISTS idx_%s_id ON %s(id)", 
                    table_name, table_name);
            sqlite3_exec(db->db, idx_query, NULL, NULL, NULL);
            
            // Get all columns and create composite indexes for common patterns
            char col_query[512];
            snprintf(col_query, sizeof(col_query), "PRAGMA table_info(%s)", table_name);
            
            sqlite3_stmt* col_stmt;
            if (sqlite3_prepare_v2(db->db, col_query, -1, &col_stmt, NULL) == SQLITE_OK) {
                char columns[10][128];
                int col_count = 0;
                
                while (sqlite3_step(col_stmt) == SQLITE_ROW && col_count < 10) {
                    const char* col_name = (const char*)sqlite3_column_text(col_stmt, 1);
                    if (strcmp(col_name, "id") != 0) {
                        strncpy(columns[col_count], col_name, 127);
                        columns[col_count][127] = '\0';
                        
                        // Create individual index for each column
                        snprintf(idx_query, sizeof(idx_query),
                                "CREATE INDEX IF NOT EXISTS idx_%s_%s ON %s(%s)",
                                table_name, col_name, table_name, col_name);
                        sqlite3_exec(db->db, idx_query, NULL, NULL, NULL);
                        
                        col_count++;
                    }
                }
                sqlite3_finalize(col_stmt);
            }
        }
        sqlite3_finalize(stmt);
    }
    
    // Incremental vacuum
    sqlite3_exec(db->db, "PRAGMA incremental_vacuum", NULL, NULL, NULL);
    
    // Checkpoint WAL
    sqlite3_exec(db->db, "PRAGMA wal_checkpoint(TRUNCATE)", NULL, NULL, NULL);
}

void sqlite_close(JsonSqliteDB* db) {
    if (!db) return;
    
    if (db->auto_optimize) {
        sqlite_optimize(db);
    }
    
    sqlite3_close(db->db);
    free(db->path);
    free(db);
}
