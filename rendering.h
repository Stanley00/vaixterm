#ifndef RENDERING_H
#define RENDERING_H

#include "terminal_state.h"

// --- Glyph Cache Functions ---
GlyphCache* glyph_cache_create();
void glyph_cache_destroy(GlyphCache* cache);

// --- OSK Key Cache Functions ---
OSKKeyCache* osk_key_cache_create();
void osk_key_cache_destroy(OSKKeyCache* cache);

// --- Rendering Functions ---
void terminal_render(SDL_Renderer* renderer, Terminal* term, TTF_Font* font, int char_w, int char_h, OnScreenKeyboard* osk, bool force_full_render, int win_w, int win_h);

// Helper for OSK rendering (called by osk.c)
void render_one_osk_key(SDL_Renderer* renderer, TTF_Font* font, OSKKeyCache* cache,
                        const char* text, SDL_Rect key_rect, bool is_selected,
                        bool is_toggled, bool is_set_name);
void render_credit_screen(SDL_Renderer* renderer, TTF_Font* font, int win_w, int win_h);

#endif // RENDERING_H
