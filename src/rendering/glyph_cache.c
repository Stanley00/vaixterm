/**
 * @file glyph_cache.c
 * @brief Glyph caching functionality implementation.
 *
 * This module implements glyph caching for performance optimization using
 * a hash table with quadratic probing and LRU eviction.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "glyph_cache.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>

uint64_t make_glyph_key(uint32_t c, unsigned char attributes, SDL_Color fg)
{
    unsigned char render_attrs = attributes & (ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE);
    uint32_t color_val = (fg.r << 16) | (fg.g << 8) | fg.b;
    uint64_t key = ((uint64_t)color_val << 40) | ((uint64_t)render_attrs << 32) | c;
    return key;
}

uint32_t hash_key(uint64_t key)
{
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key % GLYPH_CACHE_SIZE;
}

GlyphCacheEntry* glyph_cache_get(GlyphCache* cache, uint64_t key)
{
    if (!cache) {
        ERROR_LOG("Invalid parameter: cache=%p", (void*)cache);
        return NULL;
    }

    uint32_t index = hash_key(key);
    // Quadratic probing with LRU tracking
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        uint32_t probe_index = (index + (uint32_t)((i * i + i) / 2)) & (GLYPH_CACHE_SIZE - 1);
        if (cache->entries[probe_index].key == 0) {
            return NULL;
        }
        if (cache->entries[probe_index].key == key) {
            // Update LRU tracking
            cache->last_access[probe_index] = ++cache->access_counter;
            return &cache->entries[probe_index];
        }
    }
    return NULL;
}

bool glyph_cache_put(GlyphCache* cache, uint64_t key, SDL_Texture* texture, int w, int h)
{
    if (!cache || !texture || w <= 0 || h <= 0) {
        ERROR_LOG("Invalid parameters: cache=%p, texture=%p, w=%d, h=%d", (void*)cache, (void*)texture, w, h);
        return false;
    }

    uint32_t index = hash_key(key);
    // Quadratic probing with LRU eviction
    uint32_t oldest_index = 0;
    uint32_t oldest_access = UINT32_MAX;
    
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        uint32_t probe_index = (index + (uint32_t)((i * i + i) / 2)) & (GLYPH_CACHE_SIZE - 1);
        
        if (cache->entries[probe_index].key == 0) {
            // Empty slot found
            cache->entries[probe_index].key = key;
            cache->entries[probe_index].texture = texture;
            cache->entries[probe_index].w = w;
            cache->entries[probe_index].h = h;
            cache->last_access[probe_index] = ++cache->access_counter;
            cache->hits++;
            return true;
        }
        
        if (cache->entries[probe_index].key == key) {
            // Key already exists, update texture
            if (cache->entries[probe_index].texture) {
                SDL_DestroyTexture(cache->entries[probe_index].texture);
            }
            cache->entries[probe_index].texture = texture;
            cache->entries[probe_index].w = w;
            cache->entries[probe_index].h = h;
            cache->last_access[probe_index] = ++cache->access_counter;
            cache->hits++;
            return true;
        }
        
        // Track oldest entry for eviction
        if (cache->last_access[probe_index] < oldest_access) {
            oldest_access = cache->last_access[probe_index];
            oldest_index = probe_index;
        }
    }
    
    // Cache is full, evict oldest entry
    if (cache->entries[oldest_index].texture) {
        SDL_DestroyTexture(cache->entries[oldest_index].texture);
    }
    
    cache->entries[oldest_index].key = key;
    cache->entries[oldest_index].texture = texture;
    cache->entries[oldest_index].w = w;
    cache->entries[oldest_index].h = h;
    cache->last_access[oldest_index] = ++cache->access_counter;
    cache->misses++;
    
    DEBUG_LOG("Cache full, evicted oldest entry for key 0x%llx", (unsigned long long)key);
    return true;
}

bool glyph_cache_init(GlyphCache* cache, int capacity)
{
    if (!cache || capacity <= 0) {
        ERROR_LOG("Invalid parameters: cache=%p, capacity=%d", (void*)cache, capacity);
        return false;
    }

    // Use the defined cache size, but allow for future expansion
    // Initialize cache entries
    memset(cache->entries, 0, GLYPH_CACHE_SIZE * sizeof(GlyphCacheEntry));
    memset(cache->last_access, 0, GLYPH_CACHE_SIZE * sizeof(uint32_t));

    cache->access_counter = 0;
    cache->hits = 0;
    cache->misses = 0;

    DEBUG_LOG("Glyph cache initialized with capacity %d", GLYPH_CACHE_SIZE);
    return true;
}

void glyph_cache_cleanup(GlyphCache* cache)
{
    if (!cache) {
        ERROR_LOG("Invalid parameter: cache=%p", (void*)cache);
        return;
    }
    
    glyph_cache_clear(cache);
    
    // Reset counters
    cache->access_counter = 0;
    cache->hits = 0;
    cache->misses = 0;
    
    DEBUG_LOG("Glyph cache cleaned up");
}

void glyph_cache_clear(GlyphCache* cache)
{
    if (!cache) {
        ERROR_LOG("Invalid parameter: cache=%p", (void*)cache);
        return;
    }

    int cleared_count = 0;
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
            cache->entries[i].texture = NULL;
            cleared_count++;
        }
        cache->entries[i].key = 0;
        cache->entries[i].w = 0;
        cache->entries[i].h = 0;
    }
    
    memset(cache->last_access, 0, GLYPH_CACHE_SIZE * sizeof(uint32_t));
    cache->access_counter = 0;
    cache->hits = 0;
    cache->misses = 0;
    
    DEBUG_LOG("Cleared glyph cache, freed %d textures", cleared_count);
}

void glyph_cache_stats(GlyphCache* cache, int* hits, int* misses, int* size)
{
    if (!cache) {
        ERROR_LOG("Invalid parameter: cache=%p", (void*)cache);
        return;
    }

    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
    if (size) *size = GLYPH_CACHE_SIZE;
}

bool render_and_cache_glyph(SDL_Renderer* renderer, TTF_Font* font, GlyphCache* cache,
                            uint32_t c, SDL_Color fg, SDL_Color bg, unsigned char attributes,
                            SDL_Texture** texture, int* w, int* h)
{
    (void)bg;
    if (!renderer || !font || !cache || !texture || !w || !h) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, cache=%p, texture=%p, w=%p, h=%p",
                  (void*)renderer, (void*)font, (void*)cache, (void*)texture, (void*)w, (void*)h);
        return false;
    }

    // Convert UTF-32 character to UTF-8 string for rendering
    char utf8_str[5];
    int utf8_len = 0;
    
    if (c < 0x80) {
        utf8_str[0] = (char)c;
        utf8_len = 1;
    } else if (c < 0x800) {
        utf8_str[0] = 0xC0 | (c >> 6);
        utf8_str[1] = 0x80 | (c & 0x3F);
        utf8_len = 2;
    } else if (c < 0x10000) {
        utf8_str[0] = 0xE0 | (c >> 12);
        utf8_str[1] = 0x80 | ((c >> 6) & 0x3F);
        utf8_str[2] = 0x80 | (c & 0x3F);
        utf8_len = 3;
    } else {
        utf8_str[0] = 0xF0 | (c >> 18);
        utf8_str[1] = 0x80 | ((c >> 12) & 0x3F);
        utf8_str[2] = 0x80 | ((c >> 6) & 0x3F);
        utf8_str[3] = 0x80 | (c & 0x3F);
        utf8_len = 4;
    }
    utf8_str[utf8_len] = '\0';

    // Apply text styling based on attributes
    int font_style = TTF_STYLE_NORMAL;
    if (attributes & ATTR_BOLD) font_style |= TTF_STYLE_BOLD;
    if (attributes & ATTR_ITALIC) font_style |= TTF_STYLE_ITALIC;
    if (attributes & ATTR_UNDERLINE) font_style |= TTF_STYLE_UNDERLINE;

    TTF_SetFontStyle(font, font_style);

    // Render the glyph
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, utf8_str, fg);
    if (!surface) {
        ERROR_LOG("Failed to render glyph surface: %s", TTF_GetError());
        TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
        return false;
    }

    // Create texture
    SDL_Texture* glyph_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!glyph_texture) {
        ERROR_LOG("Failed to create glyph texture: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
        return false;
    }

    // Get texture dimensions
    *w = surface->w;
    *h = surface->h;
    *texture = glyph_texture;

    SDL_FreeSurface(surface);
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);

    // Cache the glyph
    uint64_t cache_key = make_glyph_key(c, attributes, fg);
    if (!glyph_cache_put(cache, cache_key, glyph_texture, *w, *h)) {
        WARN_LOG("Failed to cache glyph, but rendering succeeded");
    }

    DEBUG_LOG("Rendered and cached glyph U+%04X", c);
    return true;
}
