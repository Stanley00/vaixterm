/**
 * @file cache_manager.c
 * @brief Centralized cache management for glyphs and OSK keys.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "cache_manager.h"
#include "error_handler.h"

/**
 * @brief Creates a new glyph cache.
 */
GlyphCache* glyph_cache_create(void)
{
    GlyphCache* cache = calloc(1, sizeof(GlyphCache));
    if (!cache) {
        error_log_errno("Failed to allocate glyph cache");
        return NULL;
    }
    
    error_debug("Created glyph cache with %d entries", GLYPH_CACHE_SIZE);
    return cache;
}

/**
 * @brief Destroys a glyph cache and frees all textures.
 */
void glyph_cache_destroy(GlyphCache* cache)
{
    if (!cache) {
        return;
    }
    
    int destroyed_count = 0;
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            destroyed_count++;
        }
    }
    
    error_debug("Destroyed glyph cache, freed %d textures", destroyed_count);
    free(cache);
}

/**
 * @brief Generates a cache key for a glyph.
 */
static uint64_t glyph_cache_generate_key(uint32_t character, SDL_Color fg, SDL_Color bg, 
                                        unsigned char attributes, TTF_Font* font)
{
    uint64_t key = 0;
    
    // Character (32 bits)
    key |= (uint64_t)character;
    
    // Foreground color (8 bits each for R, G, B)
    key |= ((uint64_t)fg.r) << 32;
    key |= ((uint64_t)fg.g) << 40;
    key |= ((uint64_t)fg.b) << 48;
    
    // Background color (use only R component for space efficiency)
    key |= ((uint64_t)bg.r) << 56;
    
    // Attributes (lower 8 bits, shifted to avoid collision)
    key ^= ((uint64_t)attributes) << 24;
    
    // Font pointer (use lower bits for variation)
    key ^= ((uint64_t)(uintptr_t)font) >> 4;
    
    return key;
}

/**
 * @brief Finds a glyph in the cache.
 */
GlyphCacheEntry* glyph_cache_find(GlyphCache* cache, uint32_t character, SDL_Color fg, 
                                 SDL_Color bg, unsigned char attributes, TTF_Font* font)
{
    if (!cache) {
        return NULL;
    }
    
    uint64_t key = glyph_cache_generate_key(character, fg, bg, attributes, font);
    int index = key % GLYPH_CACHE_SIZE;
    
    GlyphCacheEntry* entry = &cache->entries[index];
    if (entry->texture && entry->key == key) {
        return entry;
    }
    
    return NULL;
}

/**
 * @brief Stores a glyph in the cache.
 */
void glyph_cache_store(GlyphCache* cache, uint32_t character, SDL_Color fg, SDL_Color bg, 
                      unsigned char attributes, TTF_Font* font, SDL_Texture* texture, 
                      int width, int height)
{
    if (!cache || !texture) {
        return;
    }
    
    uint64_t key = glyph_cache_generate_key(character, fg, bg, attributes, font);
    int index = key % GLYPH_CACHE_SIZE;
    
    GlyphCacheEntry* entry = &cache->entries[index];
    
    // Free existing texture if present
    if (entry->texture) {
        SDL_DestroyTexture(entry->texture);
    }
    
    entry->key = key;
    entry->texture = texture;
    entry->w = width;
    entry->h = height;
}

/**
 * @brief Creates a new OSK key cache.
 */
OSKKeyCache* osk_key_cache_create(void)
{
    OSKKeyCache* cache = calloc(1, sizeof(OSKKeyCache));
    if (!cache) {
        error_log_errno("Failed to allocate OSK key cache");
        return NULL;
    }
    
    error_debug("Created OSK key cache with %d entries", OSK_KEY_CACHE_SIZE);
    return cache;
}

/**
 * @brief Destroys an OSK key cache and frees all textures.
 */
