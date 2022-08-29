/* architectural definitions for lwIP */
#pragma once
#include <stdint.h>

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;

typedef int8_t s8_t;
typedef int16_t s16_t;
typedef int32_t s32_t;

/* typedef uint8_t* mem_ptr_t; */


#define U16_F "hu"
#define S16_F "d"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "uz"

#define BYTE_ORDER LITTLE_ENDIAN

#define MEM_ALIGNMENT 4

#define LWIP_CHKSUM_ALGORITHM 3 // 32bit

#define PACK_STRUCT_FIELD(x) x __attribute__((packed))
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* #include "pico/stdio.h" */
int printf(const char *format, ...);
#define LWIP_PLATFORM_DIAG(x) printf(x)
#define LWIP_PLATFORM_ASSERT(x) { printf(x); while(1); }

#define LWIP_TIMEVAL_PRIVATE 0

