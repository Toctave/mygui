#pragma once

#include <stdarg.h>

typedef struct mem_api mem_api;
typedef struct mem_stack_allocator_o mem_stack_allocator_o;

#define Kibi(v) (1024ull * (v))
#define Mebi(v) (1024ull * Kibi(v))
#define Gibi(v) (1024ull * Mebi(v))
#define Tebi(v) (1024ull * Gibi(v))

#define Kilo(v) (1000ull * (v))
#define Mega(v) (1000ull * Kilo(v))
#define Giga(v) (1000ull * Mega(v))
#define Tera(v) (1000ull * Giga(v))

#define STATIC_ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

#define offsetof(type, member) ((uint8_t*)(&((type*)0)->member) - (uint8_t*)0)

char* vtprintf(mem_api* mem,
               mem_stack_allocator_o* stack,
               const char* fmt,
               va_list args);
char* tprintf(mem_api* mem, mem_stack_allocator_o* stack, const char* fmt, ...);
