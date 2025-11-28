/**
 * @file validation.c
 * @brief Centralized validation layer implementation.
 *
 * This module implements comprehensive validation functions for all public
 * functions across the VaixTerm codebase, ensuring consistent parameter checking,
 * state validation, and error handling.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "validation.h"
#include "error_codes.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Forward declaration for utf8_char_len function
extern int utf8_char_len(const char* s);

ValidationResult validate_pointer(const void* ptr, const char* param_name)
{
    if (!param_name) {
        ERROR_LOG("Parameter name is NULL");
        return VALIDATION_ERROR_NULL_POINTER;
    }

    if (!ptr) {
        ERROR_LOG("Invalid parameter: %s is NULL", param_name);
        return VALIDATION_ERROR_NULL_POINTER;
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_string(const char* str, const char* param_name, bool allow_empty)
{
    if (!param_name) {
        ERROR_LOG("Parameter name is NULL");
        return VALIDATION_ERROR_NULL_POINTER;
    }

    if (!str) {
        ERROR_LOG("Invalid parameter: %s is NULL", param_name);
        return VALIDATION_ERROR_NULL_POINTER;
    }

    if (!allow_empty && str[0] == '\0') {
        ERROR_LOG("Invalid parameter: %s is empty", param_name);
        return VALIDATION_ERROR_INVALID_RANGE;
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_int_range(int value, int min, int max, const char* param_name)
{
    if (!param_name) {
        ERROR_LOG("Parameter name is NULL");
        return VALIDATION_ERROR_NULL_POINTER;
    }

    if (value < min || value > max) {
        ERROR_LOG("Invalid parameter: %s=%d (range: %d-%d)", param_name, value, min, max);
        return VALIDATION_ERROR_INVALID_RANGE;
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_size(int width, int height, const char* param_prefix)
{
    if (!param_prefix) {
        ERROR_LOG("Parameter prefix is NULL");
        return VALIDATION_ERROR_NULL_POINTER;
    }

    if (width <= 0) {
        ERROR_LOG("Invalid parameter: %s_width=%d (must be > 0)", param_prefix, width);
        return VALIDATION_ERROR_INVALID_SIZE;
    }

    if (height <= 0) {
        ERROR_LOG("Invalid parameter: %s_height=%d (must be > 0)", param_prefix, height);
        return VALIDATION_ERROR_INVALID_SIZE;
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_sdl_color(const SDL_Color* color, const char* param_name)
{
    ValidationResult result = validate_pointer(color, param_name);
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    // SDL_Color components should be in range 0-255
    if (color->r > 255 || color->g > 255 || color->b > 255 || color->a > 255) {
        ERROR_LOG("Invalid parameter: %s has color components > 255 (r=%d,g=%d,b=%d,a=%d)", 
                  param_name, color->r, color->g, color->b, color->a);
        return VALIDATION_ERROR_INVALID_COLOR;
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_utf8_string(const char* str, const char* param_name, size_t max_length)
{
    ValidationResult result = validate_string(str, param_name, false);
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    // Check UTF-8 validity
    const char* p = str;
    size_t byte_count = 0;
    
    while (*p) {
        int char_len = utf8_char_len(p);
        if (char_len == 0) {
            ERROR_LOG("Invalid UTF-8 string in parameter: %s", param_name);
            return VALIDATION_ERROR_INVALID_UTF8;
        }

        // Check continuation bytes
        for (int i = 1; i < char_len; i++) {
            if ((p[i] & 0xC0) != 0x80) {
                ERROR_LOG("Invalid UTF-8 continuation byte in parameter: %s", param_name);
                return VALIDATION_ERROR_INVALID_UTF8;
            }
        }

        p += char_len;
        byte_count += char_len;

        if (max_length > 0 && byte_count > max_length) {
            ERROR_LOG("UTF-8 string too long in parameter: %s (max: %zu, actual: %zu)", 
                      param_name, max_length, byte_count);
            return VALIDATION_ERROR_BUFFER_OVERFLOW;
        }
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_terminal(const Terminal* term, bool check_initialized)
{
    ValidationResult result = validate_pointer(term, "term");
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    if (check_initialized) {
        // Check if terminal appears to be properly initialized
        if (term->cols <= 0 || term->rows <= 0) {
            ERROR_LOG("Terminal not properly initialized: cols=%d, rows=%d", term->cols, term->rows);
            return VALIDATION_ERROR_INVALID_STATE;
        }

        if (!term->grid) {
            ERROR_LOG("Terminal grid not initialized");
            return VALIDATION_ERROR_INVALID_STATE;
        }

        if (!term->screen_texture) {
            ERROR_LOG("Terminal screen texture not initialized");
            return VALIDATION_ERROR_RESOURCE_UNAVAILABLE;
        }
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_osk(const OnScreenKeyboard* osk, bool check_initialized)
{
    ValidationResult result = validate_pointer(osk, "osk");
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    if (check_initialized) {
        // Check if OSK appears to be properly initialized
        if (osk->num_char_rows < 0) {
            ERROR_LOG("OSK not properly initialized: num_char_rows=%d", osk->num_char_rows);
            return VALIDATION_ERROR_INVALID_STATE;
        }

        if (osk->current_char_row < 0) {
            ERROR_LOG("OSK not properly initialized: current_char_row=%d", osk->current_char_row);
            return VALIDATION_ERROR_INVALID_STATE;
        }
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_renderer(SDL_Renderer* renderer, const char* param_name)
{
    ValidationResult result = validate_pointer(renderer, param_name);
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    // Additional renderer-specific validation could be added here
    // For now, just check if the pointer is valid

    return VALIDATION_SUCCESS;
}

ValidationResult validate_font(TTF_Font* font, const char* param_name)
{
    ValidationResult result = validate_pointer(font, param_name);
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    // Additional font-specific validation could be added here
    // For now, just check if the pointer is valid

    return VALIDATION_SUCCESS;
}

ValidationResult validate_file_descriptor(int fd, const char* param_name, bool check_writable)
{
    if (fd < 0) {
        ERROR_LOG("Invalid parameter: %s=%d (must be >= 0)", param_name, fd);
        return VALIDATION_ERROR_INVALID_RANGE;
    }

    // Check if file descriptor is valid
    if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
        ERROR_LOG("Invalid file descriptor: %s=%d (errno: %d)", param_name, fd, errno);
        return VALIDATION_ERROR_RESOURCE_UNAVAILABLE;
    }

    if (check_writable) {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1) {
            ERROR_LOG("Failed to get file descriptor flags: %s=%d", param_name, fd);
            return VALIDATION_ERROR_RESOURCE_UNAVAILABLE;
        }

        if ((flags & O_ACCMODE) == O_RDONLY) {
            ERROR_LOG("File descriptor not writable: %s=%d", param_name, fd);
            return VALIDATION_ERROR_PERMISSION_DENIED;
        }
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_buffer(const void* buffer, size_t size, size_t required_size, const char* param_name)
{
    ValidationResult result = validate_pointer(buffer, param_name);
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    if (size < required_size) {
        ERROR_LOG("Buffer too small: %s (size: %zu, required: %zu)", param_name, size, required_size);
        return VALIDATION_ERROR_BUFFER_OVERFLOW;
    }

    return VALIDATION_SUCCESS;
}

ValidationResult validate_glyph_cache(const GlyphCache* cache, const char* param_name)
{
    ValidationResult result = validate_pointer(cache, param_name);
    if (result != VALIDATION_SUCCESS) {
        return result;
    }

    // entries and last_access are arrays, not pointers, so they can't be NULL
    // Just validate the cache pointer itself

    return VALIDATION_SUCCESS;
}

ValidationResult validate_rendering_params(SDL_Renderer* renderer, TTF_Font* font, 
                                           int char_w, int char_h, int win_w, int win_h)
{
    ValidationResult result;

    result = validate_renderer(renderer, "renderer");
    if (result != VALIDATION_SUCCESS) return result;

    result = validate_font(font, "font");
    if (result != VALIDATION_SUCCESS) return result;

    result = validate_size(char_w, char_h, "char");
    if (result != VALIDATION_SUCCESS) return result;

    result = validate_size(win_w, win_h, "window");
    if (result != VALIDATION_SUCCESS) return result;

    return VALIDATION_SUCCESS;
}

ValidationResult validate_input_params(const Terminal* term, const OnScreenKeyboard* osk, int pty_fd)
{
    ValidationResult result;

    result = validate_terminal(term, true);
    if (result != VALIDATION_SUCCESS) return result;

    result = validate_osk(osk, true);
    if (result != VALIDATION_SUCCESS) return result;

    result = validate_file_descriptor(pty_fd, "pty_fd", true);
    if (result != VALIDATION_SUCCESS) return result;

    return VALIDATION_SUCCESS;
}

const char* validation_error_message(ValidationResult result)
{
    switch (result) {
        case VALIDATION_SUCCESS:
            return "Validation successful";
        case VALIDATION_ERROR_NULL_POINTER:
            return "Null pointer parameter";
        case VALIDATION_ERROR_INVALID_RANGE:
            return "Parameter out of valid range";
        case VALIDATION_ERROR_INVALID_STATE:
            return "Invalid object state";
        case VALIDATION_ERROR_INVALID_SIZE:
            return "Invalid size parameter";
        case VALIDATION_ERROR_INVALID_COLOR:
            return "Invalid color parameter";
        case VALIDATION_ERROR_INVALID_UTF8:
            return "Invalid UTF-8 string";
        case VALIDATION_ERROR_RESOURCE_UNAVAILABLE:
            return "Resource unavailable";
        case VALIDATION_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case VALIDATION_ERROR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        default:
            return "Unknown validation error";
    }
}

ErrorCode validation_to_error_code(ValidationResult result)
{
    switch (result) {
        case VALIDATION_SUCCESS:
            return ERR_SUCCESS;
        case VALIDATION_ERROR_NULL_POINTER:
            return ERR_INVALID_ARGUMENT;
        case VALIDATION_ERROR_INVALID_RANGE:
            return ERR_INVALID_ARGUMENT;
        case VALIDATION_ERROR_INVALID_STATE:
            return ERR_INVALID_STATE;
        case VALIDATION_ERROR_INVALID_SIZE:
            return ERR_INVALID_ARGUMENT;
        case VALIDATION_ERROR_INVALID_COLOR:
            return ERR_INVALID_ARGUMENT;
        case VALIDATION_ERROR_INVALID_UTF8:
            return ERR_INVALID_ARGUMENT;
        case VALIDATION_ERROR_RESOURCE_UNAVAILABLE:
            return ERR_NOT_IMPLEMENTED;
        case VALIDATION_ERROR_PERMISSION_DENIED:
            return ERR_FILE_IO;
        case VALIDATION_ERROR_BUFFER_OVERFLOW:
            return ERR_BUFFER_OVERFLOW;
        default:
            return ERR_UNKNOWN;
    }
}

bool validate_and_log(ValidationResult result, const char* function_name, const char* param_name)
{
    if (result == VALIDATION_SUCCESS) {
        return true;
    }

    const char* error_msg = validation_error_message(result);
    if (param_name) {
        ERROR_LOG("Validation failed in %s: %s (%s)", function_name, error_msg, param_name);
    } else {
        ERROR_LOG("Validation failed in %s: %s", function_name, error_msg);
    }

    return false;
}

// External function declaration for utf8_char_len
extern int utf8_char_len(const char* s);
