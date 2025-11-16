#include "json_parser.h"
#include <stdlib.h>
#include <string.h>

typedef struct CopyContext {
    const JsonValue** seen;
    JsonValue** copies;
    size_t count;
    size_t capacity;
} CopyContext;

static CopyContext* create_copy_context(void) {
    CopyContext* ctx = malloc(sizeof(CopyContext));
    if (!ctx) return NULL;
    
    ctx->capacity = 32;
    ctx->count = 0;
    ctx->seen = malloc(ctx->capacity * sizeof(JsonValue*));
    ctx->copies = malloc(ctx->capacity * sizeof(JsonValue*));
    
    if (!ctx->seen || !ctx->copies) {
        if (ctx->seen) free(ctx->seen);
        if (ctx->copies) free(ctx->copies);
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

static void free_copy_context(CopyContext* ctx) {
    if (!ctx) return;
    free(ctx->seen);
    free(ctx->copies);
    free(ctx);
}

static JsonValue* find_copy(CopyContext* ctx, const JsonValue* original) {
    for (size_t i = 0; i < ctx->count; i++) {
        if (ctx->seen[i] == original) {
            return ctx->copies[i];
        }
    }
    return NULL;
}

static void register_copy(CopyContext* ctx, const JsonValue* original, JsonValue* copy) {
    if (!ctx || !original || !copy) return;
    
    if (ctx->count >= ctx->capacity) {
        size_t new_capacity = ctx->capacity * 2;
        const JsonValue** new_seen = realloc(ctx->seen, new_capacity * sizeof(JsonValue*));
        JsonValue** new_copies = realloc(ctx->copies, new_capacity * sizeof(JsonValue*));
        
        if (!new_seen || !new_copies) {
            // Failed to expand, keep old pointers
            return;
        }
        
        ctx->seen = new_seen;
        ctx->copies = new_copies;
        ctx->capacity = new_capacity;
    }
    ctx->seen[ctx->count] = original;
    ctx->copies[ctx->count] = copy;
    ctx->count++;
}

static JsonValue* json_deep_copy_internal(const JsonValue* value, CopyContext* ctx) {
    if (!value) return NULL;

    // Check for circular reference
    JsonValue* existing = find_copy(ctx, value);
    if (existing) return existing;

    JsonValue* copy = NULL;

    switch (value->type) {
        case JSON_NULL:
            copy = json_create_null();
            break;
        case JSON_BOOL:
            copy = json_create_bool(value->data.bool_value);
            break;
        case JSON_NUMBER:
            copy = json_create_number(value->data.number_value);
            break;
        case JSON_STRING: {
            size_t len = strlen(value->data.string_value);
            copy = json_create_string(value->data.string_value);
            break;
        }
        case JSON_ARRAY: {
            copy = json_create_array();
            register_copy(ctx, value, copy);

            JsonArray* arr = value->data.array_value;
            for (size_t i = 0; i < arr->count; i++) {
                JsonValue* item_copy = json_deep_copy_internal(arr->values[i], ctx);
                json_array_append(copy, item_copy);
            }
            break;
        }
        case JSON_OBJECT: {
            copy = json_create_object();
            register_copy(ctx, value, copy);

            JsonObject* obj = value->data.object_value;
            for (size_t i = 0; i < obj->count; i++) {
                JsonValue* val_copy = json_deep_copy_internal(obj->pairs[i].value, ctx);
                json_object_set(copy, obj->pairs[i].key, val_copy);
            }
            break;
        }
    }

    if (copy && value->type != JSON_ARRAY && value->type != JSON_OBJECT) {
        register_copy(ctx, value, copy);
    }

    return copy;
}

JsonValue* json_deep_copy(const JsonValue* value) {
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Value to copy is NULL", 0, 0);
        return NULL;
    }
    
    CopyContext* ctx = create_copy_context();
    if (!ctx) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to create copy context", 0, 0);
        return NULL;
    }
    
    JsonValue* copy = json_deep_copy_internal(value, ctx);
    free_copy_context(ctx);
    
    if (!copy) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to deep copy value", 0, 0);
    }
    
    return copy;
}

bool json_equals(const JsonValue* val1, const JsonValue* val2) {
    if (!val1 && !val2) return true;
    if (!val1 || !val2) return false;
    if (val1->type != val2->type) return false;

    switch (val1->type) {
        case JSON_NULL:
            return true;
        case JSON_BOOL:
            return val1->data.bool_value == val2->data.bool_value;
        case JSON_NUMBER:
            return val1->data.number_value == val2->data.number_value;
        case JSON_STRING:
            return strcmp(val1->data.string_value, val2->data.string_value) == 0;
        case JSON_ARRAY: {
            JsonArray* arr1 = val1->data.array_value;
            JsonArray* arr2 = val2->data.array_value;
            if (arr1->count != arr2->count) return false;
            for (size_t i = 0; i < arr1->count; i++) {
                if (!json_equals(arr1->values[i], arr2->values[i])) return false;
            }
            return true;
        }
        case JSON_OBJECT: {
            JsonObject* obj1 = val1->data.object_value;
            JsonObject* obj2 = val2->data.object_value;
            if (obj1->count != obj2->count) return false;
            for (size_t i = 0; i < obj1->count; i++) {
                JsonValue* val = json_object_get(val2, obj1->pairs[i].key);
                if (!val || !json_equals(obj1->pairs[i].value, val)) return false;
            }
            return true;
        }
    }
    return false;
}

JsonValue* json_merge(const JsonValue* obj1, const JsonValue* obj2) {
    if (!obj1 || !obj2) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Cannot merge NULL objects", 0, 0);
        return NULL;
    }
    
    if (obj1->type != JSON_OBJECT || obj2->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Can only merge JSON objects", 0, 0);
        return NULL;
    }

    JsonValue* result = json_deep_copy(obj1);
    JsonObject* src = obj2->data.object_value;

    for (size_t i = 0; i < src->count; i++) {
        json_object_set(result, src->pairs[i].key, json_deep_copy(src->pairs[i].value));
    }

    return result;
}
