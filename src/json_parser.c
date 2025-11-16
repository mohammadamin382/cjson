#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

#ifdef __x86_64__
#include <immintrin.h>
#define SIMD_AVAILABLE 1
#else
#define SIMD_AVAILABLE 0
#endif

JsonError g_json_last_error = {JSON_ERROR_NONE, "", 0, 0, NULL};

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
        case JSON_ERROR_INVALID_WHITESPACE: return "Invalid whitespace character";
        case JSON_ERROR_INVALID_SURROGATE: return "Invalid UTF-16 surrogate pair";
        case JSON_ERROR_NUMBER_OUT_OF_RANGE: return "Number out of range";
        case JSON_ERROR_LEADING_ZERO: return "Leading zero not allowed";
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
    const char* end;
    size_t line;
    size_t column;
    int depth;
    Token current_token;
} Tokenizer;

static inline bool is_json_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

#if SIMD_AVAILABLE
static const char* skip_whitespace_simd(const char* ptr, size_t* line, size_t* column) {
    while (1) {
        __m128i chunk = _mm_loadu_si128((__m128i*)ptr);
        __m128i space = _mm_set1_epi8(' ');
        __m128i tab = _mm_set1_epi8('\t');
        __m128i newline = _mm_set1_epi8('\n');
        __m128i cr = _mm_set1_epi8('\r');
        
        __m128i cmp_space = _mm_cmpeq_epi8(chunk, space);
        __m128i cmp_tab = _mm_cmpeq_epi8(chunk, tab);
        __m128i cmp_newline = _mm_cmpeq_epi8(chunk, newline);
        __m128i cmp_cr = _mm_cmpeq_epi8(chunk, cr);
        
        __m128i is_ws = _mm_or_si128(_mm_or_si128(cmp_space, cmp_tab), 
                                     _mm_or_si128(cmp_newline, cmp_cr));
        
        int mask = _mm_movemask_epi8(is_ws);
        
        if (mask != 0xFFFF) {
            int non_ws_pos = __builtin_ctz(~mask & 0xFFFF);
            for (int i = 0; i < non_ws_pos; i++) {
                if (ptr[i] == '\n') {
                    (*line)++;
                    *column = 0;
                } else {
                    (*column)++;
                }
            }
            return ptr + non_ws_pos;
        }
        
        for (int i = 0; i < 16; i++) {
            if (ptr[i] == '\n') {
                (*line)++;
                *column = 0;
            } else {
                (*column)++;
            }
            if (!ptr[i]) return ptr + i;
        }
        ptr += 16;
    }
}
#endif

static void skip_whitespace(Tokenizer* t) {
#if SIMD_AVAILABLE
    if ((size_t)t->current % 16 == 0) {
        t->current = skip_whitespace_simd(t->current, &t->line, &t->column);
        return;
    }
#endif
    
    while (*t->current) {
        char c = *t->current;
        if (c == '\n') {
            t->line++;
            t->column = 0;
            t->current++;
        } else if (is_json_whitespace(c)) {
            t->current++;
            t->column++;
        } else if ((unsigned char)c > 0x20 || c == 0) {
            break;
        } else {
            json_set_error(JSON_ERROR_INVALID_WHITESPACE, "Invalid whitespace character", t->line, t->column);
            break;
        }
    }
}

static bool validate_number_format_strict(const char* start, const char* end) {
    if (!start || !end || start >= end) return false;
    
    const char* p = start;
    
    if (*p == '-') {
        p++;
        if (p >= end) return false;
    }
    
    if (*p == '0') {
        p++;
        if (p < end && isdigit(*p)) {
            return false;
        }
    } else if (*p >= '1' && *p <= '9') {
        p++;
        while (p < end && isdigit(*p)) p++;
    } else {
        return false;
    }
    
    if (p < end && *p == '.') {
        p++;
        if (p >= end || !isdigit(*p)) return false;
        while (p < end && isdigit(*p)) p++;
    }
    
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        if (p >= end || !isdigit(*p)) return false;
        while (p < end && isdigit(*p)) p++;
    }
    
    return p == end;
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
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return 0;
        }
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
    return 0;
}

