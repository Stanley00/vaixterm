/**
 * @file color_manager.c
 * @brief Color management and theme functionality implementation.
 *
 * This module implements color operations for the terminal including theme
 * loading, color parsing, and color scheme application.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "color_manager.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>
#include <SDL_ttf.h>

// External function from terminal.c
extern void sgr_to_color(Terminal* term, int color_index, SDL_Color* color);

bool parse_color_string(const char* hex_str, SDL_Color* color)
{
    if (!hex_str || !color) {
        ERROR_LOG("Invalid parameters: hex_str=%p, color=%p", (void*)hex_str, (void*)color);
        return false;
    }

    // Skip leading whitespace
    while (*hex_str && isspace(*hex_str)) hex_str++;

    // Handle different formats
    if (hex_str[0] == '#') {
        // Hex format: #RRGGBB or #RRGGBBAA
        return hex_to_rgb(hex_str, &color->r, &color->g, &color->b, &color->a);
    } else if (strncmp(hex_str, "rgb(", 4) == 0) {
        // RGB format: rgb(r,g,b)
        int r, g, b;
        if (sscanf(hex_str + 4, "%d,%d,%d", &r, &g, &b) == 3) {
            *color = rgb_to_color((uint8_t)r, (uint8_t)g, (uint8_t)b, 255);
            return validate_color(color);
        }
    } else if (strncmp(hex_str, "rgba(", 5) == 0) {
        // RGBA format: rgba(r,g,b,a)
        int r, g, b, a;
        if (sscanf(hex_str + 5, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            *color = rgb_to_color((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            return validate_color(color);
        }
    }

    ERROR_LOG("Invalid color format: %s", hex_str);
    return false;
}

SDL_Color get_foreground_color(Terminal* term, Glyph* glyph)
{
    if (!term || !glyph) {
        ERROR_LOG("Invalid parameters: term=%p, glyph=%p", (void*)term, (void*)glyph);
        SDL_Color default_color = {255, 255, 255, 255};
        return default_color;
    }

    SDL_Color color = glyph->fg;

    // Handle color indices
    if (glyph->attr & ATTR_COLOR_INDEX) {
        int color_index = glyph->fg.r; // Use red component as index
        if (color_index >= 0 && color_index < 256) {
            if (color_index < 16) {
                // ANSI colors
                color = term->palette[color_index];
            } else {
                // 256-color mode
                sgr_to_color(term, color_index, &color);
            }
        }
    }

    // Apply intensity
    if (glyph->attr & ATTR_DIM) {
        color.r = color.r / 2;
        color.g = color.g / 2;
        color.b = color.b / 2;
    }

    return color;
}

SDL_Color get_background_color(Terminal* term, Glyph* glyph)
{
    if (!term || !glyph) {
        ERROR_LOG("Invalid parameters: term=%p, glyph=%p", (void*)term, (void*)glyph);
        SDL_Color default_color = {0, 0, 0, 255};
        return default_color;
    }

    SDL_Color color = glyph->bg;

    // Handle color indices
    if (glyph->attr & ATTR_BG_COLOR_INDEX) {
        int color_index = glyph->bg.r; // Use red component as index
        if (color_index >= 0 && color_index < 256) {
            if (color_index < 16) {
                // ANSI colors
                color = term->palette[color_index];
            } else {
                // 256-color mode
                sgr_to_color(term, color_index, &color);
            }
        }
    }

    // Handle reverse video
    if (glyph->attr & ATTR_REVERSE) {
        SDL_Color temp = get_foreground_color(term, glyph);
        color = temp;
    }

    return color;
}

void apply_sgr_colors(Terminal* term, Glyph* glyph, SDL_Color* fg, SDL_Color* bg)
{
    if (!term || !glyph || !fg || !bg) {
        ERROR_LOG("Invalid parameters: term=%p, glyph=%p, fg=%p, bg=%p", 
                  (void*)term, (void*)glyph, (void*)fg, (void*)bg);
        return;
    }

    *fg = get_foreground_color(term, glyph);
    *bg = get_background_color(term, glyph);

    // Handle bold (bright foreground)
    if (glyph->attr & ATTR_BOLD && !(glyph->attr & ATTR_COLOR_INDEX)) {
        fg->r = (fg->r == 0) ? 128 : 255;
        fg->g = (fg->g == 0) ? 128 : 255;
        fg->b = (fg->b == 0) ? 128 : 255;
    }

    // Handle blink (bright background)
    if (glyph->attr & ATTR_BLINK && !(glyph->attr & ATTR_BG_COLOR_INDEX)) {
        bg->r = (bg->r == 0) ? 128 : 255;
        bg->g = (bg->g == 0) ? 128 : 255;
        bg->b = (bg->b == 0) ? 128 : 255;
    }
}

bool validate_color(const SDL_Color* color)
{
    if (!color) {
        ERROR_LOG("Invalid parameter: color=%p", (void*)color);
        return false;
    }

    // SDL_Color components are uint8_t, always in valid range (0-255)
    return true;
}

SDL_Color rgb_to_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SDL_Color color = {r, g, b, a};
    return color;
}

bool hex_to_rgb(const char* hex_str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
{
    if (!hex_str || !r || !g || !b || !a) {
        ERROR_LOG("Invalid parameters: hex_str=%p, r=%p, g=%p, b=%p, a=%p", 
                  (void*)hex_str, (void*)r, (void*)g, (void*)b, (void*)a);
        return false;
    }

    // Skip leading #
    if (hex_str[0] == '#') hex_str++;

    size_t len = strlen(hex_str);
    if (len != 6 && len != 8) {
        ERROR_LOG("Invalid hex color length: %zu (expected 6 or 8)", len);
        return false;
    }

    unsigned int value;
    if (sscanf(hex_str, "%x", &value) != 1) {
        ERROR_LOG("Failed to parse hex color: %s", hex_str);
        return false;
    }

    if (len == 6) {
        // RGB format
        *r = (value >> 16) & 0xFF;
        *g = (value >> 8) & 0xFF;
        *b = value & 0xFF;
        *a = 255;
    } else {
        // RGBA format
        *r = (value >> 24) & 0xFF;
        *g = (value >> 16) & 0xFF;
        *b = (value >> 8) & 0xFF;
        *a = value & 0xFF;
    }

    return true;
}
