/**
 * @file terminal_grid.c
 * @brief Terminal grid operations implementation.
 */

#include "terminal_grid.h"
#include "error_codes.h"
#include <string.h>

/* ===== Helper Functions ===== */

/**
 * @brief Get pointer to a line in the grid
 */
static Glyph* get_line(Terminal* term, int y) {
    if (!term || y < 0 || y >= term->rows) {
        return NULL;
    }
    
    if (term->alt_screen_active) {
        return term->alt_grid ? &term->alt_grid[y * term->cols] : NULL;
    } else {
        int phys_y = (term->top_line + y) % term->total_lines;
        return &term->grid[phys_y * term->cols];
    }
}

/* ===== Public Functions ===== */

void terminal_grid_clear_line_to_cursor(Terminal* term, int y, int end_x) {
    Glyph* line = get_line(term, y);
    if (!line) return;

    Glyph blank = {
        .character = ' ',
        .fg = term->current_fg,
        .bg = term->current_bg,
        .attributes = term->current_attributes,
        .width = 1
    };

    for (int x = 0; x <= end_x && x < term->cols; x++) {
        line[x] = blank;
    }
    terminal_grid_mark_dirty(term, y);
}

void terminal_grid_clear_line_from_start(Terminal* term, int y, int start_x) {
    Glyph* line = get_line(term, y);
    if (!line) return;

    Glyph blank = {
        .character = ' ',
        .fg = term->current_fg,
        .bg = term->current_bg,
        .attributes = term->current_attributes,
        .width = 1
    };

    for (int x = start_x; x < term->cols; x++) {
        line[x] = blank;
    }
    terminal_grid_mark_dirty(term, y);
}

void terminal_grid_clear_line(Terminal* term, int y) {
    Glyph* line = get_line(term, y);
    if (!line) return;

    Glyph blank = {
        .character = ' ',
        .fg = term->default_fg,
        .bg = term->default_bg,
        .attributes = 0,
        .width = 1
    };

    for (int x = 0; x < term->cols; x++) {
        line[x] = blank;
    }
    terminal_grid_mark_dirty(term, y);
}

void terminal_grid_clear_visible_screen(Terminal* term) {
    if (!term) return;

    for (int y = 0; y < term->rows; y++) {
        terminal_grid_clear_line(term, y);
    }
    term->cursor_x = 0;
    term->cursor_y = 0;
}

void terminal_grid_scroll_region(Terminal* term, int top_y, int bottom_y, int n_lines) {
    (void)top_y;
    (void)bottom_y;
    if (!term || n_lines == 0) return;

    if (n_lines > 0) {
        // Scroll down (delete lines from top)
        for (int y = top_y; y <= bottom_y - n_lines; y++) {
            Glyph* src = get_line(term, y + n_lines);
            Glyph* dst = get_line(term, y);
            if (src && dst) {
                memcpy(dst, src, sizeof(Glyph) * term->cols);
            }
        }
        // Clear bottom lines
        for (int y = bottom_y - n_lines + 1; y <= bottom_y; y++) {
            terminal_grid_clear_line(term, y);
        }
    } else {
        // Scroll up (insert lines at top)
        n_lines = -n_lines;
        for (int y = bottom_y; y >= top_y + n_lines; y--) {
            Glyph* src = get_line(term, y - n_lines);
            Glyph* dst = get_line(term, y);
            if (src && dst) {
                memcpy(dst, src, sizeof(Glyph) * term->cols);
            }
        }
        // Clear top lines
        for (int y = top_y; y < top_y + n_lines; y++) {
            terminal_grid_clear_line(term, y);
        }
    }

    // Mark region as dirty
    for (int y = top_y; y <= bottom_y; y++) {
        terminal_grid_mark_dirty(term, y);
    }
}

void terminal_grid_insert_chars(Terminal* term, int n) {
    if (!term || n <= 0) return;

    Glyph* line = get_line(term, term->cursor_y);
    if (!line) return;

    // Shift characters to the right
    for (int x = term->cols - 1; x >= term->cursor_x + n; x--) {
        line[x] = line[x - n];
    }

    // Insert blanks
    Glyph blank = {
        .character = ' ',
        .fg = term->current_fg,
        .bg = term->current_bg,
        .attributes = 0,
        .width = 1
    };

    for (int x = term->cursor_x; x < term->cursor_x + n && x < term->cols; x++) {
        line[x] = blank;
    }

    terminal_grid_mark_dirty(term, term->cursor_y);
}

void terminal_grid_delete_chars(Terminal* term, int n) {
    if (!term || n <= 0) return;

    Glyph* line = get_line(term, term->cursor_y);
    if (!line) return;

    // Shift characters to the left
    for (int x = term->cursor_x; x < term->cols - n; x++) {
        line[x] = line[x + n];
    }

    // Fill with blanks
    Glyph blank = {
        .character = ' ',
        .fg = term->current_fg,
        .bg = term->current_bg,
        .attributes = 0,
        .width = 1
    };

    for (int x = term->cols - n; x < term->cols; x++) {
        line[x] = blank;
    }

    terminal_grid_mark_dirty(term, term->cursor_y);
}

void terminal_grid_erase_chars(Terminal* term, int n) {
    if (!term || n <= 0) return;

    Glyph* line = get_line(term, term->cursor_y);
    if (!line) return;

    Glyph blank = {
        .character = ' ',
        .fg = term->current_fg,
        .bg = term->current_bg,
        .attributes = 0,
        .width = 1
    };

    for (int x = term->cursor_x; x < term->cursor_x + n && x < term->cols; x++) {
        line[x] = blank;
    }

    terminal_grid_mark_dirty(term, term->cursor_y);
}

Glyph* terminal_grid_get_line(Terminal* term, int y) {
    return get_line(term, y);
}

Glyph* terminal_grid_get_glyph(Terminal* term, int x, int y) {
    Glyph* line = get_line(term, y);
    if (!line || x < 0 || x >= term->cols) {
        return NULL;
    }
    return &line[x];
}

void terminal_grid_mark_dirty(Terminal* term, int y) {
    if (!term || y < 0 || y >= term->rows) return;

    if (term->dirty_lines) {
        term->dirty_lines[y] = true;
    }

    if (term->dirty_min_y == -1 || y < term->dirty_min_y) {
        term->dirty_min_y = y;
    }
    if (y > term->dirty_max_y) {
        term->dirty_max_y = y;
    }
    term->has_dirty_regions = true;
}

void terminal_grid_mark_region_dirty(Terminal* term, int x1, int y1, int x2, int y2) {
    (void)x1;
    (void)x2;
    if (!term) return;

    for (int y = y1; y <= y2 && y < term->rows; y++) {
        terminal_grid_mark_dirty(term, y);
    }
}

void terminal_grid_clear_dirty_marks(Terminal* term) {
    if (!term) return;

    if (term->dirty_lines) {
        memset(term->dirty_lines, 0, sizeof(bool) * term->rows);
    }
    term->dirty_min_y = -1;
    term->dirty_max_y = -1;
    term->has_dirty_regions = false;
}
