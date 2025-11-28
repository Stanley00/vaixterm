#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

// Only define these macros if not already defined in error_codes.h
#ifndef ERROR_LOG
// Error logging (always shown)
#define ERROR_LOG(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef INFO_LOG
// Info logging (always shown)
#define INFO_LOG(fmt, ...) fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef DEBUG_LOG
// Debug logging macros
#if DEBUG
#define DEBUG_LOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) ((void)0)
#endif
#endif

#endif // DEBUG_H
