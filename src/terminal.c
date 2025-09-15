#include "terminal.h"
#include "terminal_state.h" // For shared data structures
#include "cache_manager.h"  // For glyph cache functions
#include "dirty_region_tracker.h" // For optimized dirty tracking
#include <SDL_image.h>      // Added for IMG_LoadTexture

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isprint

// --- Internal Helper Prototypes ---
static inline Glyph* get_line(Terminal* term, int y);
static uint32_t map_char_for_charset(char c, char charset);
static int sgr_parse_extended_color(Terminal* term, int start_idx, bool is_fg);
static void parse_color_string(const char* hex_str, SDL_Color* color);

// --- Colorscheme Loading ---

/**
 * @brief Parses a hex color string (e.g., "#RRGGBB" or "RRGGBB") into an SDL_Color.
 */
static void parse_color_string(const char* hex_str_orig, SDL_Color* color)
{
    if (!hex_str_orig || !color) {
        return;
    }
    const char* hex_str = hex_str_orig;
    if (hex_str[0] == '#') {
        hex_str++;
    }
    unsigned int r, g, b, a;
    size_t len = strlen(hex_str);

    if (len == 8 && sscanf(hex_str, "%2x%2x%2x%2x", &a, &r, &g, &b) == 4) {
        color->a = (Uint8)a;
        color->r = (Uint8)r;
        color->g = (Uint8)g;
        color->b = (Uint8)b;
    } else if (len == 6 && sscanf(hex_str, "%2x%2x%2x", &r, &g, &b) == 3) {
        color->r = (Uint8)r;
        color->g = (Uint8)g;
        color->b = (Uint8)b;
        color->a = 255;
    } else {
        fprintf(stderr, "Warning: Could not parse color string '%s'\n", hex_str_orig);
    }
}

/**
 * @brief Loads a colorscheme from a file.
 * Supported keys: color0-color15, foreground, background, cursor.
 */
