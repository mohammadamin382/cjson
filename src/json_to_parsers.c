#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void skip_xml_whitespace(const char** ptr) {
    while (**ptr && isspace(**ptr)) (*ptr)++;
}

static char* parse_xml_tag_name(const char** ptr) {
    const char* start = *ptr;
    while (**ptr && **ptr != '>' && **ptr != ' ' && **ptr != '/' && **ptr != '\t' && **ptr != '\n') {
        (*ptr)++;
    }
    
    size_t len = *ptr - start;
    if (len == 0) return NULL;
    
    char* name = malloc(len + 1);
    if (!name) return NULL;
    
    strncpy(name, start, len);
    name[len] = '\0';
    return name;
}

static char* parse_xml_content(const char** ptr) {
    const char* start = *ptr;
    size_t len = 0;
    
    while (**ptr && **ptr != '<') {
        len++;
        (*ptr)++;
    }
    
    if (len == 0) return NULL;
    
    char* content = malloc(len + 1);
    if (!content) return NULL;
    
    strncpy(content, start, len);
    content[len] = '\0';
    
    char* trimmed_start = content;
    while (*trimmed_start && isspace(*trimmed_start)) trimmed_start++;
    
    if (*trimmed_start == '\0') {
        free(content);
        return NULL;
    }
    
    char* trimmed_end = content + len - 1;
    while (trimmed_end > trimmed_start && isspace(*trimmed_end)) {
        *trimmed_end = '\0';
        trimmed_end--;
    }
    
    if (trimmed_start != content) {
        memmove(content, trimmed_start, strlen(trimmed_start) + 1);
    }
    
    return content;
}

static char* parse_xml_attr_value(const char** ptr) {
    skip_xml_whitespace(ptr);
    char quote = **ptr;
    if (quote != '"' && quote != '\'') return NULL;
    (*ptr)++;
    
    const char* start = *ptr;
    while (**ptr && **ptr != quote) (*ptr)++;
    
    size_t len = *ptr - start;
    char* value = malloc(len + 1);
    if (!value) return NULL;
    
    strncpy(value, start, len);
    value[len] = '\0';
    
    if (**ptr == quote) (*ptr)++;
    return value;
}

static JsonValue* parse_xml_node(const char** ptr) {
    skip_xml_whitespace(ptr);
    
    if (**ptr != '<') return NULL;
    (*ptr)++;
    
    if (**ptr == '?') {
        while (**ptr && !(**ptr == '?' && *(*ptr + 1) == '>')) (*ptr)++;
        if (**ptr == '?') (*ptr) += 2;
        return parse_xml_node(ptr);
    }
    
    if (**ptr == '!' && *(*ptr + 1) == '-' && *(*ptr + 2) == '-') {
        (*ptr) += 3;
        while (**ptr && !(**ptr == '-' && *(*ptr + 1) == '-' && *(*ptr + 2) == '>')) (*ptr)++;
        if (**ptr == '-') (*ptr) += 3;
        return parse_xml_node(ptr);
    }
    
    if (**ptr == '!') {
        while (**ptr && **ptr != '>') (*ptr)++;
        if (**ptr == '>') (*ptr)++;
        return parse_xml_node(ptr);
    }
    
    if (**ptr == '/') return NULL;
    
    char* tag_name = parse_xml_tag_name(ptr);
    if (!tag_name) return NULL;
    
    JsonValue* obj = json_create_object();
    if (!obj) {
        free(tag_name);
        return NULL;
    }
    
    json_object_set(obj, "_tag", json_create_string(tag_name));
    
    skip_xml_whitespace(ptr);
    
    if (**ptr != '>' && **ptr != '/') {
        JsonValue* attrs = json_create_object();
        
        while (**ptr && **ptr != '>' && **ptr != '/') {
            skip_xml_whitespace(ptr);
            if (**ptr == '>' || **ptr == '/') break;
            
            const char* name_start = *ptr;
            while (**ptr && **ptr != '=' && !isspace(**ptr) && **ptr != '>') (*ptr)++;
            
            size_t name_len = *ptr - name_start;
            if (name_len == 0) break;
            
            char* attr_name = malloc(name_len + 1);
            strncpy(attr_name, name_start, name_len);
            attr_name[name_len] = '\0';
            
            skip_xml_whitespace(ptr);
            if (**ptr == '=') {
                (*ptr)++;
                char* attr_value = parse_xml_attr_value(ptr);
                if (attr_value) {
                    json_object_set(attrs, attr_name, json_create_string(attr_value));
                    free(attr_value);
                }
            }
            free(attr_name);
        }
        
        if (attrs->data.object_value->count > 0) {
            json_object_set(obj, "_attributes", attrs);
        } else {
            json_free(attrs);
        }
    }
    
    if (**ptr == '/') {
        (*ptr)++;
        if (**ptr == '>') (*ptr)++;
        free(tag_name);
        return obj;
    }
    
    if (**ptr == '>') (*ptr)++;
    
    skip_xml_whitespace(ptr);
    
    if (**ptr == '<' && *(*ptr + 1) == '/') {
        (*ptr) += 2;
        while (**ptr && **ptr != '>') (*ptr)++;
        if (**ptr == '>') (*ptr)++;
        free(tag_name);
        return obj;
    }
    
    if (**ptr != '<') {
        char* content = parse_xml_content(ptr);
        if (content && strlen(content) > 0) {
            json_object_set(obj, "_text", json_create_string(content));
        }
        if (content) free(content);
    }
    
    JsonValue* children = NULL;
    while (**ptr == '<' && *(*ptr + 1) != '/') {
        JsonValue* child = parse_xml_node(ptr);
        if (child) {
            if (!children) {
                children = json_create_array();
            }
            json_array_append(children, child);
        }
        skip_xml_whitespace(ptr);
    }
    
    if (children && children->data.array_value->count > 0) {
        json_object_set(obj, "_children", children);
    } else if (children) {
        json_free(children);
    }
    
    if (**ptr == '<' && *(*ptr + 1) == '/') {
        (*ptr) += 2;
        while (**ptr && **ptr != '>') (*ptr)++;
        if (**ptr == '>') (*ptr)++;
    }
    
    free(tag_name);
    return obj;
}

