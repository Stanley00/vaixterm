#include "terminal.h"
#include "terminal_state.h"
#include "terminal_libvterm.h"
#include "glyph_cache.h"
#include "dirty_region_tracker.h"
#include "color_manager.h"
#include "error_codes.h"
#include <SDL_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VALIDATE_TERM(term) do { if (!(term)) return; } while(0)
#define VALIDATE_TERM_RET(term, ret) do { if (!(term)) return (ret); } while(0)
#define VALIDATE_AND_RETURN_NULL(x) do { if (!(x)) return NULL; } while(0)

// --- Colorscheme Loading ---

void terminal_load_colorscheme(Terminal* term, const char* path)
{
    VALIDATE_TERM(term);
    if (!path) return;

    FILE* file = fopen(path, "r");
    if (!file) {
        WARN_LOG("Could not open colorscheme file '%s'. Using defaults.", path);
        return;
    }

    char line[256];
    char key[64];
    char value[64];

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        if (sscanf(line, " %63[^= \t] = %63s", key, value) == 2) {
            if (strncmp(key, "color", 5) == 0) {
                int color_index = atoi(key + 5);
                if (color_index >= 0 && color_index < 256) {
                    parse_color_string(value, &term->palette[color_index]);
                    if (color_index < 16) {
                        term->colors[color_index] = term->palette[color_index];
                    }
                }
            } else if (strcmp(key, "foreground") == 0) {
                parse_color_string(value, &term->default_fg);
            } else if (strcmp(key, "background") == 0) {
                parse_color_string(value, &term->default_bg);
            } else if (strcmp(key, "cursor") == 0) {
                parse_color_string(value, &term->cursor_color);
            }
        }
    }

    fclose(file);
}

// --- Terminal Lifecycle ---
Terminal* terminal_create(int cols, int rows, const Config* config, SDL_Renderer* renderer)
{
    VALIDATE_AND_RETURN_NULL(config);
    VALIDATE_AND_RETURN_NULL(renderer);
    if (cols <= 0 || rows <= 0) {
        ERROR_LOG("Invalid terminal dimensions: %dx%d", cols, rows);
        return NULL;
    }

    Terminal* term = malloc(sizeof(Terminal));
    if (!term) {
        return NULL;
    }
    memset(term, 0, sizeof(Terminal));

    const SDL_Color default_palette[16] = {
        { 0,   0,   0, 255}, {204,   0,   0, 255}, { 78, 154,   6, 255}, {196, 160,   0, 255},
        { 52, 101, 164, 255}, {117,  80, 123, 255}, {  6, 152, 154, 255}, {211, 215, 207, 255},
        { 85,  87,  83, 255}, {239,  41,  41, 255}, {138, 226,  52, 255}, {252, 233,  79, 255},
        {114, 159, 207, 255}, {173, 127, 168, 255}, { 52, 226, 226, 255}, {238, 238, 236, 255},
    };
    memcpy(term->colors, default_palette, sizeof(default_palette));
    memcpy(term->palette, default_palette, sizeof(default_palette));
    term->default_fg = term->colors[2];
    term->default_bg = term->colors[0];
    const SDL_Color initial_cursor_color = {238, 238, 236, 255};
    term->cursor_color = initial_cursor_color;

    if (config->colorscheme_path) {
        terminal_load_colorscheme(term, config->colorscheme_path);
    }

    if (memcmp(&term->cursor_color, &initial_cursor_color, sizeof(SDL_Color)) == 0) {
        term->cursor_color = term->default_fg;
    }

    term->cols = cols;
    term->rows = rows;
    term->scrollback = config->scrollback_lines;
    term->view_offset = 0;
    term->cursor_visible = true;
    term->alt_screen_active = false;
    term->cursor_style = CURSOR_STYLE_BLOCK;
    term->cursor_style_blinking = true;
    term->cursor_blink_on = true;
    term->last_blink_toggle_time = SDL_GetTicks();

    term->glyph_cache = NULL;
    term->dirty_lines = malloc(sizeof(bool) * (size_t)rows);
    if (!term->dirty_lines) {
        free(term);
        return NULL;
    }

    terminal_init_dirty_tracking(term);
    term->screen_texture = NULL;
    term->full_redraw_needed = true;

    term->background_texture = NULL;
    if (config->background_image_path) {
        term->background_texture = IMG_LoadTexture(renderer, config->background_image_path);
        if (!term->background_texture) {
            WARN_LOG("Failed to load background image '%s': %s", config->background_image_path, IMG_GetError());
        }
    }

    if (!terminal_libvterm_init(term, cols, rows)) {
        ERROR_LOG("Failed to initialize libvterm backend");
        if (term->background_texture) SDL_DestroyTexture(term->background_texture);
        free(term->dirty_lines);
        free(term);
        return NULL;
    }

    for (int i = 0; i < 256; i++) {
        terminal_libvterm_set_palette_color(term, i, term->palette[i]);
    }
    terminal_libvterm_set_default_colors(term, term->default_fg, term->default_bg);

    terminal_reset(term);

    return term;
}

void terminal_destroy(Terminal* term)
{
    if (term) {
        if (term->background_texture) {
            SDL_DestroyTexture(term->background_texture);
        }
        if (term->glyph_cache) {
            glyph_cache_cleanup(term->glyph_cache);
            free(term->glyph_cache);
        }
        terminal_libvterm_free(term);
        free(term->dirty_lines);
        free(term);
    }
}

void terminal_reset(Terminal* term)
{
    terminal_libvterm_reset(term, false);
    term->cursor_style = CURSOR_STYLE_BLOCK;
    term->cursor_blink_on = true;
    term->view_offset = 0;
}

void terminal_resize(Terminal* term, int new_cols, int new_rows)
{
    if (term->cols == new_cols && term->rows == new_rows) {
        return;
    }

    terminal_libvterm_resize(term, new_rows, new_cols);
}

// --- Terminal Grid Operations ---

Glyph* terminal_get_view_line(Terminal* term, int y)
{
    if (y < 0 || y >= term->rows) {
        return NULL;
    }
    return terminal_libvterm_get_view_line(term, y);
}

void terminal_handle_input(Terminal* term, const char* buf, size_t len)
{
    terminal_libvterm_feed(term, buf, len);
}

int terminal_get_scrollback_count(Terminal* term)
{
    return terminal_libvterm_get_scrollback_count(term);
}

void sgr_to_color(Terminal* term, int color_index, SDL_Color* color)
{
    if (!term || !color || color_index < 0) return;

    if (color_index < 16) {
        *color = term->palette[color_index];
    } else if (color_index >= 16 && color_index <= 255) {
        if (color_index >= 16 && color_index <= 231) {
            color_index -= 16;
            int r = (color_index / 36) * 51;
            int g = ((color_index % 36) / 6) * 51;
            int b = (color_index % 6) * 51;
            *color = (SDL_Color){r, g, b, 255};
        } else {
            int gray = (color_index - 232) * 10 + 8;
            *color = (SDL_Color){gray, gray, gray, 255};
        }
    } else {
        *color = (SDL_Color){255, 255, 255, 255};
    }
}
