/**
 * @file terminal_modes.h
 * @brief Terminal mode management (DECSET/DECRST sequences).
 * 
 * Handles VT100 mode setting and resetting operations.
 */

#ifndef TERMINAL_MODES_H
#define TERMINAL_MODES_H

#include "terminal_state.h"
#include <stdbool.h>

/**
 * @brief Set a terminal mode
 * @param term Terminal state
 * @param mode Mode number
 * @param is_private Whether this is a private mode (DEC mode)
 * @return true on success, false if mode not recognized
 */
bool terminal_modes_set(Terminal* term, int mode, bool is_private);

/**
 * @brief Reset a terminal mode
 * @param term Terminal state
 * @param mode Mode number
 * @param is_private Whether this is a private mode (DEC mode)
 * @return true on success, false if mode not recognized
 */
bool terminal_modes_reset(Terminal* term, int mode, bool is_private);

/**
 * @brief Set cursor key mode (DECCKM)
 * @param term Terminal state
 * @param enabled true for application mode, false for normal mode
 */
void terminal_modes_set_cursor_key_mode(Terminal* term, bool enabled);

/**
 * @brief Set autowrap mode (DECAWM)
 * @param term Terminal state
 * @param enabled true to enable autowrap, false to disable
 */
void terminal_modes_set_autowrap(Terminal* term, bool enabled);

/**
 * @brief Set text cursor enable mode (DECTCEM)
 * @param term Terminal state
 * @param enabled true to show cursor, false to hide
 */
void terminal_modes_set_cursor_visible(Terminal* term, bool enabled);

/**
 * @brief Set insert/replace mode (IRM)
 * @param term Terminal state
 * @param enabled true for insert mode, false for replace mode
 */
void terminal_modes_set_insert_mode(Terminal* term, bool enabled);

/**
 * @brief Set origin mode (DECOM)
 * @param term Terminal state
 * @param enabled true for origin mode, false for absolute mode
 */
void terminal_modes_set_origin_mode(Terminal* term, bool enabled);

/**
 * @brief Set alternate screen mode (DECSET 1049)
 * @param term Terminal state
 * @param enabled true to use alternate screen, false for normal
 */
void terminal_modes_set_alternate_screen(Terminal* term, bool enabled);

/**
 * @brief Set numeric keypad mode (DECNKM)
 * @param term Terminal state
 * @param enabled true for application mode, false for numeric
 */
void terminal_modes_set_keypad_mode(Terminal* term, bool enabled);

/**
 * @brief Get current mode state
 * @param term Terminal state
 * @param mode Mode number
 * @param is_private Whether this is a private mode
 * @return true if mode is set, false if not set or unknown
 */
bool terminal_modes_get(const Terminal* term, int mode, bool is_private);

#endif // TERMINAL_MODES_H
