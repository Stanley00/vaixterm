#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdio.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

/* ===== Error Logging Macros ===== */
/* All levels are compiled in; filtering is done at runtime via min_level. */

#define ERROR_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define WARN_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define INFO_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define DEBUG_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

void error_log_impl(LogLevel level, const char* file, int line,
                   const char* func, const char* fmt, ...);

void log_set_level(LogLevel level);
LogLevel log_get_level(void);

#endif /* ERROR_CODES_H */
