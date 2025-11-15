
#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
    int indent;
} ConvertContext;

static void ensure_capacity_conv(ConvertContext* ctx, size_t needed) {
    if (ctx->length + needed >= ctx->capacity) {
        while (ctx->length + needed >= ctx->capacity) {
            ctx->capacity *= 2;
        }
        ctx->buffer = realloc(ctx->buffer, ctx->capacity);
    }
}

static void append_conv(ConvertContext* ctx, const char* str) {
    size_t len = strlen(str);
    ensure_capacity_conv(ctx, len + 1);
    memcpy(ctx->buffer + ctx->length, str, len);
    ctx->length += len;
    ctx->buffer[ctx->length] = '\0';
}

static void append_char_conv(ConvertContext* ctx, char c) {
    ensure_capacity_conv(ctx, 2);
    ctx->buffer[ctx->length++] = c;
    ctx->buffer[ctx->length] = '\0';
}

static void append_indent_conv(ConvertContext* ctx) {
    for (int i = 0; i < ctx->indent; i++) {
        append_conv(ctx, "  ");
    }
}

static void xml_escape(ConvertContext* ctx, const char* str) {
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '<': append_conv(ctx, "&lt;"); break;
            case '>': append_conv(ctx, "&gt;"); break;
            case '&': append_conv(ctx, "&amp;"); break;
            case '"': append_conv(ctx, "&quot;"); break;
            case '\'': append_conv(ctx, "&apos;"); break;
            default: append_char_conv(ctx, *p); break;
        }
    }
}

static void json_to_xml_recursive(ConvertContext* ctx, const JsonValue* value, const char* tag) {
    if (!value) return;
    
    append_indent_conv(ctx);
    append_char_conv(ctx, '<');
    append_conv(ctx, tag);
    append_char_conv(ctx, '>');
    
    switch (value->type) {
        case JSON_NULL:
            break;
        case JSON_BOOL:
            append_conv(ctx, value->data.bool_value ? "true" : "false");
            break;
        case JSON_NUMBER: {
            char num[64];
            if (value->data.number_value == (long long)value->data.number_value) {
                snprintf(num, sizeof(num), "%lld", (long long)value->data.number_value);
            } else {
                snprintf(num, sizeof(num), "%.17g", value->data.number_value);
            }
            append_conv(ctx, num);
            break;
        }
        case JSON_STRING:
            xml_escape(ctx, value->data.string_value);
            break;
        case JSON_ARRAY: {
            append_char_conv(ctx, '\n');
            ctx->indent++;
            JsonArray* arr = value->data.array_value;
            for (size_t i = 0; i < arr->count; i++) {
                json_to_xml_recursive(ctx, arr->values[i], "item");
                append_char_conv(ctx, '\n');
            }
            ctx->indent--;
            append_indent_conv(ctx);
            break;
        }
        case JSON_OBJECT: {
            append_char_conv(ctx, '\n');
            ctx->indent++;
            JsonObject* obj = value->data.object_value;
            for (size_t i = 0; i < obj->count; i++) {
                json_to_xml_recursive(ctx, obj->pairs[i].value, obj->pairs[i].key);
                append_char_conv(ctx, '\n');
            }
            ctx->indent--;
            append_indent_conv(ctx);
            break;
        }
    }
    
    append_conv(ctx, "</");
    append_conv(ctx, tag);
    append_char_conv(ctx, '>');
}

