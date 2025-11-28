/**
 * @file keyboard_handler.h
 * @brief Keyboard input handling functionality.
 *
 * This module handles keyboard input processing including:
 * - Key event processing
 * - Text input handling
 * - Key event sending to PTY
 * - UTF-8 string utilities
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef KEYBOARD_HANDLER_H
#define KEYBOARD_HANDLER_H

#include <SDL.h>
#include "terminal_state.h"

/**
 * @brief Calculate UTF-8 string length in characters
 *
 * @param s UTF-8 string
 * @return int Number of characters
 */
int utf8_strlen(const char* s);

/**
 * @brief Get byte offset for a character index in UTF-8 string
 *
 * @param s UTF-8 string
 * @param char_index Character index
 * @return int Byte offset
 */
int utf8_offset_for_char_index(const char* s, int char_index);

/**
 * @brief Get UTF-8 character length from first byte
 *
 * @param s UTF-8 string pointer
 * @return int Character length in bytes
 */
int utf8_char_len(const char* s);

/**
 * @brief Handle key down event
 *
 * @param key Pointer to the keyboard event
 * @param pty_fd PTY file descriptor
 * @param term Pointer to the terminal structure
 */
void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term);

/**
 * @brief Send a key event to the PTY
 *
 * @param pty_fd PTY file descriptor
 * @param sym Key symbol
 * @param mod Modifier mask
 * @param term Pointer to the terminal structure
 */
void send_key_event(int pty_fd, SDL_Keycode sym, SDL_Keymod mod, const Terminal* term);

/**
 * @brief Send text input event to the PTY
 *
 * @param pty_fd PTY file descriptor
 * @param text Text to send
 */
void send_text_input_event(int pty_fd, const char* text);

/**
 * @brief Send mouse wheel event to the PTY
 *
 * @param pty_fd PTY file descriptor
 * @param y_direction Wheel direction (-1, 0, 1)
 */
void send_mouse_wheel_event(int pty_fd, int y_direction);

/**
 * @brief Process keyboard input for terminal
 *
 * @param key SDL keyboard event
 * @param pty_fd PTY file descriptor
 * @param term Pointer to the terminal structure
 * @return bool True if input was processed
 */
bool process_keyboard_input(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term);

// Remove conflicting declaration - use validation.h instead

/**
 * @brief Convert SDL key to terminal sequence
 *
 * @param sym SDL key symbol
 * @param mod Modifier mask
 * @param term Pointer to the terminal structure
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return int Number of bytes written to output
 */
int sdl_key_to_terminal_sequence(SDL_Keycode sym, SDL_Keymod mod, const Terminal* term, 
                                 char* output, int output_size);

/**
 * @brief Check if key should generate text input
 *
 * @param sym SDL key symbol
 * @param mod Modifier mask
 * @return bool True if key should generate text
 */
bool should_generate_text_input(SDL_Keycode sym, SDL_Keymod mod);

/**
 * @brief Handle special key combinations
 *
 * @param sym SDL key symbol
 * @param mod Modifier mask
 * @param pty_fd PTY file descriptor
 * @param term Pointer to the terminal structure
 * @return bool True if special key was handled
 */
bool handle_special_key_combination(SDL_Keycode sym, SDL_Keymod mod, int pty_fd, Terminal* term);

#endif // KEYBOARD_HANDLER_H
