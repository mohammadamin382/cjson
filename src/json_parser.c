#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

// Global error state
JsonError g_json_last_error = {JSON_ERROR_NONE, "", 0, 0, NULL};

// Error handling implementation
const char* json_error_message(JsonErrorCode code) {
    switch (code) {
        case JSON_ERROR_NONE: return "No error";
        case JSON_ERROR_INVALID_SYNTAX: return "Invalid JSON syntax";
        case JSON_ERROR_UNEXPECTED_TOKEN: return "Unexpected token";
        case JSON_ERROR_UNTERMINATED_STRING: return "Unterminated string";
        case JSON_ERROR_INVALID_NUMBER: return "Invalid number format";
        case JSON_ERROR_INVALID_ESCAPE: return "Invalid escape sequence";
        case JSON_ERROR_UNEXPECTED_EOF: return "Unexpected end of file";
        case JSON_ERROR_INVALID_UTF8: return "Invalid UTF-8 encoding";
        case JSON_ERROR_STACK_OVERFLOW: return "Stack overflow (nesting too deep)";
        case JSON_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case JSON_ERROR_FILE_NOT_FOUND: return "File not found";
        case JSON_ERROR_FILE_READ_ERROR: return "File read error";
        case JSON_ERROR_FILE_WRITE_ERROR: return "File write error";
        case JSON_ERROR_INVALID_TYPE: return "Invalid type for operation";
        case JSON_ERROR_KEY_NOT_FOUND: return "Key not found";
        case JSON_ERROR_INDEX_OUT_OF_BOUNDS: return "Index out of bounds";
        case JSON_ERROR_NULL_POINTER: return "Null pointer";
        case JSON_ERROR_SQLITE_ERROR: return "SQLite error";
        case JSON_ERROR_CONVERSION_FAILED: return "Format conversion failed";
        default: return "Unknown error";
    }
}

JsonError* json_get_last_error(void) {
    return &g_json_last_error;
}

void json_clear_error(void) {
    g_json_last_error.code = JSON_ERROR_NONE;
    g_json_last_error.message[0] = '\0';
    g_json_last_error.line = 0;
    g_json_last_error.column = 0;
    g_json_last_error.context = NULL;
}

void json_set_error(JsonErrorCode code, const char* message, size_t line, size_t column) {
    g_json_last_error.code = code;
    snprintf(g_json_last_error.message, sizeof(g_json_last_error.message), 
             "%s: %s", json_error_message(code), message ? message : "");
    g_json_last_error.line = line;
    g_json_last_error.column = column;
}

typedef struct {
    const char* start;
    const char* current;
    size_t line;
    size_t column;
    int depth;
} Parser;

static void skip_whitespace(Parser* parser) {
    while (*parser->current) {
        if (*parser->current == '\n') {
            parser->line++;
            parser->column = 0;
        } else if (!isspace(*parser->current)) {
            break;
        }
        parser->current++;
        parser->column++;
    }
}