void terminal_load_colorscheme(Terminal* term, const char* path)
{
    if (!path || !term) {
        return;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open colorscheme file '%s'. Using defaults.\n", path);
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
                if (color_index >= 0 && color_index < 16) {
                    parse_color_string(value, &term->colors[color_index]);
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
    Terminal* term = malloc(sizeof(Terminal));
    if (!term) {
        return NULL;
    }

    const SDL_Color default_palette[16] = {
        { 46,  52,  54, 255}, {204,   0,   0, 255}, { 78, 154,   6, 255}, {196, 160,   0, 255},
        { 52, 101, 164, 255}, {117,  80, 123, 255}, {  6, 152, 154, 255}, {211, 215, 207, 255},
        { 85,  87,  83, 255}, {239,  41,  41, 255}, {138, 226,  52, 255}, {252, 233,  79, 255},
        {114, 159, 207, 255}, {173, 127, 168, 255}, { 52, 226, 226, 255}, {238, 238, 236, 255},
    };
    memcpy(term->colors, default_palette, sizeof(default_palette));
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

    terminal_init_xterm_colors(term);

    term->cols = cols;
    term->rows = rows;
    term->scrollback = config->scrollback_lines;
    term->total_lines = term->rows + term->scrollback;
    term->grid = malloc(sizeof(Glyph) * (size_t)cols * (size_t)term->total_lines);
    if (!term->grid) {
        free(term);
        return NULL;
    }

    term->saved_cursor_x = 0;
    term->saved_cursor_y = 0;
    term->top_line = 0;
    term->view_offset = 0;
    term->history_size = 0;
    term->csi_intermediate_count = 0;
    term->csi_intermediate_chars[0] = '\0';
    term->csi_private_marker = 0;
    term->application_cursor_keys_mode = false;
    term->cursor_visible = true;
    term->application_keypad_mode = false;
    term->alt_grid = NULL;
    term->alt_screen_active = false;
    term->insert_mode = false;
    term->cursor_style = CURSOR_STYLE_BLOCK;
    term->origin_mode = false;
    term->cursor_style_blinking = true;
    term->autowrap_mode = true;
    term->normal_saved_cursor_x = 0;
    term->normal_saved_cursor_y = 0;
    term->cursor_blink_on = true;
    term->last_blink_toggle_time = SDL_GetTicks();
    term->osc_len = 0;
    term->utf8_codepoint = 0;
    term->utf8_bytes_to_read = 0;

    term->response_len = 0;
    term->glyph_cache = NULL;
    term->dirty_lines = malloc(sizeof(bool) * (size_t)rows);
    if (!term->dirty_lines) {
        free(term->grid);
        free(term);
        return NULL;
    }
    
    // Initialize dirty region tracking
    terminal_init_dirty_tracking(term);
    term->screen_texture = NULL;
    term->full_redraw_needed = true;

    term->background_texture = NULL;
    if (config->background_image_path) {
        term->background_texture = IMG_LoadTexture(renderer, config->background_image_path);
        if (!term->background_texture) {
            fprintf(stderr, "Failed to load background image '%s': %s\n", config->background_image_path, IMG_GetError());
        }
    }

    terminal_reset(term);

    return term;
}

void terminal_destroy(Terminal* term)
{
    if (term) {
        if (term->background_texture) {
            SDL_DestroyTexture(term->background_texture);
        }
        free(term->grid);
        free(term->alt_grid);
        free(term->dirty_lines);
        free(term);
    }
}

void terminal_reset(Terminal* term)
{
    term->current_fg = term->default_fg;
    term->current_bg = term->default_bg;
    term->current_attributes = 0;

    term->cursor_x = 0;
    term->cursor_y = 0;

    term->saved_cursor_x = 0;
    term->saved_cursor_y = 0;
    term->scroll_top = 1;
    term->scroll_bottom = term->rows;

    term->parse_state = STATE_NORMAL;
    term->csi_param_count = 0;
    term->csi_intermediate_count = 0;
    term->csi_intermediate_chars[0] = '\0';
    term->csi_private_marker = 0;
    memset(term->csi_params, 0, sizeof(term->csi_params));

    term->application_cursor_keys_mode = false;
    term->cursor_visible = true;
    term->application_keypad_mode = false;
    term->autowrap_mode = true;
    term->cursor_style = CURSOR_STYLE_BLOCK;
    term->cursor_style_blinking = true;
    term->insert_mode = false;
    term->cursor_blink_on = true;
    term->origin_mode = false;

    term->utf8_codepoint = 0;
    term->utf8_bytes_to_read = 0;

    term->charsets[0] = 'B';
    term->charsets[1] = 'B';
    term->active_charset = 0;

    term->top_line = 0;
    term->view_offset = 0;
    term->history_size = 0;

    for (int i = 0; i < term->cols * term->total_lines; ++i) {
        term->grid[i] = (Glyph) {
            ' ', term->default_fg, term->default_bg, 0
        };
    }
    for (int i = 0; i < term->rows; ++i) {
        term->dirty_lines[i] = true;
    }
    term->full_redraw_needed = true;
}

/**
 * @brief Resizes the terminal grid and resets its state.
 */
void terminal_resize(Terminal* term, int new_cols, int new_rows)
{
    if (term->cols == new_cols && term->rows == new_rows) {
        return;
    }

    size_t new_total_lines = (size_t)new_rows + term->scrollback;
    Glyph* new_grid = malloc(sizeof(Glyph) * (size_t)new_cols * new_total_lines);
    bool* new_dirty_lines = malloc(sizeof(bool) * (size_t)new_rows);
    Glyph* new_alt_grid = term->alt_grid ? malloc(sizeof(Glyph) * (size_t)new_cols * (size_t)new_rows) : NULL;

    if (!new_grid || !new_dirty_lines || (term->alt_grid && !new_alt_grid)) {
        fprintf(stderr, "Failed to allocate memory for terminal resize. Aborting resize.\n");
        free(new_grid);
        free(new_dirty_lines);
        free(new_alt_grid);
        return;
    }

    free(term->grid);
    free(term->dirty_lines);
    free(term->alt_grid);

    term->grid = new_grid;
    term->dirty_lines = new_dirty_lines;
    term->alt_grid = new_alt_grid;
    term->cols = new_cols;
    term->rows = new_rows;
    term->total_lines = (int)new_total_lines;

    terminal_reset(term);
}

/**
 * @brief Reloads the background texture.
 * @param term The terminal state.
 * @param renderer The SDL renderer.
 * @param path The path to the new background image.
 */
void terminal_reload_background_texture(Terminal* term, SDL_Renderer* renderer, const char* path)
{
    if (term->background_texture) {
        SDL_DestroyTexture(term->background_texture);
        term->background_texture = NULL;
    }

    if (path) {
        term->background_texture = IMG_LoadTexture(renderer, path);
        if (!term->background_texture) {
            fprintf(stderr, "Failed to load background image '%s': %s\n", path, IMG_GetError());
        }
    }
    term->full_redraw_needed = true;
}

// --- Terminal Grid Operations ---

/**
 * @brief Gets a pointer to the start of a logical line.
 * @param term The terminal state.
 * @param y The logical line number (0 to rows-1).
 * @return A pointer to the first Glyph of the line, or NULL on error.
 */
static inline Glyph* get_line(Terminal* term, int y)
{
    if (y < 0 || y >= term->rows) {
        return NULL;
    }
    if (term->alt_screen_active) {
        return term->alt_grid ? &term->alt_grid[y * term->cols] : NULL;
    } else {
        int phys_y = (term->top_line + y) % term->total_lines;
        return &term->grid[phys_y * term->cols];
    }
}

void terminal_clear_line_to_cursor(Terminal* term, int y, int end_x)
{
    Glyph* line_ptr = get_line(term, y);
    if (!line_ptr) {
        return;
    }
    for (int x = 0; x <= end_x && x < term->cols; ++x) {
        line_ptr[x] = (Glyph) {
            ' ', term->current_fg, term->current_bg, term->current_attributes
        };
    }
    terminal_mark_line_dirty(term, y);
}

void terminal_clear_line(Terminal* term, int y, int start_x)
{
    Glyph* line_ptr = get_line(term, y);
    if (!line_ptr) {
        return;
    }
    for (int x = start_x; x < term->cols; ++x) {
        line_ptr[x] = (Glyph) {
            ' ', term->current_fg, term->current_bg, term->current_attributes
        };
    }
    terminal_mark_line_dirty(term, y);
}

void terminal_clear_visible_screen(Terminal* term)
{
    for (int y = 0; y < term->rows; y++) {
        terminal_clear_line(term, y, 0);
    }
    term->full_redraw_needed = true;
}

/**
 * @brief Scrolls a specific region of the screen up or down.
 * @param term The terminal state.
 * @param top_y The 0-indexed top line of the region.
 * @param bottom_y The 0-indexed bottom line of the region.
 * @param n_lines Number of lines to scroll. Positive for up, negative for down.
 */
void terminal_scroll_region(Terminal* term, int top_y, int bottom_y, int n_lines)
{
    if (n_lines == 0 || top_y > bottom_y || top_y < 0 || bottom_y >= term->rows) {
        return;
    }

    term->full_redraw_needed = true;

    int num_to_scroll = abs(n_lines);
    int region_height = bottom_y - top_y + 1;
    if (num_to_scroll > region_height) {
        num_to_scroll = region_height;
    }

    if (term->alt_screen_active && term->alt_grid) {
        if (n_lines > 0) {
            int count = region_height - num_to_scroll;
            if (count > 0) {
                memmove(&term->alt_grid[top_y * term->cols],
                        &term->alt_grid[(top_y + num_to_scroll) * term->cols],
                        sizeof(Glyph) * (size_t)count * (size_t)term->cols);
            }
            for (int y = bottom_y - num_to_scroll + 1; y <= bottom_y; ++y) {
                terminal_clear_line(term, y, 0);
            }
        } else {
            int count = region_height - num_to_scroll;
            if (count > 0) {
                memmove(&term->alt_grid[(top_y + num_to_scroll) * term->cols],
                        &term->alt_grid[top_y * term->cols],
                        sizeof(Glyph) * (size_t)count * (size_t)term->cols);
            }
            for (int y = top_y; y < top_y + num_to_scroll; ++y) {
                terminal_clear_line(term, y, 0);
            }
        }
        return;
    }

    for (int y = top_y; y <= bottom_y; ++y) {
        terminal_mark_line_dirty(term, y);
    }

    if (n_lines > 0) {
        int count = region_height - num_to_scroll;
        if (count > 0) {
            for (int y = 0; y < count; ++y) {
                Glyph* dest_line = get_line(term, top_y + y);
                Glyph* src_line = get_line(term, top_y + num_to_scroll + y);
                if (dest_line && src_line) {
                    memcpy(dest_line, src_line, sizeof(Glyph) * (size_t)term->cols);
                }
            }
        }
        for (int y = bottom_y - num_to_scroll + 1; y <= bottom_y; ++y) {
            terminal_clear_line(term, y, 0);
        }
    } else {
        int count = region_height - num_to_scroll;
        if (count > 0) {
            for (int y = count - 1; y >= 0; --y) {
                Glyph* dest_line = get_line(term, bottom_y - y);
                Glyph* src_line = get_line(term, bottom_y - num_to_scroll - y);
                if (dest_line && src_line) {
                    memcpy(dest_line, src_line, sizeof(Glyph) * (size_t)term->cols);
                }
            }
        }
        for (int y = top_y; y < top_y + num_to_scroll; ++y) {
            terminal_clear_line(term, y, 0);
        }
    }
}

/**
 * @brief Inserts n blank characters at the cursor position.
 */
void terminal_insert_chars(Terminal* term, int n)
{
    Glyph* line = get_line(term, term->cursor_y);
    if (!line) {
        return;
    }
    int x = term->cursor_x;
    if (x >= term->cols) {
        return;
    }

    n = SDL_min(n, term->cols - x);
    int count = term->cols - x - n;
    if (count > 0) {
        memmove(&line[x + n], &line[x], sizeof(Glyph) * (size_t)count);
    }
    for (int i = 0; i < n; ++i) {
        line[x + i] = (Glyph) {
            ' ', term->current_fg, term->current_bg, term->current_attributes
        };
    }
    term->dirty_lines[term->cursor_y] = true;
}

/**
 * @brief Deletes n characters at the cursor position.
 */
void terminal_delete_chars(Terminal* term, int n)
{
    Glyph* line = get_line(term, term->cursor_y);
    if (!line) {
        return;
    }
    int x = term->cursor_x;
    if (x >= term->cols) {
        return;
    }

    n = SDL_min(n, term->cols - x);
    int count = term->cols - x - n;
    if (count > 0) {
        memmove(&line[x], &line[x + n], sizeof(Glyph) * (size_t)count);
    }
    for (int i = term->cols - n; i < term->cols; ++i) {
        line[i] = (Glyph) {
            ' ', term->current_fg, term->current_bg, term->current_attributes
        };
    }
    term->dirty_lines[term->cursor_y] = true;
}

/**
 * @brief Erases n characters at the cursor position without shifting the line.
 */
void terminal_erase_chars(Terminal* term, int n)
{
    Glyph* line = get_line(term, term->cursor_y);
    if (!line) {
        return;
    }
    int x = term->cursor_x;
    if (x >= term->cols) {
        return;
    }

    n = SDL_min(n, term->cols - x);
    for (int i = 0; i < n; ++i) {
        line[x + i] = (Glyph) {
            ' ', term->current_fg, term->current_bg, term->current_attributes
        };
    }
    term->dirty_lines[term->cursor_y] = true;
}

void terminal_newline(Terminal* term)
{
    term->cursor_y++;
    if (term->cursor_y >= term->scroll_bottom) {
        term->cursor_y = term->scroll_bottom - 1;
        if (term->scroll_top == 1 && term->scroll_bottom == term->rows) {
            terminal_scroll_up(term);
        } else {
            terminal_scroll_region(term, term->scroll_top - 1, term->scroll_bottom - 1, 1);
        }
    }
}

void terminal_scroll_up(Terminal* term)
{
    if (term->alt_screen_active) {
        if (term->alt_grid) {
            memmove(term->alt_grid, term->alt_grid + term->cols, sizeof(Glyph) * (size_t)term->cols * (size_t)(term->rows - 1));
            terminal_clear_line(term, term->rows - 1, 0);
        }
    } else {
        term->top_line = (term->top_line + 1) % term->total_lines;
        if (term->history_size < term->scrollback) {
            term->history_size++;
        }
        term->full_redraw_needed = true;
        terminal_mark_lines_dirty(term, 0, term->rows - 1);
        terminal_clear_line(term, term->rows - 1, 0);
    }
}

/**
 * @brief Maps a character to its display equivalent based on the active VT100
 *        character set (US ASCII or DEC Special Graphics).
 * @param c The input character from the PTY.
 * @param charset The active charset identifier ('B' for ASCII, '0' for Special Graphics).
 * @return The Unicode codepoint to be rendered.
 */
static uint32_t map_char_for_charset(char c, char charset)
{
    if (charset != '0' || c < '`' || c > '~') {
        return (uint32_t)(unsigned char)c;
    }

    switch (c) {
    case '`':
        return 0x25C6;
    case 'a':
        return 0x2592;
    case 'b':
        return 0x2409;
    case 'c':
        return 0x240C;
    case 'd':
        return 0x240D;
    case 'e':
        return 0x240A;
    case 'f':
        return 0x00B0;
    case 'g':
        return 0x00B1;
    case 'h':
        return 0x2424;
    case 'i':
        return 0x240B;
    case 'j':
        return 0x2518;
    case 'k':
        return 0x2510;
    case 'l':
        return 0x250C;
    case 'm':
        return 0x2514;
    case 'n':
        return 0x253C;
    case 'o':
        return 0x23BA;
    case 'p':
        return 0x23BB;
    case 'q':
        return 0x2500;
    case 'r':
        return 0x23BC;
    case 's':
        return 0x23BD;
    case 't':
        return 0x251C;
    case 'u':
        return 0x2524;
    case 'v':
        return 0x2534;
    case 'w':
        return 0x252C;
    case 'x':
        return 0x2502;
    case 'y':
        return 0x2264;
    case 'z':
        return 0x2265;
    case '{':
        return 0x03C0;
    case '|':
        return 0x2260;
    case '}':
        return 0x00A3;
    case '~':
        return 0x00B7;
    default:
        return (uint32_t)(unsigned char)c;
    }
}

/**
 * @brief Puts a character at the current cursor position, handling autowrap and insert mode.
 */
void terminal_put_char(Terminal* term, uint32_t c)
{
    if (term->autowrap_mode && term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        terminal_newline(term);
    }

    if (term->insert_mode) {
        terminal_insert_chars(term, 1);
    }

    int write_x = term->cursor_x;
    if (write_x >= term->cols) {
        write_x = term->cols - 1;
    }

    uint32_t mapped_char = (c < 128) ? map_char_for_charset((char)c, term->charsets[term->active_charset]) : c;

    Glyph* line_ptr = get_line(term, term->cursor_y);
    if (line_ptr) {
        line_ptr[write_x] = (Glyph) {
            mapped_char, term->current_fg, term->current_bg, term->current_attributes
        };
    }
    term->dirty_lines[term->cursor_y] = true;
    if (term->cursor_x < term->cols) {
        term->cursor_x++;
    }
}

/**
 * @brief Gets a pointer to a line as it should be displayed in the viewport.
 * @param term The terminal state.
 * @param y The logical line number in the viewport (0 to rows-1).
 * @return A pointer to the first Glyph of the line, or NULL on error.
 */
Glyph* terminal_get_view_line(Terminal* term, int y)
{
    if (y < 0 || y >= term->rows) {
        return NULL;
    }
    if (term->alt_screen_active) {
        return term->alt_grid ? &term->alt_grid[(size_t)y * term->cols] : NULL;
    }
    int logical_y = term->top_line - term->view_offset + y;
    int phys_y = (logical_y % term->total_lines + term->total_lines) % term->total_lines;
    return &term->grid[(size_t)phys_y * term->cols];
}

// --- ANSI Parser ---

// --- CSI Private Mode ('?') Handlers ---

static void csi_h_private(Terminal* term)
{
    for (int i = 0; i < term->csi_param_count; ++i) {
        if (term->csi_params[i] == 1) {
            term->application_cursor_keys_mode = true;
        }
        if (term->csi_params[i] == 6) {
            term->origin_mode = true;
            term->cursor_x = 0;
            term->cursor_y = term->scroll_top - 1;
        }
        if (term->csi_params[i] == 7) {
            term->autowrap_mode = true;
        }
        if (term->csi_params[i] == 66) {
            term->application_keypad_mode = true;
        }
        if (term->csi_params[i] == 25) {
            term->cursor_visible = true;
        }

        if (term->csi_params[i] == 1049) {
            if (!term->alt_screen_active) {
                if (!term->alt_grid) {
                    term->alt_grid = malloc(sizeof(Glyph) * (size_t)term->cols * (size_t)term->rows);
                }
                if (term->alt_grid) {
                    term->normal_saved_cursor_x = term->cursor_x;
                    term->normal_saved_cursor_y = term->cursor_y;
                    term->alt_screen_active = true;
                    terminal_clear_visible_screen(term);
                    term->cursor_x = 0;
                    term->cursor_y = 0;
                } else {
                    fprintf(stderr, "Failed to allocate alternate screen buffer\n");
                }
            }
        }
    }
}

static void csi_l_private(Terminal* term)
{
    for (int i = 0; i < term->csi_param_count; ++i) {
        if (term->csi_params[i] == 1) {
            term->application_cursor_keys_mode = false;
        }
        if (term->csi_params[i] == 6) {
            term->origin_mode = false;
            term->cursor_x = 0;
            term->cursor_y = 0;
        }
        if (term->csi_params[i] == 7) {
            term->autowrap_mode = false;
        }
        if (term->csi_params[i] == 66) {
            term->application_keypad_mode = false;
        }
        if (term->csi_params[i] == 25) {
            term->cursor_visible = false;
        }

        if (term->csi_params[i] == 1049) {
            if (term->alt_screen_active) {
                term->alt_screen_active = false;
                term->cursor_x = term->normal_saved_cursor_x;
                term->cursor_y = term->normal_saved_cursor_y;
                term->full_redraw_needed = true;
            }
        }
    }
}

// --- CSI Public Mode Handlers ---

static void csi_A(Terminal* term)   // Cursor Up
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    term->cursor_y = SDL_max(term->scroll_top - 1, term->cursor_y - p1);
}

static void csi_B(Terminal* term)   // Cursor Down
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    term->cursor_y = SDL_min(term->scroll_bottom - 1, term->cursor_y + p1);
}

