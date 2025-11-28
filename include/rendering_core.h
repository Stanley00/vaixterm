/**
 * @file rendering_core.h
 * @brief Core rendering functionality for the terminal.
 *
 * This module handles the main terminal rendering operations including:
 * - Terminal rendering coordination
 * - Text rendering utilities
 * - Glyph rendering at specific positions
 * - Credit screen rendering
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef RENDERING_CORE_H
#define RENDERING_CORE_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"

/**
 * @brief Render the terminal to the screen
 *
 * This is the main terminal rendering function that coordinates all rendering
 * operations including text, backgrounds, and OSK integration.
 *
 * @param renderer SDL renderer
 * @param term Pointer to the terminal structure
 * @param font Font to use for rendering
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 * @param osk Pointer to the OSK structure
 * @param force_full_render Force full screen redraw
 * @param win_w Window width in pixels
 * @param win_h Window height in pixels
 */
void terminal_render(SDL_Renderer* renderer, Terminal* term, TTF_Font* font, 
                    int char_w, int char_h, OnScreenKeyboard* osk, 
                    bool force_full_render, int win_w, int win_h);

/**
 * @brief Render text at a specific position
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param text Text to render
 * @param x X position in pixels
 * @param y Y position in pixels
 * @param color Text color
 * @param centered Whether to center the text
 * @param win_w Window width for centering
 */
void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, 
                 int x, int y, SDL_Color color, bool centered, int win_w);

/**
 * @brief Render a single glyph at a specific position
 *
 * @param renderer SDL renderer
 * @param term Pointer to the terminal structure
 * @param font Font to use for rendering
 * @param c Character to render
 * @param x X position in pixels
 * @param y Y position in pixels
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 * @param fg Foreground color
 * @param bg Background color
 * @param attributes Character attributes
 */
void render_glyph_at(SDL_Renderer* renderer, Terminal* term, TTF_Font* font,
                     uint32_t c, int x, int y, int char_w, int char_h,
                     SDL_Color fg, SDL_Color bg, unsigned char attributes);

/**
 * @brief Render the credit screen
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param win_w Window width in pixels
 * @param win_h Window height in pixels
 */
void render_credit_screen(SDL_Renderer* renderer, TTF_Font* font, int win_w, int win_h);

#endif // RENDERING_CORE_H
