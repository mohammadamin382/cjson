#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
    int indent;
    bool error;
} ConvertContext;

static bool ensure_capacity_conv(ConvertContext* ctx, size_t needed) {
    if (ctx->length + needed >= ctx->capacity) {
        size_t new_capacity = ctx->capacity;
        while (ctx->length + needed >= new_capacity) {
            new_capacity *= 2;
        }
        char* new_buffer = realloc(ctx->buffer, new_capacity);
        if (!new_buffer) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand conversion buffer", 0, 0);
            ctx->error = true;
            return false;
        }
        ctx->buffer = new_buffer;
        ctx->capacity = new_capacity;
    }
    return true;
}

static bool append_conv(ConvertContext* ctx, const char* str) {
    if (!str || ctx->error) return false;
    size_t len = strlen(str);
    if (!ensure_capacity_conv(ctx, len + 1)) return false;
    memcpy(ctx->buffer + ctx->length, str, len);
    ctx->length += len;
    ctx->buffer[ctx->length] = '\0';
    return true;
}

static bool append_char_conv(ConvertContext* ctx, char c) {
    if (ctx->error) return false;
    if (!ensure_capacity_conv(ctx, 2)) return false;
    ctx->buffer[ctx->length++] = c;
    ctx->buffer[ctx->length] = '\0';
    return true;
}

static bool append_indent_conv(ConvertContext* ctx) {
    if (ctx->error) return false;
    for (int i = 0; i < ctx->indent; i++) {
        if (!append_conv(ctx, "  ")) return false;
    }
    return true;
}

static bool xml_escape(ConvertContext* ctx, const char* str) {
    if (!str || ctx->error) return false;
    
    size_t str_len = strlen(str);
    if (str_len > 1000000) {  // Prevent DoS with huge strings
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "String too large for XML escaping", 0, 0);
        ctx->error = true;
        return false;
    }
    
    for (const char* p = str; *p; p++) {
        if (ctx->error) return false;
        
        // Validate UTF-8 sequence
        unsigned char c = (unsigned char)*p;
        if (c >= 0x80) {
            int seq_len = 0;
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;
            else {
                json_set_error(JSON_ERROR_INVALID_UTF8, "Invalid UTF-8 sequence in XML", 0, 0);
                ctx->error = true;
                return false;
            }
            
            // Verify continuation bytes
            for (int i = 1; i < seq_len; i++) {
                if (!p[i] || (p[i] & 0xC0) != 0x80) {
                    json_set_error(JSON_ERROR_INVALID_UTF8, "Invalid UTF-8 continuation byte", 0, 0);
                    ctx->error = true;
                    return false;
                }
            }
            p += seq_len - 1;
            continue;
        }
        
        switch (*p) {
            case '<': if (!append_conv(ctx, "&lt;")) return false; break;
            case '>': if (!append_conv(ctx, "&gt;")) return false; break;
            case '&': if (!append_conv(ctx, "&amp;")) return false; break;
            case '"': if (!append_conv(ctx, "&quot;")) return false; break;
            case '\'': if (!append_conv(ctx, "&apos;")) return false; break;
            default:
                if ((unsigned char)*p < 0x20 && *p != '\n' && *p != '\r' && *p != '\t') {
                    char hex[16];
                    int written = snprintf(hex, sizeof(hex), "&#x%02X;", (unsigned char)*p);
                    if (written < 0 || written >= (int)sizeof(hex)) return false;
                    if (!append_conv(ctx, hex)) return false;
                } else {
                    if (!append_char_conv(ctx, *p)) return false;
                }
                break;
        }
    }
    return true;
}

static bool json_to_xml_recursive(ConvertContext* ctx, const JsonValue* value, const char* tag);

