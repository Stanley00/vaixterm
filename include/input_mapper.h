/**
 * @file input_mapper.h
 * @brief Input mapping functionality for keyboard and controller events.
 *
 * This module handles mapping of raw input events to terminal actions:
 * - Keyboard key mapping
 * - Controller button mapping
 * - Modifier key handling
 * - Event to action conversion
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <SDL.h>
#include "terminal_state.h"

/**
 * @brief Map a controller button to a terminal action
 *
 * @param button SDL controller button
 * @return TerminalAction Mapped terminal action
 */
TerminalAction map_cbutton_to_action(SDL_GameControllerButton button);

/**
 * @brief Get combined modifiers from OSK state
 *
 * @param osk Pointer to the OSK structure
 * @return SDL_Keymod Combined modifier mask
 */
SDL_Keymod get_combined_modifiers(const OnScreenKeyboard* osk);

/**
 * @brief Get effective modifiers for sending to terminal
 *
 * @param osk Pointer to the OSK structure
 * @return SDL_Keymod Effective modifier mask
 */
SDL_Keymod get_effective_send_modifiers(const OnScreenKeyboard* osk);

/**
 * @brief Clear one-shot modifiers (like Alt and GUI)
 *
 * @param osk Pointer to the OSK structure
 * @param needs_render_ptr Pointer to render flag
 */
void clear_one_shot_modifiers(OnScreenKeyboard* osk, bool* needs_render_ptr);

/**
 * @brief Convert OSK modifier mask to SDL modifier mask
 *
 * @param osk_mask OSK modifier mask
 * @return SDL_Keymod SDL modifier mask
 */
SDL_Keymod osk_mask_to_sdl_mod(int osk_mask);

/**
 * @brief Initialize input devices
 *
 * @param osk Pointer to the OSK structure
 * @param config Pointer to the configuration
 */
void init_input_devices(OnScreenKeyboard* osk, const Config* config);

/**
 * @brief Check if a key should be processed as a terminal action
 *
 * @param key SDL keyboard event
 * @return bool True if key should be processed
 */
bool should_process_key(const SDL_KeyboardEvent* key);

/**
 * @brief Get the action for a key combination
 *
 * @param keycode Key code
 * @param mod Modifier mask
 * @return TerminalAction Mapped action
 */
TerminalAction get_key_combination_action(SDL_Keycode keycode, SDL_Keymod mod);

/**
 * @brief Check if a modifier key is pressed
 *
 * @param mod Modifier mask
 * @param modifier Modifier to check
 * @return bool True if modifier is pressed
 */
bool is_modifier_pressed(SDL_Keymod mod, SDL_Keymod modifier);

/**
 * @brief Filter out modifier keys from key events
 *
 * @param keycode Key code
 * @return bool True if key is a modifier
 */
bool is_modifier_key(SDL_Keycode keycode);

/**
 * @brief Process OSK action and return internal command
 *
 * @param action Terminal action to process
 * @param term Pointer to terminal
 * @param osk Pointer to OSK
 * @param needs_render Pointer to render flag
 * @param master_fd PTY master file descriptor
 * @return InternalCommand Internal command to execute
 */
InternalCommand process_osk_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd);

/**
 * @brief Process direct terminal action
 *
 * @param action Terminal action to process
 * @param term Pointer to terminal
 * @param osk Pointer to OSK
 * @param needs_render Pointer to render flag
 * @param master_fd PTY master file descriptor
 */
void process_direct_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd);

/**
 * @brief Handle key down event
 *
 * @param key Pointer to keyboard event
 * @param pty_fd PTY file descriptor
 * @param term Pointer to terminal
 */
void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term);

#endif // INPUT_MAPPER_H