static void csi_C(Terminal* term)   // Cursor Forward
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    term->cursor_x = SDL_min(term->cols - 1, term->cursor_x + p1);
}

static void csi_D(Terminal* term)   // Cursor Back
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    term->cursor_x = SDL_max(0, term->cursor_x - p1);
    term->cursor_x = SDL_min(term->cols - 1, term->cursor_x);
}

static void csi_G(Terminal* term)   // Cursor Horizontal Absolute
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    term->cursor_x = SDL_min(term->cols, p1 - 1);
    term->cursor_x = SDL_max(0, term->cursor_x);
    term->cursor_x = SDL_min(term->cols - 1, term->cursor_x);
}

static void csi_d(Terminal* term)   // Vertical Line Position Absolute
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    term->cursor_y = SDL_min(term->rows - 1, p1 - 1);
    term->cursor_y = SDL_max(0, term->cursor_y);
}

static void csi_H(Terminal* term)   // Cursor Position
{
    int p1 = term->csi_params[0];
    int p2 = term->csi_params[1];
    if (p1 == 0) {
        p1 = 1;
    }
    if (p2 == 0) {
        p2 = 1;
    }

    if (term->origin_mode) {
        term->cursor_y = (p1 - 1) + (term->scroll_top - 1);
        term->cursor_x = p2 - 1;
        term->cursor_y = SDL_max(term->scroll_top - 1, SDL_min(term->scroll_bottom - 1, term->cursor_y));
        term->cursor_x = SDL_max(0, SDL_min(term->cols - 1, term->cursor_x));
    } else {
        term->cursor_y = p1 - 1;
        term->cursor_x = p2 - 1;
        term->cursor_y = SDL_max(0, SDL_min(term->rows - 1, term->cursor_y));
        term->cursor_x = SDL_max(0, SDL_min(term->cols - 1, term->cursor_x));
    }
}

