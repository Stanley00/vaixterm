/**
 * @file error_codes.c
 * @brief Error handling implementation for VaixTerm.
 */

#include "error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

/* Global error state */
static struct {
    Error current_error;
    FILE* log_file;
    LogLevel min_level;
    bool initialized;
} g_error_state = {
    .current_error = {0},
    .log_file = NULL,
    .min_level = LOG_LEVEL_INFO,
    .initialized = false
};

/* ===== Error Code Names ===== */

static const char* error_code_names[] = {
    [ERR_SUCCESS] = "SUCCESS",
    [ERR_MEMORY] = "MEMORY",
    [ERR_FILE_NOT_FOUND] = "FILE_NOT_FOUND",
    [ERR_FILE_IO] = "FILE_IO",
    [ERR_INVALID_ARGUMENT] = "INVALID_ARGUMENT",
    [ERR_INVALID_STATE] = "INVALID_STATE",
    [ERR_SDL] = "SDL",
    [ERR_PTY] = "PTY",
    [ERR_PARSE] = "PARSE",
    [ERR_BUFFER_OVERFLOW] = "BUFFER_OVERFLOW",
    [ERR_NOT_IMPLEMENTED] = "NOT_IMPLEMENTED",
    [ERR_UNKNOWN] = "UNKNOWN"
};

static const char* log_level_names[] = {
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARN] = "WARN",
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_FATAL] = "FATAL"
};

/* ===== Public Functions ===== */

bool error_init(const char* log_file, LogLevel level) {
    if (g_error_state.initialized) {
        return true;
    }

    g_error_state.min_level = level;

    if (log_file) {
        g_error_state.log_file = fopen(log_file, "a");
        if (!g_error_state.log_file) {
            g_error_state.log_file = stderr;
            return false;
        }
    } else {
        g_error_state.log_file = stderr;
    }

    g_error_state.initialized = true;
    return true;
}

void error_shutdown(void) {
    if (g_error_state.log_file && g_error_state.log_file != stderr) {
        fclose(g_error_state.log_file);
    }
    g_error_state.log_file = NULL;
    g_error_state.initialized = false;
}

void error_log_impl(LogLevel level, const char* file, int line,
                   const char* func, const char* fmt, ...) {
    if (!g_error_state.initialized) {
        error_init(NULL, LOG_LEVEL_INFO);
    }

    if (level < g_error_state.min_level) {
        return;
    }

    FILE* out = g_error_state.log_file ? g_error_state.log_file : stderr;

    fprintf(out, "[%s] %s:%d in %s(): ",
            log_level_names[level], file, line, func);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);
}

void error_log_errno_impl(const char* file, int line,
                         const char* func, const char* fmt, ...) {
    if (!g_error_state.initialized) {
        error_init(NULL, LOG_LEVEL_INFO);
    }

    FILE* out = g_error_state.log_file ? g_error_state.log_file : stderr;

    fprintf(out, "[ERROR] %s:%d in %s(): ",
            file, line, func);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, " (errno: %s)\n", strerror(errno));
    fflush(out);
}

const char* error_code_to_string(ErrorCode code) {
    if (code >= 0 && code < (int)(sizeof(error_code_names) / sizeof(error_code_names[0]))) {
        return error_code_names[code];
    }
    return "UNKNOWN";
}

void error_set(ErrorCode code, const char* message) {
    g_error_state.current_error.code = code;
    if (message) {
        strncpy(g_error_state.current_error.message, message,
                sizeof(g_error_state.current_error.message) - 1);
        g_error_state.current_error.message[sizeof(g_error_state.current_error.message) - 1] = '\0';
    } else {
        g_error_state.current_error.message[0] = '\0';
    }
}

const Error* error_get(void) {
    if (g_error_state.current_error.code == ERR_SUCCESS) {
        return NULL;
    }
    return &g_error_state.current_error;
}

void error_clear(void) {
    g_error_state.current_error.code = ERR_SUCCESS;
    g_error_state.current_error.message[0] = '\0';
}
