#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

JsonValue* json_parse_file(const char* filename) {
    if (!filename) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Filename is NULL", 0, 0);
        return NULL;
    }
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", filename);
        json_set_error(JSON_ERROR_FILE_NOT_FOUND, error_msg, 0, 0);
        return NULL;
    }
    
    if (fseek(file, 0, SEEK_END) != 0) {
        json_set_error(JSON_ERROR_FILE_READ_ERROR, "Cannot seek to end of file", 0, 0);
        fclose(file);
        return NULL;
    }
    
    long size = ftell(file);
    if (size < 0) {
        json_set_error(JSON_ERROR_FILE_READ_ERROR, "Cannot determine file size", 0, 0);
        fclose(file);
        return NULL;
    }
    
    if (size == 0) {
        json_set_error(JSON_ERROR_FILE_READ_ERROR, "File is empty", 0, 0);
        fclose(file);
        return NULL;
    }
    
    if (size > 100 * 1024 * 1024) {
        json_set_error(JSON_ERROR_FILE_READ_ERROR, "File too large (>100MB)", 0, 0);
        fclose(file);
        return NULL;
    }
    
    if (fseek(file, 0, SEEK_SET) != 0) {
        json_set_error(JSON_ERROR_FILE_READ_ERROR, "Cannot seek to start of file", 0, 0);
        fclose(file);
        return NULL;
    }
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        json_set_error(JSON_ERROR_OUT_OF_MEMORY, "Cannot allocate buffer for file", 0, 0);
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, 1, size, file);
    if (bytes_read != (size_t)size) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Read %zu bytes, expected %ld", bytes_read, size);
        json_set_error(JSON_ERROR_FILE_READ_ERROR, error_msg, 0, 0);
        free(buffer);
        fclose(file);
        return NULL;
    }
    
    buffer[size] = '\0';
    fclose(file);
    
    JsonValue* result = json_parse(buffer);
    free(buffer);
    
    if (!result) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to parse JSON from file: %s", filename);
        json_set_error(JSON_ERROR_INVALID_SYNTAX, error_msg, 0, 0);
    }
    
    return result;
}

bool json_save_file(const char* filename, const JsonValue* value, bool pretty) {
    if (!filename) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Filename is NULL", 0, 0);
        return false;
    }
    
    if (!value) {
        json_set_error(JSON_ERROR_NULL_POINTER, "Value to save is NULL", 0, 0);
        return false;
    }
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Cannot open file for writing: %s", filename);
        json_set_error(JSON_ERROR_FILE_WRITE_ERROR, error_msg, 0, 0);
        return false;
    }
    
    char* json_str = json_stringify(value, pretty);
    if (!json_str) {
        fclose(file);
        json_set_error(JSON_ERROR_CONVERSION_FAILED, "Failed to stringify JSON", 0, 0);
        return false;
    }
    
    size_t len = strlen(json_str);
    size_t written = fprintf(file, "%s", json_str);
    free(json_str);
    
    if (written != len) {
        fclose(file);
        json_set_error(JSON_ERROR_FILE_WRITE_ERROR, "Failed to write complete JSON to file", 0, 0);
        return false;
    }
    
    if (fclose(file) != 0) {
        json_set_error(JSON_ERROR_FILE_WRITE_ERROR, "Failed to close file properly", 0, 0);
        return false;
    }
    
    return true;
}
