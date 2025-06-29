#include "rendering.h"
#include "terminal_state.h" // For shared data structures
#include "osk.h"            // For OSK rendering helpers (e.g., render_osk)
#include "terminal.h"       // For terminal_get_view_line
#include "manualfont.h"     // For draw_manual_char
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Internal Helper Prototypes ---
static inline uint64_t make_glyph_key(uint32_t c, unsigned char attributes, SDL_Color fg);
static inline uint32_t hash_key(uint64_t key);
static GlyphCacheEntry* glyph_cache_get(GlyphCache* cache, uint64_t key);
static bool glyph_cache_put(GlyphCache* cache, uint64_t key, SDL_Texture* texture, int w, int h);

static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color, bool centered, int win_w);
static inline uint64_t make_osk_key(const char* text, OSKKeyState state);
static OSKKeyCacheEntry* osk_key_cache_get(OSKKeyCache* cache, uint64_t key);
static bool osk_key_cache_put(OSKKeyCache* cache, uint64_t key, SDL_Texture* texture, int w, int h);

static void render_glyph_at(SDL_Renderer* renderer, Terminal* term, TTF_Font* font,
                            Glyph g, int x, int y, int char_w, int char_h);

// --- Glyph Cache Implementation ---

static inline uint64_t make_glyph_key(uint32_t c, unsigned char attributes, SDL_Color fg)
{
    unsigned char render_attrs = attributes & (ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE);
    uint32_t color_val = (fg.r << 16) | (fg.g << 8) | fg.b;
    uint64_t key = ((uint64_t)color_val << 40) | ((uint64_t)render_attrs << 32) | c;
    return key;
}

GlyphCache* glyph_cache_create()
{
    GlyphCache* cache = malloc(sizeof(GlyphCache));
    if (cache) {
        memset(cache, 0, sizeof(GlyphCache));
    }
    return cache;
}

void glyph_cache_destroy(GlyphCache* cache)
{
    if (!cache) {
        return;
    }
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
        }
    }
    free(cache);
}

static inline uint32_t hash_key(uint64_t key)
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

static GlyphCacheEntry* glyph_cache_get(GlyphCache* cache, uint64_t key)
{
    uint32_t index = hash_key(key);
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        uint32_t probe_index = (index + i) % GLYPH_CACHE_SIZE;
        if (cache->entries[probe_index].key == 0) {
            return NULL;
        }
        if (cache->entries[probe_index].key == key) {
            return &cache->entries[probe_index];
        }
    }
    return NULL;
}

static bool glyph_cache_put(GlyphCache* cache, uint64_t key, SDL_Texture* texture, int w, int h)
{
    uint32_t index = hash_key(key);
    for (int i = 0; i < GLYPH_CACHE_SIZE; ++i) {
        uint32_t probe_index = (index + i) % GLYPH_CACHE_SIZE;
        if (cache->entries[probe_index].key == 0) {
            cache->entries[probe_index].key = key;
            cache->entries[probe_index].texture = texture;
            cache->entries[probe_index].w = w;
            cache->entries[probe_index].h = h;
            return true;
        }
    }
    return false;
}

// --- OSK Key Cache Implementation ---

OSKKeyCache* osk_key_cache_create()
{
    OSKKeyCache* cache = malloc(sizeof(OSKKeyCache));
    if (cache) {
        memset(cache, 0, sizeof(OSKKeyCache));
    }
    return cache;
}

void osk_key_cache_destroy(OSKKeyCache* cache)
{
    if (!cache) {
        return;
    }
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        if (cache->entries[i].texture) {
            SDL_DestroyTexture(cache->entries[i].texture);
        }
    }
    free(cache);
}

static inline uint64_t make_osk_key(const char* text, OSKKeyState state)
{
    uint64_t hash = 14695981039346656037ULL;
    for (const char* p = text; *p; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= 1099511628211ULL;
    }
    hash ^= (uint64_t)state;
    hash *= 1099511628211ULL;
    return hash;
}

static OSKKeyCacheEntry* osk_key_cache_get(OSKKeyCache* cache, uint64_t key)
{
    uint32_t index = hash_key(key);
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        uint32_t probe_index = (index + i) % OSK_KEY_CACHE_SIZE;
        if (cache->entries[probe_index].key == 0) {
            return NULL;
        }
        if (cache->entries[probe_index].key == key) {
            return &cache->entries[probe_index];
        }
    }
    return NULL;
}

static bool osk_key_cache_put(OSKKeyCache* cache, uint64_t key, SDL_Texture* texture, int w, int h)
{
    uint32_t index = hash_key(key);
    for (int i = 0; i < OSK_KEY_CACHE_SIZE; ++i) {
        uint32_t probe_index = (index + i) % OSK_KEY_CACHE_SIZE;
        if (cache->entries[probe_index].key == 0) {
            cache->entries[probe_index].key = key;
            cache->entries[probe_index].texture = texture;
            cache->entries[probe_index].w = w;
            cache->entries[probe_index].h = h;
            return true;
        }
    }
    return false;
}

