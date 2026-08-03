#ifndef __OGG_CONFIG_TYPES_H__
#define __OGG_CONFIG_TYPES_H__

/* Deterministic LP64 configuration for the supported x86_64 Linux target. */
#define INCLUDE_INTTYPES_H 1
#define INCLUDE_STDINT_H 1
#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SHORT 2

#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
typedef uint64_t ogg_uint64_t;

#endif