void osk_key_cache_destroy(OSKKeyCache* cache)
{
    if (!cache) {
        return;
    }
    
    int destroyed_count = 0;
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            destroyed_count++;
        }
    }
    
    error_debug("Destroyed OSK key cache, freed %d textures", destroyed_count);
    free(cache);
}

/**
 * @brief Generates a cache key for an OSK key.
 */
static uint64_t osk_key_cache_generate_key(const char* text, OSKKeyState state, 
                                          int width, int height, TTF_Font* font)
{
    uint64_t key = 0;
    
    // Hash the text string
    if (text) {
        const char* p = text;
        while (*p && (p - text) < 8) { // Use up to 8 characters
            key = (key << 8) | (unsigned char)*p;
            p++;
        }
    }
    
    // Add state, dimensions, and font
    key ^= ((uint64_t)state) << 56;
    key ^= ((uint64_t)width) << 48;
    key ^= ((uint64_t)height) << 40;
    key ^= ((uint64_t)(uintptr_t)font) >> 4;
    
    return key;
}

/**
 * @brief Finds an OSK key in the cache.
 */
OSKKeyCacheEntry* osk_key_cache_find(OSKKeyCache* cache, const char* text, OSKKeyState state, 
                                    int width, int height, TTF_Font* font)
{
    if (!cache) {
        return NULL;
    }
    
    uint64_t key = osk_key_cache_generate_key(text, state, width, height, font);
    int index = key % OSK_KEY_CACHE_SIZE;
    
    OSKKeyCacheEntry* entry = &cache->entries[index];
    if (entry->texture && entry->key == key) {
        return entry;
    }
    
    return NULL;
}

/**
 * @brief Stores an OSK key in the cache.
 */
void osk_key_cache_store(OSKKeyCache* cache, const char* text, OSKKeyState state, 
                        int width, int height, TTF_Font* font, SDL_Texture* texture)
{
    if (!cache || !texture) {
        return;
    }
    
    uint64_t key = osk_key_cache_generate_key(text, state, width, height, font);
    int index = key % OSK_KEY_CACHE_SIZE;
    
    OSKKeyCacheEntry* entry = &cache->entries[index];
    
    // Free existing texture if present
    if (entry->texture) {
        SDL_DestroyTexture(entry->texture);
    }
    
    entry->key = key;
    entry->texture = texture;
    entry->w = width;
    entry->h = height;
}

/**
 * @brief Clears all entries in a glyph cache.
 */
void glyph_cache_clear(GlyphCache* cache)
{
    if (!cache) {
        return;
    }
    
    int cleared_count = 0;
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            cache->entries[i].texture = NULL;
            cache->entries[i].key = 0;
            cleared_count++;
        }
    }
    
    error_debug("Cleared glyph cache, freed %d textures", cleared_count);
}

/**
 * @brief Clears all entries in an OSK key cache.
 */
void osk_key_cache_clear(OSKKeyCache* cache)
{
    if (!cache) {
        return;
    }
    
    int cleared_count = 0;
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            cache->entries[i].texture = NULL;
            cache->entries[i].key = 0;
            cleared_count++;
        }
    }
    
    error_debug("Cleared OSK key cache, freed %d textures", cleared_count);
}

/**
 * @brief Gets cache statistics for debugging.
 */
void glyph_cache_get_stats(GlyphCache* cache, int* used_entries, int* total_entries)
{
    if (!cache || !used_entries || !total_entries) {
        return;
    }
    
    *total_entries = GLYPH_CACHE_SIZE;
    *used_entries = 0;
    
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            (*used_entries)++;
        }
    }
}

/**
 * @brief Gets OSK key cache statistics for debugging.
 */
void osk_key_cache_get_stats(OSKKeyCache* cache, int* used_entries, int* total_entries)
{
    if (!cache || !used_entries || !total_entries) {
        return;
    }
    
    *total_entries = OSK_KEY_CACHE_SIZE;
    *used_entries = 0;
    
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            (*used_entries)++;
        }
    }
}
