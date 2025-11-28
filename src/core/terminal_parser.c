/**
 * @file terminal_parser.c
 * @brief ANSI/VT100 escape sequence parser implementation.
 */

#include "terminal_parser.h"
#include "terminal_grid.h"
#include "terminal_modes.h"
#include "error_codes.h"
#include <string.h>
#include <ctype.h>
#include <wchar.h>

/* ===== UTF-8 Decoding ===== */

/* ===== Parser State Machine ===== */

bool terminal_parser_process_input(Terminal* term, const char* buf, size_t len) {
    if (!term || !buf) {
        ERROR_LOG("Invalid arguments to terminal_parser_process_input");
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = buf[i];

        switch (term->parse_state) {
        case STATE_NORMAL:
            if (c == '\x1b') {  // ESC
                term->parse_state = STATE_ESCAPE;
            } else if (c == '\n' || c == '\v' || c == '\f') {
                terminal_grid_clear_line(term, term->cursor_y);
                term->cursor_x = 0;
                if (term->cursor_y < term->rows - 1) {
                    term->cursor_y++;
                } else {
                    terminal_grid_scroll_region(term, term->scroll_top, 
                                               term->scroll_bottom, 1);
                }
                terminal_grid_mark_dirty(term, term->cursor_y);
            } else if (c == '\r') {
                term->cursor_x = 0;
            } else if (c == '\b') {
                if (term->cursor_x > 0) {
                    term->cursor_x--;
                }
            } else if (c == '\t') {
                term->cursor_x = (term->cursor_x + 8) & ~7;
                if (term->cursor_x >= term->cols) {
                    term->cursor_x = term->cols - 1;
                }
            } else if (c >= 0x20 || c == 0x7F) {
                terminal_parser_put_char(term, c);
            }
            break;

        case STATE_ESCAPE:
            if (c == '[') {
                term->parse_state = STATE_CSI;
                term->csi_param_count = 0;
                memset(term->csi_params, 0, sizeof(term->csi_params));
                term->csi_private_marker = 0;
                term->csi_intermediate_count = 0;
            } else if (c == ']') {
                term->parse_state = STATE_OSC;
                term->osc_len = 0;
            } else if (c == 'P') {
                term->parse_state = STATE_DCS;
            } else {
                term->parse_state = STATE_NORMAL;
            }
            break;

        case STATE_CSI:
            if (c == '?') {
                term->csi_private_marker = '?';
            } else if (isdigit(c)) {
                if (term->csi_param_count == 0) {
                    term->csi_param_count = 1;
                }
                term->csi_params[term->csi_param_count - 1] = 
                    term->csi_params[term->csi_param_count - 1] * 10 + (c - '0');
            } else if (c == ';') {
                if (term->csi_param_count < CSI_MAX_PARAMS) {
                    term->csi_param_count++;
                }
            } else if (c >= 0x20 && c <= 0x2F) {
                if (term->csi_intermediate_count < 4) {
                    term->csi_intermediate_chars[term->csi_intermediate_count++] = c;
                }
            } else if (c >= 0x40 && c <= 0x7E) {
                terminal_parser_handle_csi(term, c);
                term->parse_state = STATE_NORMAL;
            }
            break;

        case STATE_OSC:
            if (c == '\x07' || (c == '\\' && i > 0 && buf[i-1] == '\x1b')) {
                terminal_parser_handle_osc(term);
                term->parse_state = STATE_NORMAL;
            } else if (term->osc_len < (int)sizeof(term->osc_buffer) - 1) {
                term->osc_buffer[term->osc_len++] = c;
            }
            break;

        case STATE_DCS:
            // DCS sequences not fully implemented
            if (c == '\x07' || (c == '\\' && i > 0 && buf[i-1] == '\x1b')) {
                term->parse_state = STATE_NORMAL;
            }
            break;

        default:
            term->parse_state = STATE_NORMAL;
            break;
        }
    }

    return true;
}

void terminal_parser_put_char(Terminal* term, uint32_t c) {
    if (!term) return;

    // Handle UTF-8 continuation bytes
    if ((c & 0xC0) == 0x80) {
        // Continuation byte
        if (term->utf8_bytes_to_read > 0) {
            term->utf8_codepoint = (term->utf8_codepoint << 6) | (c & 0x3F);
            term->utf8_bytes_to_read--;
            if (term->utf8_bytes_to_read == 0) {
                c = term->utf8_codepoint;
            } else {
                return;
            }
        } else {
            return;
        }
    } else if ((c & 0x80) == 0) {
        // ASCII character
        term->utf8_bytes_to_read = 0;
    } else if ((c & 0xE0) == 0xC0) {
        // 2-byte sequence
        term->utf8_codepoint = c & 0x1F;
        term->utf8_bytes_to_read = 1;
        return;
    } else if ((c & 0xF0) == 0xE0) {
        // 3-byte sequence
        term->utf8_codepoint = c & 0x0F;
        term->utf8_bytes_to_read = 2;
        return;
    } else if ((c & 0xF8) == 0xF0) {
        // 4-byte sequence
        term->utf8_codepoint = c & 0x07;
        term->utf8_bytes_to_read = 3;
        return;
    }

    // Get current line
    Glyph* line = terminal_grid_get_line(term, term->cursor_y);
    if (!line) return;

    // Create glyph
    Glyph glyph = {
        .character = c,
        .fg = term->current_fg,
        .bg = term->current_bg,
        .attributes = term->current_attributes,
        .width = 1
    };

    // Place character
    if (term->cursor_x < term->cols) {
        line[term->cursor_x] = glyph;
        term->cursor_x++;
    }

    // Handle line wrapping
    if (term->cursor_x >= term->cols) {
        if (term->autowrap_mode) {
            terminal_grid_mark_dirty(term, term->cursor_y);
            term->cursor_x = 0;
            if (term->cursor_y < term->rows - 1) {
                term->cursor_y++;
            } else {
                terminal_grid_scroll_region(term, term->scroll_top,
                                           term->scroll_bottom, 1);
            }
            terminal_grid_mark_dirty(term, term->cursor_y);
        } else {
            term->cursor_x = term->cols - 1;
        }
    }

    terminal_grid_mark_dirty(term, term->cursor_y);
}