JsonValue* xml_to_json(const char* xml) {
    if (!xml) {
        json_set_error(JSON_ERROR_NULL_POINTER, "XML input is NULL", 0, 0);
        return NULL;
    }
    
    size_t xml_len = strlen(xml);
    if (xml_len == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "XML input is empty", 0, 0);
        return NULL;
    }
    
    if (xml_len > 10 * 1024 * 1024) {  // 10MB limit to prevent DoS
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "XML input too large (>10MB)", 0, 0);
        return NULL;
    }
    
    const char* ptr = xml;
    JsonValue* result = parse_xml_node(&ptr);
    
    if (!result) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "Failed to parse XML", 0, 0);
    }
    
    return result;
}

static int yaml_get_indent(const char* line) {
    int indent = 0;
    while (*line == ' ') {
        indent++;
        line++;
    }
    return indent;
}

static JsonValue* yaml_parse_value(const char* val_str) {
    if (!val_str || strlen(val_str) == 0) return json_create_null();
    
    const char* trimmed = val_str;
    while (*trimmed && isspace(*trimmed)) trimmed++;
    
    size_t len = strlen(trimmed);
    while (len > 0 && isspace(trimmed[len - 1])) len--;
    
    char* value = malloc(len + 1);
    if (!value) return json_create_null();
    
    strncpy(value, trimmed, len);
    value[len] = '\0';
    
    JsonValue* result;
    
    if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "on") == 0) {
        result = json_create_bool(true);
    } else if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0 || strcmp(value, "off") == 0) {
        result = json_create_bool(false);
    } else if (strcmp(value, "null") == 0 || strcmp(value, "~") == 0 || len == 0) {
        result = json_create_null();
    } else {
        char* endptr;
        double num = strtod(value, &endptr);
        if (*endptr == '\0' && *value != '\0' && !isalpha(*value)) {
            result = json_create_number(num);
        } else {
            if ((value[0] == '"' || value[0] == '\'') && len > 1 && value[len - 1] == value[0]) {
                value[len - 1] = '\0';
                result = json_create_string(value + 1);
            } else {
                result = json_create_string(value);
            }
        }
    }
    
    free(value);
    return result;
}

