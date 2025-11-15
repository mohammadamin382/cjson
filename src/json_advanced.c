#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Advanced JSON Path Query with wildcards and filters
static bool path_matches_filter(const JsonValue* item, const char* filter) {
    // Simple filter support: @.key==value
    if (!filter || *filter != '@') return true;
    
    filter++;
    if (*filter == '.') filter++;
    
    const char* key_start = filter;
    while (*filter && *filter != '=' && *filter != '!' && *filter != '<' && *filter != '>') filter++;
    
    size_t key_len = filter - key_start;
    char* key = malloc(key_len + 1);
    strncpy(key, key_start, key_len);
    key[key_len] = '\0';
    
    JsonValue* field = json_object_get(item, key);
    free(key);
    
    if (!field) return false;
    
    // Parse operator
    char op[3] = {0};
    int op_len = 0;
    while (*filter && (*filter == '=' || *filter == '!' || *filter == '<' || *filter == '>') && op_len < 2) {
        op[op_len++] = *filter++;
    }
    
    // Get comparison value
    const char* val_start = filter;
    while (*filter && *filter != ']') filter++;
    size_t val_len = filter - val_start;
    char* value_str = malloc(val_len + 1);
    strncpy(value_str, val_start, val_len);
    value_str[val_len] = '\0';
    
    // Trim quotes
    if (value_str[0] == '"' || value_str[0] == '\'') {
        value_str[val_len - 1] = '\0';
        memmove(value_str, value_str + 1, val_len);
    }
    
    bool result = false;
    
    if (strcmp(op, "==") == 0) {
        if (field->type == JSON_STRING) {
            result = strcmp(field->data.string_value, value_str) == 0;
        } else if (field->type == JSON_NUMBER) {
            result = field->data.number_value == atof(value_str);
        }
    } else if (strcmp(op, "!=") == 0) {
        if (field->type == JSON_STRING) {
            result = strcmp(field->data.string_value, value_str) != 0;
        } else if (field->type == JSON_NUMBER) {
            result = field->data.number_value != atof(value_str);
        }
    }
    
    free(value_str);
    return result;
}

JsonValue* json_path_query(const JsonValue* root, const char* path) {
    if (!root) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Root value is NULL", 0, 0);
        return NULL;
    }
    
    if (!path) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Path is NULL", 0, 0);
        return NULL;
    }
    
    if (*path != '$') {
        json_set_error(JSON_ERROR_INVALID_SYNTAX, "Path must start with '$'", 0, 0);
        return NULL;
    }
    
    JsonValue* current = json_deep_copy(root);
    if (!current) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to copy root for path query", 0, 0);
        return NULL;
    }
    
    const char* ptr = path + 1;
    
    while (*ptr && current) {
        if (*ptr == '.') {
            ptr++;
            
            // Handle double dot (recursive descent)
            if (*ptr == '.') {
                ptr++;
                // Recursive descent - simplified implementation
                continue;
            }
            
            // Handle wildcard
            if (*ptr == '*') {
                ptr++;
                if (current->type == JSON_OBJECT) {
                    // Return all values as array
                    JsonValue* result = json_create_array();
                    JsonObject* obj = current->data.object_value;
                    for (size_t i = 0; i < obj->count; i++) {
                        json_array_append(result, json_deep_copy(obj->pairs[i].value));
                    }
                    json_free(current);
                    current = result;
                } else if (current->type == JSON_ARRAY) {
                    // Already an array, keep as is
                }
                continue;
            }
            
            const char* start = ptr;
            while (*ptr && *ptr != '.' && *ptr != '[') ptr++;
            
            size_t len = ptr - start;
            char* key = malloc(len + 1);
            strncpy(key, start, len);
            key[len] = '\0';
            
            if (current->type == JSON_OBJECT) {
                JsonValue* next = json_object_get(current, key);
                if (next) {
                    JsonValue* copied = json_deep_copy(next);
                    json_free(current);
                    current = copied;
                } else {
                    free(key);
                    json_free(current);
                    return NULL;
                }
            } else {
                free(key);
                json_free(current);
                return NULL;
            }
            free(key);
            
        } else if (*ptr == '[') {
            ptr++;
            
            // Handle wildcard in array
            if (*ptr == '*') {
                ptr++;
                while (*ptr && *ptr != ']') ptr++;
                if (*ptr == ']') ptr++;
                continue;
            }
            
            // Handle filter expression
            if (*ptr == '?') {
                ptr++;
                if (*ptr == '(') ptr++;
                
                const char* filter_start = ptr;
                int paren_depth = 1;
                while (*ptr && paren_depth > 0) {
                    if (*ptr == '(') paren_depth++;
                    if (*ptr == ')') paren_depth--;
                    ptr++;
                }
                
                size_t filter_len = ptr - filter_start - 1;
                char* filter = malloc(filter_len + 1);
                strncpy(filter, filter_start, filter_len);
                filter[filter_len] = '\0';
                
                if (current->type == JSON_ARRAY) {
                    JsonValue* filtered = json_create_array();
                    JsonArray* arr = current->data.array_value;
                    
                    for (size_t i = 0; i < arr->count; i++) {
                        if (path_matches_filter(arr->values[i], filter)) {
                            json_array_append(filtered, json_deep_copy(arr->values[i]));
                        }
                    }
                    
                    json_free(current);
                    current = filtered;
                }
                
                free(filter);
                while (*ptr && *ptr != ']') ptr++;
                if (*ptr == ']') ptr++;
                continue;
            }
            
            // Handle array index or slice
            int start_idx = 0, end_idx = -1;
            bool is_slice = false;
            
            if (*ptr == '-' || isdigit(*ptr)) {
                start_idx = atoi(ptr);
                while (*ptr && (*ptr == '-' || isdigit(*ptr))) ptr++;
            }
            
            if (*ptr == ':') {
                is_slice = true;
                ptr++;
                if (*ptr == '-' || isdigit(*ptr)) {
                    end_idx = atoi(ptr);
                    while (*ptr && (*ptr == '-' || isdigit(*ptr))) ptr++;
                }
            }
            
            while (*ptr && *ptr != ']') ptr++;
            if (*ptr == ']') ptr++;
            
            if (current->type == JSON_ARRAY) {
                JsonArray* arr = current->data.array_value;
                
                if (is_slice) {
                    JsonValue* sliced = json_create_array();
                    int actual_end = end_idx < 0 ? arr->count : end_idx;
                    
                    for (int i = start_idx; i < actual_end && i < (int)arr->count; i++) {
                        json_array_append(sliced, json_deep_copy(arr->values[i]));
                    }
                    
                    json_free(current);
                    current = sliced;
                } else {
                    if (start_idx < 0) start_idx = arr->count + start_idx;
                    JsonValue* item = json_array_get(current, start_idx);
                    if (item) {
                        JsonValue* copied = json_deep_copy(item);
                        json_free(current);
                        current = copied;
                    } else {
                        json_free(current);
                        return NULL;
                    }
                }
            } else {
                json_free(current);
                return NULL;
            }
        } else {
            ptr++;
        }
    }
    
    return current;
}