static bool json_to_xml_recursive(ConvertContext* ctx, const JsonValue* value, const char* tag) {
    if (!value || !tag || ctx->error) return false;
    
    if (!append_indent_conv(ctx)) return false;
    if (!append_char_conv(ctx, '<')) return false;
    if (!append_conv(ctx, tag)) return false;
    if (!append_char_conv(ctx, '>')) return false;
    
    switch (value->type) {
        case JSON_NULL:
            if (!append_conv(ctx, "<null/>")) return false;
            break;
        case JSON_BOOL:
            if (!append_conv(ctx, value->data.bool_value ? "true" : "false")) return false;
            break;
        case JSON_NUMBER: {
            char num[128];
            if (value->data.number_value == (long long)value->data.number_value) {
                snprintf(num, sizeof(num), "%lld", (long long)value->data.number_value);
            } else {
                snprintf(num, sizeof(num), "%.17g", value->data.number_value);
            }
            if (!append_conv(ctx, num)) return false;
            break;
        }
        case JSON_STRING:
            if (!xml_escape(ctx, value->data.string_value)) return false;
            break;
        case JSON_ARRAY: {
            if (!append_char_conv(ctx, '\n')) return false;
            ctx->indent++;
            JsonArray* arr = value->data.array_value;
            for (size_t i = 0; i < arr->count; i++) {
                if (!json_to_xml_recursive(ctx, arr->values[i], "item")) return false;
                if (!append_char_conv(ctx, '\n')) return false;
            }
            ctx->indent--;
            if (!append_indent_conv(ctx)) return false;
            break;
        }
        case JSON_OBJECT: {
            if (!append_char_conv(ctx, '\n')) return false;
            ctx->indent++;
            JsonObject* obj = value->data.object_value;
            for (size_t i = 0; i < obj->count; i++) {
                if (!json_to_xml_recursive(ctx, obj->pairs[i].value, obj->pairs[i].key)) return false;
                if (!append_char_conv(ctx, '\n')) return false;
            }
            ctx->indent--;
            if (!append_indent_conv(ctx)) return false;
            break;
        }
    }
    
    if (!append_conv(ctx, "</")) return false;
    if (!append_conv(ctx, tag)) return false;
    if (!append_char_conv(ctx, '>')) return false;
    return true;
}