static void csi_J(Terminal* term)   // Erase in Display
{
    int p1 = term->csi_params[0];
    if (p1 == 2) {
        terminal_clear_visible_screen(term);
    } else if (p1 == 0) {
        terminal_clear_line(term, term->cursor_y, term->cursor_x);
        for (int y = term->cursor_y + 1; y < term->rows; ++y) {
            terminal_clear_line(term, y, 0);
        }
    } else if (p1 == 1) {
        for (int y = 0; y < term->cursor_y; ++y) {
            terminal_clear_line(term, y, 0);
        }
        terminal_clear_line_to_cursor(term, term->cursor_y, term->cursor_x);
    }
}

static void csi_K(Terminal* term)   // Erase in Line
{
    int p1 = term->csi_params[0];
    if (p1 == 0) {
        terminal_clear_line(term, term->cursor_y, term->cursor_x);
    } else if (p1 == 1) {
        terminal_clear_line_to_cursor(term, term->cursor_y, term->cursor_x);
    } else if (p1 == 2) {
        terminal_clear_line(term, term->cursor_y, 0);
    }
}

static void csi_c(Terminal* term)   // Device Attributes
{
    if (term->csi_params[0] == 0 && term->response_len == 0) {
        term->response_len = snprintf(term->response_buffer, RESPONSE_BUFFER_SIZE, "\x1b[?1;2c");
    }
}

