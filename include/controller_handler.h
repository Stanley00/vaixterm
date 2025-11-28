/**
 * @file controller_handler.h
 * @brief Game controller input handling functionality.
 *
 * This module handles game controller input processing including:
 * - Controller button mapping
 * - Controller axis handling
 * - Controller-specific actions
 * - Controller state management
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef CONTROLLER_HANDLER_H
#define CONTROLLER_HANDLER_H

#include <SDL.h>
#include "terminal_state.h"

/**
 * @brief Initialize game controller subsystem
 *
 * @return bool True if initialization succeeded
 */
bool init_controller_subsystem(void);

/**
 * @brief Cleanup game controller subsystem
 */
void cleanup_controller_subsystem(void);

/**
 * @brief Open a game controller
 *
 * @param joystick_index Joystick index
 * @return SDL_GameController* Pointer to controller or NULL
 */
SDL_GameController* open_controller(int joystick_index);

/**
 * @brief Close a game controller
 *
 * @param controller Pointer to controller
 */
void close_controller(SDL_GameController* controller);

/**
 * @brief Handle controller button press
 *
 * @param button Controller button
 * @param state Button state (pressed/released)
 * @param pty_fd PTY file descriptor
 * @param term Pointer to the terminal structure
 * @param osk Pointer to the OSK structure
 * @param needs_render Pointer to render flag
 * @return bool True if event was handled
 */
bool handle_controller_button(SDL_GameControllerButton button, bool state, 
                               int pty_fd, Terminal* term, OnScreenKeyboard* osk, bool* needs_render);

/**
 * @brief Handle controller axis motion
 *
 * @param axis Controller axis
 * @param value Axis value
 * @param term Pointer to the terminal structure
 * @param osk Pointer to the OSK structure
 * @param needs_render Pointer to render flag
 * @return bool True if event was handled
 */
bool handle_controller_axis(SDL_GameControllerAxis axis, int value, 
                             Terminal* term, OnScreenKeyboard* osk, bool* needs_render);

/**
 * @brief Check if controller button is a navigation button
 *
 * @param button Controller button
 * @return bool True if button is for navigation
 */
bool is_navigation_button(SDL_GameControllerButton button);

/**
 * @brief Get controller name
 *
 * @param controller Pointer to controller
 * @return const char* Controller name or NULL
 */
const char* get_controller_name(SDL_GameController* controller);

/**
 * @brief Check if controller is connected
 *
 * @param controller Pointer to controller
 * @return bool True if controller is connected
 */
bool is_controller_connected(SDL_GameController* controller);

/**
 * @brief Get controller button state
 *
 * @param controller Pointer to controller
 * @param button Controller button
 * @return bool True if button is pressed
 */
bool get_controller_button_state(SDL_GameController* controller, SDL_GameControllerButton button);

/**
 * @brief Get controller axis value
 *
 * @param controller Pointer to controller
 * @param axis Controller axis
 * @return int16_t Axis value
 */
int16_t get_controller_axis_value(SDL_GameController* controller, SDL_GameControllerAxis axis);

/**
 * @brief Handle controller connection event
 *
 * @param joystick_index Joystick index
 * @param term Pointer to the terminal structure
 * @param osk Pointer to the OSK structure
 * @return bool True if controller was connected
 */
bool handle_controller_connection(int joystick_index, Terminal* term, OnScreenKeyboard* osk);

/**
 * @brief Handle controller disconnection event
 *
 * @param joystick_index Joystick index
 * @param term Pointer to the terminal structure
 * @param osk Pointer to the OSK structure
 * @return bool True if controller was disconnected
 */
bool handle_controller_disconnection(int joystick_index, Terminal* term, OnScreenKeyboard* osk);

/**
 * @brief Update controller state
 *
 * @param term Pointer to the terminal structure
 * @param osk Pointer to the OSK structure
 * @param needs_render Pointer to render flag
 */
void update_controller_state(Terminal* term, OnScreenKeyboard* osk, bool* needs_render);

/**
 * @brief Rumble controller for feedback
 *
 * @param controller Pointer to controller
 * @param low_frequency Low frequency rumble
 * @param high_frequency High frequency rumble
 * @param duration_ms Duration in milliseconds
 * @return bool True if rumble was started
 */
bool rumble_controller(SDL_GameController* controller, uint16_t low_frequency, 
                        uint16_t high_frequency, uint32_t duration_ms);

#endif // CONTROLLER_HANDLER_H