static unsigned int parse_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static int encode_utf8(unsigned int codepoint, char* output) {
    if (codepoint <= 0x7F) {
        output[0] = (char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        output[0] = (char)(0xC0 | (codepoint >> 6));
        output[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint <= 0xFFFF) {
        output[0] = (char)(0xE0 | (codepoint >> 12));
        output[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        output[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        output[0] = (char)(0xF0 | (codepoint >> 18));
        output[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        output[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        output[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }
    json_set_error(JSON_ERROR_INVALID_UTF8, "Invalid Unicode codepoint", 0, 0);
    return 0;
}

static char* parse_string_value(Parser* parser) {
    if (*parser->current != '"') {
        json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected '\"'", parser->line, parser->column);
        return NULL;
    }
    parser->current++;
    parser->column++;
    
    size_t length = 0;
    size_t capacity = 64;
    char* result = malloc(capacity);
    if (!result) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate string", parser->line, parser->column);
        return NULL;
    }
    
    while (*parser->current && *parser->current != '"') {
        if (length + 5 >= capacity) {
            capacity *= 2;
            char* new_result = realloc(result, capacity);
            if (!new_result) {
                free(result);
                json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand string buffer", parser->line, parser->column);
                return NULL;
            }
            result = new_result;
        }
        
        if (*parser->current == '\\') {
            parser->current++;
            parser->column++;
            if (!*parser->current) {
                free(result);
                json_set_error(JSON_ERROR_UNTERMINATED_STRING, "Unterminated escape sequence", parser->line, parser->column);
                return NULL;
            }
            
            switch (*parser->current) {
                case 'n': result[length++] = '\n'; break;
                case 't': result[length++] = '\t'; break;
                case 'r': result[length++] = '\r'; break;
                case 'b': result[length++] = '\b'; break;
                case 'f': result[length++] = '\f'; break;
                case '"': result[length++] = '"'; break;
                case '\\': result[length++] = '\\'; break;
                case '/': result[length++] = '/'; break;
                case 'u': {
                    parser->current++;
                    parser->column++;
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        if (!*parser->current || !isxdigit(*parser->current)) {
                            free(result);
                            json_set_error(JSON_ERROR_INVALID_ESCAPE, "Invalid Unicode escape", parser->line, parser->column);
                            return NULL;
                        }
                        codepoint = (codepoint << 4) | parse_hex_digit(*parser->current);
                        parser->current++;
                        parser->column++;
                    }
                    parser->current--;
                    parser->column--;
                    
                    // Handle UTF-16 surrogate pairs
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        if (parser->current[1] == '\\' && parser->current[2] == 'u') {
                            parser->current += 3;
                            parser->column += 3;
                            unsigned int low_surrogate = 0;
                            for (int i = 0; i < 4; i++) {
                                if (!*parser->current || !isxdigit(*parser->current)) {
                                    free(result);
                                    json_set_error(JSON_ERROR_INVALID_ESCAPE, "Invalid surrogate pair", parser->line, parser->column);
                                    return NULL;
                                }
                                low_surrogate = (low_surrogate << 4) | parse_hex_digit(*parser->current);
                                parser->current++;
                                parser->column++;
                            }
                            parser->current--;
                            parser->column--;
                            
                            if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
                                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low_surrogate - 0xDC00);
                            }
                        }
                    }
                    
                    char utf8_buffer[5];
                    int utf8_len = encode_utf8(codepoint, utf8_buffer);
                    if (utf8_len == 0) {
                        free(result);
                        return NULL;
                    }
                    for (int i = 0; i < utf8_len; i++) {
                        result[length++] = utf8_buffer[i];
                    }
                    break;
                }
                default:
                    free(result);
                    json_set_error(JSON_ERROR_INVALID_ESCAPE, "Unknown escape sequence", parser->line, parser->column);
                    return NULL;
            }
            parser->current++;
            parser->column++;
        } else {
            result[length++] = *parser->current++;
            parser->column++;
        }
    }
    
    if (*parser->current != '"') {
        free(result);
        json_set_error(JSON_ERROR_UNTERMINATED_STRING, "Expected closing '\"'", parser->line, parser->column);
        return NULL;
    }
    parser->current++;
    parser->column++;
    
    result[length] = '\0';
    return result;
}

static double parse_number_value(Parser* parser) {
    char* end;
    errno = 0;
    double value = strtod(parser->current, &end);
    
    if (errno == ERANGE) {
        json_set_error(JSON_ERROR_INVALID_NUMBER, "Number out of range", parser->line, parser->column);
        return 0.0;
    }
    
    if (end == parser->current) {
        json_set_error(JSON_ERROR_INVALID_NUMBER, "Invalid number format", parser->line, parser->column);
        return 0.0;
    }
    
    parser->column += (end - parser->current);
    parser->current = end;
    return value;
}

static JsonValue* parse_value(Parser* parser);

static JsonValue* parse_null(Parser* parser) {
    if (strncmp(parser->current, "null", 4) == 0) {
        parser->current += 4;
        parser->column += 4;
        return json_create_null();
    }
    json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected 'null'", parser->line, parser->column);
    return NULL;
}

static JsonValue* parse_bool(Parser* parser) {
    if (strncmp(parser->current, "true", 4) == 0) {
        parser->current += 4;
        parser->column += 4;
        return json_create_bool(true);
    }
    if (strncmp(parser->current, "false", 5) == 0) {
        parser->current += 5;
        parser->column += 5;
        return json_create_bool(false);
    }
    json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected 'true' or 'false'", parser->line, parser->column);
    return NULL;
}

#define MAX_NESTING_DEPTH 1000

static JsonValue* parse_array(Parser* parser) {
    if (*parser->current != '[') {
        json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected '['", parser->line, parser->column);
        return NULL;
    }
    
    if (++parser->depth > MAX_NESTING_DEPTH) {
        json_set_error(JSON_ERROR_STACK_OVERFLOW, "Nesting too deep", parser->line, parser->column);
        return NULL;
    }
    
    parser->current++;
    parser->column++;
    
    JsonValue* array = json_create_array();
    if (!array) return NULL;
    
    skip_whitespace(parser);
    
    if (*parser->current == ']') {
        parser->current++;
        parser->column++;
        parser->depth--;
        return array;
    }
    
    while (true) {
        skip_whitespace(parser);
        JsonValue* value = parse_value(parser);
        if (!value) {
            json_free(array);
            parser->depth--;
            return NULL;
        }
        
        if (!json_array_append(array, value)) {
            json_free(value);
            json_free(array);
            parser->depth--;
            return NULL;
        }
        
        skip_whitespace(parser);
        if (*parser->current == ',') {
            parser->current++;
            parser->column++;
        } else if (*parser->current == ']') {
            parser->current++;
            parser->column++;
            break;
        } else {
            json_free(array);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected ',' or ']'", parser->line, parser->column);
            parser->depth--;
            return NULL;
        }
    }
    
    parser->depth--;
    return array;
}

static JsonValue* parse_object(Parser* parser) {
    if (*parser->current != '{') {
        json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected '{'", parser->line, parser->column);
        return NULL;
    }
    
    if (++parser->depth > MAX_NESTING_DEPTH) {
        json_set_error(JSON_ERROR_STACK_OVERFLOW, "Nesting too deep", parser->line, parser->column);
        return NULL;
    }
    
    parser->current++;
    parser->column++;
    
    JsonValue* object = json_create_object();
    if (!object) {
        parser->depth--;
        return NULL;
    }
    
    skip_whitespace(parser);
    
    if (*parser->current == '}') {
        parser->current++;
        parser->column++;
        parser->depth--;
        return object;
    }
    
    while (true) {
        skip_whitespace(parser);
        
        if (*parser->current != '"') {
            json_free(object);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected string key", parser->line, parser->column);
            parser->depth--;
            return NULL;
        }
        
        char* key = parse_string_value(parser);
        if (!key) {
            json_free(object);
            parser->depth--;
            return NULL;
        }
        
        skip_whitespace(parser);
        if (*parser->current != ':') {
            free(key);
            json_free(object);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected ':'", parser->line, parser->column);
            parser->depth--;
            return NULL;
        }
        parser->current++;
        parser->column++;
        
        skip_whitespace(parser);
        JsonValue* value = parse_value(parser);
        if (!value) {
            free(key);
            json_free(object);
            parser->depth--;
            return NULL;
        }
        
        if (!json_object_set(object, key, value)) {
            free(key);
            json_free(value);
            json_free(object);
            parser->depth--;
            return NULL;
        }
        free(key);
        
        skip_whitespace(parser);
        if (*parser->current == ',') {
            parser->current++;
            parser->column++;
        } else if (*parser->current == '}') {
            parser->current++;
            parser->column++;
            break;
        } else {
            json_free(object);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected ',' or '}'", parser->line, parser->column);
            parser->depth--;
            return NULL;
        }
    }
    
    parser->depth--;
    return object;
}

static JsonValue* parse_value(Parser* parser) {
    skip_whitespace(parser);
    
    if (!*parser->current) {
        json_set_error(JSON_ERROR_UNEXPECTED_EOF, "Unexpected end of input", parser->line, parser->column);
        return NULL;
    }
    
    if (*parser->current == 'n') return parse_null(parser);
    if (*parser->current == 't' || *parser->current == 'f') return parse_bool(parser);
    if (*parser->current == '"') {
        char* str = parse_string_value(parser);
        if (!str) return NULL;
        JsonValue* val = json_create_string(str);
        free(str);
        return val;
    }
    if (*parser->current == '[') return parse_array(parser);
    if (*parser->current == '{') return parse_object(parser);
    if (*parser->current == '-' || isdigit(*parser->current)) {
        double num = parse_number_value(parser);
        if (g_json_last_error.code != JSON_ERROR_NONE) return NULL;
        return json_create_number(num);
    }
    
    json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Unexpected character", parser->line, parser->column);
    return NULL;
}

JsonValue* json_parse_with_error(const char* json_string, JsonError* error) {
    json_clear_error();
    
    if (!json_string) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Input string is NULL", 0, 0);
        if (error) *error = g_json_last_error;
        return NULL;
    }
    
    Parser parser = { json_string, json_string, 1, 0, 0 };
    JsonValue* result = parse_value(&parser);
    
    if (error) *error = g_json_last_error;
    return result;
}

JsonValue* json_parse(const char* json_string) {
    return json_parse_with_error(json_string, NULL);
}

// Safe constructors
JsonValue* json_create_null(void) {
    JsonValue* value = malloc(sizeof(JsonValue));
    if (!value) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonValue", 0, 0);
        return NULL;
    }
    value->type = JSON_NULL;
    return value;
}

