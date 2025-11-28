/**
 * @file terminal_modes.c
 * @brief Terminal mode management implementation.
 */

#include "terminal_modes.h"
#include "error_codes.h"

/* ===== Public Functions ===== */

bool terminal_modes_set(Terminal* term, int mode, bool is_private) {
    if (!term) return false;

    if (is_private) {
        // DEC private modes
        switch (mode) {
        case 1:
            terminal_modes_set_cursor_key_mode(term, true);
            return true;
        case 7:
            terminal_modes_set_autowrap(term, true);
            return true;
        case 25:
            terminal_modes_set_cursor_visible(term, true);
            return true;
        case 1049:
            terminal_modes_set_alternate_screen(term, true);
            return true;
        default:
            DEBUG_LOG("Unknown DEC private mode: %d", mode);
            return false;
        }
    } else {
        // Standard modes
        switch (mode) {
        case 4:
            terminal_modes_set_insert_mode(term, true);
            return true;
        default:
            DEBUG_LOG("Unknown standard mode: %d", mode);
            return false;
        }
    }
}

bool terminal_modes_reset(Terminal* term, int mode, bool is_private) {
    if (!term) return false;

    if (is_private) {
        // DEC private modes
        switch (mode) {
        case 1:
            terminal_modes_set_cursor_key_mode(term, false);
            return true;
        case 7:
            terminal_modes_set_autowrap(term, false);
            return true;
        case 25:
            terminal_modes_set_cursor_visible(term, false);
            return true;
        case 1049:
            terminal_modes_set_alternate_screen(term, false);
            return true;
        default:
            DEBUG_LOG("Unknown DEC private mode: %d", mode);
            return false;
        }
    } else {
        // Standard modes
        switch (mode) {
        case 4:
            terminal_modes_set_insert_mode(term, false);
            return true;
        default:
            DEBUG_LOG("Unknown standard mode: %d", mode);
            return false;
        }
    }
}

void terminal_modes_set_cursor_key_mode(Terminal* term, bool enabled) {
    if (term) {
        term->application_cursor_keys_mode = enabled;
        DEBUG_LOG("Cursor key mode: %s", enabled ? "application" : "normal");
    }
}

void terminal_modes_set_autowrap(Terminal* term, bool enabled) {
    if (term) {
        term->autowrap_mode = enabled;
        DEBUG_LOG("Autowrap mode: %s", enabled ? "enabled" : "disabled");
    }
}

void terminal_modes_set_cursor_visible(Terminal* term, bool enabled) {
    if (term) {
        term->cursor_visible = enabled;
        DEBUG_LOG("Cursor visible: %s", enabled ? "yes" : "no");
    }
}

void terminal_modes_set_insert_mode(Terminal* term, bool enabled) {
    if (term) {
        term->insert_mode = enabled;
        DEBUG_LOG("Insert mode: %s", enabled ? "insert" : "replace");
    }
}

void terminal_modes_set_origin_mode(Terminal* term, bool enabled) {
    if (term) {
        term->origin_mode = enabled;
        DEBUG_LOG("Origin mode: %s", enabled ? "relative" : "absolute");
    }
}

void terminal_modes_set_alternate_screen(Terminal* term, bool enabled) {
    if (!term) return;

    if (enabled && !term->alt_screen_active) {
        // Switch to alternate screen
        if (!term->alt_grid) {
            term->alt_grid = malloc(sizeof(Glyph) * term->cols * term->rows);
            if (!term->alt_grid) {
                ERROR_LOG("Failed to allocate alternate screen");
                return;
            }
            // Initialize with blanks
            Glyph blank = {' ', term->default_fg, term->default_bg, 0, 1, 0};
            for (int i = 0; i < term->cols * term->rows; i++) {
                term->alt_grid[i] = blank;
            }
        }
        term->alt_screen_active = true;
        term->cursor_x = 0;
        term->cursor_y = 0;
        DEBUG_LOG("Switched to alternate screen");
    } else if (!enabled && term->alt_screen_active) {
        // Switch back to normal screen
        term->alt_screen_active = false;
        term->cursor_x = 0;
        term->cursor_y = 0;
        DEBUG_LOG("Switched to normal screen");
    }
}

void terminal_modes_set_keypad_mode(Terminal* term, bool enabled) {
    if (term) {
        term->application_keypad_mode = enabled;
        DEBUG_LOG("Keypad mode: %s", enabled ? "application" : "numeric");
    }
}

bool terminal_modes_get(const Terminal* term, int mode, bool is_private) {
    if (!term) return false;

    if (is_private) {
        switch (mode) {
        case 1:
            return term->application_cursor_keys_mode;
        case 7:
            return term->autowrap_mode;
        case 25:
            return term->cursor_visible;
        case 1049:
            return term->alt_screen_active;
        default:
            return false;
        }
    } else {
        switch (mode) {
        case 4:
            return term->insert_mode;
        default:
            return false;
        }
    }
}
