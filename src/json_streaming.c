#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define STREAM_BUFFER_SIZE 8192
#define MAX_STREAM_DEPTH 256

typedef enum {
    STREAM_STATE_START,
    STREAM_STATE_IN_OBJECT,
    STREAM_STATE_IN_ARRAY,
    STREAM_STATE_IN_STRING,
    STREAM_STATE_IN_NUMBER,
    STREAM_STATE_IN_VALUE,
    STREAM_STATE_ERROR
} StreamState;

struct JsonStreamParser {
    JsonStreamCallback callback;
    void* user_data;
    char* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    size_t line;
    size_t column;
    int depth;
    bool in_string;
    bool escaped;
    JsonError error;
    StreamState state;
    size_t objects_parsed;
    size_t arrays_parsed;
    size_t values_parsed;
};

JsonStreamParser* json_stream_parser_create(JsonStreamCallback callback, void* user_data) {
    if (!callback) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Callback is NULL", 0, 0);
        return NULL;
    }
    
    JsonStreamParser* parser = malloc(sizeof(JsonStreamParser));
    if (!parser) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate stream parser", 0, 0);
        return NULL;
    }
    
    parser->callback = callback;
    parser->user_data = user_data;
    parser->buffer_capacity = STREAM_BUFFER_SIZE;
    parser->buffer_size = 0;
    parser->buffer = malloc(parser->buffer_capacity);
    parser->line = 1;
    parser->column = 0;
    parser->depth = 0;
    parser->in_string = false;
    parser->escaped = false;
    parser->error.code = JSON_ERROR_NONE;
    parser->state = STREAM_STATE_START;
    parser->objects_parsed = 0;
    parser->arrays_parsed = 0;
    parser->values_parsed = 0;
    
    if (!parser->buffer) {
        free(parser);
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate stream buffer", 0, 0);
        return NULL;
    }
    
    return parser;
}

static bool stream_emit_event(JsonStreamParser* parser, JsonStreamEvent* event) {
    return parser->callback(event, parser->user_data);
}

static bool stream_try_parse_complete_value(JsonStreamParser* parser) {
    if (parser->buffer_size == 0) return true;
    
    JsonError error;
    JsonValue* value = json_parse_with_error(parser->buffer, &error);
    
    if (value) {
        JsonStreamEvent event = {JSON_EVENT_VALUE, value, NULL};
        bool result = stream_emit_event(parser, &event);
        json_free(value);
        parser->buffer_size = 0;
        parser->buffer[0] = '\0';
        return result;
    }
    
    if (error.code != JSON_ERROR_UNEXPECTED_EOF) {
        parser->error = error;
        JsonStreamEvent event = {JSON_EVENT_ERROR, NULL, NULL};
        stream_emit_event(parser, &event);
        return false;
    }
    
    return true;
}

bool json_stream_parse_chunk(JsonStreamParser* parser, const char* chunk, size_t length) {
    if (!parser || !chunk) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Parser or chunk is NULL", 0, 0);
        return false;
    }
    
    if (length > 100 * 1024 * 1024) {  // 100MB per chunk limit
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Chunk too large (>100MB)", 0, 0);
        return false;
    }
    
    for (size_t i = 0; i < length; i++) {
        char c = chunk[i];
        
        if (parser->buffer_size + 2 >= parser->buffer_capacity) {
            // Prevent buffer from growing too large
            if (parser->buffer_capacity >= 100 * 1024 * 1024) {
                json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Stream buffer too large (>100MB)", parser->line, parser->column);
                return false;
            }
            
            size_t new_capacity = parser->buffer_capacity * 2;
            if (new_capacity > 100 * 1024 * 1024) {
                new_capacity = 100 * 1024 * 1024;
            }
            
            char* new_buffer = realloc(parser->buffer, new_capacity);
            if (!new_buffer) {
                json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand stream buffer", parser->line, parser->column);
                return false;
            }
            parser->buffer = new_buffer;
            parser->buffer_capacity = new_capacity;
        }
        
        parser->buffer[parser->buffer_size++] = c;
        parser->buffer[parser->buffer_size] = '\0';
        
        if (c == '\n') {
            parser->line++;
            parser->column = 0;
        } else {
            parser->column++;
        }
        
        if (parser->in_string) {
            if (parser->escaped) {
                parser->escaped = false;
            } else if (c == '\\') {
                parser->escaped = true;
            } else if (c == '"') {
                parser->in_string = false;
            }
        } else {
            if (c == '"') {
                parser->in_string = true;
            } else if (c == '{' || c == '[') {
                if (parser->depth >= MAX_STREAM_DEPTH) {
                    json_set_error(JSON_ERROR_STACK_OVERFLOW, "Stream depth too deep", parser->line, parser->column);
                    return false;
                }
                parser->depth++;
                if (parser->depth == 1 && parser->buffer_size == 1) {
                    if (c == '{') {
                        parser->objects_parsed++;
                    } else {
                        parser->arrays_parsed++;
                    }
                    JsonStreamEvent event = {
                        c == '{' ? JSON_EVENT_OBJECT_START : JSON_EVENT_ARRAY_START,
                        NULL, NULL
                    };
                    if (!stream_emit_event(parser, &event)) return false;
                }
            } else if (c == '}' || c == ']') {
                parser->depth--;
                if (parser->depth == 0) {
                    if (!stream_try_parse_complete_value(parser)) {
                        return false;
                    }
                    parser->values_parsed++;
                    JsonStreamEvent event = {
                        c == '}' ? JSON_EVENT_OBJECT_END : JSON_EVENT_ARRAY_END,
                        NULL, NULL
                    };
                    if (!stream_emit_event(parser, &event)) return false;
                }
            } else if (parser->depth == 0 && !isspace(c)) {
                if (c == ',' || c == ':') {
                    continue;
                }
            }
        }
    }
    
    if (parser->depth == 0 && parser->buffer_size > 0) {
        return stream_try_parse_complete_value(parser);
    }
    
    return true;
}

bool json_stream_parse_file(JsonStreamParser* parser, const char* filename) {
    if (!parser || !filename) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Parser or filename is NULL", 0, 0);
        return false;
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        json_set_error(JSON_ERROR_FILE_NOT_FOUND, filename, 0, 0);
        return false;
    }
    
    char chunk[STREAM_BUFFER_SIZE];
    size_t bytes_read;
    bool success = true;
    
    while ((bytes_read = fread(chunk, 1, STREAM_BUFFER_SIZE, file)) > 0) {
        if (!json_stream_parse_chunk(parser, chunk, bytes_read)) {
            success = false;
            break;
        }
    }
    
    fclose(file);
    
    if (success && parser->buffer_size > 0) {
        success = stream_try_parse_complete_value(parser);
    }
    
    if (success) {
        JsonStreamEvent event = {JSON_EVENT_EOF, NULL, NULL};
        stream_emit_event(parser, &event);
    }
    
    return success;
}

void json_stream_parser_free(JsonStreamParser* parser) {
    if (!parser) return;
    free(parser->buffer);
    free(parser);
}