JsonValue* yaml_to_json(const char* yaml) {
    if (!yaml) {
        json_set_error(JSON_ERROR_NULL_POINTER, "YAML input is NULL", 0, 0);
        return NULL;
    }
    
    if (strlen(yaml) == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "YAML input is empty", 0, 0);
        return NULL;
    }
    
    JsonValue* root = json_create_object();
    if (!root) return NULL;
    
    const char* ptr = yaml;
    JsonValue* stack[64];
    int indent_stack[64];
    int stack_depth = 0;
    stack[0] = root;
    indent_stack[0] = -1;
    
    char line_buffer[8192];
    
    while (*ptr) {
        const char* line_start = ptr;
        size_t line_len = 0;
        
        while (*ptr && *ptr != '\n' && line_len < sizeof(line_buffer) - 1) {
            line_buffer[line_len++] = *ptr++;
        }
        line_buffer[line_len] = '\0';
        if (*ptr == '\n') ptr++;
        
        const char* line = line_buffer;
        while (*line && isspace(*line) && *line != '\n') line++;
        if (*line == '\0' || *line == '#') continue;
        
        int indent = yaml_get_indent(line_buffer);
        
        while (stack_depth > 0 && indent <= indent_stack[stack_depth]) {
            stack_depth--;
        }
        
        JsonValue* current = stack[stack_depth];
        
        if (line[0] == '-' && (line[1] == ' ' || line[1] == '\0')) {
            line += 2;
            while (*line == ' ') line++;
            
            if (current->type != JSON_ARRAY) {
                continue;
            }
            
            const char* colon = strchr(line, ':');
            if (colon) {
                JsonValue* item_obj = json_create_object();
                
                size_t key_len = colon - line;
                char* key = malloc(key_len + 1);
                strncpy(key, line, key_len);
                key[key_len] = '\0';
                while (key_len > 0 && isspace(key[key_len - 1])) key[--key_len] = '\0';
                
                const char* val_str = colon + 1;
                while (*val_str == ' ') val_str++;
                
                JsonValue* value = yaml_parse_value(val_str);
                json_object_set(item_obj, key, value);
                free(key);
                
                json_array_append(current, item_obj);
            } else {
                JsonValue* value = yaml_parse_value(line);
                json_array_append(current, value);
            }
            continue;
        }
        
        const char* colon = strchr(line, ':');
        if (!colon) continue;
        
        size_t key_len = colon - line;
        char* key = malloc(key_len + 1);
        strncpy(key, line, key_len);
        key[key_len] = '\0';
        while (key_len > 0 && isspace(key[key_len - 1])) key[--key_len] = '\0';
        
        const char* val_str = colon + 1;
        while (*val_str == ' ') val_str++;
        
        if (*val_str == '\0' || *val_str == '\n') {
            JsonValue* nested = json_create_object();
            
            if (current->type == JSON_OBJECT) {
                json_object_set(current, key, nested);
            }
            
            if (stack_depth < 63) {
                stack_depth++;
                stack[stack_depth] = nested;
                indent_stack[stack_depth] = indent;
            }
        } else if (*val_str == '[') {
            JsonValue* arr = json_create_array();
            val_str++;
            
            while (*val_str && *val_str != ']') {
                while (*val_str && (*val_str == ' ' || *val_str == ',')) val_str++;
                if (*val_str == ']') break;
                
                const char* item_start = val_str;
                while (*val_str && *val_str != ',' && *val_str != ']') val_str++;
                
                size_t item_len = val_str - item_start;
                char* item_str = malloc(item_len + 1);
                strncpy(item_str, item_start, item_len);
                item_str[item_len] = '\0';
                
                JsonValue* item = yaml_parse_value(item_str);
                json_array_append(arr, item);
                free(item_str);
            }
            
            if (current->type == JSON_OBJECT) {
                json_object_set(current, key, arr);
            }
        } else {
            JsonValue* value = yaml_parse_value(val_str);
            if (current->type == JSON_OBJECT) {
                json_object_set(current, key, value);
            }
        }
        
        free(key);
    }
    
    return root;
}