static char* tokenize_string(Tokenizer* t) {
    if (t->current >= t->end || *t->current != '"') return NULL;
    
    t->current++;
    t->column++;
    
    size_t length = 0;
    size_t capacity = 256;
    char* result = malloc(capacity);
    if (!result) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate string", t->line, t->column);
        return NULL;
    }
    memset(result, 0, capacity);
    
    while (t->current < t->end && *t->current && *t->current != '"') {
        if (length + 5 >= capacity) {
            size_t new_capacity = capacity * 2;
            char* new_result = realloc(result, new_capacity);
            if (!new_result) {
                free(result);
                json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand string buffer", t->line, t->column);
                return NULL;
            }
            memset(new_result + capacity, 0, new_capacity - capacity);
            result = new_result;
            capacity = new_capacity;
        }
        
        if (*t->current == '\\') {
            t->current++;
            t->column++;
            if (t->current >= t->end || !*t->current) {
                free(result);
                json_set_error(JSON_ERROR_UNTERMINATED_STRING, "Unterminated escape sequence", t->line, t->column);
                return NULL;
            }
            
            switch (*t->current) {
                case 'n': result[length++] = '\n'; break;
                case 't': result[length++] = '\t'; break;
                case 'r': result[length++] = '\r'; break;
                case 'b': result[length++] = '\b'; break;
                case 'f': result[length++] = '\f'; break;
                case '"': result[length++] = '"'; break;
                case '\\': result[length++] = '\\'; break;
                case '/': result[length++] = '/'; break;
                case 'u': {
                    t->current++;
                    t->column++;
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        if (!*t->current || !isxdigit(*t->current)) {
                            free(result);
                            json_set_error(JSON_ERROR_INVALID_ESCAPE, "Invalid Unicode escape", t->line, t->column);
                            return NULL;
                        }
                        codepoint = (codepoint << 4) | parse_hex_digit(*t->current);
                        if (i < 3) {
                            t->current++;
                            t->column++;
                        }
                    }
                    
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        if (t->current[1] == '\\' && t->current[2] == 'u') {
                            t->current += 3;
                            t->column += 3;
                            unsigned int low_surrogate = 0;
                            for (int i = 0; i < 4; i++) {
                                if (!*t->current || !isxdigit(*t->current)) {
                                    free(result);
                                    json_set_error(JSON_ERROR_INVALID_SURROGATE, "Invalid surrogate pair", t->line, t->column);
                                    return NULL;
                                }
                                low_surrogate = (low_surrogate << 4) | parse_hex_digit(*t->current);
                                if (i < 3) {
                                    t->current++;
                                    t->column++;
                                }
                            }
                            
                            if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) {
                                free(result);
                                json_set_error(JSON_ERROR_INVALID_SURROGATE, "Invalid low surrogate", t->line, t->column);
                                return NULL;
                            }
                            
                            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low_surrogate - 0xDC00);
                        } else {
                            free(result);
                            json_set_error(JSON_ERROR_INVALID_SURROGATE, "High surrogate without low surrogate", t->line, t->column);
                            return NULL;
                        }
                    } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                        free(result);
                        json_set_error(JSON_ERROR_INVALID_SURROGATE, "Unexpected low surrogate", t->line, t->column);
                        return NULL;
                    }
                    
                    char utf8_buffer[5];
                    int utf8_len = encode_utf8(codepoint, utf8_buffer);
                    if (utf8_len == 0) {
                        free(result);
                        json_set_error(JSON_ERROR_INVALID_UTF8, "Invalid codepoint", t->line, t->column);
                        return NULL;
                    }
                    for (int i = 0; i < utf8_len; i++) {
                        result[length++] = utf8_buffer[i];
                    }
                    break;
                }
                default:
                    free(result);
                    json_set_error(JSON_ERROR_INVALID_ESCAPE, "Unknown escape sequence", t->line, t->column);
                    return NULL;
            }
            t->current++;
            t->column++;
        } else if ((unsigned char)*t->current < 0x20) {
            free(result);
            json_set_error(JSON_ERROR_INVALID_SYNTAX, "Unescaped control character in string", t->line, t->column);
            return NULL;
        } else {
            result[length++] = *t->current++;
            t->column++;
        }
    }
    
    if (*t->current != '"') {
        free(result);
        json_set_error(JSON_ERROR_UNTERMINATED_STRING, "Expected closing '\"'", t->line, t->column);
        return NULL;
    }
    t->current++;
    t->column++;
    
    result[length] = '\0';
    return result;
}

