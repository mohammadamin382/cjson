#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
    int indent_level;
    bool pretty;
} StringifyContext;

static void ensure_capacity(StringifyContext* ctx, size_t needed) {
    if (ctx->length + needed >= ctx->capacity) {
        while (ctx->length + needed >= ctx->capacity) {
            ctx->capacity *= 2;
        }
        char* new_buffer = realloc(ctx->buffer, ctx->capacity);
        if (!new_buffer) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand stringify buffer", 0, 0);
            return;
        }
        ctx->buffer = new_buffer;
    }
}

static void append_string(StringifyContext* ctx, const char* str) {
    size_t len = strlen(str);
    ensure_capacity(ctx, len + 1);
    memcpy(ctx->buffer + ctx->length, str, len);
    ctx->length += len;
    ctx->buffer[ctx->length] = '\0';
}

static void append_char(StringifyContext* ctx, char c) {
    ensure_capacity(ctx, 2);
    ctx->buffer[ctx->length++] = c;
    ctx->buffer[ctx->length] = '\0';
}

static void append_indent(StringifyContext* ctx) {
    if (!ctx->pretty) return;
    append_char(ctx, '\n');
    for (int i = 0; i < ctx->indent_level * 2; i++) {
        append_char(ctx, ' ');
    }
}

static void escape_and_append_string(StringifyContext* ctx, const char* str) {
    append_char(ctx, '"');
    
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '"':
                append_string(ctx, "\\\"");
                break;
            case '\\':
                append_string(ctx, "\\\\");
                break;
            case '\b':
                append_string(ctx, "\\b");
                break;
            case '\f':
                append_string(ctx, "\\f");
                break;
            case '\n':
                append_string(ctx, "\\n");
                break;
            case '\r':
                append_string(ctx, "\\r");
                break;
            case '\t':
                append_string(ctx, "\\t");
                break;
            default:
                if ((unsigned char)*p < 0x20) {
                    char hex[7];
                    snprintf(hex, sizeof(hex), "\\u%04x", (unsigned char)*p);
                    append_string(ctx, hex);
                } else {
                    append_char(ctx, *p);
                }
                break;
        }
    }
    
    append_char(ctx, '"');
}

static void stringify_value(StringifyContext* ctx, const JsonValue* value);

static void stringify_array(StringifyContext* ctx, const JsonValue* value) {
    JsonArray* arr = value->data.array_value;
    
    append_char(ctx, '[');
    
    if (arr->count > 0) {
        ctx->indent_level++;
        
        for (size_t i = 0; i < arr->count; i++) {
            if (ctx->pretty) {
                append_indent(ctx);
            }
            
            stringify_value(ctx, arr->values[i]);
            
            if (i < arr->count - 1) {
                append_char(ctx, ',');
            }
        }
        
        ctx->indent_level--;
        
        if (ctx->pretty) {
            append_indent(ctx);
        }
    }
    
    append_char(ctx, ']');
}

static void stringify_object(StringifyContext* ctx, const JsonValue* value) {
    JsonObject* obj = value->data.object_value;
    
    append_char(ctx, '{');
    
    if (obj->count > 0) {
        ctx->indent_level++;
        
        for (size_t i = 0; i < obj->count; i++) {
            if (ctx->pretty) {
                append_indent(ctx);
            }
            
            escape_and_append_string(ctx, obj->pairs[i].key);
            append_char(ctx, ':');
            
            if (ctx->pretty) {
                append_char(ctx, ' ');
            }
            
            stringify_value(ctx, obj->pairs[i].value);
            
            if (i < obj->count - 1) {
                append_char(ctx, ',');
            }
        }
        
        ctx->indent_level--;
        
        if (ctx->pretty) {
            append_indent(ctx);
        }
    }
    
    append_char(ctx, '}');
}

static void stringify_value(StringifyContext* ctx, const JsonValue* value) {
    if (!value) {
        append_string(ctx, "null");
        return;
    }
    
    switch (value->type) {
        case JSON_NULL:
            append_string(ctx, "null");
            break;
            
        case JSON_BOOL:
            append_string(ctx, value->data.bool_value ? "true" : "false");
            break;
            
        case JSON_NUMBER: {
            char num_buffer[64];
            double num = value->data.number_value;
            
            if (isnan(num) || isinf(num)) {
                append_string(ctx, "null");
            } else if (num == (long long)num) {
                snprintf(num_buffer, sizeof(num_buffer), "%lld", (long long)num);
            } else {
                snprintf(num_buffer, sizeof(num_buffer), "%.17g", num);
            }
            append_string(ctx, num_buffer);
            break;
        }
            
        case JSON_STRING:
            escape_and_append_string(ctx, value->data.string_value);
            break;
            
        case JSON_ARRAY:
            stringify_array(ctx, value);
            break;
            
        case JSON_OBJECT:
            stringify_object(ctx, value);
            break;
    }
}

char* json_stringify(const JsonValue* value, bool pretty) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot stringify NULL value", 0, 0);
        return NULL;
    }
    
    StringifyContext ctx = {
        .buffer = malloc(1024),
        .length = 0,
        .capacity = 1024,
        .indent_level = 0,
        .pretty = pretty
    };
    
    if (!ctx.buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate stringify buffer", 0, 0);
        return NULL;
    }
    
    ctx.buffer[0] = '\0';
    
    stringify_value(&ctx, value);
    
    if (pretty && (value->type == JSON_OBJECT || value->type == JSON_ARRAY)) {
        append_char(&ctx, '\n');
    }
    
    return ctx.buffer;
}
