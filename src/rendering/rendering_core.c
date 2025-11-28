/**
 * @file rendering_core.c
 * @brief Core rendering functionality implementation.
 *
 * This module implements the main terminal rendering operations including
 * terminal rendering coordination, text rendering, and credit screen.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "rendering_core.h"
#include "glyph_cache.h"
#include "color_manager.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>

// External function from terminal.c
extern Glyph* terminal_get_view_line(Terminal* term, int y);
extern void terminal_clear_dirty_lines(Terminal* term);

void terminal_render(SDL_Renderer* renderer, Terminal* term, TTF_Font* font,
                     int char_w, int char_h, OnScreenKeyboard* osk, 
                    bool force_full_render, int win_w, int win_h)
{
    (void)osk;
    if (!renderer || !term || !font || char_w <= 0 || char_h <= 0 || win_w <= 0 || win_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, term=%p, font=%p, char_w=%d, char_h=%d, win_w=%d, win_h=%d",
                  (void*)renderer, (void*)term, (void*)font, char_w, char_h, win_w, win_h);
        return;
    }

    // Fast path: Skip rendering if nothing changed and not forced
    if (!force_full_render && !term->full_redraw_needed && !term->has_dirty_regions && !term->cursor_visible) {
        return; // Nothing to render
    }
    
    bool needs_texture_update = term->full_redraw_needed || force_full_render || term->has_dirty_regions;

    if (needs_texture_update) {
        SDL_SetRenderTarget(renderer, term->screen_texture);

        bool force_full_repaint_this_frame = term->full_redraw_needed || force_full_render;

        // Skip SDL_RenderClear to prevent flicker - the texture will be overwritten anyway
        if (term->background_texture && force_full_repaint_this_frame) {
            SDL_RenderCopy(renderer, term->background_texture, NULL, NULL);
        }

        // Optimized rendering using dirty region bounds
        if (force_full_repaint_this_frame) {
            // Full screen render
            for (int y = 0; y < term->rows; ++y) {
                Glyph* line = terminal_get_view_line(term, y);
                if (line) {
                    for (int x = 0; x < term->cols; ++x) {
                        render_glyph_at(renderer, term, font, line[x].character, x, y, char_w, char_h, line[x].fg, line[x].bg, line[x].attributes);
                    }
                }
            }
        } else if (term->has_dirty_regions) {
            // Only render dirty region bounds for maximum performance
            if (term->background_texture) {
                // Render background for dirty regions only
                SDL_Rect src_rect = {0, term->dirty_min_y * char_h, win_w, (term->dirty_max_y - term->dirty_min_y + 1) * char_h};
                SDL_Rect dst_rect = {0, term->dirty_min_y * char_h, win_w, (term->dirty_max_y - term->dirty_min_y + 1) * char_h};
                SDL_RenderCopy(renderer, term->background_texture, &src_rect, &dst_rect);
            } else {
                // Clear dirty region with background color
                SDL_SetRenderDrawColor(renderer, term->default_bg.r, term->default_bg.g, term->default_bg.b, 255);
                SDL_Rect clear_rect = {0, term->dirty_min_y * char_h, win_w, (term->dirty_max_y - term->dirty_min_y + 1) * char_h};
                SDL_RenderFillRect(renderer, &clear_rect);
            }
            
            for (int y = term->dirty_min_y; y <= term->dirty_max_y; ++y) {
                if (term->dirty_lines[y]) {
                    Glyph* line = terminal_get_view_line(term, y);
                    if (line) {
                        for (int x = 0; x < term->cols; ++x) {
                            render_glyph_at(renderer, term, font, line[x].character, x, y, char_w, char_h, line[x].fg, line[x].bg, line[x].attributes);
                        }
                    }
                }
            }
        }
        terminal_clear_dirty_lines(term);
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
        float view_ratio = (float)term->rows / content_total_lines;
        float thumb_h = view_ratio * term->rows * char_h;
        if (thumb_h < min_thumb_h) thumb_h = min_thumb_h;

        float max_scroll = content_total_lines - term->rows;
        float scroll_ratio = (float)term->view_offset / max_scroll;
        float thumb_y = scroll_ratio * (term->rows * char_h - thumb_h);

        SDL_Rect scrollbar_bg = {win_w - scrollbar_w, 0, scrollbar_w, term->rows * char_h};
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 150);
        SDL_RenderFillRect(renderer, &scrollbar_bg);

        SDL_Rect scrollbar_thumb = {win_w - scrollbar_w, (int)thumb_y, scrollbar_w, (int)thumb_h};
        SDL_SetRenderDrawColor(renderer, 120, 120, 120, 200);
        SDL_RenderFillRect(renderer, &scrollbar_thumb);
    }

    DEBUG_LOG("Terminal render completed");
}

void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, 
                 int x, int y, SDL_Color color, bool centered, int win_w)
{
    if (!renderer || !font || !text) {
        ERROR_LOG("Invalid parameters: renderer=%p, font=%p, text=%p", (void*)renderer, (void*)font, (void*)text);
        return;
    }

    if (!validate_color(&color)) {
        ERROR_LOG("Invalid color provided");
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        ERROR_LOG("Failed to render text surface: %s", TTF_GetError());
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        ERROR_LOG("Failed to create texture from surface: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    int text_w = surface->w;
    int text_h = surface->h;
    SDL_Rect dst_rect;

    if (centered) {
        dst_rect.x = (win_w - text_w) / 2;
        dst_rect.y = y;
    } else {
        dst_rect.x = x;
        dst_rect.y = y;
    }

    dst_rect.w = text_w;
    dst_rect.h = text_h;

    SDL_RenderCopy(renderer, texture, NULL, &dst_rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);

    DEBUG_LOG("Rendered text: '%s' at (%d, %d)", text, dst_rect.x, dst_rect.y);
}

void render_glyph_at(SDL_Renderer* renderer, Terminal* term, TTF_Font* font,
                     uint32_t c, int x, int y, int char_w, int char_h,
                     SDL_Color fg, SDL_Color bg, unsigned char attributes)
{
    if (!renderer || !term || !font || char_w <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: renderer=%p, term=%p, font=%p, char_w=%d, char_h=%d",
                  (void*)renderer, (void*)term, (void*)font, char_w, char_h);
        return;
    }

    if (!validate_color(&fg) || !validate_color(&bg)) {
        ERROR_LOG("Invalid colors provided");
        return;
    }

    // Apply SGR attributes to colors
    SDL_Color actual_fg = fg;
    SDL_Color actual_bg = bg;
    apply_sgr_colors(term, &(Glyph){.character = c, .fg = fg, .bg = bg, .attributes = attributes}, &actual_fg, &actual_bg);

    // Render background
    SDL_Rect bg_rect = {x * char_w, y * char_h, char_w, char_h};
    SDL_SetRenderDrawColor(renderer, actual_bg.r, actual_bg.g, actual_bg.b, actual_bg.a);
    SDL_RenderFillRect(renderer, &bg_rect);

    // Skip rendering for space character
    if (c == ' ') {
        return;
    }

    // Try to get from glyph cache first
    uint64_t cache_key = make_glyph_key(c, attributes, actual_fg);
    GlyphCacheEntry* entry = glyph_cache_get(term->glyph_cache, cache_key);

    SDL_Texture* texture = NULL;
    int texture_w = 0, texture_h = 0;

    if (entry) {
        // Cache hit
        texture = entry->texture;
        texture_w = entry->w;
        texture_h = entry->h;
    } else {
        // Cache miss, render and cache
        if (!render_and_cache_glyph(renderer, font, term->glyph_cache, c, actual_fg, actual_bg, attributes, &texture, &texture_w, &texture_h)) {
            ERROR_LOG("Failed to render and cache glyph");
            return;
        }
    }

    if (texture) {
        // Center the glyph in the character cell
        int glyph_x = x * char_w + (char_w - texture_w) / 2;
        int glyph_y = y * char_h + (char_h - texture_h) / 2;
        SDL_Rect dst_rect = {glyph_x, glyph_y, texture_w, texture_h};
        SDL_RenderCopy(renderer, texture, NULL, &dst_rect);
    }
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
                                 NULL
                               };

    const char* usage_col2[] = { "--- OSK Off (Terminal) ---",
                                 "L1:          Scroll Up",
                                 "R1:          Scroll Down",
                                 "--- OSK On (Modifiers) ---",
                                 "L1:          Shift",
                                 "R1:          Ctrl",
                                 "L2:          Alt",
                                 "R2:          GUI",
                                 NULL
                               };

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
