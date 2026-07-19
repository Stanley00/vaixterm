/**
 * @file osk_parser.h
 * @brief On-Screen Keyboard (OSK) layout and key set parsing functionality.
 *
 * This module handles parsing of OSK layout files and key set files including:
 * - Layout file parsing (.kb files)
 * - Key set file parsing (.keys files)
 * - Section header parsing with modifier masks
 * - Token processing and validation
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef OSK_PARSER_H
#define OSK_PARSER_H

#include <stdbool.h>
#include "terminal_state.h"

/**
 * @brief Process a single layout line into a SpecialKeySet
 *
 * @param input Input line to process
 * @return SpecialKeySet Processed key set
 */
SpecialKeySet process_layout_line(const char* input);

/**
 * @brief Free the contents of a SpecialKeySet
 *
 * @param set Pointer to the SpecialKeySet to free
 */
void free_special_key_set_contents(SpecialKeySet* set);

/**
 * @brief Free character layout rows
 *
 * @param rows Array of SpecialKeySet rows
 * @param num_rows Number of rows to free
 */
void free_char_layout_rows(SpecialKeySet* rows, int num_rows);

/**
 * @brief Get modifier mask from modifier name
 *
 * @param mod_name_non_const Modifier name (will be modified)
 * @return int Modifier mask
 */
int get_modifier_mask_from_name_part(char* mod_name_non_const);

/**
 * @brief Parse section header masks from section name
 *
 * @param section_name Section name to parse
 * @param show_mask Output for show mask
 * @param active_mask Output for active mask
 * @return bool True if parsing succeeded
 */
bool parse_section_header_masks(char* section_name, int* show_mask, int* active_mask);

/**
 * @brief Parse layout content into temporary key sets
 *
 * @param content Layout content to parse
 * @param temp_key_sets_by_modifier Output array for parsed key sets
 * @param temp_num_rows Output for number of rows
 * @param temp_capacity Output for capacity
 * @return bool True if parsing succeeded
 */
bool parse_layout_content(const char* content, SpecialKeySet** temp_key_sets_by_modifier, 
                          int* temp_num_rows, int* temp_capacity);

/**
 * @brief Flatten parsed layouts into OSK structure
 *
 * @param osk Pointer to the OSK structure
 * @param parsed_sets Array of parsed sets
 * @param parsed_num_rows Array of row counts
 */
void osk_flatten_layouts(OnScreenKeyboard* osk, SpecialKeySet** parsed_sets, int* parsed_num_rows);

/**
 * @brief Parse a key set line into a SpecialKey
 *
 * @param line Line to parse
 * @param key Output SpecialKey structure
 * @return bool True if parsing succeeded
 */
bool parse_key_set_line(char* line, SpecialKey* key);

/**
 * @brief Check if a dynamic key set is loaded
 *
 * @param osk Pointer to the OSK structure
 * @param name Name of the key set
 * @return bool True if loaded
 */
bool is_dynamic_key_set_loaded(const OnScreenKeyboard* osk, const char* name);

/**
 * @brief Add a loaded key set name to the OSK
 *
 * @param osk Pointer to the OSK structure
 * @param name Name of the key set
 */
void add_loaded_key_set_name(OnScreenKeyboard* osk, const char* name);

/**
 * @brief Remove a loaded key set name from the OSK
 *
 * @param osk Pointer to the OSK structure
 * @param name Name of the key set
 */
void remove_loaded_key_set_name(OnScreenKeyboard* osk, const char* name);

/**
 * @brief Find the control key set in the OSK
 *
 * @param osk Pointer to the OSK structure
 * @return SpecialKeySet* Pointer to control set or NULL
 */
SpecialKeySet* find_control_set(OnScreenKeyboard* osk);

/**
 * @brief Add a key set to the available list
 *
 * @param osk Pointer to the OSK structure
 * @param path Path to the key set file
 * @return bool True if added successfully
 */
bool add_to_available_list(OnScreenKeyboard* osk, const char* path);

/**
 * @brief Rebuild the control set with dynamic keys
 *
 * @param osk Pointer to the OSK structure
 */
void osk_rebuild_control_set_dynamic_keys(OnScreenKeyboard* osk);

/**
 * @brief Parse a .keys file into a single SpecialKeySet.
 *
 * All non-comment, non-empty lines are parsed via parse_key_set_line() and
 * accumulated into one named set (name = file base name without extension).
 *
 * @param osk Pointer to the OSK structure (used for name registry)
 * @param path Path to the .keys file
 * @return SpecialKeySet Parsed set (keys == NULL if file empty/invalid)
 */
SpecialKeySet osk_parse_key_set_file(const OnScreenKeyboard* osk, const char* path);

#endif // OSK_PARSER_H