char* json_to_xml(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot convert NULL to XML", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(4096),
        .length = 0,
        .capacity = 4096,
        .indent = 0,
        .error = false
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate XML buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    if (!append_conv(&ctx, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")) {
        free(ctx.buffer);
        return NULL;
    }
    
    if (!json_to_xml_recursive(&ctx, value, "root")) {
        free(ctx.buffer);
        return NULL;
    }
    
    if (!append_char_conv(&ctx, '\n')) {
        free(ctx.buffer);
        return NULL;
    }
    
    return ctx.buffer;
}

static bool json_to_yaml_recursive(ConvertContext* ctx, const JsonValue* value, bool is_array_item);

static bool json_to_yaml_recursive(ConvertContext* ctx, const JsonValue* value, bool is_array_item) {
    if (!value || ctx->error) return false;
    
    switch (value->type) {
        case JSON_NULL:
            if (!append_conv(ctx, "null")) return false;
            break;
        case JSON_BOOL:
            if (!append_conv(ctx, value->data.bool_value ? "true" : "false")) return false;
            break;
        case JSON_NUMBER: {
            char num[128];
            if (value->data.number_value == (long long)value->data.number_value) {
                snprintf(num, sizeof(num), "%lld", (long long)value->data.number_value);
            } else {
                snprintf(num, sizeof(num), "%.17g", value->data.number_value);
            }
            if (!append_conv(ctx, num)) return false;
            break;
        }
        case JSON_STRING: {
            bool needs_quotes = false;
            const char* str = value->data.string_value;
            
            if (strlen(str) == 0) {
                needs_quotes = true;
            } else if (strchr(str, ':') || strchr(str, '\n') || strchr(str, '#') || 
                strchr(str, '[') || strchr(str, ']') || strchr(str, '{') || strchr(str, '}') ||
                strchr(str, ',') || strchr(str, '|') || strchr(str, '>') || strchr(str, '@') ||
                strchr(str, '`') || strchr(str, '!') || strchr(str, '%') || strchr(str, '&') ||
                strchr(str, '*')) {
                needs_quotes = true;
            }
            
            if (strcmp(str, "true") == 0 || strcmp(str, "false") == 0 || 
                strcmp(str, "null") == 0 || strcmp(str, "~") == 0 ||
                strcmp(str, "yes") == 0 || strcmp(str, "no") == 0 ||
                strcmp(str, "on") == 0 || strcmp(str, "off") == 0) {
                needs_quotes = true;
            }
            
            char* endptr;
            strtod(str, &endptr);
            if (*endptr == '\0' && *str != '\0') {
                needs_quotes = true;
            }
            
            if (needs_quotes || (str[0] >= '0' && str[0] <= '9')) {
                if (!append_char_conv(ctx, '"')) return false;
                for (const char* p = str; *p; p++) {
                    if (*p == '"' || *p == '\\') {
                        if (!append_char_conv(ctx, '\\')) return false;
                    }
                    if (!append_char_conv(ctx, *p)) return false;
                }
                if (!append_char_conv(ctx, '"')) return false;
            } else {
                if (!append_conv(ctx, str)) return false;
            }
            break;
        }
        case JSON_ARRAY: {
            JsonArray* arr = value->data.array_value;
            if (arr->count == 0) {
                if (!append_conv(ctx, "[]")) return false;
            } else {
                bool is_simple = true;
                for (size_t i = 0; i < arr->count; i++) {
                    if (arr->values[i]->type == JSON_OBJECT || arr->values[i]->type == JSON_ARRAY) {
                        is_simple = false;
                        break;
                    }
                }
                
                if (is_simple && arr->count <= 5) {
                    if (!append_char_conv(ctx, '[')) return false;
                    for (size_t i = 0; i < arr->count; i++) {
                        if (!json_to_yaml_recursive(ctx, arr->values[i], false)) return false;
                        if (i < arr->count - 1) {
                            if (!append_conv(ctx, ", ")) return false;
                        }
                    }
                    if (!append_char_conv(ctx, ']')) return false;
                } else {
                    for (size_t i = 0; i < arr->count; i++) {
                        if (!append_char_conv(ctx, '\n')) return false;
                        if (!append_indent_conv(ctx)) return false;
                        if (!append_conv(ctx, "- ")) return false;
                        
                        if (arr->values[i]->type == JSON_OBJECT || arr->values[i]->type == JSON_ARRAY) {
                            ctx->indent++;
                            if (!json_to_yaml_recursive(ctx, arr->values[i], true)) return false;
                            ctx->indent--;
                        } else {
                            if (!json_to_yaml_recursive(ctx, arr->values[i], true)) return false;
                        }
                    }
                }
            }
            break;
        }
        case JSON_OBJECT: {
            JsonObject* obj = value->data.object_value;
            if (obj->count == 0) {
                if (!append_conv(ctx, "{}")) return false;
            } else {
                for (size_t i = 0; i < obj->count; i++) {
                    if (i > 0 || is_array_item) {
                        if (!append_char_conv(ctx, '\n')) return false;
                        if (!append_indent_conv(ctx)) return false;
                    }
                    
                    if (!append_conv(ctx, obj->pairs[i].key)) return false;
                    if (!append_conv(ctx, ": ")) return false;
                    
                    if (obj->pairs[i].value->type == JSON_OBJECT || obj->pairs[i].value->type == JSON_ARRAY) {
                        ctx->indent++;
                        if (!json_to_yaml_recursive(ctx, obj->pairs[i].value, false)) return false;
                        ctx->indent--;
                    } else {
                        if (!json_to_yaml_recursive(ctx, obj->pairs[i].value, false)) return false;
                    }
                }
            }
            break;
        }
    }
    return true;
}

char* json_to_yaml(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot convert NULL to YAML", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(4096),
        .length = 0,
        .capacity = 4096,
        .indent = 0,
        .error = false
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate YAML buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    if (!json_to_yaml_recursive(&ctx, value, false)) {
        free(ctx.buffer);
        return NULL;
    }
    
    if (!append_char_conv(&ctx, '\n')) {
        free(ctx.buffer);
        return NULL;
    }
    
    return ctx.buffer;
}

char* json_to_csv(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot convert NULL to CSV", 0, 0);
        return NULL;
    }
    
    if (value->type != JSON_ARRAY) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "CSV conversion requires array of objects", 0, 0);
        return NULL;
    }
    
    JsonArray* arr = value->data.array_value;
    if (arr->count == 0) {
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "Cannot convert empty array to CSV", 0, 0);
        return NULL;
    }
    
    if (arr->count > 1000000) {  // Prevent DoS
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Array too large for CSV conversion", 0, 0);
        return NULL;
    }
    
    if (arr->values[0]->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "CSV conversion requires array of objects", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(8192),
        .length = 0,
        .capacity = 8192,
        .indent = 0,
        .error = false
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate CSV buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    JsonObject* first_obj = arr->values[0]->data.object_value;
    
    for (size_t i = 0; i < first_obj->count; i++) {
        bool needs_quotes = false;
        const char* key = first_obj->pairs[i].key;
        
        if (strchr(key, ',') || strchr(key, '"') || strchr(key, '\n') || strchr(key, '\r')) {
            needs_quotes = true;
        }
        
        if (needs_quotes) {
            if (!append_char_conv(&ctx, '"')) goto error;
            for (const char* p = key; *p; p++) {
                if (*p == '"') {
                    if (!append_char_conv(&ctx, '"')) goto error;
                }
                if (!append_char_conv(&ctx, *p)) goto error;
            }
            if (!append_char_conv(&ctx, '"')) goto error;
        } else {
            if (!append_conv(&ctx, key)) goto error;
        }
        
        if (i < first_obj->count - 1) {
            if (!append_char_conv(&ctx, ',')) goto error;
        }
    }
    if (!append_char_conv(&ctx, '\n')) goto error;
    
    for (size_t row = 0; row < arr->count; row++) {
        if (arr->values[row]->type != JSON_OBJECT) continue;
        
        for (size_t col = 0; col < first_obj->count; col++) {
            const char* key = first_obj->pairs[col].key;
            JsonValue* val = json_object_get(arr->values[row], key);
            
            if (val) {
                bool needs_quotes = false;
                
                if (val->type == JSON_STRING) {
                    const char* str = val->data.string_value;
                    if (strchr(str, ',') || strchr(str, '"') || strchr(str, '\n') || strchr(str, '\r')) {
                        needs_quotes = true;
                    }
                    
                    if (needs_quotes) {
                        if (!append_char_conv(&ctx, '"')) goto error;
                    }
                    
                    for (const char* p = str; *p; p++) {
                        if (*p == '"') {
                            if (!append_char_conv(&ctx, '"')) goto error;
                        }
                        if (!append_char_conv(&ctx, *p)) goto error;
                    }
                    
                    if (needs_quotes) {
                        if (!append_char_conv(&ctx, '"')) goto error;
                    }
                } else if (val->type == JSON_NUMBER) {
                    char num[128];
                    if (val->data.number_value == (long long)val->data.number_value) {
                        snprintf(num, sizeof(num), "%lld", (long long)val->data.number_value);
                    } else {
                        snprintf(num, sizeof(num), "%.17g", val->data.number_value);
                    }
                    if (!append_conv(&ctx, num)) goto error;
                } else if (val->type == JSON_BOOL) {
                    if (!append_conv(&ctx, val->data.bool_value ? "true" : "false")) goto error;
                } else if (val->type == JSON_NULL) {
                    // Empty field for null
                }
            }
            
            if (col < first_obj->count - 1) {
                if (!append_char_conv(&ctx, ',')) goto error;
            }
        }
        if (!append_char_conv(&ctx, '\n')) goto error;
    }
    
    return ctx.buffer;

error:
    free(ctx.buffer);
    return NULL;
}

