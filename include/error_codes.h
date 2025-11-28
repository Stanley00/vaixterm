/**
 * @file error_codes.h
 * @brief Error codes and error handling framework for VaixTerm.
 * 
 * Provides a standardized error handling system with error codes,
 * error context, and logging utilities.
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Error codes for VaixTerm operations
 */
typedef enum {
    ERR_SUCCESS = 0,           /**< Operation successful */
    ERR_MEMORY,                /**< Memory allocation failed */
    ERR_FILE_NOT_FOUND,        /**< File not found */
    ERR_FILE_IO,               /**< File I/O error */
    ERR_INVALID_ARGUMENT,      /**< Invalid argument provided */
    ERR_INVALID_STATE,         /**< Invalid state for operation */
    ERR_SDL,                   /**< SDL error */
    ERR_PTY,                   /**< PTY error */
    ERR_PARSE,                 /**< Parsing error */
    ERR_BUFFER_OVERFLOW,       /**< Buffer overflow */
    ERR_NOT_IMPLEMENTED,       /**< Feature not implemented */
    ERR_UNKNOWN                /**< Unknown error */
} ErrorCode;

/**
 * @brief Error context structure
 */
typedef struct {
    ErrorCode code;            /**< Error code */
    char message[256];         /**< Error message */
    const char* file;          /**< Source file where error occurred */
    int line;                  /**< Line number where error occurred */
    const char* function;      /**< Function name where error occurred */
} Error;

/**
 * @brief Log levels for error logging
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

/* ===== Error Logging Macros ===== */

/**
 * @brief Log an error message
 */
#define ERROR_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an error with errno
 */
#define ERROR_LOG_ERRNO(fmt, ...) \
    error_log_errno_impl(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a warning message
 */
#define WARN_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an info message
 */
#define INFO_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a debug message (only in DEBUG builds)
 */
#ifdef DEBUG
#define DEBUG_LOG(fmt, ...) \
    error_log_impl(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) do { } while(0)
#endif

/* ===== Error Logging Functions ===== */

/**
 * @brief Initialize error logging system
 * @param log_file Path to log file, or NULL for stderr
 * @param level Minimum log level to output
 * @return true on success, false on failure
 */
bool error_init(const char* log_file, LogLevel level);

/**
 * @brief Shutdown error logging system
 */
void error_shutdown(void);

/**
 * @brief Log an error message
 * @param level Log level
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fmt Format string
 */
void error_log_impl(LogLevel level, const char* file, int line,
                   const char* func, const char* fmt, ...);

/**
 * @brief Log an error with errno
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fmt Format string
 */
void error_log_errno_impl(const char* file, int line,
                         const char* func, const char* fmt, ...);

/**
 * @brief Get error code name as string
 * @param code Error code
 * @return String name of error code
 */
const char* error_code_to_string(ErrorCode code);

/**
 * @brief Set current error
 * @param code Error code
 * @param message Error message
 */
void error_set(ErrorCode code, const char* message);

/**
 * @brief Get current error
 * @return Current error, or NULL if no error
 */
const Error* error_get(void);

/**
 * @brief Clear current error
 */
void error_clear(void);

#endif // ERROR_CODES_H
