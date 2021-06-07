/*! \file
 *  \brief Definition of standard integer types with specified widths and their limits
 */

#pragma once

// Standard Integer Types
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#if defined(__i386__)
typedef unsigned long      uintptr_t;
#elif defined(__x86_64__)
typedef unsigned long long uintptr_t;
#else
#error Unknown architecture
#endif
typedef __SIZE_TYPE__      size_t;

typedef char               int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
#if defined(__i386__)
typedef long               intptr_t;
typedef int32_t            ssize_t;
#elif defined(__x86_64__)
typedef long long          intptr_t;
typedef long int           ssize_t;
#else
#error Unknown architecture
#endif

typedef __PTRDIFF_TYPE__   ptrdiff_t;


// validate typedef size
static_assert(sizeof(int8_t)   == (1), "Wrong size for 'int8_t'");
static_assert(sizeof(int16_t)  == (2), "Wrong size for 'int16_t'");
static_assert(sizeof(int32_t)  == (4), "Wrong size for 'int32_t'");
static_assert(sizeof(int64_t)  == (8), "Wrong size for 'int64_t'");
static_assert(sizeof(intptr_t) == sizeof(void*), "Wrong size for 'intptr_t'");
static_assert(sizeof(uint8_t)   == (1), "Wrong size for 'uint8_t'");
static_assert(sizeof(uint16_t)  == (2), "Wrong size for 'uint16_t'");
static_assert(sizeof(uint32_t)  == (4), "Wrong size for 'uint32_t'");
static_assert(sizeof(uint64_t)  == (8), "Wrong size for 'uint64_t'");
static_assert(sizeof(uintptr_t) == sizeof(void*), "Wrong size for 'uintptr_t'");


#ifndef NULL
#define NULL ((uintptr_t)0)
#endif

// Limits
#define INT8_MIN            (-__INT8_MAX__-1)
#define INT8_MAX            (__INT8_MAX__)
#define INT16_MIN           (-__INT16_MAX__-1)
#define INT16_MAX           (__INT16_MAX__)
#define INT32_MIN           (-__INT32_MAX__-1)
#define INT32_MAX           (__INT32_MAX__)
#define INT64_MIN           (-__INT64_MAX__-1)
#define INT64_MAX           (__INT64_MAX__)
#define INTPTR_MIN          (-__INTPTR_MAX__-1)
#define INTPTR_MAX          (__INTPTR_MAX__)

#define UINT8_MAX           (__UINT8_MAX__)
#define UINT16_MAX          (__UINT16_MAX__)
#define UINT32_MAX          (__UINT32_MAX__)
#define UINT64_MAX          (__UINT64_MAX__)
#define UINTPTR_MAX         (__UINTPTR_MAX__)

#define PTRDIFF_MIN         (-__PTRDIFF_MAX__-1)
#define PTRDIFF_MAX         (__PTRDIFF_MAX__)

#define SIZE_MAX            (__SIZE_MAX__)
#if defined(__i386__)
#define SSIZE_MIN           (-__INT32_MAX__-1)
#define SSIZE_MAX           (__INT32_MAX__)
#elif defined(__x86_64__)
#define SSIZE_MIN           (-__INT64_MAX__-1)
#define SSIZE_MAX           (__INT64_MAX__)
#else
#error Unknown architecture
#endif
