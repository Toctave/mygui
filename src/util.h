#pragma once

#define Kibi(v) (1024ull * v)
#define Mebi(v) (1024ull * Kibi(v))
#define Gibi(v) (1024ull * Mebi(v))
#define Tebi(v) (1024ull * Gibi(v))

#define Kilo(v) (1000ull * v)
#define Mega(v) (1000ull * Kilo(v))
#define Giga(v) (1000ull * Mega(v))
#define Tera(v) (1000ull * Giga(v))

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
