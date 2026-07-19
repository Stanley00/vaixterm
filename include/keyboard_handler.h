/**
 * @file keyboard_handler.h
 * @brief Keyboard input handling — delegates all encoding to libvterm.
 */

#ifndef KEYBOARD_HANDLER_H
#define KEYBOARD_HANDLER_H

#include <SDL.h>
#include "terminal_state.h"
#include "terminal_libvterm.h"

int utf8_char_len(const char* s);

void handle_key_down(const SDL_KeyboardEvent* key, Terminal* term);

/* Translate an SDL keycode to a libvterm key (shared by keyboard + OSK). */
VTermKey sdl_key_to_vterm(SDL_Keycode sym);

#endif // KEYBOARD_HANDLER_H
