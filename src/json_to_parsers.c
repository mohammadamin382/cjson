#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// XML to JSON Parser
static void skip_xml_whitespace(const char** ptr) {
    while (**ptr && isspace(**ptr)) (*ptr)++;
}

static char* parse_xml_tag_name(const char** ptr) {
    const char* start = *ptr;
    while (**ptr && **ptr != '>' && **ptr != ' ' && **ptr != '/') (*ptr)++;
    
    size_t len = *ptr - start;
    char* name = malloc(len + 1);
    strncpy(name, start, len);
    name[len] = '\0';
    return name;
}

static char* parse_xml_content(const char** ptr) {
    const char* start = *ptr;
    while (**ptr && **ptr != '<') (*ptr)++;
    
    size_t len = *ptr - start;
    char* content = malloc(len + 1);
    strncpy(content, start, len);
    content[len] = '\0';
    
    // Trim whitespace
    char* end = content + len - 1;
    while (end > content && isspace(*end)) end--;
    *(end + 1) = '\0';
    
    return content;
}

static JsonValue* parse_xml_element(const char** ptr);

static char* parse_xml_attr_value(const char** ptr) {
    skip_xml_whitespace(ptr);
    char quote = **ptr;
    if (quote != '"' && quote != '\'') return NULL;
    (*ptr)++;
    
    const char* start = *ptr;
    while (**ptr && **ptr != quote) (*ptr)++;
    
    size_t len = *ptr - start;
    char* value = malloc(len + 1);
    strncpy(value, start, len);
    value[len] = '\0';
    
    if (**ptr == quote) (*ptr)++;
    return value;
}

static JsonValue* parse_xml_attributes(const char** ptr) {
    JsonValue* attrs = json_create_object();
    
    while (**ptr && **ptr != '>' && **ptr != '/') {
        skip_xml_whitespace(ptr);
        if (**ptr == '>' || **ptr == '/') break;
        
        const char* name_start = *ptr;
        while (**ptr && **ptr != '=' && **ptr != ' ' && **ptr != '>') (*ptr)++;
        
        size_t name_len = *ptr - name_start;
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
    
    return attrs;
}

static JsonValue* parse_xml_node(const char** ptr) {
    skip_xml_whitespace(ptr);
    
    if (**ptr != '<') return NULL;
    (*ptr)++;
    
    // Skip XML declaration and comments
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
    
    if (**ptr == '/') return NULL;
    
    char* tag_name = parse_xml_tag_name(ptr);
    JsonValue* obj = json_create_object();
    json_object_set(obj, "_tag", json_create_string(tag_name));
    
    // Parse attributes
    skip_xml_whitespace(ptr);
    if (**ptr != '>' && **ptr != '/') {
        JsonValue* attrs = parse_xml_attributes(ptr);
        if (attrs->data.object_value->count > 0) {
            json_object_set(obj, "_attributes", attrs);
        } else {
            json_free(attrs);
        }
    }
    
    // Self-closing tag
    if (**ptr == '/') {
        (*ptr)++;
        if (**ptr == '>') (*ptr)++;
        free(tag_name);
        return obj;
    }
    
    if (**ptr == '>') (*ptr)++;
    
    skip_xml_whitespace(ptr);
    
    // Check for closing tag immediately
    if (**ptr == '<' && *(*ptr + 1) == '/') {
        (*ptr) += 2;
        while (**ptr && **ptr != '>') (*ptr)++;
        if (**ptr == '>') (*ptr)++;
        free(tag_name);
        return obj;
    }
    
    // Parse text content
    if (**ptr != '<') {
        char* content = parse_xml_content(ptr);
        if (content && strlen(content) > 0) {
            json_object_set(obj, "_text", json_create_string(content));
        }
        if (content) free(content);
    }
    
    // Parse children
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
    
    // Parse closing tag
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
    
    if (strlen(xml) == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "XML input is empty", 0, 0);
        return NULL;
    }
    
    const char* ptr = xml;
    JsonValue* result = parse_xml_node(&ptr);
    
    if (!result) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "Failed to parse XML", 0, 0);
    }
    
    return result;
}