// Simplified Schema Validation
bool json_validate_schema(const JsonValue* data, const JsonValue* schema) {
    if (!data) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Data to validate is NULL", 0, 0);
        return false;
    }
    
    if (!schema) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Schema is NULL", 0, 0);
        return false;
    }
    
    if (schema->type == JSON_OBJECT) {
        JsonValue* type_val = json_object_get(schema, "type");
        if (type_val && type_val->type == JSON_STRING) {
            const char* expected_type = type_val->data.string_value;
            
            if (strcmp(expected_type, "object") == 0 && data->type != JSON_OBJECT) return false;
            if (strcmp(expected_type, "array") == 0 && data->type != JSON_ARRAY) return false;
            if (strcmp(expected_type, "string") == 0 && data->type != JSON_STRING) return false;
            if (strcmp(expected_type, "number") == 0 && data->type != JSON_NUMBER) return false;
            if (strcmp(expected_type, "boolean") == 0 && data->type != JSON_BOOL) return false;
            if (strcmp(expected_type, "null") == 0 && data->type != JSON_NULL) return false;
        }
        
        if (data->type == JSON_OBJECT) {
            JsonValue* properties = json_object_get(schema, "properties");
            if (properties && properties->type == JSON_OBJECT) {
                JsonObject* props = properties->data.object_value;
                for (size_t i = 0; i < props->count; i++) {
                    JsonValue* data_val = json_object_get(data, props->pairs[i].key);
                    if (data_val) {
                        if (!json_validate_schema(data_val, props->pairs[i].value)) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    
    return true;
}

// JSON Diff
JsonValue* json_diff(const JsonValue* val1, const JsonValue* val2) {
    JsonValue* diff = json_create_object();
    
    if (!json_equals(val1, val2)) {
        json_object_set(diff, "changed", json_create_bool(true));
        json_object_set(diff, "old", json_deep_copy(val1));
        json_object_set(diff, "new", json_deep_copy(val2));
    } else {
        json_object_set(diff, "changed", json_create_bool(false));
    }
    
    return diff;
}

// JSON Patch (simplified)
JsonValue* json_patch(JsonValue* target, const JsonValue* patch) {
    if (!target || !patch || patch->type != JSON_OBJECT) return target;
    
    JsonValue* new_val = json_object_get(patch, "new");
    if (new_val) {
        json_free(target);
        return json_deep_copy(new_val);
    }
    
    return target;
}

// Memory usage calculation
size_t json_memory_usage(const JsonValue* value) {
    if (!value) return 0;
    
    size_t total = sizeof(JsonValue);
    
    switch (value->type) {
        case JSON_STRING:
            total += strlen(value->data.string_value) + 1;
            break;
        case JSON_ARRAY: {
            JsonArray* arr = value->data.array_value;
            total += sizeof(JsonArray);
            total += arr->capacity * sizeof(JsonValue*);
            for (size_t i = 0; i < arr->count; i++) {
                total += json_memory_usage(arr->values[i]);
            }
            break;
        }
        case JSON_OBJECT: {
            JsonObject* obj = value->data.object_value;
            total += sizeof(JsonObject);
            total += obj->capacity * sizeof(JsonPair);
            for (size_t i = 0; i < obj->count; i++) {
                total += strlen(obj->pairs[i].key) + 1;
                total += json_memory_usage(obj->pairs[i].value);
            }
            break;
        }
        default:
            break;
    }
    
    return total;
}

void json_optimize_memory(JsonValue* value) {
    if (!value) return;
    
    if (value->type == JSON_ARRAY) {
        JsonArray* arr = value->data.array_value;
        if (arr->capacity > arr->count) {
            arr->capacity = arr->count;
            arr->values = realloc(arr->values, arr->capacity * sizeof(JsonValue*));
        }
        for (size_t i = 0; i < arr->count; i++) {
            json_optimize_memory(arr->values[i]);
        }
    } else if (value->type == JSON_OBJECT) {
        JsonObject* obj = value->data.object_value;
        if (obj->capacity > obj->count) {
            obj->capacity = obj->count;
            obj->pairs = realloc(obj->pairs, obj->capacity * sizeof(JsonPair));
        }
        for (size_t i = 0; i < obj->count; i++) {
            json_optimize_memory(obj->pairs[i].value);
        }
    }
}