/**
 * @brief Renders a single glyph to the screen, handling caching.
 */
static void render_glyph_at(SDL_Renderer* renderer, Terminal* term, TTF_Font* font,
                            Glyph g, int x, int y, int char_w, int char_h)
{
    SDL_Color actual_fg = g.fg;
    SDL_Color actual_bg = g.bg;
    if (g.attributes & ATTR_INVERSE) {
        SDL_Color temp = actual_fg;
        actual_fg = actual_bg;
        actual_bg = temp;
    }

    if ((g.attributes & ATTR_BLINK) && !term->cursor_blink_on) {
        actual_fg = actual_bg;
    }

    bool should_draw_bg = true;
    if (term->background_texture) {
        if (actual_bg.r == term->default_bg.r &&
                actual_bg.g == term->default_bg.g &&
                actual_bg.b == term->default_bg.b) {
            should_draw_bg = false;
        }
    }

    if (should_draw_bg) {
        SDL_Rect bg_rect = {x * char_w, y * char_h, char_w, char_h};
        SDL_SetRenderDrawColor(renderer, actual_bg.r, actual_bg.g, actual_bg.b, actual_bg.a);
        SDL_RenderFillRect(renderer, &bg_rect);
    }

    if (draw_manual_char(renderer, g.character, x * char_w, y * char_h, char_w, char_h, actual_fg)) {
        return;
    }

    bool is_control_char = (g.character <= 0x1F) || (g.character >= 0x7F && g.character <= 0x9F);
    if (g.character == ' ' || is_control_char) {
        return;
    }

    unsigned char render_attrs = g.attributes & (ATTR_BOLD | ATTR_ITALIC | ATTR_UNDERLINE);
    uint64_t key = make_glyph_key(g.character, render_attrs, actual_fg);
    GlyphCacheEntry* entry = glyph_cache_get(term->glyph_cache, key);

    if (!entry) {
        char text[5] = {0};
        if (g.character < 0x80) {
            text[0] = (char)g.character;
        } else if (g.character < 0x800) {
            text[0] = (char)(0xC0 | (g.character >> 6));
            text[1] = (char)(0x80 | (g.character & 0x3F));
        } else if (g.character < 0x10000) {
            text[0] = (char)(0xE0 | (g.character >> 12));
            text[1] = (char)(0x80 | ((g.character >> 6) & 0x3F));
            text[2] = (char)(0x80 | (g.character & 0x3F));
        } else if (g.character < 0x110000) {
            text[0] = (char)(0xF0 | (g.character >> 18));
            text[1] = (char)(0x80 | ((g.character >> 12) & 0x3F));
            text[2] = (char)(0x80 | ((g.character >> 6) & 0x3F));
            text[3] = (char)(0x80 | (g.character & 0x3F));
        } else {
            text[0] = '?';
        }

        int font_style = TTF_STYLE_NORMAL;
        if (render_attrs & ATTR_BOLD) {
            font_style |= TTF_STYLE_BOLD;
        }
        if (render_attrs & ATTR_ITALIC) {
            font_style |= TTF_STYLE_ITALIC;
        }
        if (render_attrs & ATTR_UNDERLINE) {
            font_style |= TTF_STYLE_UNDERLINE;
        }
        TTF_SetFontStyle(font, font_style);

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, actual_fg);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture && glyph_cache_put(term->glyph_cache, key, texture, surface->w, surface->h)) {
                entry = glyph_cache_get(term->glyph_cache, key);
            } else if (texture) {
                SDL_Rect glyph_rect = {x * char_w, y * char_h, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, NULL, &glyph_rect);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
        TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
    }

    if (entry) {
        SDL_Rect glyph_rect = {x * char_w, y * char_h, entry->w, entry->h};
        SDL_RenderCopy(renderer, entry->texture, NULL, &glyph_rect);
    }
}

