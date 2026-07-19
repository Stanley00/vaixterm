/**
 * @file keyboard_handler.h
 * @brief Keyboard input handling — delegates all encoding to libvterm.
 */

#ifndef KEYBOARD_HANDLER_H
#define KEYBOARD_HANDLER_H

#include <SDL.h>
#include "terminal_state.h"

int utf8_char_len(const char* s);

void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term);

#endif
