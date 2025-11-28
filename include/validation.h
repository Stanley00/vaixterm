/**
 * @file validation.h
 * @brief Centralized validation layer for all public functions.
 *
 * This module provides a comprehensive validation framework for all public
 * functions across the VaixTerm codebase, ensuring consistent parameter checking,
 * state validation, and error handling.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef VALIDATION_H
#define VALIDATION_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"
#include "error_codes.h"

// Validation result codes
typedef enum {
    VALIDATION_SUCCESS = 0,
    VALIDATION_ERROR_NULL_POINTER,
    VALIDATION_ERROR_INVALID_RANGE,
    VALIDATION_ERROR_INVALID_STATE,
    VALIDATION_ERROR_INVALID_SIZE,
    VALIDATION_ERROR_INVALID_COLOR,
    VALIDATION_ERROR_INVALID_UTF8,
    VALIDATION_ERROR_RESOURCE_UNAVAILABLE,
    VALIDATION_ERROR_PERMISSION_DENIED,
    VALIDATION_ERROR_BUFFER_OVERFLOW
} ValidationResult;

/**
 * @brief Validate a pointer parameter
 *
 * @param ptr Pointer to validate
 * @param param_name Name of the parameter (for error messages)
 * @return ValidationResult Validation result
 */
ValidationResult validate_pointer(const void* ptr, const char* param_name);

/**
 * @brief Validate a string parameter
 *
 * @param str String to validate
 * @param param_name Name of the parameter
 * @param allow_empty Whether empty strings are allowed
 * @return ValidationResult Validation result
 */
ValidationResult validate_string(const char* str, const char* param_name, bool allow_empty);

/**
 * @brief Validate integer range
 *
 * @param value Integer value to validate
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @param param_name Name of the parameter
 * @return ValidationResult Validation result
 */
ValidationResult validate_int_range(int value, int min, int max, const char* param_name);

/**
 * @brief Validate size parameters
 *
 * @param width Width to validate
 * @param height Height to validate
 * @param param_prefix Prefix for parameter names
 * @return ValidationResult Validation result
 */
ValidationResult validate_size(int width, int height, const char* param_prefix);

/**
 * @brief Validate SDL color structure
 *
 * @param color Color to validate
 * @param param_name Name of the parameter
 * @return ValidationResult Validation result
 */
ValidationResult validate_sdl_color(const SDL_Color* color, const char* param_name);

/**
 * @brief Validate UTF-8 string
 *
 * @param str UTF-8 string to validate
 * @param param_name Name of the parameter
 * @param max_length Maximum allowed length (0 = no limit)
 * @return ValidationResult Validation result
 */
ValidationResult validate_utf8_string(const char* str, const char* param_name, size_t max_length);

/**
 * @brief Validate terminal state
 *
 * @param term Terminal structure to validate
 * @param check_initialized Whether to check if terminal is properly initialized
 * @return ValidationResult Validation result
 */
ValidationResult validate_terminal(const Terminal* term, bool check_initialized);

/**
 * @brief Validate OSK state
 *
 * @param osk OSK structure to validate
 * @param check_initialized Whether to check if OSK is properly initialized
 * @return ValidationResult Validation result
 */
ValidationResult validate_osk(const OnScreenKeyboard* osk, bool check_initialized);

/**
 * @brief Validate SDL renderer
 *
 * @param renderer SDL renderer to validate
 * @param param_name Name of the parameter
 * @return ValidationResult Validation result
 */
ValidationResult validate_renderer(SDL_Renderer* renderer, const char* param_name);

/**
 * @brief Validate SDL font
 *
 * @param font SDL font to validate
 * @param param_name Name of the parameter
 * @return ValidationResult Validation result
 */
ValidationResult validate_font(TTF_Font* font, const char* param_name);

/**
 * @brief Validate file descriptor
 *
 * @param fd File descriptor to validate
 * @param param_name Name of the parameter
 * @param check_writable Whether to check if descriptor is writable
 * @return ValidationResult Validation result
 */
ValidationResult validate_file_descriptor(int fd, const char* param_name, bool check_writable);

/**
 * @brief Validate buffer parameters
 *
 * @param buffer Buffer to validate
 * @param size Buffer size
 * @param required_size Minimum required size
 * @param param_name Name of the parameter
 * @return ValidationResult Validation result
 */
ValidationResult validate_buffer(const void* buffer, size_t size, size_t required_size, const char* param_name);

/**
 * @brief Validate glyph cache
 *
 * @param cache Glyph cache to validate
 * @param param_name Name of the parameter
 * @return ValidationResult Validation result
 */
ValidationResult validate_glyph_cache(const GlyphCache* cache, const char* param_name);

/**
 * @brief Validate rendering parameters
 *
 * @param renderer SDL renderer
 * @param font SDL font
 * @param char_w Character width
 * @param char_h Character height
 * @param win_w Window width
 * @param win_h Window height
 * @return ValidationResult Validation result
 */
ValidationResult validate_rendering_params(SDL_Renderer* renderer, TTF_Font* font, 
                                           int char_w, int char_h, int win_w, int win_h);

/**
 * @brief Validate input parameters
 *
 * @param term Terminal structure
 * @param osk OSK structure
 * @param pty_fd PTY file descriptor
 * @return ValidationResult Validation result
 */
ValidationResult validate_input_params(const Terminal* term, const OnScreenKeyboard* osk, int pty_fd);

/**
 * @brief Get error message for validation result
 *
 * @param result Validation result
 * @return const char* Error message
 */
const char* validation_error_message(ValidationResult result);

/**
 * @brief Convert validation result to error code
 *
 * @param result Validation result
 * @return ErrorCode Corresponding error code
 */
ErrorCode validation_to_error_code(ValidationResult result);

/**
 * @brief Validate function preconditions and log errors
 *
 * This is a convenience function that combines validation with error logging.
 *
 * @param result Validation result
 * @param function_name Name of the function being validated
 * @param param_name Name of the parameter that failed validation
 * @return bool True if validation passed
 */
bool validate_and_log(ValidationResult result, const char* function_name, const char* param_name);

#endif // VALIDATION_H
