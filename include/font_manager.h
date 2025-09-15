/**
 * @file font_manager.h
 * @brief Font management and dynamic font size changing functions.
 */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <SDL_ttf.h>
#include <stdbool.h>

#include "terminal_state.h"

/**
 * @brief Changes the font size and updates all related components.
 * @param font Font pointer (will be modified).
 * @param config Application configuration.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param char_w Character width pointer (will be modified).
 * @param char_h Character height pointer (will be modified).
 * @param master_fd PTY master file descriptor.
 * @param delta Font size change (+1 to increase, -1 to decrease).
 * @return true on success, false on failure.
 */
bool font_change_size(TTF_Font** font, Config* config, Terminal* term, OnScreenKeyboard* osk,
                     int* char_w, int* char_h, int master_fd, int delta);

#endif // FONT_MANAGER_H