JsonValue* json_create_bool(bool val) {
    JsonValue* value = malloc(sizeof(JsonValue));
    if (!value) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonValue", 0, 0);
        return NULL;
    }
    value->type = JSON_BOOL;
    value->data.bool_value = val;
    return value;
}

JsonValue* json_create_number(double val) {
    if (isnan(val) || isinf(val)) {
        json_set_error(JSON_ERROR_INVALID_NUMBER, "Number is NaN or Infinity", 0, 0);
        return NULL;
    }
    
    JsonValue* value = malloc(sizeof(JsonValue));
    if (!value) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonValue", 0, 0);
        return NULL;
    }
    value->type = JSON_NUMBER;
    value->data.number_value = val;
    return value;
}

JsonValue* json_create_string(const char* val) {
    if (!val) {
        json_set_error(JSON_ERROR_NULL_POINTER, "String value is NULL", 0, 0);
        return NULL;
    }
    
    JsonValue* value = malloc(sizeof(JsonValue));
    if (!value) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonValue", 0, 0);
        return NULL;
    }
    
    value->type = JSON_STRING;
    value->data.string_value = malloc(strlen(val) + 1);
    if (!value->data.string_value) {
        free(value);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate string", 0, 0);
        return NULL;
    }
    strcpy(value->data.string_value, val);
    return value;
}

