/**
 * @file glyph_cache.h
 * @brief Glyph caching functionality for performance optimization.
 *
 * This module handles caching of rendered glyphs to improve rendering performance:
 * - Glyph cache management
 * - Cache hit/miss operations
 * - Texture lifecycle management
 * - Cache statistics and cleanup
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef GLYPH_CACHE_H
#define GLYPH_CACHE_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"

/**
 * @brief Create a glyph key for caching
 *
 * @param c Character code
 * @param attributes Character attributes
 * @param fg Foreground color
 * @return uint64_t Cache key
 */
uint64_t make_glyph_key(uint32_t c, unsigned char attributes, SDL_Color fg);

/**
 * @brief Hash a cache key
 *
 * @param key Cache key to hash
 * @return uint32_t Hash value
 */
uint32_t hash_key(uint64_t key);

/**
 * @brief Get a cached glyph texture
 *
 * @param cache Pointer to the glyph cache
 * @param key Cache key
 * @return GlyphCacheEntry* Pointer to cache entry or NULL
 */
GlyphCacheEntry* glyph_cache_get(GlyphCache* cache, uint64_t key);

/**
 * @brief Put a glyph texture into the cache
 *
 * @param cache Pointer to the glyph cache
 * @param key Cache key
 * @param texture SDL texture to cache
 * @param w Texture width
 * @param h Texture height
 * @return bool True if successfully cached
 */
bool glyph_cache_put(GlyphCache* cache, uint64_t key, SDL_Texture* texture, int w, int h);

/**
 * @brief Initialize the glyph cache
 *
 * @param cache Pointer to the cache structure
 * @param capacity Initial cache capacity
 * @return bool True if initialization succeeded
 */
bool glyph_cache_init(GlyphCache* cache, int capacity);

/**
 * @brief Cleanup the glyph cache
 *
 * @param cache Pointer to the cache structure
 */
void glyph_cache_cleanup(GlyphCache* cache);

/**
 * @brief Clear the glyph cache
 *
 * @param cache Pointer to the cache structure
 */
void glyph_cache_clear(GlyphCache* cache);

/**
 * @brief Get cache statistics
 *
 * @param cache Pointer to the cache structure
 * @param hits Output for cache hits
 * @param misses Output for cache misses
 * @param size Output for current cache size
 */
void glyph_cache_stats(GlyphCache* cache, int* hits, int* misses, int* size);

/**
 * @brief Render a glyph and cache it if needed
 *
 * This function combines glyph rendering and caching in one operation.
 *
 * @param renderer SDL renderer
 * @param font Font to use for rendering
 * @param cache Glyph cache
 * @param c Character to render
 * @param fg Foreground color
 * @param bg Background color
 * @param attributes Character attributes
 * @param texture Output for the cached texture
 * @param w Output for texture width
 * @param h Output for texture height
 * @return bool True if rendering succeeded
 */
bool render_and_cache_glyph(SDL_Renderer* renderer, TTF_Font* font, GlyphCache* cache,
                            uint32_t c, SDL_Color fg, SDL_Color bg, unsigned char attributes,
                            SDL_Texture** texture, int* w, int* h);

#endif // GLYPH_CACHE_H
