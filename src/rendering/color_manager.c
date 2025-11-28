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

bool load_colorscheme(Terminal* term, const char* path)
{
    if (!term || !path) {
        ERROR_LOG("Invalid parameters: term=%p, path=%p", (void*)term, (void*)path);
        return false;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        ERROR_LOG("Failed to open colorscheme file: %s", path);
        return false;
    }

    char line[256];
    bool success = true;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline and skip empty lines/comments
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip whitespace and comments
        char* p = line;
        while (*p && isspace(*p)) p++;
        if (*p == '\0' || *p == '#') continue;

        // Parse key=value format
        char* equals = strchr(p, '=');
        if (!equals) continue;

        *equals = '\0';
        char* key = p;
        char* value = equals + 1;

        // Trim whitespace
        while (*key && isspace(*key)) key++;
        while (*value && isspace(*value)) value++;

        SDL_Color color;
        if (parse_color_string(value, &color)) {
            if (strcmp(key, "foreground") == 0) {
                term->default_fg = color;
            } else if (strcmp(key, "background") == 0) {
                term->default_bg = color;
            } else if (strcmp(key, "cursor") == 0) {
                term->cursor_color = color;
            } else if (strncmp(key, "color", 5) == 0) {
                // Parse color0, color1, etc.
                int index = atoi(key + 5);
                if (index >= 0 && index < 16) {
                    term->palette[index] = color;
                }
            }
        } else {
            WARN_LOG("Failed to parse color: %s=%s", key, value);
        }
    }

    fclose(file);
    INFO_LOG("Loaded colorscheme: %s", path);
    return success;
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

bool load_background_image(Terminal* term, SDL_Renderer* renderer, const char* path)
{
    if (!term || !renderer || !path) {
        ERROR_LOG("Invalid parameters: term=%p, renderer=%p, path=%p", (void*)term, (void*)renderer, (void*)path);
        return false;
    }

    // Free existing background
    free_background_image(term);

    // Load image
    SDL_Surface* surface = SDL_LoadBMP(path);
    if (!surface) {
        ERROR_LOG("Failed to load background image: %s", SDL_GetError());
        return false;
    }

    // Create texture
    term->background_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!term->background_texture) {
        ERROR_LOG("Failed to create background texture: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    }

    SDL_FreeSurface(surface);

    // Store path for reloading
    term->background_path = strdup(path);
    if (!term->background_path) {
        ERROR_LOG("Failed to allocate background path");
        free_background_image(term);
        return false;
    }

    INFO_LOG("Loaded background image: %s", path);
    return true;
}

void reload_background_texture(Terminal* term, SDL_Renderer* renderer, const char* path)
{
    load_background_image(term, renderer, path);
}

void free_background_image(Terminal* term)
{
    if (!term) {
        ERROR_LOG("Invalid parameter: term=%p", (void*)term);
        return;
    }

    if (term->background_texture) {
        SDL_DestroyTexture(term->background_texture);
        term->background_texture = NULL;
    }

    if (term->background_path) {
        free(term->background_path);
        term->background_path = NULL;
    }

    DEBUG_LOG("Background image freed");
}

bool validate_color(const SDL_Color* color)
{
    if (!color) {
        ERROR_LOG("Invalid parameter: color=%p", (void*)color);
        return false;
    }

    // All color components should be in valid range (0-255)
    return color->r <= 255 && color->g <= 255 && color->b <= 255 && color->a <= 255;
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

SDL_Color blend_colors(SDL_Color color1, SDL_Color color2, float factor)
{
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    SDL_Color result;
    result.r = (uint8_t)(color1.r * (1.0f - factor) + color2.r * factor);
    result.g = (uint8_t)(color1.g * (1.0f - factor) + color2.g * factor);
    result.b = (uint8_t)(color1.b * (1.0f - factor) + color2.b * factor);
    result.a = (uint8_t)(color1.a * (1.0f - factor) + color2.a * factor);

    return result;
}

void get_default_colors(SDL_Color* fg, SDL_Color* bg)
{
    if (!fg || !bg) {
        ERROR_LOG("Invalid parameters: fg=%p, bg=%p", (void*)fg, (void*)bg);
        return;
    }

    *fg = rgb_to_color(255, 255, 255, 255); // White
    *bg = rgb_to_color(0, 0, 0, 255);       // Black
}
