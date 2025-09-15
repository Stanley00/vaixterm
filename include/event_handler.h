/**
 * @file event_handler.h
 * @brief Event handling and terminal action processing functions.
 */

#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

#include "terminal_state.h"

/**
 * @brief Central handler for all abstract terminal actions.
 * @param action The terminal action to handle.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param needs_render Pointer to render flag.
 * @param master_fd PTY master file descriptor.
 * @param font Font pointer (may be modified).
 * @param config Application configuration.
 * @param char_w Character width pointer (may be modified).
 * @param char_h Character height pointer (may be modified).
 */
void event_handle_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, 
                                 bool* needs_render, int master_fd, TTF_Font** font, 
                                 Config* config, int* char_w, int* char_h);

/**
 * @brief Processes a terminal action and sets up for button repeating.
 * @param action The terminal action to process.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param needs_render Pointer to render flag.
 * @param master_fd PTY master file descriptor.
 * @param font Font pointer (may be modified).
 * @param config Application configuration.
 * @param char_w Character width pointer (may be modified).
 * @param char_h Character height pointer (may be modified).
 * @param repeat_state Button repeat state.
 */
void event_process_and_repeat_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, 
                                    bool* needs_render, int master_fd, TTF_Font** font, 
                                    Config* config, int* char_w, int* char_h, 
                                    ButtonRepeatState* repeat_state);

/**
 * @brief Stops repeating an action.
 * @param action The action to stop repeating.
 * @param repeat_state Button repeat state.
 */
void event_stop_repeating_action(TerminalAction action, ButtonRepeatState* repeat_state);

/**
 * @brief Main event handler function.
 * @param event SDL event to handle.
 * @param running Pointer to running flag.
 * @param needs_render Pointer to render flag.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param master_fd PTY master file descriptor.
 * @param font Font pointer (may be modified).
 * @param config Application configuration.
 * @param char_w Character width pointer (may be modified).
 * @param char_h Character height pointer (may be modified).
 * @param repeat_state Button repeat state.
 */
void event_handle(SDL_Event* event, bool* running, bool* needs_render, Terminal* term, 
                 OnScreenKeyboard* osk, int master_fd, TTF_Font** font, Config* config, 
                 int* char_w, int* char_h, ButtonRepeatState* repeat_state);

#endif // EVENT_HANDLER_H
