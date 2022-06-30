#pragma once

#include <inttypes.h>

typedef struct hash_t
{
    uint32_t bucket_count;
    uint64_t* keys;
    uint64_t* values;
} hash_t;

uint64_t hash_find(const hash_t* hash, uint64_t key, uint64_t default_value);
void hash_insert(const hash_t* hash, uint64_t key, uint64_t value);
void hash_remove(const hash_t* hash, uint64_t key);

uint64_t hash_combine(uint64_t base, uint64_t n);
uint64_t hash_string(const char* txt);
