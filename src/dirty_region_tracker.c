#include "terminal_state.h"
#include <string.h>

/**
 * @brief Marks a line as dirty and updates dirty region bounds
 */
void terminal_mark_line_dirty(Terminal* term, int y) {
    if (y < 0 || y >= term->rows) {
        return;
    }
    
    if (!term->dirty_lines[y]) {
        term->dirty_lines[y] = true;
        
        // Update dirty region bounds
        if (!term->has_dirty_regions) {
            term->dirty_min_y = y;
            term->dirty_max_y = y;
            term->has_dirty_regions = true;
        } else {
            if (y < term->dirty_min_y) {
                term->dirty_min_y = y;
            }
            if (y > term->dirty_max_y) {
                term->dirty_max_y = y;
            }
        }
    }
}

/**
 * @brief Marks a range of lines as dirty
 */
void terminal_mark_lines_dirty(Terminal* term, int start_y, int end_y) {
    if (start_y < 0) start_y = 0;
    if (end_y >= term->rows) end_y = term->rows - 1;
    
    for (int y = start_y; y <= end_y; y++) {
        terminal_mark_line_dirty(term, y);
    }
}

/**
 * @brief Clears all dirty line flags and resets dirty region tracking
 */
void terminal_clear_dirty_lines(Terminal* term) {
    if (term->has_dirty_regions) {
        memset(term->dirty_lines, 0, sizeof(bool) * (size_t)term->rows);
        term->has_dirty_regions = false;
        term->dirty_min_y = -1;
        term->dirty_max_y = -1;
    }
}

/**
 * @brief Initializes dirty region tracking for a terminal
 */
void terminal_init_dirty_tracking(Terminal* term) {
    term->has_dirty_regions = false;
    term->dirty_min_y = -1;
    term->dirty_max_y = -1;
    term->skip_render_frame = false;
    term->last_render_time = 0;
}
