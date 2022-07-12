#include "hash.h"
#include "assert.h"

#define KEY_NONE 0
#define KEY_TUMBSTONE UINT64_MAX

static uint32_t hash_find_bucket(const hash_t* hash, uint64_t key)
{
    ASSERT(key != KEY_NONE && key != KEY_TUMBSTONE);
    uint64_t bucket_index = key % hash->bucket_count;
    uint32_t visited = 0;

    while (hash->keys[bucket_index] && hash->keys[bucket_index] != key)
    {
        bucket_index = (bucket_index + 1) % hash->bucket_count;
        visited++;
        ASSERT_OP(visited, <, hash->bucket_count, "%u");
    }

    return bucket_index;
}

static uint32_t hash_find_free_bucket(const hash_t* hash, uint64_t key)
{
    ASSERT(key != KEY_NONE && key != KEY_TUMBSTONE);
    uint64_t bucket_index = key % hash->bucket_count;
    uint32_t visited = 0;

    while (hash->keys[bucket_index]                     //
           && hash->keys[bucket_index] != KEY_TUMBSTONE //
           && hash->keys[bucket_index] != key)
    {
        bucket_index = (bucket_index + 1) % hash->bucket_count;
        visited++;
        ASSERT_OP(visited, <, hash->bucket_count, "%u");
    }

    return bucket_index;
}

// TODO(octave) : better hash functions. MurmurHash ? MeowHash ?
uint64_t hash_combine(uint64_t base, uint64_t new)
{
    return base * 37 + new + 1;
}

uint64_t hash_string(const char* txt)
{
    uint64_t h = 1;
    char c;
    while ((c = *txt++))
    {
        h = hash_combine(h, c);
    }

    return h;
}

uint64_t hash_find(const hash_t* hash, uint64_t key, uint64_t default_value)
{
    uint32_t bucket = hash_find_bucket(hash, key);
    if (hash->keys[bucket] == key)
    {
        return hash->values[bucket];
    }
    else
    {
        return default_value;
    }
}

void hash_insert(const hash_t* hash, uint64_t key, uint64_t value)
{
    uint32_t bucket = hash_find_free_bucket(hash, key);

    hash->keys[bucket] = key;
    hash->values[bucket] = value;
}

void hash_remove(const hash_t* hash, uint64_t key)
{
    uint32_t bucket = hash_find_bucket(hash, key);

    if (hash->keys[bucket] == key)
    {
        hash->keys[bucket] = KEY_TUMBSTONE;
    }
}