JsonValue* json_create_array(void) {
    JsonValue* value = malloc(sizeof(JsonValue));
    if (!value) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonValue", 0, 0);
        return NULL;
    }
    
    value->type = JSON_ARRAY;
    value->data.array_value = calloc(1, sizeof(JsonArray));
    if (!value->data.array_value) {
        free(value);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonArray", 0, 0);
        return NULL;
    }
    return value;
}

JsonValue* json_create_object(void) {
    JsonValue* value = malloc(sizeof(JsonValue));
    if (!value) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonValue", 0, 0);
        return NULL;
    }
    
    value->type = JSON_OBJECT;
    value->data.object_value = calloc(1, sizeof(JsonObject));
    if (!value->data.object_value) {
        free(value);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate JsonObject", 0, 0);
        return NULL;
    }
    return value;
}

// Safe operations
bool json_array_append(JsonValue* array, JsonValue* value) {
    if (!array || !value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Array or value is NULL", 0, 0);
        return false;
    }
    
    if (array->type != JSON_ARRAY) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an array", 0, 0);
        return false;
    }
    
    JsonArray* arr = array->data.array_value;
    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        JsonValue** new_values = realloc(arr->values, new_capacity * sizeof(JsonValue*));
        if (!new_values) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand array", 0, 0);
            return false;
        }
        arr->values = new_values;
        arr->capacity = new_capacity;
    }
    arr->values[arr->count++] = value;
    return true;
}

