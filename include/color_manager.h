/**
 * @file color_manager.h
 * @brief Color management and theme functionality.
 */

#ifndef COLOR_MANAGER_H
#define COLOR_MANAGER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"

bool parse_color_string(const char* hex_str, SDL_Color* color);
SDL_Color get_foreground_color(Terminal* term, Glyph* glyph);
SDL_Color get_background_color(Terminal* term, Glyph* glyph);
void apply_sgr_colors(Terminal* term, Glyph* glyph, SDL_Color* fg, SDL_Color* bg);
bool validate_color(const SDL_Color* color);
SDL_Color rgb_to_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
bool hex_to_rgb(const char* hex_str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);

#endif // COLOR_MANAGER_H