char* json_to_xml(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot convert NULL to XML", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(2048),
        .length = 0,
        .capacity = 2048,
        .indent = 0
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate XML buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    append_conv(&ctx, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    json_to_xml_recursive(&ctx, value, "root");
    append_char_conv(&ctx, '\n');
    
    return ctx.buffer;
}

static void json_to_yaml_recursive(ConvertContext* ctx, const JsonValue* value, bool is_array_item) {
    if (!value) {
        append_conv(ctx, "null");
        return;
    }
    
    switch (value->type) {
        case JSON_NULL:
            append_conv(ctx, "null");
            break;
        case JSON_BOOL:
            append_conv(ctx, value->data.bool_value ? "true" : "false");
            break;
        case JSON_NUMBER: {
            char num[64];
            if (value->data.number_value == (long long)value->data.number_value) {
                snprintf(num, sizeof(num), "%lld", (long long)value->data.number_value);
            } else {
                snprintf(num, sizeof(num), "%.17g", value->data.number_value);
            }
            append_conv(ctx, num);
            break;
        }
        case JSON_STRING: {
            bool needs_quotes = false;
            const char* str = value->data.string_value;
            
            if (strchr(str, ':') || strchr(str, '\n') || strchr(str, '#') || 
                strchr(str, '[') || strchr(str, ']') || strchr(str, '{') || strchr(str, '}')) {
                needs_quotes = true;
            }
            
            if (strcmp(str, "true") == 0 || strcmp(str, "false") == 0 || 
                strcmp(str, "null") == 0 || strcmp(str, "~") == 0) {
                needs_quotes = true;
            }
            
            if (needs_quotes) {
                append_char_conv(ctx, '"');
                for (const char* p = str; *p; p++) {
                    if (*p == '"' || *p == '\\') {
                        append_char_conv(ctx, '\\');
                    }
                    append_char_conv(ctx, *p);
                }
                append_char_conv(ctx, '"');
            } else {
                append_conv(ctx, str);
            }
            break;
        }
        case JSON_ARRAY: {
            JsonArray* arr = value->data.array_value;
            if (arr->count == 0) {
                append_conv(ctx, "[]");
            } else {
                bool is_simple = true;
                for (size_t i = 0; i < arr->count; i++) {
                    if (arr->values[i]->type == JSON_OBJECT || arr->values[i]->type == JSON_ARRAY) {
                        is_simple = false;
                        break;
                    }
                }
                
                if (is_simple && arr->count <= 5) {
                    append_char_conv(ctx, '[');
                    for (size_t i = 0; i < arr->count; i++) {
                        json_to_yaml_recursive(ctx, arr->values[i], false);
                        if (i < arr->count - 1) {
                            append_conv(ctx, ", ");
                        }
                    }
                    append_char_conv(ctx, ']');
                } else {
                    for (size_t i = 0; i < arr->count; i++) {
                        append_char_conv(ctx, '\n');
                        append_indent_conv(ctx);
                        append_conv(ctx, "- ");
                        
                        if (arr->values[i]->type == JSON_OBJECT || arr->values[i]->type == JSON_ARRAY) {
                            ctx->indent++;
                            json_to_yaml_recursive(ctx, arr->values[i], true);
                            ctx->indent--;
                        } else {
                            json_to_yaml_recursive(ctx, arr->values[i], true);
                        }
                    }
                }
            }
            break;
        }
        case JSON_OBJECT: {
            JsonObject* obj = value->data.object_value;
            if (obj->count == 0) {
                append_conv(ctx, "{}");
            } else {
                for (size_t i = 0; i < obj->count; i++) {
                    if (i > 0 || is_array_item) {
                        append_char_conv(ctx, '\n');
                        append_indent_conv(ctx);
                    }
                    
                    append_conv(ctx, obj->pairs[i].key);
                    append_conv(ctx, ": ");
                    
                    if (obj->pairs[i].value->type == JSON_OBJECT || obj->pairs[i].value->type == JSON_ARRAY) {
                        ctx->indent++;
                        json_to_yaml_recursive(ctx, obj->pairs[i].value, false);
                        ctx->indent--;
                    } else {
                        json_to_yaml_recursive(ctx, obj->pairs[i].value, false);
                    }
                }
            }
            break;
        }
    }
}

char* json_to_yaml(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot convert NULL to YAML", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(2048),
        .length = 0,
        .capacity = 2048,
        .indent = 0
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate YAML buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    json_to_yaml_recursive(&ctx, value, false);
    append_char_conv(&ctx, '\n');
    
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
    
    if (arr->values[0]->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "CSV conversion requires array of objects", 0, 0);
        return NULL;
    }
    
    ConvertContext ctx = {
        .buffer = malloc(4096),
        .length = 0,
        .capacity = 4096,
        .indent = 0
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate CSV buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    JsonObject* first_obj = arr->values[0]->data.object_value;
    
    for (size_t i = 0; i < first_obj->count; i++) {
        append_conv(&ctx, first_obj->pairs[i].key);
        if (i < first_obj->count - 1) {
            append_char_conv(&ctx, ',');
        }
    }
    append_char_conv(&ctx, '\n');
    
    for (size_t row = 0; row < arr->count; row++) {
        if (arr->values[row]->type != JSON_OBJECT) continue;
        
        JsonObject* obj = arr->values[row]->data.object_value;
        
        for (size_t col = 0; col < first_obj->count; col++) {
            const char* key = first_obj->pairs[col].key;
            JsonValue* val = json_object_get(arr->values[row], key);
            
            if (val) {
                bool needs_quotes = false;
                
                if (val->type == JSON_STRING) {
                    const char* str = val->data.string_value;
                    if (strchr(str, ',') || strchr(str, '"') || strchr(str, '\n')) {
                        needs_quotes = true;
                    }
                    
                    if (needs_quotes) append_char_conv(&ctx, '"');
                    
                    for (const char* p = str; *p; p++) {
                        if (*p == '"') {
                            append_char_conv(&ctx, '"');
                        }
                        append_char_conv(&ctx, *p);
                    }
                    
                    if (needs_quotes) append_char_conv(&ctx, '"');
                } else if (val->type == JSON_NUMBER) {
                    char num[64];
                    if (val->data.number_value == (long long)val->data.number_value) {
                        snprintf(num, sizeof(num), "%lld", (long long)val->data.number_value);
                    } else {
                        snprintf(num, sizeof(num), "%.17g", val->data.number_value);
                    }
                    append_conv(&ctx, num);
                } else if (val->type == JSON_BOOL) {
                    append_conv(&ctx, val->data.bool_value ? "true" : "false");
                } else if (val->type == JSON_NULL) {
                    // Empty field
                }
            }
            
            if (col < first_obj->count - 1) {
                append_char_conv(&ctx, ',');
            }
        }
        append_char_conv(&ctx, '\n');
    }
    
    return ctx.buffer;
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
        .buffer = malloc(2048),
        .length = 0,
        .capacity = 2048,
        .indent = 0
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate INI buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    JsonObject* obj = value->data.object_value;
    
    for (size_t i = 0; i < obj->count; i++) {
        JsonValue* section_val = obj->pairs[i].value;
        
        append_char_conv(&ctx, '[');
        append_conv(&ctx, obj->pairs[i].key);
        append_char_conv(&ctx, ']');
        append_char_conv(&ctx, '\n');
        
        if (section_val->type == JSON_OBJECT) {
            JsonObject* section = section_val->data.object_value;
            for (size_t j = 0; j < section->count; j++) {
                append_conv(&ctx, section->pairs[j].key);
                append_char_conv(&ctx, '=');
                
                JsonValue* val = section->pairs[j].value;
                if (val->type == JSON_STRING) {
                    append_conv(&ctx, val->data.string_value);
                } else if (val->type == JSON_NUMBER) {
                    char num[64];
                    if (val->data.number_value == (long long)val->data.number_value) {
                        snprintf(num, sizeof(num), "%lld", (long long)val->data.number_value);
                    } else {
                        snprintf(num, sizeof(num), "%.17g", val->data.number_value);
                    }
                    append_conv(&ctx, num);
                } else if (val->type == JSON_BOOL) {
                    append_conv(&ctx, val->data.bool_value ? "true" : "false");
                } else if (val->type == JSON_NULL) {
                    append_conv(&ctx, "null");
                }
                append_char_conv(&ctx, '\n');
            }
        } else if (section_val->type == JSON_STRING) {
            append_conv(&ctx, obj->pairs[i].key);
            append_char_conv(&ctx, '=');
            append_conv(&ctx, section_val->data.string_value);
            append_char_conv(&ctx, '\n');
        }
        
        if (i < obj->count - 1) {
            append_char_conv(&ctx, '\n');
        }
    }
    
    return ctx.buffer;
}