bool json_object_set(JsonValue* object, const char* key, JsonValue* value) {
    if (!object || !key || !value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Object, key or value is NULL", 0, 0);
        return false;
    }
    
    if (object->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an object", 0, 0);
        return false;
    }
    
    JsonObject* obj = object->data.object_value;
    
    // Check if key exists
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            json_free(obj->pairs[i].value);
            obj->pairs[i].value = value;
            return true;
        }
    }
    
    // Add new key
    if (obj->count >= obj->capacity) {
        size_t new_capacity = obj->capacity == 0 ? 8 : obj->capacity * 2;
        JsonPair* new_pairs = realloc(obj->pairs, new_capacity * sizeof(JsonPair));
        if (!new_pairs) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand object", 0, 0);
            return false;
        }
        obj->pairs = new_pairs;
        obj->capacity = new_capacity;
    }
    
    obj->pairs[obj->count].key = malloc(strlen(key) + 1);
    if (!obj->pairs[obj->count].key) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate key", 0, 0);
        return false;
    }
    strcpy(obj->pairs[obj->count].key, key);
    obj->pairs[obj->count].value = value;
    obj->count++;
    return true;
}

JsonValue* json_object_get(const JsonValue* object, const char* key) {
    if (!object || !key) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Object or key is NULL", 0, 0);
        return NULL;
    }
    
    if (object->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an object", 0, 0);
        return NULL;
    }
    
    JsonObject* obj = object->data.object_value;
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            return obj->pairs[i].value;
        }
    }
    
    json_set_error(JSON_ERROR_KEY_NOT_FOUND, key, 0, 0);
    return NULL;
}

JsonValue* json_array_get(const JsonValue* array, size_t index) {
    if (!array) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Array is NULL", 0, 0);
        return NULL;
    }
    
    if (array->type != JSON_ARRAY) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an array", 0, 0);
        return NULL;
    }
    
    if (index >= array->data.array_value->count) {
        json_set_error(JSON_ERROR_INDEX_OUT_OF_BOUNDS, "Index out of bounds", 0, 0);
        return NULL;
    }
    
    return array->data.array_value->values[index];
}

// Type checking
bool json_is_null(const JsonValue* value) { return value && value->type == JSON_NULL; }
bool json_is_bool(const JsonValue* value) { return value && value->type == JSON_BOOL; }
bool json_is_number(const JsonValue* value) { return value && value->type == JSON_NUMBER; }
bool json_is_string(const JsonValue* value) { return value && value->type == JSON_STRING; }
bool json_is_array(const JsonValue* value) { return value && value->type == JSON_ARRAY; }
bool json_is_object(const JsonValue* value) { return value && value->type == JSON_OBJECT; }

size_t json_array_size(const JsonValue* array) {
    if (!array || array->type != JSON_ARRAY) return 0;
    return array->data.array_value->count;
}

size_t json_object_size(const JsonValue* object) {
    if (!object || object->type != JSON_OBJECT) return 0;
    return object->data.object_value->count;
}

bool json_object_has(const JsonValue* object, const char* key) {
    if (!object || object->type != JSON_OBJECT || !key) return false;
    JsonObject* obj = object->data.object_value;
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) return true;
    }
    return false;
}

void json_free(JsonValue* value) {
    if (!value) return;
    
    switch (value->type) {
        case JSON_STRING:
            free(value->data.string_value);
            break;
        case JSON_ARRAY: {
            JsonArray* arr = value->data.array_value;
            for (size_t i = 0; i < arr->count; i++) {
                json_free(arr->values[i]);
            }
            free(arr->values);
            free(arr);
            break;
        }
        case JSON_OBJECT: {
            JsonObject* obj = value->data.object_value;
            for (size_t i = 0; i < obj->count; i++) {
                free(obj->pairs[i].key);
                json_free(obj->pairs[i].value);
            }
            free(obj->pairs);
            free(obj);
            break;
        }
        default:
            break;
    }
    free(value);
}

bool json_validate(const char* json_string) {
    json_clear_error();
    JsonValue* value = json_parse(json_string);
    if (value) {
        json_free(value);
        return true;
    }
    return false;
}