void terminal_parser_handle_csi(Terminal* term, char command) {
    if (!term) return;

    // Ensure at least one parameter
    if (term->csi_param_count == 0) {
        term->csi_param_count = 1;
        term->csi_params[0] = 0;
    }

    switch (command) {
    case 'A':  // Cursor Up
        term->cursor_y = (term->cursor_y > 0) ? term->cursor_y - 1 : 0;
        break;
    case 'B':  // Cursor Down
        term->cursor_y = (term->cursor_y < term->rows - 1) ? term->cursor_y + 1 : term->rows - 1;
        break;
    case 'C':  // Cursor Forward
        term->cursor_x = (term->cursor_x < term->cols - 1) ? term->cursor_x + 1 : term->cols - 1;
        break;
    case 'D':  // Cursor Backward
        term->cursor_x = (term->cursor_x > 0) ? term->cursor_x - 1 : 0;
        break;
    case 'H':  // Cursor Position
    case 'f': {
        int row = (term->csi_param_count > 0 && term->csi_params[0] > 0) ? term->csi_params[0] - 1 : 0;
        int col = (term->csi_param_count > 1 && term->csi_params[1] > 0) ? term->csi_params[1] - 1 : 0;
        term->cursor_y = (row < term->rows) ? row : term->rows - 1;
        term->cursor_x = (col < term->cols) ? col : term->cols - 1;
        break;
    }
    case 'J':  // Erase Display
        terminal_grid_clear_visible_screen(term);
        break;
    case 'K':  // Erase Line
        terminal_grid_clear_line(term, term->cursor_y);
        break;
    case 'm':  // Select Graphic Rendition
        terminal_parser_handle_sgr(term);
        break;
    case 'h':  // Set Mode
        if (term->csi_private_marker == '?') {
            terminal_modes_set(term, term->csi_params[0], true);
        }
        break;
    case 'l':  // Reset Mode
        if (term->csi_private_marker == '?') {
            terminal_modes_reset(term, term->csi_params[0], true);
        }
        break;
    case 's':  // Save Cursor
        term->saved_cursor_x = term->cursor_x;
        term->saved_cursor_y = term->cursor_y;
        break;
    case 'u':  // Restore Cursor
        term->cursor_x = term->saved_cursor_x;
        term->cursor_y = term->saved_cursor_y;
        break;
    default:
        DEBUG_LOG("Unknown CSI command: %c", command);
        break;
    }
}

void terminal_parser_handle_osc(Terminal* term) {
    if (!term) return;
    DEBUG_LOG("OSC sequence: %.*s", term->osc_len, term->osc_buffer);
    // OSC sequences not fully implemented
}

void terminal_parser_handle_sgr(Terminal* term) {
    if (!term) return;

    if (term->csi_param_count == 0 || term->csi_params[0] == 0) {
        // Reset all attributes
        term->current_fg = term->default_fg;
        term->current_bg = term->default_bg;
        term->current_attributes = 0;
        return;
    }

    for (int i = 0; i < term->csi_param_count; i++) {
        int param = term->csi_params[i];
        
        if (param == 0) {
            term->current_fg = term->default_fg;
            term->current_bg = term->default_bg;
            term->current_attributes = 0;
        } else if (param == 1) {
            term->current_attributes |= ATTR_BOLD;
        } else if (param == 4) {
            term->current_attributes |= ATTR_UNDERLINE;
        } else if (param == 7) {
            term->current_attributes |= ATTR_INVERSE;
        } else if (param >= 30 && param <= 37) {
            term->current_fg = term->colors[param - 30];
        } else if (param >= 40 && param <= 47) {
            term->current_bg = term->colors[param - 40];
        } else if (param >= 90 && param <= 97) {
            term->current_fg = term->colors[param - 90 + 8];
        } else if (param >= 100 && param <= 107) {
            term->current_bg = term->colors[param - 100 + 8];
        }
    }
}

void terminal_parser_reset(Terminal* term) {
    if (!term) return;
    term->parse_state = STATE_NORMAL;
    term->csi_param_count = 0;
    term->osc_len = 0;
    term->utf8_bytes_to_read = 0;
    memset(term->csi_params, 0, sizeof(term->csi_params));
}