static void csi_n(Terminal* term)   // Device Status Report
{
    if (term->csi_params[0] == 6 && term->response_len == 0) {
        term->response_len = snprintf(term->response_buffer, RESPONSE_BUFFER_SIZE, "\x1b[%d;%dR", term->cursor_y + 1, term->cursor_x + 1);
    }
}

static void csi_m(Terminal* term)   // Select Graphic Rendition
{
    if (term->csi_param_count == 1 && term->csi_params[0] == 0) {
        term->current_fg = term->default_fg;
        term->current_bg = term->default_bg;
        term->current_attributes = 0;
    } else {
        sgr_to_color(term);
    }
}

static void csi_h(Terminal* term)   // Set Mode
{
    for (int i = 0; i < term->csi_param_count; ++i) {
        if (term->csi_params[i] == 4) {
            term->insert_mode = true;
        }
    }
}

static void csi_l(Terminal* term)   // Reset Mode
{
    for (int i = 0; i < term->csi_param_count; ++i) {
        if (term->csi_params[i] == 4) {
            term->insert_mode = false;
        }
    }
}

static void csi_q(Terminal* term)   // Set cursor style (DECSCUSR)
{
    if (strcmp(term->csi_intermediate_chars, " ") == 0) {
        int style = (term->csi_param_count > 0) ? term->csi_params[0] : 1;
        const struct {
            CursorStyle s;
            bool b;
        } map[] = {
            [0] = {CURSOR_STYLE_BLOCK, true}, [1] = {CURSOR_STYLE_BLOCK, true},
            [2] = {CURSOR_STYLE_BLOCK, false}, [3] = {CURSOR_STYLE_UNDERLINE, true},
            [4] = {CURSOR_STYLE_UNDERLINE, false}, [5] = {CURSOR_STYLE_BAR, true},
            [6] = {CURSOR_STYLE_BAR, false}
        };
        if (style >= 0 && style <= 6) {
            term->cursor_style = map[style].s;
            term->cursor_style_blinking = map[style].b;
        }
    }
}

static void csi_s(Terminal* term)   // Save Cursor Position
{
    term->saved_cursor_x = term->cursor_x;
    term->saved_cursor_y = term->cursor_y;
}

