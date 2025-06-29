#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include "terminal_state.h" // For TerminalAction, Terminal, OnScreenKeyboard, Config


// --- Function Prototypes ---

// UTF-8 string helpers
int utf8_strlen(const char* s);
int utf8_offset_for_char_index(const char* s, int char_index);
int utf8_char_len(const char* s);

// Input mapping and sending
TerminalAction map_cbutton_to_action(SDL_GameControllerButton button);
TerminalAction map_keyboard_to_action(const SDL_KeyboardEvent* key);
void send_key_event(int pty_fd, SDL_Keycode sym, SDL_Keymod mod, const Terminal* term);
void send_text_input_event(int pty_fd, const char* text);
void send_mouse_wheel_event(int pty_fd, int y_direction);

// Event handlers
void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term);
InternalCommand process_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd);
void init_input_devices(OnScreenKeyboard* osk, const Config* config); // Modified: Added Config* parameter

#endif // INPUT_H
