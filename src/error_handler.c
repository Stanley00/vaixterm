/**
 * @file error_handler.c
 * @brief Centralized error handling and logging system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#include "error_handler.h"

static ErrorLevel current_log_level = ERROR_LEVEL_INFO;
static FILE* log_file = NULL;

/**
 * @brief Sets the minimum log level for output.
 */
void error_set_log_level(ErrorLevel level)
{
    current_log_level = level;
}

/**
 * @brief Sets a log file for error output (optional).
 */
bool error_set_log_file(const char* filename)
{
    if (log_file && log_file != stderr) {
        fclose(log_file);
    }
    
    if (!filename) {
        log_file = NULL;
        return true;
    }
    
    log_file = fopen(filename, "a");
    if (!log_file) {
        fprintf(stderr, "Warning: Could not open log file '%s': %s\n", 
                filename, strerror(errno));
        return false;
    }
    
    return true;
}

/**
 * @brief Internal logging function.
 */
static void log_message(ErrorLevel level, const char* format, va_list args)
{
    if (level < current_log_level) {
        return;
    }
    
    const char* level_str;
    switch (level) {
    case ERROR_LEVEL_DEBUG: level_str = "DEBUG"; break;
    case ERROR_LEVEL_INFO: level_str = "INFO"; break;
    case ERROR_LEVEL_WARNING: level_str = "WARNING"; break;
    case ERROR_LEVEL_ERROR: level_str = "ERROR"; break;
    case ERROR_LEVEL_FATAL: level_str = "FATAL"; break;
    default: level_str = "UNKNOWN"; break;
    }
    
    // Get timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Output to stderr
    fprintf(stderr, "[%s] %s: ", timestamp, level_str);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    
    // Output to log file if set
    if (log_file) {
        fprintf(log_file, "[%s] %s: ", timestamp, level_str);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

/**
 * @brief Logs a debug message.
 */
void error_debug(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_message(ERROR_LEVEL_DEBUG, format, args);
    va_end(args);
}

/**
 * @brief Logs an info message.
 */
void error_info(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_message(ERROR_LEVEL_INFO, format, args);
    va_end(args);
}

/**
 * @brief Logs a warning message.
 */
void error_warning(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_message(ERROR_LEVEL_WARNING, format, args);
    va_end(args);
}

/**
 * @brief Logs an error message.
 */
void error_log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_message(ERROR_LEVEL_ERROR, format, args);
    va_end(args);
}

/**
 * @brief Logs an error message with errno information.
 */
void error_log_errno(const char* message)
{
    error_log("%s: %s", message, strerror(errno));
}

/**
 * @brief Logs a fatal error and exits the program.
 */
void error_fatal(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_message(ERROR_LEVEL_FATAL, format, args);
    va_end(args);
    
    exit(1);
}

/**
 * @brief Validates a pointer and logs an error if NULL.
 */
bool error_check_ptr(const void* ptr, const char* name)
{
    if (!ptr) {
        error_log("NULL pointer: %s", name);
        return false;
    }
    return true;
}

/**
 * @brief Validates a file descriptor and logs an error if invalid.
 */
bool error_check_fd(int fd, const char* name)
{
    if (fd < 0) {
        error_log("Invalid file descriptor: %s (%d)", name, fd);
        return false;
    }
    return true;
}

/**
 * @brief Validates an SDL result and logs an error if failed.
 */
bool error_check_sdl(int result, const char* operation)
{
    if (result != 0) {
        error_log("SDL operation failed: %s - %s", operation, SDL_GetError());
        return false;
    }
    return true;
}

/**
 * @brief Cleanup function to close log file.
 */
void error_cleanup(void)
{
    if (log_file && log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
}
