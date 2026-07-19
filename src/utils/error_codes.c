#include "error_codes.h"
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

static struct {
    FILE* log_file;
    LogLevel min_level;
} g_error_state = {
    .log_file = NULL,
    .min_level = LOG_LEVEL_WARN,
};

static const char* log_level_names[] = {
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARN] = "WARN",
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_FATAL] = "FATAL"
};

__attribute__((constructor))
static void log_init_env(void) {
    const char* env = getenv("VAIXTERM_LOG_LEVEL");
    if (!env) return;

    if (strcasecmp(env, "debug") == 0) g_error_state.min_level = LOG_LEVEL_DEBUG;
    else if (strcasecmp(env, "info") == 0) g_error_state.min_level = LOG_LEVEL_INFO;
    else if (strcasecmp(env, "warn") == 0) g_error_state.min_level = LOG_LEVEL_WARN;
    else if (strcasecmp(env, "error") == 0) g_error_state.min_level = LOG_LEVEL_ERROR;
    else if (strcasecmp(env, "fatal") == 0) g_error_state.min_level = LOG_LEVEL_FATAL;
}

void log_set_level(LogLevel level) {
    g_error_state.min_level = level;
}

LogLevel log_get_level(void) {
    return g_error_state.min_level;
}

void error_log_impl(LogLevel level, const char* file, int line,
                   const char* func, const char* fmt, ...) {
    if (level < g_error_state.min_level) return;

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