static void csi_u(Terminal* term)   // Restore Cursor Position
{
    term->cursor_x = term->saved_cursor_x;
    term->cursor_y = term->saved_cursor_y;
}

static void csi_r(Terminal* term)   // Set Scrolling Region
{
    int top = (term->csi_param_count > 0 && term->csi_params[0] > 0) ? term->csi_params[0] : 1;
    int bottom = (term->csi_param_count > 1 && term->csi_params[1] > 0) ? term->csi_params[1] : term->rows;

    if (top < bottom && bottom <= term->rows) {
        term->scroll_top = top;
        term->scroll_bottom = bottom;
        term->cursor_x = 0;
        term->cursor_y = 0;
    }
}

static void csi_at(Terminal* term)   // Insert Characters
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    terminal_insert_chars(term, p1);
}

static void csi_L(Terminal* term)   // Insert Lines
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    if (term->cursor_y >= (term->scroll_top - 1) && term->cursor_y < term->scroll_bottom) {
        terminal_scroll_region(term, term->cursor_y, term->scroll_bottom - 1, -p1);
    }
}

static void csi_M(Terminal* term)   // Delete Lines
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    if (term->cursor_y >= (term->scroll_top - 1) && term->cursor_y < term->scroll_bottom) {
        terminal_scroll_region(term, term->cursor_y, term->scroll_bottom - 1, p1);
    }
}

static void csi_P(Terminal* term)   // Delete Characters
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    terminal_delete_chars(term, p1);
}

static void csi_S(Terminal* term)   // Scroll Up
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    terminal_scroll_region(term, term->scroll_top - 1, term->scroll_bottom - 1, p1);
}

static void csi_T(Terminal* term)   // Scroll Down
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    terminal_scroll_region(term, term->scroll_top - 1, term->scroll_bottom - 1, -p1);
}

static void csi_X(Terminal* term)   // Erase Characters
{
    int p1 = (term->csi_params[0] == 0) ? 1 : term->csi_params[0];
    terminal_erase_chars(term, p1);
}

static void csi_t(Terminal* term)   // Window Manipulation
{
    if (term->csi_params[0] == 18 && term->response_len == 0) {
        term->response_len = snprintf(term->response_buffer, RESPONSE_BUFFER_SIZE, "\x1b[8;%d;%dt", term->rows, term->cols);
    }
}

/**
 * @brief Handles CSI (Control Sequence Introducer) sequences by dispatching to a handler.
 */
void handle_csi(Terminal* term, char command)
{
    typedef void (*CsiHandler)(Terminal* term);
    struct CsiCommand {
        char cmd;
        char private_marker;
        CsiHandler handler;
    };

    static const struct CsiCommand csi_commands[] = {
        {'h', '?', csi_h_private}, {'l', '?', csi_l_private},
        {'A', 0, csi_A}, {'B', 0, csi_B}, {'C', 0, csi_C}, {'D', 0, csi_D},
        {'G', 0, csi_G}, {'d', 0, csi_d}, {'H', 0, csi_H}, {'f', 0, csi_H},
        {'J', 0, csi_J}, {'K', 0, csi_K}, {'c', 0, csi_c}, {'n', 0, csi_n},
        {'m', 0, csi_m}, {'h', 0, csi_h}, {'l', 0, csi_l}, {'q', 0, csi_q},
        {'s', 0, csi_s}, {'u', 0, csi_u}, {'r', 0, csi_r}, {'@', 0, csi_at},
        {'L', 0, csi_L}, {'M', 0, csi_M}, {'P', 0, csi_P}, {'S', 0, csi_S},
        {'T', 0, csi_T}, {'^', 0, csi_T}, {'X', 0, csi_X}, {'t', 0, csi_t},
    };

    for (size_t i = 0; i < sizeof(csi_commands) / sizeof(csi_commands[0]); ++i) {
        if (csi_commands[i].cmd == command && csi_commands[i].private_marker == term->csi_private_marker) {
            csi_commands[i].handler(term);
            return;
        }
    }

    fprintf(stderr, "Debug: Unhandled CSI command '%c' (private marker: %c)\n", command, term->csi_private_marker ? term->csi_private_marker : ' ');
}

/**
 * @brief Parses extended color sequences for SGR (38/48).
 */
static int sgr_parse_extended_color(Terminal* term, int start_idx, bool is_fg)
{
    if (start_idx + 1 >= term->csi_param_count) {
        return 0;
    }

    int color_mode = term->csi_params[start_idx + 1];
    SDL_Color* target_color = is_fg ? &term->current_fg : &term->current_bg;

    if (color_mode == 5) {
        if (start_idx + 2 < term->csi_param_count) {
            int color_idx = term->csi_params[start_idx + 2];
            if (color_idx >= 0 && color_idx <= 255) {
                *target_color = term->xterm_colors[color_idx];
            }
            return 2;
        }
    } else if (color_mode == 2) {
        if (start_idx + 4 < term->csi_param_count) {
            int r = term->csi_params[start_idx + 2];
            int g = term->csi_params[start_idx + 3];
            int b = term->csi_params[start_idx + 4];
            *target_color = (SDL_Color) {
                (Uint8)r, (Uint8)g, (Uint8)b, 255
            };
            return 4;
        }
    }
    return 0;
}