static Token next_token(Tokenizer* t) {
    Token token = {TOKEN_ERROR, t->current, 0, t->line, t->column, {0}};
    
    skip_whitespace(t);
    
    if (t->current >= t->end || !*t->current) {
        token.type = TOKEN_EOF;
        return token;
    }
    
    token.start = t->current;
    token.line = t->line;
    token.column = t->column;
    
    switch (*t->current) {
        case '{':
            token.type = TOKEN_LBRACE;
            t->current++;
            t->column++;
            token.length = 1;
            return token;
        case '}':
            token.type = TOKEN_RBRACE;
            t->current++;
            t->column++;
            token.length = 1;
            return token;
        case '[':
            token.type = TOKEN_LBRACKET;
            t->current++;
            t->column++;
            token.length = 1;
            return token;
        case ']':
            token.type = TOKEN_RBRACKET;
            t->current++;
            t->column++;
            token.length = 1;
            return token;
        case ':':
            token.type = TOKEN_COLON;
            t->current++;
            t->column++;
            token.length = 1;
            return token;
        case ',':
            token.type = TOKEN_COMMA;
            t->current++;
            t->column++;
            token.length = 1;
            return token;
        case 'n':
            if (t->current + 4 <= t->end && strncmp(t->current, "null", 4) == 0) {
                token.type = TOKEN_NULL;
                token.length = 4;
                t->current += 4;
                t->column += 4;
                return token;
            }
            break;
        case 't':
            if (t->current + 4 <= t->end && strncmp(t->current, "true", 4) == 0) {
                token.type = TOKEN_TRUE;
                token.length = 4;
                t->current += 4;
                t->column += 4;
                return token;
            }
            break;
        case 'f':
            if (t->current + 5 <= t->end && strncmp(t->current, "false", 5) == 0) {
                token.type = TOKEN_FALSE;
                token.length = 5;
                t->current += 5;
                t->column += 5;
                return token;
            }
            break;
        case '"': {
            char* str = tokenize_string(t);
            if (str) {
                token.type = TOKEN_STRING;
                token.value.string = str;
                token.length = t->current - token.start;
                return token;
            }
            return token;
        }
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
            const char* start = t->current;
            const char* p = start;
            
            if (p >= t->end) {
                json_set_error(JSON_ERROR_UNEXPECTED_EOF, "Unexpected end in number", t->line, t->column);
                return token;
            }
            
            if (*p == '-') {
                p++;
                if (p >= t->end) {
                    json_set_error(JSON_ERROR_INVALID_NUMBER, "Number expected after minus", t->line, t->column);
                    return token;
                }
            }
            
            if (*p == '0') {
                p++;
                if (p < t->end && isdigit(*p)) {
                    json_set_error(JSON_ERROR_LEADING_ZERO, "Leading zeros not allowed", t->line, t->column);
                    return token;
                }
            } else if (*p >= '1' && *p <= '9') {
                p++;
                while (p < t->end && isdigit(*p)) p++;
            } else {
                json_set_error(JSON_ERROR_INVALID_NUMBER, "Invalid number format", t->line, t->column);
                return token;
            }
            
            if (p < t->end && *p == '.') {
                p++;
                if (p >= t->end || !isdigit(*p)) {
                    json_set_error(JSON_ERROR_INVALID_NUMBER, "Digit required after decimal point", t->line, t->column);
                    return token;
                }
                while (p < t->end && isdigit(*p)) p++;
            }
            
            if (p < t->end && (*p == 'e' || *p == 'E')) {
                p++;
                if (p < t->end && (*p == '+' || *p == '-')) p++;
                if (p >= t->end || !isdigit(*p)) {
                    json_set_error(JSON_ERROR_INVALID_NUMBER, "Digit required in exponent", t->line, t->column);
                    return token;
                }
                while (p < t->end && isdigit(*p)) p++;
            }
            
            if (!validate_number_format_strict(start, p)) {
                json_set_error(JSON_ERROR_INVALID_NUMBER, "Invalid number format", t->line, t->column);
                return token;
            }
            
            errno = 0;
            char* end;
            double value = strtod(start, &end);
            
            if (errno == ERANGE) {
                json_set_error(JSON_ERROR_NUMBER_OUT_OF_RANGE, "Number out of range", t->line, t->column);
                return token;
            }
            
            if (isinf(value) || isnan(value)) {
                json_set_error(JSON_ERROR_NUMBER_OUT_OF_RANGE, "Number is infinity or NaN", t->line, t->column);
                return token;
            }
            
            token.type = TOKEN_NUMBER;
            token.value.number = value;
            token.length = p - start;
            t->column += (p - t->current);
            t->current = p;
            return token;
        }
    }
    
    json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Unexpected character", t->line, t->column);
    return token;
}

