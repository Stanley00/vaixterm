/**
 * @file error_handling.c
 * @brief Enhanced error handling framework implementation.
 *
 * This module implements comprehensive error handling capabilities including
 * error context tracking, recovery strategies, and enhanced logging.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "error_handling.h"
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>

// Maximum number of error handlers
#define MAX_ERROR_HANDLERS 16

// Global error handling state
static struct {
    bool initialized;
    ErrorContext last_error;
    int total_errors;
    int critical_errors;
    uint64_t last_error_time;
    ErrorSeverity log_level;
    ErrorHandler handlers[MAX_ERROR_HANDLERS];
    void* handler_data[MAX_ERROR_HANDLERS];
    int num_handlers;
} g_error_state = {0};

bool error_handling_init(void)
{
    if (g_error_state.initialized) {
        DEBUG_LOG("Error handling already initialized");
        return true;
    }

    memset(&g_error_state, 0, sizeof(g_error_state));
    g_error_state.log_level = ERROR_SEVERITY_DEBUG;
    g_error_state.initialized = true;

    INFO_LOG("Error handling framework initialized");
    return true;
}

void error_handling_cleanup(void)
{
    if (!g_error_state.initialized) {
        return;
    }

    memset(&g_error_state, 0, sizeof(g_error_state));
    DEBUG_LOG("Error handling framework cleaned up");
}

static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

ErrorRecoveryStrategy report_error(ErrorCode code, ErrorSeverity severity, 
                                   const char* function, const char* file, int line,
                                   const char* message, const void* context_data, size_t context_size)
{
    if (!g_error_state.initialized) {
        // Fallback to simple logging if not initialized
        fprintf(stderr, "ERROR: %s in %s (%s:%d) - %s\n", 
                get_error_description(code), function, file, line, message ? message : "No message");
        return RECOVERY_NONE;
    }

    // Update error statistics
    g_error_state.total_errors++;
    if (severity == ERROR_SEVERITY_CRITICAL) {
        g_error_state.critical_errors++;
    }
    g_error_state.last_error_time = get_timestamp_ms();

    // Create error context
    ErrorContext error = {
        .code = code,
        .severity = severity,
        .function = function,
        .file = file,
        .line = line,
        .message = message,
        .context_data = (void*)context_data,
        .context_size = context_size,
        .timestamp = g_error_state.last_error_time
    };

    // Store as last error
    g_error_state.last_error = error;

    // Log the error
    if (should_log_error(severity)) {
        log_error(&error);
    }

    // Call error handlers
    bool handled = false;
    for (int i = 0; i < g_error_state.num_handlers; i++) {
        if (g_error_state.handlers[i]) {
            if (g_error_state.handlers[i](&error, g_error_state.handler_data[i])) {
                handled = true;
                break;
            }
        }
    }

    // Return recommended recovery strategy
    return handled ? RECOVERY_RETRY : get_recovery_strategy(code);
}

ErrorRecoveryStrategy report_validation_error(ValidationResult result, const char* function, const char* param_name)
{
    if (result == VALIDATION_SUCCESS) {
        return RECOVERY_NONE;
    }

    ErrorCode code = validation_to_error_code(result);
    const char* message = validation_error_message(result);
    
    char full_message[256];
    snprintf(full_message, sizeof(full_message), "Validation failed for parameter '%s': %s", 
             param_name ? param_name : "unknown", message);

    return report_error(code, ERROR_SEVERITY_ERROR, function, NULL, 0, full_message, NULL, 0);
}

ErrorRecoveryStrategy report_sdl_error(const char* sdl_operation, const char* function, const char* file, int line)
{
    const char* sdl_error = SDL_GetError();
    char message[512];
    snprintf(message, sizeof(message), "SDL operation failed: %s - %s", 
             sdl_operation ? sdl_operation : "unknown", sdl_error ? sdl_error : "No SDL error");

    return report_error(ERR_SDL, ERROR_SEVERITY_ERROR, function, file, line, message, NULL, 0);
}

ErrorRecoveryStrategy report_ttf_error(const char* ttf_operation, const char* function, const char* file, int line)
{
    const char* ttf_error = TTF_GetError();
    char message[512];
    snprintf(message, sizeof(message), "TTF operation failed: %s - %s", 
             ttf_operation ? ttf_operation : "unknown", ttf_error ? ttf_error : "No TTF error");

    return report_error(ERR_SDL, ERROR_SEVERITY_ERROR, function, file, line, message, NULL, 0);
}

bool register_error_handler(ErrorHandler handler, void* user_data)
{
    if (!g_error_state.initialized) {
        ERROR_LOG("Error handling not initialized");
        return false;
    }

    if (!handler) {
        ERROR_LOG("Invalid error handler");
        return false;
    }

    if (g_error_state.num_handlers >= MAX_ERROR_HANDLERS) {
        ERROR_LOG("Too many error handlers registered");
        return false;
    }

    g_error_state.handlers[g_error_state.num_handlers] = handler;
    g_error_state.handler_data[g_error_state.num_handlers] = user_data;
    g_error_state.num_handlers++;

    DEBUG_LOG("Error handler registered");
    return true;
}

void unregister_error_handler(ErrorHandler handler)
{
    if (!g_error_state.initialized || !handler) {
        return;
    }

    for (int i = 0; i < g_error_state.num_handlers; i++) {
        if (g_error_state.handlers[i] == handler) {
            // Shift remaining handlers
            for (int j = i; j < g_error_state.num_handlers - 1; j++) {
                g_error_state.handlers[j] = g_error_state.handlers[j + 1];
                g_error_state.handler_data[j] = g_error_state.handler_data[j + 1];
            }
            g_error_state.num_handlers--;
            DEBUG_LOG("Error handler unregistered");
            return;
        }
    }
}

const ErrorContext* get_last_error(void)
{
    if (!g_error_state.initialized) {
        return NULL;
    }

    return &g_error_state.last_error;
}

void clear_last_error(void)
{
    if (!g_error_state.initialized) {
        return;
    }

    memset(&g_error_state.last_error, 0, sizeof(g_error_state.last_error));
}

void get_error_statistics(int* total_errors, int* critical_errors, uint64_t* last_error_time)
{
    if (!g_error_state.initialized) {
        if (total_errors) *total_errors = 0;
        if (critical_errors) *critical_errors = 0;
        if (last_error_time) *last_error_time = 0;
        return;
    }

    if (total_errors) *total_errors = g_error_state.total_errors;
    if (critical_errors) *critical_errors = g_error_state.critical_errors;
    if (last_error_time) *last_error_time = g_error_state.last_error_time;
}

bool is_error_recoverable(ErrorCode code)
{
    switch (code) {
        case ERR_SUCCESS:
            return true;
        case ERR_INVALID_ARGUMENT:
        case ERR_INVALID_STATE:
        case ERR_BUFFER_OVERFLOW:
            return true;
        case ERR_MEMORY:
            return false;
        case ERR_NOT_IMPLEMENTED:
            return false;
        case ERR_FILE_IO:
            return false;
        case ERR_SDL:
        case ERR_PARSE:
        case ERR_FILE_NOT_FOUND:
        case ERR_PTY:
            return true;
        case ERR_UNKNOWN:
        default:
            return false;
    }
}

ErrorRecoveryStrategy get_recovery_strategy(ErrorCode code)
{
    switch (code) {
        case ERR_SUCCESS:
            return RECOVERY_NONE;
        case ERR_INVALID_ARGUMENT:
        case ERR_INVALID_STATE:
            return RECOVERY_FALLBACK;
        case ERR_MEMORY:
            return RECOVERY_RETRY;
        case ERR_NOT_IMPLEMENTED:
            return RECOVERY_RETRY;
        case ERR_FILE_IO:
            return RECOVERY_RETRY;
        case ERR_SDL:
            return RECOVERY_RESET;
        case ERR_PARSE:
            return RECOVERY_RESET;
        case ERR_FILE_NOT_FOUND:
            return RECOVERY_FALLBACK;
        case ERR_PTY:
            return RECOVERY_RESET;
        case ERR_BUFFER_OVERFLOW:
            return RECOVERY_RETRY;
        case ERR_UNKNOWN:
        default:
            return RECOVERY_ABORT;
    }
}

bool attempt_recovery(ErrorRecoveryStrategy strategy, const ErrorContext* error)
{
    if (!error) {
        return false;
    }

    switch (strategy) {
        case RECOVERY_NONE:
            return false;
            
        case RECOVERY_RETRY:
            DEBUG_LOG("Attempting retry recovery for error: %s", get_error_description(error->code));
            // Retry logic would be implemented by the calling function
            return true;
            
        case RECOVERY_FALLBACK:
            DEBUG_LOG("Attempting fallback recovery for error: %s", get_error_description(error->code));
            // Fallback logic would be implemented by the calling function
            return true;
            
        case RECOVERY_RESET:
            DEBUG_LOG("Attempting reset recovery for error: %s", get_error_description(error->code));
            // Reset logic would be implemented by the calling function
            return true;
            
        case RECOVERY_ABORT:
            DEBUG_LOG("Aborting due to error: %s", get_error_description(error->code));
            return false;
    }

    return false;
}

void log_error(const ErrorContext* error)
{
    if (!error) {
        return;
    }

    const char* severity_str = "";
    switch (error->severity) {
        case ERROR_SEVERITY_DEBUG: severity_str = "DEBUG"; break;
        case ERROR_SEVERITY_INFO: severity_str = "INFO"; break;
        case ERROR_SEVERITY_WARNING: severity_str = "WARNING"; break;
        case ERROR_SEVERITY_ERROR: severity_str = "ERROR"; break;
        case ERROR_SEVERITY_CRITICAL: severity_str = "CRITICAL"; break;
    }

    if (error->file && error->line > 0) {
        fprintf(stderr, "[%s] %s in %s (%s:%d) - %s\n", 
                severity_str, get_error_description(error->code), 
                error->function, error->file, error->line,
                error->message ? error->message : "No message");
    } else {
        fprintf(stderr, "[%s] %s in %s - %s\n", 
                severity_str, get_error_description(error->code), 
                error->function,
                error->message ? error->message : "No message");
    }

    // Log context data if available
    if (error->context_data && error->context_size > 0) {
        fprintf(stderr, "  Context: %zu bytes\n", error->context_size);
    }
}

const char* get_error_description(ErrorCode code)
{
    switch (code) {
        case ERR_SUCCESS: return "No error";
        case ERR_INVALID_ARGUMENT: return "Invalid argument";
        case ERR_INVALID_STATE: return "Invalid state";
        case ERR_MEMORY: return "Memory allocation failed";
        case ERR_NOT_IMPLEMENTED: return "Feature not implemented";
        case ERR_FILE_IO: return "File I/O error";
        case ERR_SDL: return "SDL error";
        case ERR_PARSE: return "Parsing error";
        case ERR_FILE_NOT_FOUND: return "File not found";
        case ERR_PTY: return "PTY error";
        case ERR_BUFFER_OVERFLOW: return "Buffer overflow";
        case ERR_UNKNOWN: return "Unknown error";
        default: return "Undefined error";
    }
}

bool should_log_error(ErrorSeverity severity)
{
    return severity >= g_error_state.log_level;
}

void set_log_level(ErrorSeverity severity)
{
    if (!g_error_state.initialized) {
        return;
    }

    g_error_state.log_level = severity;
    DEBUG_LOG("Error log level set to %d", severity);
}