void terminal_render(SDL_Renderer* renderer, Terminal* term, TTF_Font* font, int char_w, int char_h, OnScreenKeyboard* osk, bool force_full_render, int win_w, int win_h)
{
    bool needs_texture_update = term->full_redraw_needed;
    if (force_full_render) {
        needs_texture_update = true;
    } else {
        if (!needs_texture_update) {
            for (int i = 0; i < term->rows; i++) {
                if (term->dirty_lines[i]) {
                    needs_texture_update = true;
                    break;
                }
            }
        }
    }

    if (needs_texture_update) {
        SDL_SetRenderTarget(renderer, term->screen_texture);

        bool force_full_repaint_this_frame = term->full_redraw_needed || force_full_render;

        if (force_full_repaint_this_frame) {
            SDL_SetRenderDrawColor(renderer, term->default_bg.r, term->default_bg.g, term->default_bg.b, 255);
            SDL_RenderClear(renderer);
            if (term->background_texture) {
                SDL_RenderCopy(renderer, term->background_texture, NULL, NULL);
            }
        }

        for (int y = 0; y < term->rows; ++y) {
            if (term->dirty_lines[y] || force_full_repaint_this_frame) {
                if (!force_full_repaint_this_frame && term->background_texture) {
                    SDL_Rect line_rect = {0, y * char_h, win_w, char_h};
                    SDL_RenderCopy(renderer, term->background_texture, &line_rect, &line_rect);
                }

                Glyph* line = terminal_get_view_line(term, y);
                if (line) {
                    for (int x = 0; x < term->cols; ++x) {
                        render_glyph_at(renderer, term, font, line[x], x, y, char_w, char_h);
                    }
                }
            }
        }
        memset(term->dirty_lines, 0, sizeof(bool) * (size_t)term->rows);
        term->full_redraw_needed = false;
    }

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, term->screen_texture, NULL, NULL);

    if (term->view_offset == 0 && term->cursor_visible) {
        bool should_draw_cursor = !term->cursor_style_blinking || term->cursor_blink_on;

        if (should_draw_cursor) {
            SDL_Rect cursor_rect;
            SDL_SetRenderDrawColor(renderer, term->cursor_color.r, term->cursor_color.g, term->cursor_color.b, term->cursor_color.a);

            switch (term->cursor_style) {
            case CURSOR_STYLE_BLOCK:
                cursor_rect = (SDL_Rect) {
                    term->cursor_x * char_w, term->cursor_y * char_h, char_w, char_h
                };
                SDL_RenderFillRect(renderer, &cursor_rect);
                break;
            case CURSOR_STYLE_UNDERLINE:
                cursor_rect = (SDL_Rect) {
                    term->cursor_x * char_w, term->cursor_y * char_h + char_h - 2, char_w, 2
                };
                SDL_RenderFillRect(renderer, &cursor_rect);
                break;
            case CURSOR_STYLE_BAR:
                cursor_rect = (SDL_Rect) {
                    term->cursor_x * char_w, term->cursor_y * char_h, 2, char_h
                };
                SDL_RenderFillRect(renderer, &cursor_rect);
                break;
            }
        }
    }

    if (!term->alt_screen_active && term->history_size > 0) {
        const int scrollbar_w = 4;
        const int min_thumb_h = 20;

        float content_total_lines = (float)term->history_size + term->rows;
        float thumb_h = (term->rows / content_total_lines) * win_h;
        thumb_h = SDL_max(thumb_h, min_thumb_h);

        float scrollable_track_h = (float)win_h - thumb_h;
        float scroll_progress = (float)(term->history_size - term->view_offset) / term->history_size;
        float thumb_y = scroll_progress * scrollable_track_h;

        SDL_Rect thumb_rect = {win_w - scrollbar_w, (int)thumb_y, scrollbar_w, (int)thumb_h};
        SDL_SetRenderDrawColor(renderer, 120, 120, 120, 192);
        SDL_RenderFillRect(renderer, &thumb_rect);
    }

    if (osk->active) {
        render_osk(renderer, font, osk, term, win_w, win_h, char_w, char_h);
    }
}

/**
 * @brief Renders a single line of text to the screen.
 */