void sgr_to_color(Terminal* term)
{
    for (int i = 0; i < term->csi_param_count; ++i) {
        int code = term->csi_params[i];

        switch (code) {
        case 0:
            term->current_fg = term->default_fg;
            term->current_bg = term->default_bg;
            term->current_attributes = 0;
            break;
        case 1:
            term->current_attributes |= ATTR_BOLD;
            break;
        case 3:
            term->current_attributes |= ATTR_ITALIC;
            break;
        case 4:
            term->current_attributes |= ATTR_UNDERLINE;
            break;
        case 5:
            term->current_attributes |= ATTR_BLINK;
            break;
        case 7:
            term->current_attributes |= ATTR_INVERSE;
            break;
        case 22:
            term->current_attributes &= ~ATTR_BOLD;
            break;
        case 23:
            term->current_attributes &= ~ATTR_ITALIC;
            break;
        case 24:
            term->current_attributes &= ~ATTR_UNDERLINE;
            break;
        case 25:
            term->current_attributes &= ~ATTR_BLINK;
            break;
        case 27:
            term->current_attributes &= ~ATTR_INVERSE;
            break;

        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            term->current_fg = term->colors[code - 30];
            break;
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            term->current_bg = term->colors[code - 40];
            break;
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            term->current_fg = term->colors[code - 90 + 8];
            break;
        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            term->current_bg = term->colors[code - 100 + 8];
            break;

        case 38:
            i += sgr_parse_extended_color(term, i, true);
            break;
        case 39:
            term->current_fg = term->default_fg;
            break;
        case 48:
            i += sgr_parse_extended_color(term, i, false);
            break;
        case 49:
            term->current_bg = term->default_bg;
            break;
        }
    }
}

void handle_osc(Terminal* term)
{
    term->osc_buffer[term->osc_len] = '\0';

    if (strncmp(term->osc_buffer, "4;", 2) != 0) {
        return;
    }

    char* spec_str;
    long color_index = strtol(term->osc_buffer + 2, &spec_str, 10);

    if (spec_str == term->osc_buffer + 2 || *spec_str != ';') {
        return;
    }
    spec_str++;

    if (color_index < 0 || color_index > 15) {
        return;
    }

    unsigned int r, g, b;
    if (sscanf(spec_str, "rgb:%x/%x/%x", &r, &g, &b) == 3) {
        term->colors[color_index] = (SDL_Color) {
            (Uint8)r, (Uint8)g, (Uint8)b, 255
        };
    } else if (spec_str[0] == '#' && strlen(spec_str) == 7) {
        unsigned int val;
        if (sscanf(spec_str + 1, "%x", &val) == 1) {
            r = (val >> 16) & 0xFF;
            g = (val >> 8) & 0xFF;
            b = val & 0xFF;
            term->colors[color_index] = (SDL_Color) {
                (Uint8)r, (Uint8)g, (Uint8)b, 255
            };
        }
    }

    term->xterm_colors[color_index] = term->colors[color_index];
}

void terminal_init_xterm_colors(Terminal* term)
{
    for (int i = 0; i < 16; ++i) {
        term->xterm_colors[i] = term->colors[i];
    }

    int levels[] = {0, 95, 135, 175, 215, 255};
    int idx = 16;
    for (int r = 0; r < 6; ++r) {
        for (int g = 0; g < 6; ++g) {
            for (int b = 0; b < 6; ++b) {
                term->xterm_colors[idx++] = (SDL_Color) {
                    (Uint8)levels[r], (Uint8)levels[g], (Uint8)levels[b], 255
                };
            }
        }
    }

    for (int i = 0; i < 24; ++i) {
        int gray = 8 + i * 10;
        term->xterm_colors[idx++] = (SDL_Color) {
            (Uint8)gray, (Uint8)gray, (Uint8)gray, 255
        };
    }
}

/**
 * @brief Handles incoming data from the PTY, parsing ANSI escape codes and updating terminal state.
 */
