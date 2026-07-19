/**
 * @file osk_core.h
 * @brief Core On-Screen Keyboard (OSK) functionality and state management.
 *
 * This module handles the core OSK functionality including:
 * - OSK state management and lifecycle
 * - Layout and key set management
 * - Position and visibility calculations
 * - Key access and validation functions
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef OSK_CORE_H
#define OSK_CORE_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"

/**
 * @brief Get the Y position for OSK rendering based on cursor position
 *
 * @param osk Pointer to the OSK structure
 * @param term Pointer to the terminal structure
 * @param win_h Window height in pixels
 * @param char_h Character height in pixels
 * @return int Y position in pixels
 */
int get_osk_y_position(const OnScreenKeyboard* osk, const Terminal* term, int win_h, int bar_h);

/**
 * @brief Get the current number of character rows in the active layout
 *
 * @param osk Pointer to the OSK structure
 * @return int Number of character rows
 */
int get_current_num_char_rows(const OnScreenKeyboard* osk);

/**
 * @brief Validate and clamp the current row index to valid range
 *
 * @param osk Pointer to the OSK structure
 */
void osk_validate_row_index(OnScreenKeyboard* osk);

/**
 * @brief Get pointer to the effective row (character or special) based on current state
 *
 * @param osk Pointer to the OSK structure
 * @param set_idx Index of the key set
 * @return const SpecialKeySet* Pointer to the effective row
 */
const SpecialKeySet* osk_get_effective_row_ptr(const OnScreenKeyboard* osk, int set_idx);

/**
 * @brief Get pointer to an effective character key based on current state
 *
 * @param osk Pointer to the OSK structure
 * @param set_idx Index of the key set
 * @param char_idx Index of the character within the set
 * @return const SpecialKey* Pointer to the character key
 */
const SpecialKey* osk_get_effective_char_ptr(const OnScreenKeyboard* osk, int set_idx, int char_idx);

/**
 * @brief Find a layout token by string
 *
 * @param str_start Pointer to the start of the string to match
 * @return const LayoutToken* Pointer to the matching token or NULL
 */
const LayoutToken* osk_find_layout_token(const char* str_start);

/**
 * @brief Load a layout file into the OSK
 *
 * @param osk Pointer to the OSK structure
 * @param path Path to the layout file
 */
void osk_load_layout(OnScreenKeyboard* osk, const char* path);

/**
 * @brief Make a key set available for loading
 *
 * @param osk Pointer to the OSK structure
 * @param path Path to the key set file
 */
void osk_make_set_available(OnScreenKeyboard* osk, const char* path);

/**
 * @brief Initialize all available key sets
 *
 * @param osk Pointer to the OSK structure
 */
void osk_init_all_sets(OnScreenKeyboard* osk);

/**
 * @brief Add a custom key set
 *
 * @param osk Pointer to the OSK structure
 * @param path Path to the custom key set file
 */
void osk_add_custom_set(OnScreenKeyboard* osk, const char* path);

/**
 * @brief Remove a custom key set
 *
 * @param osk Pointer to the OSK structure
 * @param name Name of the key set to remove
 */
void osk_remove_custom_set(OnScreenKeyboard* osk, const char* name);

/**
 * @brief Free all key sets and associated memory
 *
 * @param osk Pointer to the OSK structure
 */
void osk_free_all_sets(OnScreenKeyboard* osk);

/**
 * @brief Get the physical modifier mask from OSK state
 *
 * @param osk Pointer to the OSK structure
 * @return int Modifier mask
 */
int get_physical_modifier_mask(const OnScreenKeyboard* osk);

/**
 * @brief Get the currently active special key set
 *
 * Returns the control set when no dynamic sets are loaded or when
 * set_idx is out of range. Otherwise returns the dynamic set at set_idx.
 *
 * @param osk Pointer to the OSK structure
 * @return const SpecialKeySet* Pointer to the active special key set
 */
const SpecialKeySet* get_active_special_set(const OnScreenKeyboard* osk);

#endif // OSK_CORE_H