char* json_to_ini(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot convert NULL to INI", 0, 0);
        return NULL;
    }
    
    if (value->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "INI conversion requires object", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(4096),
        .length = 0,
        .capacity = 4096,
        .indent = 0,
        .error = false
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate INI buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    JsonObject* obj = value->data.object_value;
    
    for (size_t i = 0; i < obj->count; i++) {
        JsonValue* section_val = obj->pairs[i].value;
        
        if (!append_char_conv(&ctx, '[')) goto error;
        if (!append_conv(&ctx, obj->pairs[i].key)) goto error;
        if (!append_char_conv(&ctx, ']')) goto error;
        if (!append_char_conv(&ctx, '\n')) goto error;
        
        if (section_val->type == JSON_OBJECT) {
            JsonObject* section = section_val->data.object_value;
            for (size_t j = 0; j < section->count; j++) {
                if (!append_conv(&ctx, section->pairs[j].key)) goto error;
                if (!append_char_conv(&ctx, '=')) goto error;
                
                JsonValue* val = section->pairs[j].value;
                if (val->type == JSON_STRING) {
                    const char* str = val->data.string_value;
                    bool needs_quotes = strchr(str, '\n') || strchr(str, '\r') || strchr(str, ';') || strchr(str, '#');
                    if (needs_quotes) {
                        if (!append_char_conv(&ctx, '"')) goto error;
                        if (!append_conv(&ctx, str)) goto error;
                        if (!append_char_conv(&ctx, '"')) goto error;
                    } else {
                        if (!append_conv(&ctx, str)) goto error;
                    }
                } else if (val->type == JSON_NUMBER) {
                    char num[128];
                    if (val->data.number_value == (long long)val->data.number_value) {
                        snprintf(num, sizeof(num), "%lld", (long long)val->data.number_value);
                    } else {
                        snprintf(num, sizeof(num), "%.17g", val->data.number_value);
                    }
                    if (!append_conv(&ctx, num)) goto error;
                } else if (val->type == JSON_BOOL) {
                    if (!append_conv(&ctx, val->data.bool_value ? "true" : "false")) goto error;
                } else if (val->type == JSON_NULL) {
                    if (!append_conv(&ctx, "null")) goto error;
                }
                if (!append_char_conv(&ctx, '\n')) goto error;
            }
        } else if (section_val->type == JSON_STRING) {
            if (!append_conv(&ctx, obj->pairs[i].key)) goto error;
            if (!append_char_conv(&ctx, '=')) goto error;
            if (!append_conv(&ctx, section_val->data.string_value)) goto error;
            if (!append_char_conv(&ctx, '\n')) goto error;
        }
        
        if (i < obj->count - 1) {
            if (!append_char_conv(&ctx, '\n')) goto error;
        }
    }
    
    return ctx.buffer;

error:
    free(ctx.buffer);
    return NULL;
}