void terminal_handle_input(Terminal* term, const char* buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = buf[i];
        switch (term->parse_state) {
        case STATE_NORMAL:
            if (term->utf8_bytes_to_read > 0) {
                if ((c & 0xC0) == 0x80) {
                    term->utf8_codepoint = (term->utf8_codepoint << 6) | (c & 0x3F);
                    term->utf8_bytes_to_read--;
                    if (term->utf8_bytes_to_read == 0) {
                        terminal_put_char(term, term->utf8_codepoint);
                    }
                } else {
                    fprintf(stderr, "Debug: Invalid UTF-8 continuation byte: 0x%02x\n", c);
                    term->utf8_bytes_to_read = 0;
                    i--;
                }
            } else if (c < 0x80) {
                switch (c) {
                case '\x1b':
                    term->parse_state = STATE_ESCAPE;
                    break;
                case '\x0e':
                    term->active_charset = 1;
                    break;
                case '\x0f':
                    term->active_charset = 0;
                    break;
                case '\n':
                    terminal_newline(term);
                    break;
                case '\r':
                    term->cursor_x = 0;
                    break;
                case '\b':
                    term->cursor_x = SDL_max(0, term->cursor_x - 1);
                    break;
                case '\t':
                    term->cursor_x = (term->cursor_x + 8) & ~7;
                    if (term->cursor_x >= term->cols) {
                        term->cursor_x = 0;
                        terminal_newline(term);
                    }
                    break;
                default:
                    if (c >= ' ') {
                        terminal_put_char(term, c);
                    }
                    break;
                }
            } else if ((c & 0xE0) == 0xC0) {
                term->utf8_bytes_to_read = 1;
                term->utf8_codepoint = c & 0x1F;
            } else if ((c & 0xF0) == 0xE0) {
                term->utf8_bytes_to_read = 2;
                term->utf8_codepoint = c & 0x0F;
            } else if ((c & 0xF8) == 0xF0) {
                term->utf8_bytes_to_read = 3;
                term->utf8_codepoint = c & 0x07;
            } else {
                fprintf(stderr, "Debug: Invalid UTF-8 start byte: 0x%02x\n", c);
            }
            break;

        case STATE_ESCAPE:
            term->utf8_bytes_to_read = 0;
            if (c == '\x1b') {
                break;
            }
            if (c == '[') {
                term->parse_state = STATE_CSI;
                term->csi_param_count = 0;
                term->csi_private_marker = 0;
                term->csi_intermediate_count = 0;
                term->csi_intermediate_chars[0] = '\0';
                memset(term->csi_params, 0, sizeof(term->csi_params));
            } else if (c == '(' || c == ')') {
                if (i + 1 < len) {
                    char charset_id = buf[i + 1];
                    if (charset_id == 'B' || charset_id == '0') {
                        int charset_idx = (c == ')') ? 1 : 0;
                        term->charsets[charset_idx] = charset_id;
                    }
                    i++;
                }
                term->parse_state = STATE_NORMAL;
            } else if (c == '7') {
                term->saved_cursor_x = term->cursor_x;
                term->saved_cursor_y = term->cursor_y;
                term->parse_state = STATE_NORMAL;
            } else if (c == '8') {
                term->cursor_x = term->saved_cursor_x;
                term->cursor_y = term->saved_cursor_y;
                term->parse_state = STATE_NORMAL;
            } else if (c == '#') {
                if (i + 1 < len && buf[i + 1] == '8') {
                    for (int y = 0; y < term->rows; ++y) {
                        for (int x = 0; x < term->cols; ++x) {
                            Glyph* line_ptr = get_line(term, y);
                            if (line_ptr) {
                                line_ptr[x] = (Glyph) {
                                    'E', term->default_fg, term->default_bg, 0
                                };
                            }
                        }
                    }
                    term->full_redraw_needed = true;
                    i++;
                }
                term->parse_state = STATE_NORMAL;
            } else if (c == 'P') {
                term->parse_state = STATE_DCS;
            } else if (c == '\\') {
                term->parse_state = STATE_NORMAL;
            } else if (c == '<') {
                term->parse_state = STATE_NORMAL;
            } else if (c == ']') {
                term->parse_state = STATE_OSC;
                term->osc_len = 0;
            } else if (c == 'D') {
                terminal_newline(term);
                term->parse_state = STATE_NORMAL;
            } else if (c == 'M') {
                term->cursor_y--;
                if (term->cursor_y < (term->scroll_top - 1)) {
                    term->cursor_y = term->scroll_top - 1;
                    terminal_scroll_region(term, term->scroll_top - 1, term->scroll_bottom - 1, -1);
                }
                term->parse_state = STATE_NORMAL;
            } else if (c == '=') {
                term->application_keypad_mode = true;
                term->parse_state = STATE_NORMAL;
            } else if (c == '>') {
                term->application_keypad_mode = false;
                term->parse_state = STATE_NORMAL;
            } else {
                if (c == 'c') {
                    terminal_reset(term);
                    break;
                }
                fprintf(stderr, "Debug: Unhandled ESC sequence: ESC %c (0x%02x)\n", isprint(c) ? c : '?', c);
                term->parse_state = STATE_NORMAL;
            }
            break;

        case STATE_OSC:
            term->utf8_bytes_to_read = 0;
            if (c == '\x07') {
                handle_osc(term);
                term->parse_state = STATE_NORMAL;
            } else if (c == '\x1b') {
                handle_osc(term);
                term->parse_state = STATE_ESCAPE;
            } else if (isprint((unsigned char)c) || c == ';') {
                if ((size_t)term->osc_len < sizeof(term->osc_buffer) - 1) {
                    term->osc_buffer[term->osc_len++] = c;
                }
            } else {
                fprintf(stderr, "Debug: Aborting OSC sequence due to unexpected character: 0x%02x\n", c);
                term->parse_state = STATE_NORMAL;
            }
            break;

        case STATE_DCS:
            if (c == '\x1b') {
                term->parse_state = STATE_ESCAPE;
            }
            break;

        case STATE_CSI:
            term->utf8_bytes_to_read = 0;
            if (c == '\x1b') {
                term->parse_state = STATE_ESCAPE;
                break;
            }
            if (isdigit((unsigned char)c)) {
                if (term->csi_param_count == 0) {
                    term->csi_param_count = 1;
                }
                if (term->csi_param_count > 0 && term->csi_param_count <= CSI_MAX_PARAMS) {
                    term->csi_params[term->csi_param_count - 1] = term->csi_params[term->csi_param_count - 1] * 10 + (c - '0');
                }
            } else if (c == ';') {
                if (term->csi_param_count == 0) {
                    term->csi_param_count = 1;
                }
                if (term->csi_param_count < CSI_MAX_PARAMS) {
                    term->csi_param_count++;
                    term->csi_params[term->csi_param_count - 1] = 0;
                } else {
                }
            } else if (c >= '<' && c <= '?') {
                term->csi_private_marker = c;
            } else if (c >= 0x20 && c <= 0x2F) {
                if (term->csi_intermediate_count < (int)sizeof(term->csi_intermediate_chars) - 1) {
                    term->csi_intermediate_chars[term->csi_intermediate_count++] = c;
                    term->csi_intermediate_chars[term->csi_intermediate_count] = '\0';
                }
            } else if (c >= '@' && c <= '~') {
                if (term->csi_param_count == 0) {
                    term->csi_param_count = 1;
                }
                handle_csi(term, c);
                term->parse_state = STATE_NORMAL;
            } else {
                i--;
                fprintf(stderr, "Debug: Unhandled character in CSI state: '%c' (0x%02x)\n", isprint(c) ? c : '?', c);
                term->parse_state = STATE_NORMAL;
            }
            break;
        }
    }
}
