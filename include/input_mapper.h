#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <SDL.h>
#include "terminal_state.h"

TerminalAction map_cbutton_to_action(SDL_GameControllerButton button);
SDL_Keymod get_combined_modifiers(const OnScreenKeyboard* osk);
SDL_Keymod get_effective_send_modifiers(const OnScreenKeyboard* osk);
void clear_one_shot_modifiers(OnScreenKeyboard* osk, bool* needs_render_ptr);
SDL_Keymod osk_mask_to_sdl_mod(int osk_mask);
void init_input_devices(OnScreenKeyboard* osk, const Config* config);
bool should_process_key(const SDL_KeyboardEvent* key);
TerminalAction get_key_combination_action(SDL_Keycode keycode, SDL_Keymod mod);
bool is_modifier_pressed(SDL_Keymod mod, SDL_Keymod modifier);
bool is_modifier_key(SDL_Keycode keycode);
InternalCommand process_osk_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd);
void process_direct_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd);
void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term);

#endif // INPUT_MAPPER_H