JsonValue* csv_to_json(const char* csv) {
    if (!csv) {
        json_set_error(JSON_ERROR_NULL_POINTER, "CSV input is NULL", 0, 0);
        return NULL;
    }
    
    size_t csv_len = strlen(csv);
    if (csv_len == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "CSV input is empty", 0, 0);
        return NULL;
    }
    
    if (csv_len > 50 * 1024 * 1024) {  // 50MB limit
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "CSV input too large (>50MB)", 0, 0);
        return NULL;
    }
    
    JsonValue* array = json_create_array();
    if (!array) return NULL;
    
    const char* ptr = csv;
    
    char** headers = NULL;
    size_t header_count = 0;
    size_t header_capacity = 16;
    headers = malloc(header_capacity * sizeof(char*));
    if (!headers) {
        json_free(array);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate headers array", 0, 0);
        return NULL;
    }
    memset(headers, 0, header_capacity * sizeof(char*));
    
    while (*ptr && *ptr != '\n') {
        while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
        if (*ptr == '\n') break;
        
        const char* start = ptr;
        bool in_quotes = false;
        
        if (*ptr == '"') {
            in_quotes = true;
            ptr++;
            start = ptr;
            while (*ptr && !(*ptr == '"' && *(ptr + 1) != '"')) {
                if (*ptr == '"' && *(ptr + 1) == '"') ptr += 2;
                else ptr++;
            }
        } else {
            while (*ptr && *ptr != ',' && *ptr != '\n') ptr++;
        }
        
        size_t len = ptr - start;
        char* header = malloc(len + 1);
        strncpy(header, start, len);
        header[len] = '\0';
        
        if (in_quotes && *ptr == '"') ptr++;
        
        if (header_count >= header_capacity) {
            header_capacity *= 2;
            char** new_headers = realloc(headers, header_capacity * sizeof(char*));
            if (!new_headers) {
                for (size_t i = 0; i < header_count; i++) free(headers[i]);
                free(headers);
                free(header);
                json_free(array);
                return NULL;
            }
            headers = new_headers;
        }
        headers[header_count++] = header;
        
        if (*ptr == ',') ptr++;
    }
    if (*ptr == '\n') ptr++;
    
    while (*ptr) {
        JsonValue* row = json_create_object();
        if (!row) break;
        
        for (size_t i = 0; i < header_count && *ptr; i++) {
            while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
            if (*ptr == '\n' || !*ptr) break;
            
            const char* start = ptr;
            bool in_quotes = false;
            
            if (*ptr == '"') {
                in_quotes = true;
                ptr++;
                start = ptr;
                while (*ptr && !(*ptr == '"' && (*(ptr + 1) == ',' || *(ptr + 1) == '\n' || !*(ptr + 1)))) {
                    if (*ptr == '"' && *(ptr + 1) == '"') ptr += 2;
                    else ptr++;
                }
            } else {
                while (*ptr && *ptr != ',' && *ptr != '\n') ptr++;
            }
            
            size_t len = ptr - start;
            char* value_str = malloc(len + 1);
            strncpy(value_str, start, len);
            value_str[len] = '\0';
            
            if (in_quotes && *ptr == '"') ptr++;
            
            JsonValue* value = json_create_string(value_str);
            json_object_set(row, headers[i], value);
            free(value_str);
            
            if (*ptr == ',') ptr++;
        }
        
        json_array_append(array, row);
        while (*ptr && *ptr != '\n') ptr++;
        if (*ptr == '\n') ptr++;
    }
    
    for (size_t i = 0; i < header_count; i++) {
        free(headers[i]);
    }
    free(headers);
    
    return array;
}

JsonValue* ini_to_json(const char* ini) {
    if (!ini) {
        json_set_error(JSON_ERROR_NULL_POINTER, "INI input is NULL", 0, 0);
        return NULL;
    }
    
    if (strlen(ini) == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "INI input is empty", 0, 0);
        return NULL;
    }
    
    JsonValue* obj = json_create_object();
    if (!obj) return NULL;
    
    const char* ptr = ini;
    char* current_section = NULL;
    
    while (*ptr) {
        while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
        if (*ptr == '\n') {
            ptr++;
            continue;
        }
        if (!*ptr) break;
        
        if (*ptr == ';' || *ptr == '#') {
            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') ptr++;
            continue;
        }
        
        if (*ptr == '[') {
            ptr++;
            const char* start = ptr;
            while (*ptr && *ptr != ']') ptr++;
            
            size_t len = ptr - start;
            if (current_section) free(current_section);
            current_section = malloc(len + 1);
            strncpy(current_section, start, len);
            current_section[len] = '\0';
            
            if (!json_object_get(obj, current_section)) {
                json_object_set(obj, current_section, json_create_object());
            }
            
            if (*ptr == ']') ptr++;
            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') ptr++;
            continue;
        }
        
        const char* key_start = ptr;
        while (*ptr && *ptr != '=' && *ptr != '\n') ptr++;
        if (*ptr != '=') {
            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') ptr++;
            continue;
        }
        
        size_t key_len = ptr - key_start;
        char* key = malloc(key_len + 1);
        strncpy(key, key_start, key_len);
        key[key_len] = '\0';
        
        while (key_len > 0 && isspace(key[key_len - 1])) key[--key_len] = '\0';
        
        ptr++;
        while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
        
        const char* val_start = ptr;
        bool in_quotes = false;
        if (*ptr == '"') {
            in_quotes = true;
            ptr++;
            val_start = ptr;
            while (*ptr && *ptr != '"') ptr++;
        } else {
            while (*ptr && *ptr != '\n' && *ptr != ';' && *ptr != '#') ptr++;
        }
        
        size_t val_len = ptr - val_start;
        char* val_str = malloc(val_len + 1);
        strncpy(val_str, val_start, val_len);
        val_str[val_len] = '\0';
        
        if (in_quotes && *ptr == '"') ptr++;
        
        while (val_len > 0 && isspace(val_str[val_len - 1])) val_str[--val_len] = '\0';
        
        JsonValue* value = json_create_string(val_str);
        
        if (current_section) {
            JsonValue* section = json_object_get(obj, current_section);
            json_object_set(section, key, value);
        } else {
            json_object_set(obj, key, value);
        }
        
        free(key);
        free(val_str);
        
        if (*ptr == '\n') ptr++;
    }
    
    if (current_section) free(current_section);
    return obj;
}
