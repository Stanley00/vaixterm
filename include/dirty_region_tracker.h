#ifndef DIRTY_REGION_TRACKER_H
#define DIRTY_REGION_TRACKER_H

#include "terminal_state.h"

/**
 * @brief Marks a line as dirty and updates dirty region bounds
 * @param term Terminal instance
 * @param y Line number to mark dirty
 */
void terminal_mark_line_dirty(Terminal* term, int y);

/**
 * @brief Marks a range of lines as dirty
 * @param term Terminal instance
 * @param start_y Start line (inclusive)
 * @param end_y End line (inclusive)
 */
void terminal_mark_lines_dirty(Terminal* term, int start_y, int end_y);

/**
 * @brief Clears all dirty line flags and resets dirty region tracking
 * @param term Terminal instance
 */
void terminal_clear_dirty_lines(Terminal* term);

/**
 * @brief Initializes dirty region tracking for a terminal
 * @param term Terminal instance
 */
void terminal_init_dirty_tracking(Terminal* term);

#endif // DIRTY_REGION_TRACKER_H
