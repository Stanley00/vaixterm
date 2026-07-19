#ifndef TERMINAL_H
#define TERMINAL_H

#include "terminal_state.h"

// --- Terminal Lifecycle ---
Terminal* terminal_create(int cols, int rows, const Config* config, SDL_Renderer* renderer);
void terminal_destroy(Terminal* term);
void terminal_reset(Terminal* term);
void terminal_resize(Terminal* term, int new_cols, int new_rows);

// --- Terminal Grid Operations ---
Glyph* terminal_get_view_line(Terminal* term, int y);

// --- ANSI Parser ---
void terminal_handle_input(Terminal* term, const char* buf, size_t len);
void terminal_load_colorscheme(Terminal* term, const char* path);
void sgr_to_color(Terminal* term, int color_index, SDL_Color* color);
int terminal_get_scrollback_count(Terminal* term);

#endif // TERMINAL_H
