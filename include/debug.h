#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

// Debug logging macros
#if DEBUG
#define DEBUG_LOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) ((void)0)
#endif

// Error logging (always shown)
#define ERROR_LOG(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

// Info logging (always shown)
#define INFO_LOG(fmt, ...) fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)

#endif // DEBUG_H
