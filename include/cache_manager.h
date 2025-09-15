/**
 * @file cache_manager.h
 * @brief Centralized cache management for glyphs and OSK keys.
 */

#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>

#include "terminal_state.h"

/**
 * @brief Creates a new glyph cache.
 * @return New glyph cache or NULL on failure.
 */
GlyphCache* glyph_cache_create(void);

/**
 * @brief Destroys a glyph cache and frees all textures.
 * @param cache Glyph cache to destroy.
 */
void glyph_cache_destroy(GlyphCache* cache);

/**
 * @brief Finds a glyph in the cache.
 * @param cache Glyph cache to search.
 * @param character Unicode character.
 * @param fg Foreground color.
 * @param bg Background color.
 * @param attributes Text attributes.
 * @param font Font used for rendering.
 * @return Cache entry or NULL if not found.
 */
GlyphCacheEntry* glyph_cache_find(GlyphCache* cache, uint32_t character, SDL_Color fg, 
                                 SDL_Color bg, unsigned char attributes, TTF_Font* font);

/**
 * @brief Stores a glyph in the cache.
 * @param cache Glyph cache.
 * @param character Unicode character.
 * @param fg Foreground color.
 * @param bg Background color.
 * @param attributes Text attributes.
 * @param font Font used for rendering.
 * @param texture Rendered texture.
 * @param width Texture width.
 * @param height Texture height.
 */
void glyph_cache_store(GlyphCache* cache, uint32_t character, SDL_Color fg, SDL_Color bg, 
                      unsigned char attributes, TTF_Font* font, SDL_Texture* texture, 
                      int width, int height);

/**
 * @brief Creates a new OSK key cache.
 * @return New OSK key cache or NULL on failure.
 */
OSKKeyCache* osk_key_cache_create(void);

/**
 * @brief Destroys an OSK key cache and frees all textures.
 * @param cache OSK key cache to destroy.
 */
void osk_key_cache_destroy(OSKKeyCache* cache);

/**
 * @brief Finds an OSK key in the cache.
 * @param cache OSK key cache to search.
 * @param text Key text.
 * @param state Key state.
 * @param width Key width.
 * @param height Key height.
 * @param font Font used for rendering.
 * @return Cache entry or NULL if not found.
 */
OSKKeyCacheEntry* osk_key_cache_find(OSKKeyCache* cache, const char* text, OSKKeyState state, 
                                    int width, int height, TTF_Font* font);

/**
 * @brief Stores an OSK key in the cache.
 * @param cache OSK key cache.
 * @param text Key text.
 * @param state Key state.
 * @param width Key width.
 * @param height Key height.
 * @param font Font used for rendering.
 * @param texture Rendered texture.
 */
void osk_key_cache_store(OSKKeyCache* cache, const char* text, OSKKeyState state, 
                        int width, int height, TTF_Font* font, SDL_Texture* texture);

/**
 * @brief Clears all entries in a glyph cache.
 * @param cache Glyph cache to clear.
 */
void glyph_cache_clear(GlyphCache* cache);

/**
 * @brief Clears all entries in an OSK key cache.
 * @param cache OSK key cache to clear.
 */
void osk_key_cache_clear(OSKKeyCache* cache);

/**
 * @brief Gets cache statistics for debugging.
 * @param cache Glyph cache.
 * @param used_entries Pointer to store used entry count.
 * @param total_entries Pointer to store total entry count.
 */
void glyph_cache_get_stats(GlyphCache* cache, int* used_entries, int* total_entries);

/**
 * @brief Gets OSK key cache statistics for debugging.
 * @param cache OSK key cache.
 * @param used_entries Pointer to store used entry count.
 * @param total_entries Pointer to store total entry count.
 */
void osk_key_cache_get_stats(OSKKeyCache* cache, int* used_entries, int* total_entries);

#endif // CACHE_MANAGER_H
