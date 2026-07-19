/**
 * @file osk_renderer.h
 * @brief On-Screen Keyboard (OSK) rendering functionality.
 *
 * This module handles all OSK rendering operations including:
 * - Main OSK rendering coordination
 * - Character key rendering
 * - Special key rendering
 * - Modifier indicator rendering
 * - Key tape (key sequence display) rendering
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef OSK_RENDERER_H
#define OSK_RENDERER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"

/**
 * @brief Render the complete On-Screen Keyboard
 *
 * This is the main OSK rendering function that coordinates all rendering
 * components including characters, special keys, and modifier indicators.
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param osk Pointer to the OSK structure
 * @param term Pointer to the terminal structure
 * @param win_w Window width in pixels
 * @param win_h Window height in pixels
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 */
void render_osk(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, const Terminal* term, 
                int win_w, int win_h, int char_w, int char_h, const Config* config);

/**
 * @brief Render the key tape (display of pressed keys)
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param osk Pointer to the OSK structure
 * @param win_w Window width in pixels
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 */
void render_key_tape(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                     int win_w, int char_w, int char_h);

/**
 * @brief Render character keys
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param osk Pointer to the OSK structure
 * @param available_width Available width for rendering
 * @param osk_y Y position to start rendering
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 */
void render_osk_chars(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                      int available_width, int osk_y, int char_w, int char_h);

/**
 * @brief Render special keys
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param osk Pointer to the OSK structure
 * @param term Pointer to the terminal structure
 * @param available_width Available width for rendering
 * @param osk_y Y position to start rendering
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 */
void render_osk_special(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                        const Terminal* term, int available_width, int osk_y, 
                        int char_w, int char_h);

/**
 * @brief Render modifier indicators
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param osk Pointer to the OSK structure
 * @param win_w Window width in pixels
 * @param osk_y Y position to start rendering
 * @param char_w Character width in pixels
 * @param char_h Character height in pixels
 */
void render_modifier_indicators(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                                int win_w, int osk_y, int char_h, int char_w);

/**
 * @brief Get the width needed for modifier indicators
 *
 * @param font Font to use for calculation
 * @param char_w Character width in pixels
 * @return int Width in pixels
 */
int get_modifier_indicators_width(TTF_Font* font, int char_w);

/**
 * @brief Invalidate the OSK render cache
 *
 * @param osk Pointer to the OSK structure
 */
void osk_invalidate_render_cache(OnScreenKeyboard* osk);

/**
 * @brief Create OSK key cache
 *
 * @return OSKKeyCache* Pointer to created cache or NULL on failure
 */
OSKKeyCache* osk_key_cache_create(void);

/**
 * @brief Destroy OSK key cache
 *
 * @param cache Pointer to cache to destroy
 */
void osk_key_cache_destroy(OSKKeyCache* cache);

#endif // OSK_RENDERER_H
