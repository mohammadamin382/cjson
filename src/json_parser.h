#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    JSON_ERROR_NONE = 0,
    JSON_ERROR_INVALID_SYNTAX,
    JSON_ERROR_UNEXPECTED_TOKEN,
    JSON_ERROR_UNTERMINATED_STRING,
    JSON_ERROR_INVALID_NUMBER,
    JSON_ERROR_INVALID_ESCAPE,
    JSON_ERROR_UNEXPECTED_EOF,
    JSON_ERROR_INVALID_UTF8,
    JSON_ERROR_STACK_OVERFLOW,
    JSON_ERROR_OUT_OF_MEMORY,
    JSON_ERROR_FILE_NOT_FOUND,
    JSON_ERROR_FILE_READ_ERROR,
    JSON_ERROR_FILE_WRITE_ERROR,
    JSON_ERROR_INVALID_TYPE,
    JSON_ERROR_KEY_NOT_FOUND,
    JSON_ERROR_INDEX_OUT_OF_BOUNDS,
    JSON_ERROR_NULL_POINTER,
    JSON_ERROR_SQLITE_ERROR,
    JSON_ERROR_CONVERSION_FAILED
} JsonErrorCode;

typedef struct {
    JsonErrorCode code;
    char message[512];
    size_t line;
    size_t column;
    const char* context;
} JsonError;

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct {
    void* db;
    char* path;
    bool auto_optimize;
    JsonError last_error;
} JsonSqliteDB;

typedef struct JsonValue JsonValue;
typedef struct JsonObject JsonObject;
typedef struct JsonArray JsonArray;

struct JsonValue {
    JsonType type;
    union {
        bool bool_value;
        double number_value;
        char* string_value;
        JsonArray* array_value;
        JsonObject* object_value;
    } data;
};

typedef struct {
    char* key;
    JsonValue* value;
} JsonPair;

struct JsonObject {
    JsonPair* pairs;
    size_t count;
    size_t capacity;
};

struct JsonArray {
    JsonValue** values;
    size_t count;
    size_t capacity;
};

// Global error state
extern JsonError g_json_last_error;

// Error handling functions
const char* json_error_message(JsonErrorCode code);
JsonError* json_get_last_error(void);
void json_clear_error(void);
void json_set_error(JsonErrorCode code, const char* message, size_t line, size_t column);

// Core parsing with error handling
JsonValue* json_parse(const char* json_string);
JsonValue* json_parse_with_error(const char* json_string, JsonError* error);
char* json_stringify(const JsonValue* value, bool pretty);
void json_free(JsonValue* value);

// Safe constructors with validation
JsonValue* json_create_null(void);
JsonValue* json_create_bool(bool value);
JsonValue* json_create_number(double value);
JsonValue* json_create_string(const char* value);
JsonValue* json_create_array(void);
JsonValue* json_create_object(void);

// Safe array operations
bool json_array_append(JsonValue* array, JsonValue* value);
JsonValue* json_array_get(const JsonValue* array, size_t index);
bool json_array_remove(JsonValue* array, size_t index);
size_t json_array_size(const JsonValue* array);

// Safe object operations
bool json_object_set(JsonValue* object, const char* key, JsonValue* value);
JsonValue* json_object_get(const JsonValue* object, const char* key);
bool json_object_has(const JsonValue* object, const char* key);
bool json_object_remove(JsonValue* object, const char* key);
size_t json_object_size(const JsonValue* object);
const char** json_object_keys(const JsonValue* object, size_t* count);

// Type checking
bool json_is_null(const JsonValue* value);
bool json_is_bool(const JsonValue* value);
bool json_is_number(const JsonValue* value);
bool json_is_string(const JsonValue* value);
bool json_is_array(const JsonValue* value);
bool json_is_object(const JsonValue* value);

// Format conversion with error handling
char* json_to_xml(const JsonValue* value);
char* json_to_yaml(const JsonValue* value);
char* json_to_csv(const JsonValue* value);
char* json_to_ini(const JsonValue* value);

bool json_validate(const char* json_string);
JsonValue* json_merge(const JsonValue* obj1, const JsonValue* obj2);
JsonValue* json_deep_copy(const JsonValue* value);
bool json_equals(const JsonValue* val1, const JsonValue* val2);

// Enhanced file I/O with error handling
JsonValue* json_parse_file(const char* filename);
bool json_save_file(const char* filename, const JsonValue* value, bool pretty);

// Advanced parsers with validation
JsonValue* xml_to_json(const char* xml);
JsonValue* yaml_to_json(const char* yaml);
JsonValue* csv_to_json(const char* csv);
JsonValue* ini_to_json(const char* ini);

// Enhanced SQLite integration
JsonSqliteDB* json_to_sqlite(const JsonValue* value, const char* db_path);
JsonValue* sqlite_to_json(const char* db_path, const char* table_name);
bool sqlite_insert(JsonSqliteDB* db, const char* table, const JsonValue* value);
JsonValue* sqlite_query(JsonSqliteDB* db, const char* table, const char* key, const char* value);
JsonValue* sqlite_get_all(JsonSqliteDB* db, const char* table);
bool sqlite_update(JsonSqliteDB* db, const char* table, const char* key, const char* key_value, const JsonValue* new_data);
bool sqlite_delete(JsonSqliteDB* db, const char* table, const char* key, const char* value);
void sqlite_optimize(JsonSqliteDB* db);
void sqlite_close(JsonSqliteDB* db);

// JSON Path queries with error handling
JsonValue* json_path_query(const JsonValue* root, const char* path);

// JSON Schema validation
bool json_validate_schema(const JsonValue* data, const JsonValue* schema);

// JSON Diff & Patch
JsonValue* json_diff(const JsonValue* val1, const JsonValue* val2);
JsonValue* json_patch(JsonValue* target, const JsonValue* patch);

// Performance & Memory
size_t json_memory_usage(const JsonValue* value);
void json_optimize_memory(JsonValue* value);

#endif
