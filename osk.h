#ifndef OSK_H
#define OSK_H

#include "terminal_state.h"

typedef struct {
    const char* token;       // e.g., "{ENTER}"
    const char* display;     // e.g., "ENT"
    SpecialKeyType type;     // e.g., SK_SEQUENCE
    SDL_Keycode keycode;     // e.g., SDLK_RETURN
} LayoutToken;

// Function pointer types for OSK key rendering callbacks
typedef const char* (*OSK_GetDisplayNameFunc)(const OnScreenKeyboard* osk, int set_idx, int char_idx);
typedef bool (*OSK_IsKeyToggledFunc)(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx);

// Helper to determine OSK's vertical position
int get_osk_y_position(const OnScreenKeyboard* osk, const Terminal* term, int win_h, int char_h);

// Helper to get the currently active character set (normal or shifted).
const SpecialKeySet* osk_get_effective_row_ptr(const OnScreenKeyboard* osk, int set_idx);

// Helper to get the number of rows for the current character layer.
int get_current_num_char_rows(const OnScreenKeyboard* osk);
void osk_validate_row_index(OnScreenKeyboard* osk);

// Helper to find the definitive character pointer after handling all fallbacks.
const SpecialKey* osk_get_effective_char_ptr(const OnScreenKeyboard* osk, int set_idx, int char_idx);

// Main OSK rendering function
void render_osk(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, const Terminal* term, int win_w, int win_h, int char_w, int char_h);

// OSK layout loading
void osk_load_layout(OnScreenKeyboard* osk, const char* path);

// Functions for loading and managing custom key sets
void osk_init_all_sets(OnScreenKeyboard* osk);
void osk_make_set_available(OnScreenKeyboard* osk, const char* path); // New for adding to CONTROL set without loading
void osk_add_custom_set(OnScreenKeyboard* osk, const char* path); // For dynamic loading
void osk_remove_custom_set(OnScreenKeyboard* osk, const char* name); // New for dynamic unloading
void osk_free_all_sets(OnScreenKeyboard* osk); // Renamed from osk_free_custom_sets

const LayoutToken* osk_find_layout_token(const char* str_start);

#endif // OSK_H
