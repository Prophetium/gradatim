#ifndef _BEKTER_H
#define _BEKTER_H

#include <stddef.h>
#include <stdint.h>

/* [size_t capacity] [size_t size] [payload] */
typedef char *bekter;

bekter bekter_create();
void bekter_drop(bekter b);

void bekter_ensure_space(bekter *b, size_t bytes);

#if defined(_MSC_VER)
    #define TYPEOF(x) decltype(x)
#elif defined(__GNUC__)
    #define TYPEOF(x) typeof(x)
#else
    #define TYPEOF(x) void
#endif

#define bekter(...) bekter

#define bekter_size(__b)        (((size_t *)(__b))[-1])
#define bekter_capacity(__b)    (((size_t *)(__b))[-2])

#define bekter_at_ptr(__b, __i, __type) ((__type *)(__b) + (__i))
#define bekter_at(__b, __i, __type)     (*bekter_at_ptr(__b, __i, __type))

#define bekter_pushback(__b, __v) do { \
    bekter_ensure_space(&(__b), sizeof(__v)); \
    *(TYPEOF(__v) *)((__b) + bekter_size(__b)) = (__v); \
    bekter_size(__b) += sizeof(__v); \
} while (0)

#define bekter_popback(__b, __v) do { \
    if (bekter_size(__b) < sizeof(__v)) break; \
    bekter_size(__b) -= sizeof(__v); \
    (__v) = *(TYPEOF(__v) *)((__b) + bekter_size(__b)); \
} while (0)

#define bekter_remove_at(__b, __i, __type) do { \
    __type v; \
    bekter_popback(__b, v); \
    *bekter_at_ptr(__b, __i, __type) = v; \
} while (0)

#define bekter_clear(__b) (bekter_size(__b) = 0)

#define bekter_each(__b, __i, __v) \
    ((__i) = 0; (__v) = *(TYPEOF(__v) *)((__b) + (__i)), \
        (__i) < bekter_size(__b); (__i) += sizeof(__v))

#define bekter_each_ptr(__b, __i, __v) \
    ((__i) = 0; (__v) = (TYPEOF(__v))((__b) + (__i)), \
        (__i) < bekter_size(__b); (__i) += sizeof(*(__v)))

#endif