static JsonValue* parse_value(Tokenizer* t);

#define MAX_NESTING_DEPTH 1000

static JsonValue* parse_array(Tokenizer* t) {
    if (++t->depth > MAX_NESTING_DEPTH) {
        json_set_error(JSON_ERROR_STACK_OVERFLOW, "Nesting too deep", t->line, t->column);
        t->depth--;
        return NULL;
    }
    
    JsonValue* array = json_create_array();
    if (!array) {
        t->depth--;
        return NULL;
    }
    
    t->current_token = next_token(t);
    
    if (t->current_token.type == TOKEN_RBRACKET) {
        t->depth--;
        return array;
    }
    
    while (true) {
        JsonValue* value = parse_value(t);
        if (!value) {
            json_free(array);
            t->depth--;
            return NULL;
        }
        
        if (!json_array_append(array, value)) {
            json_free(value);
            json_free(array);
            t->depth--;
            return NULL;
        }
        
        t->current_token = next_token(t);
        
        if (t->current_token.type == TOKEN_COMMA) {
            t->current_token = next_token(t);
        } else if (t->current_token.type == TOKEN_RBRACKET) {
            break;
        } else {
            json_free(array);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected ',' or ']'", t->current_token.line, t->current_token.column);
            t->depth--;
            return NULL;
        }
    }
    
    t->depth--;
    return array;
}

static JsonValue* parse_object(Tokenizer* t) {
    if (++t->depth > MAX_NESTING_DEPTH) {
        json_set_error(JSON_ERROR_STACK_OVERFLOW, "Nesting too deep", t->line, t->column);
        t->depth--;
        return NULL;
    }
    
    JsonValue* object = json_create_object();
    if (!object) {
        t->depth--;
        return NULL;
    }
    
    t->current_token = next_token(t);
    
    if (t->current_token.type == TOKEN_RBRACE) {
        t->depth--;
        return object;
    }
    
    while (true) {
        if (t->current_token.type != TOKEN_STRING) {
            json_free(object);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected string key", t->current_token.line, t->current_token.column);
            t->depth--;
            return NULL;
        }
        
        char* key = t->current_token.value.string;
        
        t->current_token = next_token(t);
        if (t->current_token.type != TOKEN_COLON) {
            free(key);
            json_free(object);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected ':'", t->current_token.line, t->current_token.column);
            t->depth--;
            return NULL;
        }
        
        t->current_token = next_token(t);
        JsonValue* value = parse_value(t);
        if (!value) {
            free(key);
            json_free(object);
            t->depth--;
            return NULL;
        }
        
        if (!json_object_set(object, key, value)) {
            free(key);
            json_free(value);
            json_free(object);
            t->depth--;
            return NULL;
        }
        free(key);
        
        t->current_token = next_token(t);
        
        if (t->current_token.type == TOKEN_COMMA) {
            t->current_token = next_token(t);
        } else if (t->current_token.type == TOKEN_RBRACE) {
            break;
        } else {
            json_free(object);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Expected ',' or '}'", t->current_token.line, t->current_token.column);
            t->depth--;
            return NULL;
        }
    }
    
    t->depth--;
    return object;
}

static JsonValue* parse_value(Tokenizer* t) {
    switch (t->current_token.type) {
        case TOKEN_NULL:
            return json_create_null();
        case TOKEN_TRUE:
            return json_create_bool(true);
        case TOKEN_FALSE:
            return json_create_bool(false);
        case TOKEN_NUMBER:
            return json_create_number(t->current_token.value.number);
        case TOKEN_STRING: {
            JsonValue* val = json_create_string(t->current_token.value.string);
            free(t->current_token.value.string);
            return val;
        }
        case TOKEN_LBRACKET:
            return parse_array(t);
        case TOKEN_LBRACE:
            return parse_object(t);
        case TOKEN_EOF:
            json_set_error(JSON_ERROR_UNEXPECTED_EOF, "Unexpected end of input", t->line, t->column);
            return NULL;
        default:
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Unexpected token", t->current_token.line, t->current_token.column);
            return NULL;
    }
}

