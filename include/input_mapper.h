#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <SDL.h>
#include "terminal_state.h"

TerminalAction map_cbutton_to_action(SDL_GameControllerButton button);
void init_input_devices(OnScreenKeyboard* osk, const Config* config);
bool should_process_key(const SDL_KeyboardEvent* key);
bool is_modifier_key(SDL_Keycode keycode);
InternalCommand process_osk_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd);
void process_direct_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd);

#endif
