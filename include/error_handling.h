/**
 * @file error_handling.h
 * @brief Enhanced error handling framework for VaixTerm.
 *
 * This module provides comprehensive error handling capabilities including
 * error context tracking, recovery strategies, and enhanced logging.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "error_codes.h"
#include "validation.h"

// Error severity levels
typedef enum {
    ERROR_SEVERITY_DEBUG,
    ERROR_SEVERITY_INFO,
    ERROR_SEVERITY_WARNING,
    ERROR_SEVERITY_ERROR,
    ERROR_SEVERITY_CRITICAL
} ErrorSeverity;

// Error context structure
typedef struct {
    ErrorCode code;
    ErrorSeverity severity;
    const char* function;
    const char* file;
    int line;
    const char* message;
    void* context_data;
    size_t context_size;
    uint64_t timestamp;
} ErrorContext;

// Error recovery strategies
typedef enum {
    RECOVERY_NONE,
    RECOVERY_RETRY,
    RECOVERY_FALLBACK,
    RECOVERY_RESET,
    RECOVERY_ABORT
} ErrorRecoveryStrategy;

// Error handler callback type
typedef bool (*ErrorHandler)(const ErrorContext* error, void* user_data);

/**
 * @brief Initialize the error handling framework
 *
 * @return bool True if initialization succeeded
 */
bool error_handling_init(void);

/**
 * @brief Cleanup the error handling framework
 */
void error_handling_cleanup(void);

/**
 * @brief Report an error with context
 *
 * @param code Error code
 * @param severity Error severity
 * @param function Function name where error occurred
 * @param file File name where error occurred
 * @param line Line number where error occurred
 * @param message Error message
 * @param context_data Optional context data
 * @param context_size Size of context data
 * @return ErrorRecoveryStrategy Recommended recovery strategy
 */
ErrorRecoveryStrategy report_error(ErrorCode code, ErrorSeverity severity, 
                                   const char* function, const char* file, int line,
                                   const char* message, const void* context_data, size_t context_size);

/**
 * @brief Report a validation error
 *
 * @param result Validation result
 * @param function Function name
 * @param param_name Parameter name that failed validation
 * @return ErrorRecoveryStrategy Recommended recovery strategy
 */
ErrorRecoveryStrategy report_validation_error(ValidationResult result, const char* function, const char* param_name);

/**
 * @brief Report an SDL error
 *
 * @param sdl_operation SDL operation that failed
 * @param function Function name
 * @param file File name
 * @param line Line number
 * @return ErrorRecoveryStrategy Recommended recovery strategy
 */
ErrorRecoveryStrategy report_sdl_error(const char* sdl_operation, const char* function, const char* file, int line);

/**
 * @brief Report a TTF error
 *
 * @param ttf_operation TTF operation that failed
 * @param function Function name
 * @param file File name
 * @param line Line number
 * @return ErrorRecoveryStrategy Recommended recovery strategy
 */
ErrorRecoveryStrategy report_ttf_error(const char* ttf_operation, const char* function, const char* file, int line);

/**
 * @brief Register an error handler callback
 *
 * @param handler Error handler function
 * @param user_data User data to pass to handler
 * @return bool True if registration succeeded
 */
bool register_error_handler(ErrorHandler handler, void* user_data);

/**
 * @brief Unregister an error handler callback
 *
 * @param handler Error handler function
 */
void unregister_error_handler(ErrorHandler handler);

/**
 * @brief Get the last error context
 *
 * @return const ErrorContext* Last error context or NULL
 */
const ErrorContext* get_last_error(void);

/**
 * @brief Clear the last error context
 */
void clear_last_error(void);

/**
 * @brief Get error statistics
 *
 * @param total_errors Output for total error count
 * @param critical_errors Output for critical error count
 * @param last_error_time Output for last error timestamp
 */
void get_error_statistics(int* total_errors, int* critical_errors, uint64_t* last_error_time);

/**
 * @brief Check if an error is recoverable
 *
 * @param code Error code
 * @return bool True if error is recoverable
 */
bool is_error_recoverable(ErrorCode code);

/**
 * @brief Get recommended recovery strategy for an error code
 *
 * @param code Error code
 * @return ErrorRecoveryStrategy Recommended recovery strategy
 */
ErrorRecoveryStrategy get_recovery_strategy(ErrorCode code);

/**
 * @brief Attempt error recovery
 *
 * @param strategy Recovery strategy to attempt
 * @param error Error context
 * @return bool True if recovery succeeded
 */
bool attempt_recovery(ErrorRecoveryStrategy strategy, const ErrorContext* error);

/**
 * @brief Log error with enhanced formatting
 *
 * @param error Error context
 */
void log_error(const ErrorContext* error);

/**
 * @brief Get human-readable error description
 *
 * @param code Error code
 * @return const char* Error description
 */
const char* get_error_description(ErrorCode code);

/**
 * @brief Check if error should be logged based on severity
 *
 * @param severity Error severity
 * @return bool True if error should be logged
 */
bool should_log_error(ErrorSeverity severity);

/**
 * @brief Set minimum error severity for logging
 *
 * @param severity Minimum severity level
 */
void set_log_level(ErrorSeverity severity);

// Convenience macros for error reporting
#define REPORT_ERROR(code, severity, message) \
    report_error(code, severity, __func__, __FILE__, __LINE__, message, NULL, 0)

#define REPORT_ERROR_WITH_CONTEXT(code, severity, message, context, size) \
    report_error(code, severity, __func__, __FILE__, __LINE__, message, context, size)

#define REPORT_SDL_ERROR(operation) \
    report_sdl_error(operation, __func__, __FILE__, __LINE__)

#define REPORT_TTF_ERROR(operation) \
    report_ttf_error(operation, __func__, __FILE__, __LINE__)

#define REPORT_VALIDATION_ERROR(result, param) \
    report_validation_error(result, __func__, param)

#define VALIDATE_AND_RETURN(ptr, param) \
    do { \
        ValidationResult _result = validate_pointer(ptr, param); \
        if (_result != VALIDATION_SUCCESS) { \
            REPORT_VALIDATION_ERROR(_result, param); \
            return false; \
        } \
    } while(0)

#define VALIDATE_AND_RETURN_NULL(ptr, param) \
    do { \
        ValidationResult _result = validate_pointer(ptr, param); \
        if (_result != VALIDATION_SUCCESS) { \
            REPORT_VALIDATION_ERROR(_result, param); \
            return NULL; \
        } \
    } while(0)

#endif // ERROR_HANDLING_H
