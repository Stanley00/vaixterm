/**
 * @file color_manager.h
 * @brief Color management and theme functionality.
 *
 * This module handles color operations for the terminal including:
 * - Theme loading and management
 * - Color parsing and validation
 * - Color scheme application
 * - Background image handling
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef COLOR_MANAGER_H
#define COLOR_MANAGER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"

/**
 * @brief Parse a color string in various formats
 *
 * Supported formats:
 * - "#RRGGBB" (hex)
 * - "#RRGGBBAA" (hex with alpha)
 * - "rgb(r,g,b)" (decimal)
 * - "rgba(r,g,b,a)" (decimal with alpha)
 *
 * @param hex_str Color string to parse
 * @param color Output color structure
 * @return bool True if parsing succeeded
 */
bool parse_color_string(const char* hex_str, SDL_Color* color);

/**
 * @brief Load a color scheme from a theme file
 *
 * @param term Pointer to the terminal structure
 * @param path Path to the theme file
 * @return bool True if loading succeeded
 */
bool load_colorscheme(Terminal* term, const char* path);

/**
 * @brief Get the foreground color for a character
 *
 * @param term Pointer to the terminal structure
 * @param glyph Pointer to the glyph
 * @return SDL_Color Foreground color
 */
SDL_Color get_foreground_color(Terminal* term, Glyph* glyph);

/**
 * @brief Get the background color for a character
 *
 * @param term Pointer to the terminal structure
 * @param glyph Pointer to the glyph
 * @return SDL_Color Background color
 */
SDL_Color get_background_color(Terminal* term, Glyph* glyph);

/**
 * @brief Apply SGR (Select Graphic Rendition) attributes to colors
 *
 * @param term Pointer to the terminal structure
 * @param glyph Pointer to the glyph
 * @param fg Output foreground color
 * @param bg Output background color
 */
void apply_sgr_colors(Terminal* term, Glyph* glyph, SDL_Color* fg, SDL_Color* bg);

/**
 * @brief Load a background image
 *
 * @param term Pointer to the terminal structure
 * @param renderer SDL renderer
 * @param path Path to the image file
 * @return bool True if loading succeeded
 */
bool load_background_image(Terminal* term, SDL_Renderer* renderer, const char* path);

/**
 * @brief Reload the background texture
 *
 * @param term Pointer to the terminal structure
 * @param renderer SDL renderer
 * @param path Path to the image file
 */
void reload_background_texture(Terminal* term, SDL_Renderer* renderer, const char* path);

/**
 * @brief Free background image resources
 *
 * @param term Pointer to the terminal structure
 */
void free_background_image(Terminal* term);

/**
 * @brief Validate a color structure
 *
 * @param color Color to validate
 * @return bool True if color is valid
 */
bool validate_color(const SDL_Color* color);

/**
 * @brief Convert RGB values to SDL_Color
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255, default 255)
 * @return SDL_Color Color structure
 */
SDL_Color rgb_to_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/**
 * @brief Convert hex string to RGB values
 *
 * @param hex_str Hex color string (e.g., "#FF0000")
 * @param r Output red component
 * @param g Output green component
 * @param b Output blue component
 * @param a Output alpha component
 * @return bool True if conversion succeeded
 */
bool hex_to_rgb(const char* hex_str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

/**
 * @brief Blend two colors
 *
 * @param color1 First color
 * @param color2 Second color
 * @param factor Blend factor (0.0 to 1.0)
 * @return SDL_Color Blended color
 */
SDL_Color blend_colors(SDL_Color color1, SDL_Color color2, float factor);

/**
 * @brief Get the default terminal colors
 *
 * @param fg Output default foreground color
 * @param bg Output default background color
 */
void get_default_colors(SDL_Color* fg, SDL_Color* bg);

#endif // COLOR_MANAGER_H