JsonValue* json_parse_with_error(const char* json_string, JsonError* error) {
    json_clear_error();
    
    if (!json_string) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Input string is NULL", 0, 0);
        if (error) *error = g_json_last_error;
        return NULL;
    }
    
    size_t input_length = strlen(json_string);
    Tokenizer tokenizer = {
        json_string, 
        json_string, 
        json_string + input_length,
        1, 
        0, 
        0, 
        {TOKEN_ERROR, NULL, 0, 0, 0, {0}}
    };
    tokenizer.current_token = next_token(&tokenizer);
    
    JsonValue* result = parse_value(&tokenizer);
    
    if (result) {
        Token next = next_token(&tokenizer);
        if (next.type != TOKEN_EOF) {
            json_free(result);
            json_set_error(JSON_ERROR_UNEXPECTED_TOKEN, "Extra data after JSON value", next.line, next.column);
            result = NULL;
        }
    }
    
    if (error) *error = g_json_last_error;
    return result;
}

JsonValue* json_parse(const char* json_string) {
    return json_parse_with_error(json_string, NULL);
}

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
        size_t new_capacity = arr->capacity == 0 ? 16 : arr->capacity * 2;
        JsonValue** new_values = realloc(arr->values, new_capacity * sizeof(JsonValue*));
        if (!new_values) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand array", 0, 0);
            return false;
        }
        memset(new_values + arr->capacity, 0, (new_capacity - arr->capacity) * sizeof(JsonValue*));
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
    
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            json_free(obj->pairs[i].value);
            obj->pairs[i].value = value;
            return true;
        }
    }
    
    if (obj->count >= obj->capacity) {
        size_t new_capacity = obj->capacity == 0 ? 16 : obj->capacity * 2;
        JsonPair* new_pairs = realloc(obj->pairs, new_capacity * sizeof(JsonPair));
        if (!new_pairs) {
            json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to expand object", 0, 0);
            return false;
        }
        memset(new_pairs + obj->capacity, 0, (new_capacity - obj->capacity) * sizeof(JsonPair));
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

bool json_array_remove(JsonValue* array, size_t index) {
    if (!array) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Array is NULL", 0, 0);
        return false;
    }
    
    if (array->type != JSON_ARRAY) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an array", 0, 0);
        return false;
    }
    
    JsonArray* arr = array->data.array_value;
    if (index >= arr->count) {
        json_set_error(JSON_ERROR_INDEX_OUT_OF_BOUNDS, "Index out of bounds", 0, 0);
        return false;
    }
    
    json_free(arr->values[index]);
    
    for (size_t i = index; i < arr->count - 1; i++) {
        arr->values[i] = arr->values[i + 1];
    }
    arr->count--;
    
    return true;
}

bool json_object_remove(JsonValue* object, const char* key) {
    if (!object || !key) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Object or key is NULL", 0, 0);
        return false;
    }
    
    if (object->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an object", 0, 0);
        return false;
    }
    
    JsonObject* obj = object->data.object_value;
    
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->pairs[i].key, key) == 0) {
            free(obj->pairs[i].key);
            json_free(obj->pairs[i].value);
            
            for (size_t j = i; j < obj->count - 1; j++) {
                obj->pairs[j] = obj->pairs[j + 1];
            }
            obj->count--;
            return true;
        }
    }
    
    json_set_error(JSON_ERROR_KEY_NOT_FOUND, key, 0, 0);
    return false;
}

const char** json_object_keys(const JsonValue* object, size_t* count) {
    if (!object || !count) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Object or count pointer is NULL", 0, 0);
        return NULL;
    }
    
    if (object->type != JSON_OBJECT) {
        json_set_error(JSON_ERROR_INVALID_TYPE, "Not an object", 0, 0);
        *count = 0;
        return NULL;
    }
    
    JsonObject* obj = object->data.object_value;
    *count = obj->count;
    
    if (obj->count == 0) return NULL;
    
    const char** keys = malloc(obj->count * sizeof(char*));
    if (!keys) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Failed to allocate keys array", 0, 0);
        *count = 0;
        return NULL;
    }
    
    for (size_t i = 0; i < obj->count; i++) {
        keys[i] = obj->pairs[i].key;
    }
    
    return keys;
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
