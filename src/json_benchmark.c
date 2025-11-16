#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

typedef struct {
    double parse_time;
    double stringify_time;
    size_t memory_used;
    size_t input_size;
} BenchmarkResult;

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

BenchmarkResult benchmark_simple_object(void) {
    BenchmarkResult result = {0};
    
    const char* json = "{\"name\":\"Test\",\"value\":123,\"active\":true,\"data\":null}";
    result.input_size = strlen(json);
    
    double start = get_time_ms();
    JsonValue* value = json_parse(json);
    double end = get_time_ms();
    result.parse_time = end - start;
    
    if (value) {
        result.memory_used = json_memory_usage(value);
        
        start = get_time_ms();
        char* output = json_stringify(value, false);
        end = get_time_ms();
        result.stringify_time = end - start;
        
        free(output);
        json_free(value);
    }
    
    return result;
}

BenchmarkResult benchmark_large_array(int size) {
    BenchmarkResult result = {0};
    
    char* json = malloc(size * 100);
    strcpy(json, "[");
    
    for (int i = 0; i < size; i++) {
        if (i > 0) strcat(json, ",");
        char buf[100];
        sprintf(buf, "{\"id\":%d,\"name\":\"Item%d\",\"value\":%d}", i, i, i * 10);
        strcat(json, buf);
    }
    strcat(json, "]");
    
    result.input_size = strlen(json);
    
    double start = get_time_ms();
    JsonValue* value = json_parse(json);
    double end = get_time_ms();
    result.parse_time = end - start;
    
    if (value) {
        result.memory_used = json_memory_usage(value);
        
        start = get_time_ms();
        char* output = json_stringify(value, false);
        end = get_time_ms();
        result.stringify_time = end - start;
        
        free(output);
        json_free(value);
    }
    
    free(json);
    return result;
}

BenchmarkResult benchmark_deep_nesting(int depth) {
    BenchmarkResult result = {0};
    
    char* json = malloc(depth * 20);
    json[0] = '\0';
    
    for (int i = 0; i < depth; i++) {
        strcat(json, "{\"nested\":");
    }
    strcat(json, "\"value\"");
    for (int i = 0; i < depth; i++) {
        strcat(json, "}");
    }
    
    result.input_size = strlen(json);
    
    double start = get_time_ms();
    JsonValue* value = json_parse(json);
    double end = get_time_ms();
    result.parse_time = end - start;
    
    if (value) {
        result.memory_used = json_memory_usage(value);
        
        start = get_time_ms();
        char* output = json_stringify(value, false);
        end = get_time_ms();
        result.stringify_time = end - start;
        
        free(output);
        json_free(value);
    }
    
    free(json);
    return result;
}

void print_benchmark_result(const char* name, BenchmarkResult result) {
    printf("%-30s | Parse: %8.3f ms | Stringify: %8.3f ms | Memory: %8zu bytes | Input: %8zu bytes\n",
           name, result.parse_time, result.stringify_time, result.memory_used, result.input_size);
}

void run_all_benchmarks(void) {
    printf("\n=== JSON Parser Benchmarks ===\n\n");
    printf("%-30s | %-17s | %-21s | %-20s | %s\n", 
           "Test Name", "Parse Time", "Stringify Time", "Memory Usage", "Input Size");
    printf("---------------------------------------------------------------------------------------------------------------------------\n");
    
    BenchmarkResult r1 = benchmark_simple_object();
    print_benchmark_result("Simple Object", r1);
    
    BenchmarkResult r2 = benchmark_large_array(100);
    print_benchmark_result("Array (100 items)", r2);
    
    BenchmarkResult r3 = benchmark_large_array(1000);
    print_benchmark_result("Array (1000 items)", r3);
    
    BenchmarkResult r4 = benchmark_large_array(10000);
    print_benchmark_result("Array (10000 items)", r4);
    
    BenchmarkResult r5 = benchmark_deep_nesting(50);
    print_benchmark_result("Deep Nesting (50 levels)", r5);
    
    BenchmarkResult r6 = benchmark_deep_nesting(100);
    print_benchmark_result("Deep Nesting (100 levels)", r6);
    
    printf("\n");
}