static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color, bool centered, int win_w)
{
    if (!text || !text[0]) {
        return;
    }
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) {
        fprintf(stderr, "Warning: TTF_RenderText_Blended failed: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        fprintf(stderr, "Warning: SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect rect = {x, y, surf->w, surf->h};
    if (centered) {
        rect.x = (win_w - surf->w) / 2;
    }

    SDL_RenderCopy(renderer, tex, NULL, &rect);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void render_credit_screen(SDL_Renderer* renderer, TTF_Font* font, int win_w, int win_h)
{
    (void)win_h;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color title_color = {138, 226,  52, 255};
    SDL_Color text_color = {211, 215, 207, 255};
    SDL_Color header_color = {252, 233,  79, 255};

    int text_h;
    TTF_SizeUTF8(font, "W", NULL, &text_h);
    size_t y = text_h/2;
    const char* ascii_art_title[] = {
        " ___      ___ ________  ___     ___    ___",
        "|\\  \\    /  /|\\   __  \\|\\  \\   |\\  \\  /  /|",
        "\\ \\  \\  /  / \\ \\  \\|\\  \\ \\  \\  \\ \\  \\/  / /",
        " \\ \\  \\/  / / \\ \\   __  \\ \\  \\  \\ \\    / /",
        "  \\ \\    / /   \\ \\  \\ \\  \\ \\  \\  /     \\/",
        "   \\ \\__/ /     \\ \\__\\ \\__\\ \\__\\/  /\\   \\",
        "    \\|__|/       \\|__|\\|__|\\|__/__/ /\\ __\\",
        "        Terminal Emulator      |__|/ \\|__|",
    };
    const size_t ascii_lines = sizeof(ascii_art_title) / sizeof(ascii_art_title[0]);

    const char* usage_col1[] = { "--- General ---",
                                 "D-Pad:       Arrows",
                                 "A:           Select/Type",
                                 "B:           Backspace",
                                 "X:           Toggle OSK",
                                 "Y:           Space",
                                 "Start:       Enter",
                                 "Back:        Tab",
                                 "Back+Start:  Exit",
                                 NULL };

    const char* usage_col2[] = { "--- OSK Off (Terminal) ---",
                                 "L1:          Scroll Up",
                                 "R1:          Scroll Down",
                                 "",
                                 "--- OSK On (Modifiers) ---",
                                 "L1:          Shift",
                                 "R1:          Ctrl",
                                 "L2:          Alt",
                                 "R2:          GUI",
                                 NULL };

    const int spacing_after_sepates = 3*text_h;

    for (size_t i = 0; i < ascii_lines; ++i) {
        render_text(renderer, font, ascii_art_title[i], 0, y, title_color, true, win_w);
        y += text_h;
    }

    y += spacing_after_sepates;
    render_text(renderer, font, "Press ANY key to continue.", 0, y, header_color, true, win_w);

    y += spacing_after_sepates;

    const int col1_x = win_w / 10;
    const int col2_x = win_w / 2 + win_w / 20;
    const int start_y = y;

    for (size_t i = 0; usage_col1[i]; ++i) {
        render_text(renderer, font, usage_col1[i], col1_x, y, usage_col1[i][0]=='-'?header_color:text_color, false, win_w);
        y += text_h;
    }
    size_t guide_y_end = y;
    y = start_y;
    for (size_t i = 0; usage_col2[i]; ++i) {
        render_text(renderer, font, usage_col2[i], col2_x, y, usage_col2[i][0]=='-'?header_color:text_color, false, win_w);
        y += text_h;
    }
    y = SDL_max(guide_y_end, y);

    y += spacing_after_sepates;
    render_text(renderer, font, "by Stanley[._]?(00)?", 0, y, title_color, true, win_w);
}

/**
 * @brief Renders a single key for the OSK tape, using a cache for performance.
 */
void render_one_osk_key(SDL_Renderer* renderer, TTF_Font* font, OSKKeyCache* cache,
                        const char* text, SDL_Rect key_rect, bool is_selected, bool is_toggled,
                        bool is_set_name)
{
    OSKKeyState state = OSK_KEY_STATE_NORMAL;
    SDL_Color text_color = {200, 200, 200, 255};
    SDL_Color set_name_color = {252, 233, 79, 255}; // A bright yellow for the set name

    if (is_set_name) {
        state = OSK_KEY_STATE_SET_NAME;
        text_color = set_name_color;
    }

    // Set background color based on state. Toggled has precedence over not-selected.
    if (is_toggled) {
        state = OSK_KEY_STATE_TOGGLED;
        SDL_SetRenderDrawColor(renderer, 150, 80, 80, 255); // Red for toggled
        SDL_RenderFillRect(renderer, &key_rect);
    }

    // Selected state can override background and always sets text color.
    if (is_selected) {
        state = OSK_KEY_STATE_SELECTED;
        text_color = (SDL_Color) {
            255, 255, 0, 255
        };
        SDL_SetRenderDrawColor(renderer, 80, 80, 150, 255);
        SDL_RenderFillRect(renderer, &key_rect);
    }

    uint64_t key = make_osk_key(text, state); // The state now includes SET_NAME
    OSKKeyCacheEntry* entry = osk_key_cache_get(cache, key);

    if (!entry) {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, text_color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex && osk_key_cache_put(cache, key, tex, surf->w, surf->h)) {
                entry = osk_key_cache_get(cache, key);
            } else if (tex) {
                SDL_Rect dst_rect = { key_rect.x + (key_rect.w - surf->w) / 2, key_rect.y + (key_rect.h - surf->h) / 2, surf->w, surf->h };
                SDL_RenderCopy(renderer, tex, NULL, &dst_rect);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    if (entry) {
        SDL_Rect dst_rect = { key_rect.x + (key_rect.w - entry->w) / 2, key_rect.y + (key_rect.h - entry->h) / 2, entry->w, entry->h };
        SDL_RenderCopy(renderer, entry->texture, NULL, &dst_rect);
    }
}