// Advanced YAML to JSON Parser with nested support
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
    
    // Trim whitespace
    while (*val_str && isspace(*val_str)) val_str++;
    size_t len = strlen(val_str);
    while (len > 0 && isspace(val_str[len - 1])) len--;
    
    char* trimmed = malloc(len + 1);
    strncpy(trimmed, val_str, len);
    trimmed[len] = '\0';
    
    JsonValue* value;
    
    if (strcmp(trimmed, "true") == 0) {
        value = json_create_bool(true);
    } else if (strcmp(trimmed, "false") == 0) {
        value = json_create_bool(false);
    } else if (strcmp(trimmed, "null") == 0 || strcmp(trimmed, "~") == 0 || len == 0) {
        value = json_create_null();
    } else {
        char* endptr;
        double num = strtod(trimmed, &endptr);
        if (*endptr == '\0' && *trimmed != '\0') {
            value = json_create_number(num);
        } else {
            // Handle quoted strings
            if ((trimmed[0] == '"' || trimmed[0] == '\'') && len > 1 && trimmed[len - 1] == trimmed[0]) {
                trimmed[len - 1] = '\0';
                value = json_create_string(trimmed + 1);
            } else {
                value = json_create_string(trimmed);
            }
        }
    }
    
    free(trimmed);
    return value;
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
    if (!root) {
        return NULL;
    }
    
    const char* ptr = yaml;
    
    JsonValue* stack[32];
    int indent_stack[32];
    int stack_depth = 0;
    stack[0] = root;
    indent_stack[0] = -1;
    
    char line_buffer[4096];
    
    while (*ptr) {
        // Read line
        const char* line_start = ptr;
        size_t line_len = 0;
        while (*ptr && *ptr != '\n') {
            if (line_len < sizeof(line_buffer) - 1) {
                line_buffer[line_len++] = *ptr;
            }
            ptr++;
        }
        line_buffer[line_len] = '\0';
        if (*ptr == '\n') ptr++;
        
        // Skip empty lines and comments
        const char* line = line_buffer;
        while (*line && isspace(*line)) line++;
        if (*line == '\0' || *line == '#') continue;
        
        int indent = yaml_get_indent(line_buffer);
        
        // Adjust stack based on indentation
        while (stack_depth > 0 && indent <= indent_stack[stack_depth]) {
            stack_depth--;
        }
        
        JsonValue* current = stack[stack_depth];
        
        // Check for list item
        if (line[0] == '-' && (line[1] == ' ' || line[1] == '\0')) {
            line += 2;
            while (*line == ' ') line++;
            
            if (current->type != JSON_ARRAY) {
                // Need to convert or create array context
                continue;
            }
            
            // Check if line has key:value
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
        
        // Parse key:value
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
            // Nested object or array coming
            JsonValue* nested = json_create_object();
            
            if (current->type == JSON_OBJECT) {
                json_object_set(current, key, nested);
            }
            
            stack_depth++;
            stack[stack_depth] = nested;
            indent_stack[stack_depth] = indent;
        } else if (*val_str == '[') {
            // Inline array
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

// CSV to JSON Parser
JsonValue* csv_to_json(const char* csv) {
    if (!csv) {
        json_set_error(JSON_ERROR_NULL_POINTER, "CSV input is NULL", 0, 0);
        return NULL;
    }
    
    if (strlen(csv) == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "CSV input is empty", 0, 0);
        return NULL;
    }
    
    JsonValue* array = json_create_array();
    if (!array) {
        return NULL;
    }
    
    const char* ptr = csv;
    
    // Parse header
    char** headers = NULL;
    size_t header_count = 0;
    size_t header_capacity = 8;
    headers = malloc(header_capacity * sizeof(char*));
    
    while (*ptr && *ptr != '\n') {
        while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
        if (*ptr == '\n') break;
        
        const char* start = ptr;
        while (*ptr && *ptr != ',' && *ptr != '\n') ptr++;
        
        size_t len = ptr - start;
        char* header = malloc(len + 1);
        strncpy(header, start, len);
        header[len] = '\0';
        
        if (header_count >= header_capacity) {
            header_capacity *= 2;
            headers = realloc(headers, header_capacity * sizeof(char*));
        }
        headers[header_count++] = header;
        
        if (*ptr == ',') ptr++;
    }
    if (*ptr == '\n') ptr++;
    
    // Parse rows
    while (*ptr) {
        JsonValue* row = json_create_object();
        
        for (size_t i = 0; i < header_count; i++) {
            while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
            if (*ptr == '\n' || !*ptr) break;
            
            const char* start = ptr;
            bool in_quotes = false;
            
            if (*ptr == '"') {
                in_quotes = true;
                ptr++;
                start = ptr;
                while (*ptr && !(*ptr == '"' && *(ptr + 1) != '"')) ptr++;
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

// INI to JSON Parser
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
    if (!obj) {
        return NULL;
    }
    
    const char* ptr = ini;
    char* current_section = NULL;
    
    while (*ptr) {
        while (*ptr && isspace(*ptr) && *ptr != '\n') ptr++;
        if (*ptr == '\n') {
            ptr++;
            continue;
        }
        if (!*ptr) break;
        
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
        while (*ptr && *ptr != '\n') ptr++;
        
        size_t val_len = ptr - val_start;
        char* val_str = malloc(val_len + 1);
        strncpy(val_str, val_start, val_len);
        val_str[val_len] = '\0';
        
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
