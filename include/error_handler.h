/**
 * @file error_handler.h
 * @brief Centralized error handling and logging system.
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdbool.h>
#include <SDL.h>

/**
 * @brief Error severity levels.
 */
typedef enum {
    ERROR_LEVEL_DEBUG,
    ERROR_LEVEL_INFO,
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_FATAL
} ErrorLevel;

/**
 * @brief Sets the minimum log level for output.
 * @param level Minimum error level to log.
 */
void error_set_log_level(ErrorLevel level);

/**
 * @brief Sets a log file for error output (optional).
 * @param filename Path to log file, or NULL to disable file logging.
 * @return true on success, false on failure.
 */
bool error_set_log_file(const char* filename);

/**
 * @brief Logs a debug message.
 * @param format Printf-style format string.
 * @param ... Format arguments.
 */
void error_debug(const char* format, ...);

/**
 * @brief Logs an info message.
 * @param format Printf-style format string.
 * @param ... Format arguments.
 */
void error_info(const char* format, ...);

/**
 * @brief Logs a warning message.
 * @param format Printf-style format string.
 * @param ... Format arguments.
 */
void error_warning(const char* format, ...);

/**
 * @brief Logs an error message.
 * @param format Printf-style format string.
 * @param ... Format arguments.
 */
void error_log(const char* format, ...);

/**
 * @brief Logs an error message with errno information.
 * @param message Description of the error.
 */
void error_log_errno(const char* message);

/**
 * @brief Logs a fatal error and exits the program.
 * @param format Printf-style format string.
 * @param ... Format arguments.
 */
void error_fatal(const char* format, ...);

/**
 * @brief Validates a pointer and logs an error if NULL.
 * @param ptr Pointer to validate.
 * @param name Name of the pointer for error messages.
 * @return true if valid, false if NULL.
 */
bool error_check_ptr(const void* ptr, const char* name);

/**
 * @brief Validates a file descriptor and logs an error if invalid.
 * @param fd File descriptor to validate.
 * @param name Name of the FD for error messages.
 * @return true if valid, false if invalid.
 */
bool error_check_fd(int fd, const char* name);

/**
 * @brief Validates an SDL result and logs an error if failed.
 * @param result SDL function return value.
 * @param operation Name of the SDL operation.
 * @return true if successful, false if failed.
 */
bool error_check_sdl(int result, const char* operation);

/**
 * @brief Cleanup function to close log file.
 */
void error_cleanup(void);

#endif // ERROR_HANDLER_H
