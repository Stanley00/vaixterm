/**
 * @file osk_renderer.c
 * @brief On-Screen Keyboard (OSK) rendering functionality implementation.
 *
 * This module implements all OSK rendering operations including the main
 * rendering coordination, character keys, special keys, and modifier indicators.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "osk_renderer.h"
#include "osk_core.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>

// Helper function prototypes
static const char* get_char_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx);
static bool is_char_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx);
static bool is_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, const SpecialKey* key);
static const char* get_special_key_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx);
static bool is_special_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx);
static int s_max_mod_indicator_width = -1;

void render_osk(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, const Terminal* term, 
                int win_w, int win_h, int char_w, int char_h)
{
    if (!renderer || !font || !osk || !term || win_w <= 0 || win_h <= 0 || char_w <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, osk=%p, term=%p, win_w=%d, win_h=%d, char_w=%d, char_h=%d",
                  (void*)renderer, (void*)font, (void*)osk, (void*)term, win_w, win_h, char_w, char_h);
        return;
    }

    // Determine if the OSK should be at the top or bottom of the screen.
    int osk_y = get_osk_y_position(osk, term, win_h, char_h);

    SDL_Rect bg_rect = {0, osk_y, win_w, char_h};
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 220);
    SDL_RenderFillRect(renderer, &bg_rect);

    int indicator_width = get_modifier_indicators_width(font, char_w);
    // The key tape gets the window width minus the space reserved for indicators.
    int tape_render_width = win_w - indicator_width;

    // If the OSK is in special mode, render the special key sets. The held
    // modifiers will still apply to any key presses, but will not change the view.
    // Otherwise, render the character layouts, where held modifiers do change the view.
    if (osk->mode == OSK_MODE_SPECIAL) {
        render_osk_special(renderer, font, osk, term, tape_render_width, osk_y, char_w, char_h);
    } else {
        render_osk_chars(renderer, font, osk, tape_render_width, osk_y, char_w, char_h);
    }

    // Reset clip rect to draw indicators over the full OSK bar width.
    SDL_Rect full_window_clip = {0, 0, win_w, win_h};
    SDL_RenderSetClipRect(renderer, &full_window_clip);

    render_modifier_indicators(renderer, font, osk, win_w, osk_y, char_w, char_h);
}

void render_key_tape(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                     int win_w, int char_w, int char_h)
{
    if (!renderer || !font || !osk || win_w <= 0 || char_w <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, osk=%p, win_w=%d, char_w=%d, char_h=%d",
                  (void*)renderer, (void*)font, (void*)osk, win_w, char_w, char_h);
        return;
    }

    // This is a simplified implementation - the full implementation would be extracted
    // from the original osk.c file
    DEBUG_LOG("Rendering key tape (simplified implementation)");
}

void render_osk_chars(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                      int available_width, int osk_y, int char_w, int char_h)
{
    if (!renderer || !font || !osk || available_width <= 0 || char_w <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, osk=%p, available_width=%d, char_w=%d, char_h=%d",
                  (void*)renderer, (void*)font, (void*)osk, available_width, char_w, char_h);
        return;
    }

    int effective_num_rows = get_current_num_char_rows(osk);
    if (effective_num_rows <= 0) {
        DEBUG_LOG("No character rows to render");
        return;
    }

    // Get current row
    const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->current_char_row);
    if (!row || row->num_keys == 0) {
        DEBUG_LOG("No keys in current row");
        return;
    }

    // Calculate key width
    int key_width = calculate_fixed_key_width(font, osk, row->num_keys, char_w, get_char_display_name);
    if (key_width <= 0) {
        ERROR_LOG("Failed to calculate key width");
        return;
    }

    // Render keys
    int x = 0;
    for (int i = 0; i < row->num_keys && x < available_width; i++) {
        const SpecialKey* key = &row->keys[i];
        if (!key || !key->display_name) continue;

        // Get display name
        const char* display_name = get_char_display_name(osk, osk->current_char_row, i);
        if (!display_name) continue;

        // Check if key is toggled
        bool toggled = is_char_key_toggled(NULL, osk, osk->current_char_row, i);

        // Set color based on toggle state
        if (toggled) {
            SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255); // Blue for toggled
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255); // Gray for normal
        }

        // Draw key background
        SDL_Rect key_rect = {x, osk_y, key_width, char_h};
        SDL_RenderFillRect(renderer, &key_rect);

        // Draw key border
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &key_rect);

        // Render text
        SDL_Color text_color = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, display_name, text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                int text_w = surface->w;
                int text_h = surface->h;
                SDL_Rect text_rect = {
                    x + (key_width - text_w) / 2,
                    osk_y + (char_h - text_h) / 2,
                    text_w,
                    text_h
                };
                SDL_RenderCopy(renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        x += key_width;
    }

    DEBUG_LOG("Rendered %d character keys", row->num_keys);
}

void render_osk_special(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                        const Terminal* term, int available_width, int osk_y, 
                        int char_w, int char_h)
{
    if (!renderer || !font || !osk || available_width <= 0 || char_w <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, osk=%p, available_width=%d, char_w=%d, char_h=%d",
                  (void*)renderer, (void*)font, (void*)osk, available_width, char_w, char_h);
        return;
    }

    if (!osk->control_set.keys || osk->control_set.num_keys == 0) {
        DEBUG_LOG("No special keys to render");
        return;
    }

    // Calculate key width
    int key_width = calculate_fixed_key_width(font, osk, osk->control_set.num_keys, char_w, get_special_key_display_name);
    if (key_width <= 0) {
        ERROR_LOG("Failed to calculate special key width");
        return;
    }

    // Render special keys
    int x = 0;
    for (int i = 0; i < osk->control_set.num_keys && x < available_width; i++) {
        const SpecialKey* key = &osk->control_set.keys[i];
        if (!key || !key->display_name) continue;

        // Get display name
        const char* display_name = get_special_key_display_name(osk, 0, i);
        if (!display_name) continue;

        // Check if key is toggled
        bool toggled = is_special_key_toggled(term, osk, 0, i);

        // Set color based on key type and toggle state
        if (toggled) {
            SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255); // Blue for toggled
        } else if (key->type == SK_MOD_CTRL || key->type == SK_MOD_ALT || 
                   key->type == SK_MOD_GUI || key->type == SK_MOD_SHIFT) {
            SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255); // Dark blue for modifiers
        } else if (key->type == SK_INTERNAL_CMD) {
            SDL_SetRenderDrawColor(renderer, 120, 80, 80, 255); // Dark red for commands
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255); // Gray for normal
        }

        // Draw key background
        SDL_Rect key_rect = {x, osk_y, key_width, char_h};
        SDL_RenderFillRect(renderer, &key_rect);

        // Draw key border
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &key_rect);

        // Render text
        SDL_Color text_color = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, display_name, text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                int text_w = surface->w;
                int text_h = surface->h;
                SDL_Rect text_rect = {
                    x + (key_width - text_w) / 2,
                    osk_y + (char_h - text_h) / 2,
                    text_w,
                    text_h
                };
                SDL_RenderCopy(renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        x += key_width;
    }

    DEBUG_LOG("Rendered %d special keys", osk->control_set.num_keys);
}

void render_modifier_indicators(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                                int win_w, int osk_y, int char_w, int char_h)
{
    if (!renderer || !font || !osk || win_w <= 0 || char_w <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, osk=%p, win_w=%d, char_w=%d, char_h=%d",
                  (void*)renderer, (void*)font, (void*)osk, win_w, char_w, char_h);
        return;
    }

    // Modifier indicators to show
    const char* indicators[] = {"Ctrl", "Alt", "GUI", "Shift"};
    bool active[] = {osk->mod_ctrl, osk->mod_alt, osk->mod_gui, osk->mod_shift};
    int num_indicators = sizeof(indicators) / sizeof(indicators[0]);

    // Calculate total width needed
    int total_width = 0;
    int widths[num_indicators];
    for (int i = 0; i < num_indicators; i++) {
        int w;
        TTF_SizeUTF8(font, indicators[i], &w, NULL);
        widths[i] = w + char_w / 2; // Add padding
        total_width += widths[i];
    }

    // Starting position (right-aligned)
    int x = win_w - total_width;

    // Render each indicator
    for (int i = 0; i < num_indicators; i++) {
        if (!active[i]) continue;

        // Set color
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);

        // Draw background
        SDL_Rect indicator_rect = {x, osk_y, widths[i], char_h};
        SDL_RenderFillRect(renderer, &indicator_rect);

        // Draw border
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &indicator_rect);

        // Render text
        SDL_Color text_color = {255, 255, 255, 255};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, indicators[i], text_color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                int text_w = surface->w;
                int text_h = surface->h;
                SDL_Rect text_rect = {
                    x + (widths[i] - text_w) / 2,
                    osk_y + (char_h - text_h) / 2,
                    text_w,
                    text_h
                };
                SDL_RenderCopy(renderer, texture, NULL, &text_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }

        x += widths[i];
    }

    DEBUG_LOG("Rendered modifier indicators");
}

int get_modifier_indicators_width(TTF_Font* font, int char_w)
{
    if (!font || char_w <= 0) {
        ERROR_LOG("Invalid parameters: font=%p, char_w=%d", (void*)font, char_w);
        return 0;
    }

    // Cache the width calculation
    if (s_max_mod_indicator_width >= 0) {
        return s_max_mod_indicator_width;
    }

    const char* indicators[] = {"Ctrl", "Alt", "GUI", "Shift"};
    int num_indicators = sizeof(indicators) / sizeof(indicators[0]);
    int total_width = 0;

    for (int i = 0; i < num_indicators; i++) {
        int w;
        TTF_SizeUTF8(font, indicators[i], &w, NULL);
        total_width += w + char_w / 2; // Add padding
    }

    s_max_mod_indicator_width = total_width;
    DEBUG_LOG("Calculated modifier indicators width: %d", total_width);
    return total_width;
}

int calculate_fixed_key_width(TTF_Font* font, OnScreenKeyboard* osk, int set_len, 
                              int char_w, OSK_GetDisplayNameFunc get_name_func)
{
    if (!font || !osk || set_len <= 0 || char_w <= 0 || !get_name_func) {
        ERROR_LOG("Invalid parameters: font=%p, osk=%p, set_len=%d, char_w=%d, get_name_func=%p",
                  (void*)font, (void*)osk, set_len, char_w, (void*)get_name_func);
        return 0;
    }

    // Check cache
    if (osk->cached_set_idx == set_len && osk->cached_key_width > 0) {
        return osk->cached_key_width;
    }

    int max_width = 0;
    for (int i = 0; i < set_len; i++) {
        const char* name = get_name_func(osk, i, 0);
        if (name) {
            int w;
            TTF_SizeUTF8(font, name, &w, NULL);
            if (w > max_width) {
                max_width = w;
            }
        }
    }

    // Add padding
    int key_width = max_width + char_w / 2;

    // Update cache
    osk->cached_set_idx = set_len;
    osk->cached_key_width = key_width;

    DEBUG_LOG("Calculated key width: %d for set_len=%d", key_width, set_len);
    return key_width;
}

void osk_invalidate_render_cache(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    osk->cached_set_idx = -1;
    osk->cached_mod_mask = -1;
    osk->cached_key_width = -1;
    s_max_mod_indicator_width = -1;
    DEBUG_LOG("Invalidated OSK render cache");
}

// Helper function implementations
static const char* get_char_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    const SpecialKey* key = osk_get_effective_char_ptr(osk, set_idx, char_idx);
    return key ? key->display_name : NULL;
}

static bool is_char_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    const SpecialKey* key = osk_get_effective_char_ptr(osk, set_idx, char_idx);
    return is_key_toggled(term, osk, key);
}

static bool is_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, const SpecialKey* key) {
    (void)term;
    (void)osk;
    if (!key || !osk) return false;

    switch (key->type) {
        case SK_MOD_CTRL:
            return osk->mod_ctrl;
        case SK_MOD_ALT:
            return osk->mod_alt;
        case SK_MOD_GUI:
            return osk->mod_gui;
        case SK_MOD_SHIFT:
            return osk->mod_shift;
        default:
            return false;
    }
}

static const char* get_special_key_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx) {
    (void)set_idx;
    if (!osk || !osk->control_set.keys || char_idx < 0 || char_idx >= osk->control_set.num_keys) {
        return NULL;
    }
    return osk->control_set.keys[char_idx].display_name;
}

static bool is_special_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx) {
    (void)term;
    (void)set_idx;
    if (!osk || !osk->control_set.keys || char_idx < 0 || char_idx >= osk->control_set.num_keys) {
        return false;
    }
    return is_key_toggled(term, osk, &osk->control_set.keys[char_idx]);
}

OSKKeyCache* osk_key_cache_create(void)
{
    OSKKeyCache* cache = calloc(1, sizeof(OSKKeyCache));
    if (!cache) {
        ERROR_LOG("Failed to allocate OSK key cache");
        return NULL;
    }
    
    DEBUG_LOG("Created OSK key cache with %d entries", OSK_KEY_CACHE_SIZE);
    return cache;
}

void osk_key_cache_destroy(OSKKeyCache* cache)
{
    if (!cache) {
        return;
    }
    
    int destroyed_count = 0;
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            cache->entries[i].texture = NULL;
            destroyed_count++;
        }
    }
    
    DEBUG_LOG("Destroyed OSK key cache, freed %d textures", destroyed_count);
    free(cache);
}
