/**
 * @file keyboard_handler.c
 * @brief Keyboard input handling functionality implementation.
 *
 * This module implements keyboard input processing with proper UTF-8 support,
 * key event handling, and terminal communication.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "keyboard_handler.h"
#include "input_mapper.h"
#include "error_codes.h"
#include "validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>

int utf8_strlen(const char* s)
{
    if (!s) {
        ERROR_LOG("Invalid parameter: s=%p", (void*)s);
        return 0;
    }

    int len = 0;
    while (*s) {
        if ((*s & 0xC0) != 0x80) { // It's a start byte or a single-byte char
            len++;
        }
        s++;
    }
    return len;
}

int utf8_offset_for_char_index(const char* s, int char_index)
{
    if (!s) {
        ERROR_LOG("Invalid parameter: s=%p", (void*)s);
        return 0;
    }

    int current_char = 0;
    const char* start = s;
    while (*s) {
        if (current_char == char_index) {
            return (int)(s - start);
        }
        if ((*s & 0xC0) != 0x80) {
            current_char++;
        }
        s++;
    }
    return (int)(s - start); // Return end of string if index is out of bounds
}

int utf8_char_len(const char* s)
{
    if (!s) {
        ERROR_LOG("Invalid parameter: s=%p", (void*)s);
        return 0;
    }

    unsigned char first_byte = (unsigned char)*s;
    if ((first_byte & 0x80) == 0x00) {
        return 1; // 0xxxxxxx - ASCII
    } else if ((first_byte & 0xE0) == 0xC0) {
        return 2; // 110xxxxx - 2 bytes
    } else if ((first_byte & 0xF0) == 0xE0) {
        return 3; // 1110xxxx - 3 bytes
    } else if ((first_byte & 0xF8) == 0xF0) {
        return 4; // 11110xxx - 4 bytes
    } else {
        // Invalid UTF-8 start byte
        return 1;
    }
}

void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term)
{
    if (!key || !term) {
        ERROR_LOG("Invalid parameters: key=%p, term=%p", (void*)key, (void*)term);
        return;
    }

    if (pty_fd < 0) {
        ERROR_LOG("Invalid PTY file descriptor: %d", pty_fd);
        return;
    }

    if (!should_process_key(key)) {
        return;
    }

    // Check for special key combinations first
    if (handle_special_key_combination(key->keysym.sym, key->keysym.mod, pty_fd, term)) {
        return;
    }

    // Handle regular key events
    send_key_event(pty_fd, key->keysym.sym, key->keysym.mod, term);

    DEBUG_LOG("Processed key down: sym=%d, mod=%d", key->keysym.sym, key->keysym.mod);
}

void send_key_event(int pty_fd, SDL_Keycode sym, SDL_Keymod mod, const Terminal* term)
{
    if (pty_fd < 0) {
        ERROR_LOG("Invalid PTY file descriptor: %d", pty_fd);
        return;
    }

    bool ctrl_pressed = (mod & KMOD_CTRL) != 0;

    // --- Priority 1: Ctrl Key Combinations ---
    if (ctrl_pressed) {
        // Handle Ctrl + a-z -> 0x01-0x1A
        if (sym >= SDLK_a && sym <= SDLK_z) {
            char c = (char)(sym - SDLK_a + 1);
            write(pty_fd, &c, 1);
            return;
        }
        // Handle Ctrl + Space -> NUL
        if (sym == SDLK_SPACE) {
            write(pty_fd, "\0", 1);
            return;
        }
        // Handle Ctrl + [ -> ESC
        if (sym == SDLK_LEFTBRACKET) {
            write(pty_fd, "\x1b", 1);
            return;
        }
        // Handle Ctrl + \ -> FS
        if (sym == SDLK_BACKSLASH) {
            write(pty_fd, "\x1c", 1);
            return;
        }
        // Handle Ctrl + ] -> GS
        if (sym == SDLK_RIGHTBRACKET) {
            write(pty_fd, "\x1d", 1);
            return;
        }
        // Handle Ctrl + ^ -> RS
        if (sym == SDLK_CARET) {
            write(pty_fd, "\x1e", 1);
            return;
        }
        // Handle Ctrl + _ -> US
        if (sym == SDLK_UNDERSCORE) {
            write(pty_fd, "\x1f", 1);
            return;
        }
    }

    // --- Priority 2: Special Keys (Function Keys, Arrows, etc.) ---
    char sequence[32];
    int seq_len = sdl_key_to_terminal_sequence(sym, mod, term, sequence, sizeof(sequence));
    if (seq_len > 0) {
        write(pty_fd, sequence, seq_len);
        return;
    }

    // --- Priority 3: Regular Characters ---
    if (should_generate_text_input(sym, mod)) {
        // Let SDL handle text input for regular characters
        // This will be handled by SDL_TEXTINPUT events
        return;
    }

    DEBUG_LOG("Key event sent: sym=%d, mod=%d", sym, mod);
}

void send_text_input_event(int pty_fd, const char* text)
{
    if (pty_fd < 0 || !text) {
        ERROR_LOG("Invalid parameters: pty_fd=%d, text=%p", pty_fd, (void*)text);
        return;
    }

    if (!validate_utf8_string(text, "text", 1024)) {
        ERROR_LOG("Invalid UTF-8 string received");
        return;
    }

    write(pty_fd, text, strlen(text));
    DEBUG_LOG("Text input sent: '%s'", text);
}

void send_mouse_wheel_event(int pty_fd, int y_direction)
{
    if (pty_fd < 0) {
        ERROR_LOG("Invalid PTY file descriptor: %d", pty_fd);
        return;
    }

    if (y_direction == 0) {
        return; // No scroll
    }

    // Send mouse wheel event as escape sequence
    char sequence[16];
    if (y_direction > 0) {
        strcpy(sequence, "\x1b[5~"); // Page Up (scroll up)
    } else {
        strcpy(sequence, "\x1b[6~"); // Page Down (scroll down)
    }

    write(pty_fd, sequence, strlen(sequence));
    DEBUG_LOG("Mouse wheel event sent: direction=%d", y_direction);
}

bool process_keyboard_input(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term)
{
    if (!key || !term) {
        ERROR_LOG("Invalid parameters: key=%p, term=%p", (void*)key, (void*)term);
        return false;
    }

    if (pty_fd < 0) {
        ERROR_LOG("Invalid PTY file descriptor: %d", pty_fd);
        return false;
    }

    handle_key_down(key, pty_fd, term);
    return true;
}

int sdl_key_to_terminal_sequence(SDL_Keycode sym, SDL_Keymod mod, const Terminal* term, 
                                 char* output, int output_size)
{
    if (!output || output_size <= 0) {
        ERROR_LOG("Invalid parameters: output=%p, output_size=%d", (void*)output, output_size);
        return 0;
    }

    bool ctrl_pressed = (mod & KMOD_CTRL) != 0;
    bool shift_pressed = (mod & KMOD_SHIFT) != 0;

    // Handle function keys
    if (sym >= SDLK_F1 && sym <= SDLK_F12) {
        int f_num = sym - SDLK_F1 + 1;
        if (shift_pressed) {
            snprintf(output, output_size, "\x1b[23;%d~", f_num + 11); // Shift+F1-F12
        } else {
            snprintf(output, output_size, "\x1b[%d~", f_num + 10); // F1-F12
        }
        return strlen(output);
    }

    // Handle arrow keys (considering application cursor keys mode)
    if (term && term->application_cursor_keys_mode) {
        switch (sym) {
            case SDLK_UP:    strcpy(output, "\x1bOA"); return 3;
            case SDLK_DOWN:  strcpy(output, "\x1bOB"); return 3;
            case SDLK_RIGHT: strcpy(output, "\x1bOC"); return 3;
            case SDLK_LEFT:  strcpy(output, "\x1bOD"); return 3;
        }
    } else {
        switch (sym) {
            case SDLK_UP:    strcpy(output, "\x1b[A"); return 3;
            case SDLK_DOWN:  strcpy(output, "\x1b[B"); return 3;
            case SDLK_RIGHT: strcpy(output, "\x1b[C"); return 3;
            case SDLK_LEFT:  strcpy(output, "\x1b[D"); return 3;
        }
    }

    // Handle other special keys
    switch (sym) {
        case SDLK_HOME:     strcpy(output, "\x1b[H"); return 3;
        case SDLK_END:      strcpy(output, "\x1b[F"); return 3;
        case SDLK_PAGEUP:   strcpy(output, "\x1b[5~"); return 4;
        case SDLK_PAGEDOWN: strcpy(output, "\x1b[6~"); return 4;
        case SDLK_INSERT:   strcpy(output, "\x1b[2~"); return 4;
        case SDLK_DELETE:   strcpy(output, "\x1b[3~"); return 4;
        case SDLK_BACKSPACE: strcpy(output, ctrl_pressed ? "\x7f" : "\x08"); return 1;
        case SDLK_TAB:      strcpy(output, shift_pressed ? "\x1b[Z" : "\t"); return shift_pressed ? 3 : 1;
        case SDLK_RETURN:   strcpy(output, "\r"); return 1;
        case SDLK_ESCAPE:   strcpy(output, "\x1b"); return 1;
    }

    return 0; // No special sequence
}

bool should_generate_text_input(SDL_Keycode sym, SDL_Keymod mod)
{
    // Don't generate text for modifier keys or special keys
    if (is_modifier_key(sym)) {
        return false;
    }

    // Don't generate text for keys with Ctrl modifier (except specific cases)
    if ((mod & KMOD_CTRL) && sym != SDLK_2 && sym != SDLK_6 && sym != SDLK_MINUS) {
        return false;
    }

    // Don't generate text for Alt combinations
    if (mod & KMOD_ALT) {
        return false;
    }

    return true;
}

bool handle_special_key_combination(SDL_Keycode sym, SDL_Keymod mod, int pty_fd, Terminal* term)
{
    if (pty_fd < 0 || !term) {
        ERROR_LOG("Invalid parameters: pty_fd=%d, term=%p", pty_fd, (void*)term);
        return false;
    }

    // Handle Ctrl+Alt combinations (often used for window management)
    if ((mod & KMOD_CTRL) && (mod & KMOD_ALT)) {
        switch (sym) {
            case SDLK_DELETE:
                // Ctrl+Alt+Delete - system request
                write(pty_fd, "\x1b[31~", 4);
                return true;
            case SDLK_BACKSPACE:
                // Ctrl+Alt+Backspace - reset terminal
                write(pty_fd, "\x1b[?25l\x1b[H\x1b[2J\x1b[c", 12);
                return true;
            default:
                break;
        }
    }

    // Handle Ctrl+Shift combinations
    if ((mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) {
        switch (sym) {
            case SDLK_c:
                // Ctrl+Shift+C - copy (handled by application)
                return true;
            case SDLK_v:
                // Ctrl+Shift+V - paste (handled by application)
                return true;
            default:
                break;
        }
    }

    return false; // No special combination handled
}
