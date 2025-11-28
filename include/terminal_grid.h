/**
 * @file terminal_grid.h
 * @brief Terminal grid operations for character and attribute management.
 * 
 * Provides functions for manipulating the terminal grid including
 * character insertion/deletion, line operations, and scrolling.
 */

#ifndef TERMINAL_GRID_H
#define TERMINAL_GRID_H

#include "terminal_state.h"

/**
 * @brief Clear a line from cursor to end
 * @param term Terminal state
 * @param y Line number
 * @param end_x End column (inclusive)
 */
void terminal_grid_clear_line_to_cursor(Terminal* term, int y, int end_x);

/**
 * @brief Clear a line from start to cursor
 * @param term Terminal state
 * @param y Line number
 * @param start_x Start column
 */
void terminal_grid_clear_line_from_start(Terminal* term, int y, int start_x);

/**
 * @brief Clear entire line
 * @param term Terminal state
 * @param y Line number
 */
void terminal_grid_clear_line(Terminal* term, int y);

/**
 * @brief Clear visible screen
 * @param term Terminal state
 */
void terminal_grid_clear_visible_screen(Terminal* term);

/**
 * @brief Scroll region up or down
 * @param term Terminal state
 * @param top_y Top line of region
 * @param bottom_y Bottom line of region
 * @param n_lines Number of lines to scroll (positive = down, negative = up)
 */
void terminal_grid_scroll_region(Terminal* term, int top_y, int bottom_y, int n_lines);

/**
 * @brief Insert blank characters at cursor
 * @param term Terminal state
 * @param n Number of characters to insert
 */
void terminal_grid_insert_chars(Terminal* term, int n);

/**
 * @brief Delete characters at cursor
 * @param term Terminal state
 * @param n Number of characters to delete
 */
void terminal_grid_delete_chars(Terminal* term, int n);

/**
 * @brief Erase characters at cursor
 * @param term Terminal state
 * @param n Number of characters to erase
 */
void terminal_grid_erase_chars(Terminal* term, int n);

/**
 * @brief Get pointer to a line in the grid
 * @param term Terminal state
 * @param y Line number
 * @return Pointer to line, or NULL on error
 */
Glyph* terminal_grid_get_line(Terminal* term, int y);

/**
 * @brief Get a glyph from the grid
 * @param term Terminal state
 * @param x Column
 * @param y Row
 * @return Pointer to glyph, or NULL on error
 */
Glyph* terminal_grid_get_glyph(Terminal* term, int x, int y);

/**
 * @brief Mark a line as dirty for rendering
 * @param term Terminal state
 * @param y Line number
 */
void terminal_grid_mark_dirty(Terminal* term, int y);

/**
 * @brief Mark a region as dirty for rendering
 * @param term Terminal state
 * @param x1 Start column
 * @param y1 Start row
 * @param x2 End column
 * @param y2 End row
 */
void terminal_grid_mark_region_dirty(Terminal* term, int x1, int y1, int x2, int y2);

/**
 * @brief Clear all dirty marks
 * @param term Terminal state
 */
void terminal_grid_clear_dirty_marks(Terminal* term);

#endif // TERMINAL_GRID_H
