#ifndef TERMINAL_H
#define TERMINAL_H

#include "terminal_state.h"

// --- Terminal Lifecycle ---
/**
 * @brief Creates and initializes a new Terminal instance.
 * @param cols The initial number of columns.
 * @param rows The initial number of rows.
 * @param config A pointer to the application configuration.
 * @param renderer The SDL renderer to use for texture creation (e.g., background).
 * @return A pointer to the new Terminal, or NULL on failure.
 */
Terminal* terminal_create(int cols, int rows, const Config* config, SDL_Renderer* renderer);
void terminal_init_xterm_colors(Terminal* term);
void terminal_destroy(Terminal* term);
void terminal_reset(Terminal* term);
void terminal_resize(Terminal* term, int new_cols, int new_rows);
void terminal_reload_background_texture(Terminal* term, SDL_Renderer* renderer, const char* path);

// --- Terminal Grid Operations ---
void terminal_clear_line_to_cursor(Terminal* term, int y, int end_x);
void terminal_clear_line(Terminal* term, int y, int start_x);
void terminal_clear_visible_screen(Terminal* term);
void terminal_scroll_region(Terminal* term, int top_y, int bottom_y, int n_lines);
void terminal_insert_chars(Terminal* term, int n);
void terminal_delete_chars(Terminal* term, int n);
void terminal_erase_chars(Terminal* term, int n);
void terminal_newline(Terminal* term);
void terminal_scroll_up(Terminal* term);
void terminal_put_char(Terminal* term, uint32_t c);
Glyph* terminal_get_view_line(Terminal* term, int y);

// --- ANSI Parser ---
void terminal_handle_input(Terminal* term, const char* buf, size_t len);
void terminal_load_colorscheme(Terminal* term, const char* path);
void handle_csi(Terminal* term, char command);
void handle_osc(Terminal* term);
void sgr_to_color(Terminal* term);

#endif // TERMINAL_H
