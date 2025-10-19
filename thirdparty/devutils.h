#ifndef DEVUTILS_H
#define DEVUTILS_H

#include <stdlib.h>
#include <stdio.h>

// Copied from https://github.com/tsoding/nob.h/blob/main/nob.h
#define UNUSED(value) (void)(value)
#define UNREACHABLE(message) do { fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

#define UNIMPLEMENTED(message) do { fprintf(stderr, "%s:%d: UNIMPLEMENTED: %s\n", __FILE__, __LINE__, message); abort(); } while(0)

// Copied from https://github.com/tsoding/la/blob/master/lag.c
#if defined(__GNUC__) || defined(__clang__)
#define CHECK_PRINTF_FMT(a, b) __attribute__ ((format (printf, a, b)))
#else
#define CHECK_PRINTF_FMT(...)
#endif

#endif // DEVUTILS_H
