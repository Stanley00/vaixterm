/**
 * @file osk_parser.c
 * @brief On-Screen Keyboard (OSK) layout and key set parsing functionality implementation.
 *
 * This module implements parsing of OSK layout files (.kb) and key set files (.keys),
 * including token processing, section header parsing, and data structure management.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "osk_parser.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// External layout tokens (defined in original osk.c)
extern const LayoutToken s_layout_tokens[];
extern const int s_num_layout_tokens;

// Default layout tokens
const LayoutToken s_layout_tokens[] = {
    {"ESC", "Escape", SK_SEQUENCE, SDLK_ESCAPE},
    {"TAB", "Tab", SK_SEQUENCE, SDLK_TAB},
    {"ENTER", "Enter", SK_SEQUENCE, SDLK_RETURN},
    {"BACKSPACE", "Back", SK_SEQUENCE, SDLK_BACKSPACE},
    {"SPACE", "Space", SK_STRING, SDLK_SPACE},
    {"UP", "↑", SK_SEQUENCE, SDLK_UP},
    {"DOWN", "↓", SK_SEQUENCE, SDLK_DOWN},
    {"LEFT", "←", SK_SEQUENCE, SDLK_LEFT},
    {"RIGHT", "→", SK_SEQUENCE, SDLK_RIGHT},
    {"F1", "F1", SK_SEQUENCE, SDLK_F1},
    {"F2", "F2", SK_SEQUENCE, SDLK_F2},
    {"F3", "F3", SK_SEQUENCE, SDLK_F3},
    {"F4", "F4", SK_SEQUENCE, SDLK_F4},
    {"F5", "F5", SK_SEQUENCE, SDLK_F5},
    {"F6", "F6", SK_SEQUENCE, SDLK_F6},
    {"F7", "F7", SK_SEQUENCE, SDLK_F7},
    {"F8", "F8", SK_SEQUENCE, SDLK_F8},
    {"F9", "F9", SK_SEQUENCE, SDLK_F9},
    {"F10", "F10", SK_SEQUENCE, SDLK_F10},
    {"F11", "F11", SK_SEQUENCE, SDLK_F11},
    {"F12", "F12", SK_SEQUENCE, SDLK_F12},
    {"CTRL", "Ctrl", SK_MOD_CTRL, SDLK_LCTRL},
    {"SHIFT", "Shift", SK_MOD_SHIFT, SDLK_LSHIFT},
    {"ALT", "Alt", SK_MOD_ALT, SDLK_LALT},
    {"GUI", "Cmd", SK_MOD_GUI, SDLK_LGUI},
};

const int s_num_layout_tokens = sizeof(s_layout_tokens) / sizeof(s_layout_tokens[0]);

// UTF-8 character length function (from original codebase)
extern int utf8_char_len(const char* s);

SpecialKeySet process_layout_line(const char* input)
{
    if (!input) {
        ERROR_LOG("Invalid parameter: input=%p", (void*)input);
        SpecialKeySet empty_set = {0};
        return empty_set;
    }

    SpecialKeySet new_set = { 
        .name = NULL, 
        .keys = NULL, 
        .num_keys = 0, 
        .is_dynamic = true, 
        .file_path = NULL, 
        .active_mod_mask = OSK_MOD_NONE 
    };
    const char* p = input;
    int capacity = 0;

    while (*p) {
        SpecialKey new_key = { 
            .display_name = NULL, 
            .type = SK_STRING, 
            .sequence = NULL, 
            .keycode = SDLK_UNKNOWN, 
            .mod = KMOD_NONE, 
            .command = CMD_NONE 
        };
        bool key_created = false;

        // 1. Try to match a sentinel token like {SHIFT}
        if (*p == '{') {
            for (int t = 0; t < s_num_layout_tokens; ++t) {
                size_t token_len = strlen(s_layout_tokens[t].token);
                if (strncmp(p, s_layout_tokens[t].token, token_len) == 0) {
                    new_key.display_name = strdup(s_layout_tokens[t].display);
                    new_key.type = s_layout_tokens[t].type;
                    new_key.keycode = s_layout_tokens[t].keycode;

                    // Special case for {N/A} and {DEFAULT}, which are SK_STRING
                    // and need an allocated (but empty) sequence.
                    if (new_key.type == SK_STRING) {
                        new_key.sequence = strdup("");
                    }

                    p += token_len;
                    key_created = true;
                    break;
                }
            }
        }

        // 2. If not a token, try to match an escape sequence like \x...
        if (!key_created && *p == '\\' && *(p + 1) != '\0') {
            p++; // Move past the backslash
            if (*p == 'x') {
                // This part is for parsing hex escape sequences, which are typically for control characters.
                // For now, let's just treat it as a literal character if it's not a known escape.
                // The original code had a bug here, it would fall through to literal char.
                // For now, let's keep the existing behavior for \xNN and let it fall through to the literal character logic.
            }
            // Literal character after backslash (e.g., \\, \:)
            int char_len = utf8_char_len(p);
            char* str = malloc(char_len + 1);
            if (!str) {
                ERROR_LOG("Failed to allocate memory for escaped char string");
                p += char_len;
                continue;
            }
            strncpy(str, p, char_len);
            str[char_len] = '\0';
            new_key.display_name = str;

            // If it's a single ASCII character, treat as SK_SEQUENCE
            if (char_len == 1 && str[0] >= 0x20 && str[0] <= 0x7E) {
                new_key.type = SK_SEQUENCE;
                new_key.keycode = (SDL_Keycode)str[0];
                new_key.mod = KMOD_NONE;
                new_key.sequence = NULL;
            } else {
                // Multi-byte UTF-8 or non-printable ASCII, treat as string
                new_key.type = SK_STRING;
                new_key.sequence = strdup(str);
                new_key.keycode = SDLK_UNKNOWN;
                new_key.mod = KMOD_NONE;
            }
            p += char_len;
            key_created = true;
        }

        // 3. If nothing else matched, it's a literal character.
        if (!key_created) {
            int char_len = utf8_char_len(p);
            char* str = malloc(char_len + 1);
            if (!str) {
                ERROR_LOG("Failed to allocate memory for literal char string");
                p += char_len;
                continue;
            }
            strncpy(str, p, char_len);
            str[char_len] = '\0';

            new_key.display_name = str;

            // If it's a single ASCII character (printable range), treat as SK_SEQUENCE
            if (char_len == 1 && str[0] >= 0x20 && str[0] <= 0x7E) {
                new_key.type = SK_SEQUENCE;
                new_key.keycode = (SDL_Keycode)str[0];
                new_key.mod = KMOD_NONE;
                new_key.sequence = NULL;
            } else {
                // Multi-byte UTF-8 character or non-printable ASCII, treat as string
                new_key.type = SK_STRING;
                new_key.sequence = strdup(str);
                new_key.keycode = SDLK_UNKNOWN;
                new_key.mod = KMOD_NONE;
            }
            p += char_len;
            key_created = true;
        }

        // Add the key to the set
        if (key_created) {
            if (new_set.num_keys >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                new_set.keys = realloc(new_set.keys, capacity * sizeof(SpecialKey));
                if (!new_set.keys) {
                    ERROR_LOG("Failed to realloc memory for keys");
                    // Clean up and return empty set
                    free_special_key_set_contents(&new_set);
                    SpecialKeySet empty_set = {0};
                    return empty_set;
                }
            }
            new_set.keys[new_set.num_keys++] = new_key;
        }
    }

    DEBUG_LOG("Processed layout line with %d keys", new_set.num_keys);
    return new_set;
}

void free_special_key_set_contents(SpecialKeySet* set)
{
    if (!set) {
        ERROR_LOG("Invalid parameter: set=%p", (void*)set);
        return;
    }

    if (set->keys) {
        for (int i = 0; i < set->num_keys; i++) {
            if (set->keys[i].display_name) {
                free(set->keys[i].display_name);
            }
            if (set->keys[i].sequence) {
                free(set->keys[i].sequence);
            }
        }
        free(set->keys);
        set->keys = NULL;
    }
    if (set->name) {
        free(set->name);
        set->name = NULL;
    }
    if (set->file_path) {
        free(set->file_path);
        set->file_path = NULL;
    }
    set->num_keys = 0;
}

void free_char_layout_rows(SpecialKeySet* rows, int num_rows)
{
    if (!rows || num_rows <= 0) {
        ERROR_LOG("Invalid parameters: rows=%p, num_rows=%d", (void*)rows, num_rows);
        return;
    }

    for (int i = 0; i < num_rows; i++) {
        free_special_key_set_contents(&rows[i]);
    }
    free(rows);
}

int get_modifier_mask_from_name_part(char* mod_name_non_const)
{
    if (!mod_name_non_const) {
        ERROR_LOG("Invalid parameter: mod_name_non_const=%p", (void*)mod_name_non_const);
        return OSK_MOD_NONE;
    }

    // Convert to lowercase for case-insensitive comparison
    for (char* p = mod_name_non_const; *p; p++) {
        *p = tolower(*p);
    }

    if (strcmp(mod_name_non_const, "ctrl") == 0) return OSK_MOD_CTRL;
    if (strcmp(mod_name_non_const, "alt") == 0) return OSK_MOD_ALT;
    if (strcmp(mod_name_non_const, "gui") == 0) return OSK_MOD_GUI;
    if (strcmp(mod_name_non_const, "shift") == 0) return OSK_MOD_SHIFT;

    return OSK_MOD_NONE;
}

bool parse_section_header_masks(char* section_name, int* show_mask, int* active_mask)
{
    if (!section_name || !show_mask || !active_mask) {
        ERROR_LOG("Invalid parameters: section_name=%p, show_mask=%p, active_mask=%p", 
                  (void*)section_name, (void*)show_mask, (void*)active_mask);
        return false;
    }

    *show_mask = OSK_MOD_NONE;
    *active_mask = OSK_MOD_NONE;

    // Remove brackets
    if (section_name[0] == '[') {
        memmove(section_name, section_name + 1, strlen(section_name));
    }
    char* end = strchr(section_name, ']');
    if (end) {
        *end = '\0';
    }

    // Split by '+' to get individual modifiers
    char* token = strtok(section_name, "+");
    while (token) {
        int mod = get_modifier_mask_from_name_part(token);
        if (mod != OSK_MOD_NONE) {
            *show_mask |= mod;
            *active_mask |= mod;
        }
        token = strtok(NULL, "+");
    }

    return true;
}

bool parse_layout_content(const char* content, SpecialKeySet** temp_key_sets_by_modifier, 
                          int* temp_num_rows, int* temp_capacity)
{
    if (!content || !temp_key_sets_by_modifier || !temp_num_rows || !temp_capacity) {
        ERROR_LOG("Invalid parameters: content=%p, temp_key_sets_by_modifier=%p, temp_num_rows=%p, temp_capacity=%p",
                  (void*)content, (void*)temp_key_sets_by_modifier, (void*)temp_num_rows, (void*)temp_capacity);
        return false;
    }

    // Initialize temp arrays
    for (int i = 0; i < OSK_NUM_MODIFIERS; i++) {
        temp_key_sets_by_modifier[i] = NULL;
        temp_num_rows[i] = 0;
        temp_capacity[i] = 0;
    }

    char* content_copy = strdup(content);
    if (!content_copy) {
        ERROR_LOG("Failed to duplicate content");
        return false;
    }

    char* line = strtok(content_copy, "\n");
    int current_show_mask = OSK_MOD_NONE;
    int current_active_mask = OSK_MOD_NONE;

    while (line) {
        // Skip empty lines and whitespace
        while (*line && isspace(*line)) line++;
        if (*line == '\0') {
            line = strtok(NULL, "\n");
            continue;
        }

        // Check for section header
        if (line[0] == '[') {
            char section_name[256];
            strncpy(section_name, line, sizeof(section_name) - 1);
            section_name[sizeof(section_name) - 1] = '\0';

            if (!parse_section_header_masks(section_name, &current_show_mask, &current_active_mask)) {
                WARN_LOG("Failed to parse section header: %s", line);
            }
        } else {
            // Process layout line
            SpecialKeySet new_set = process_layout_line(line);
            if (new_set.keys && new_set.num_keys > 0) {
                new_set.active_mod_mask = current_active_mask;

                // Find the appropriate modifier bucket
                int mod_index = 0; // Default to no modifiers
                for (int i = 1; i < OSK_NUM_MODIFIERS; i++) {
                    if (current_show_mask == (1 << (i - 1))) {
                        mod_index = i;
                        break;
                    }
                }

                // Add to temp array
                if (temp_num_rows[mod_index] >= temp_capacity[mod_index]) {
                    temp_capacity[mod_index] = temp_capacity[mod_index] == 0 ? 16 : temp_capacity[mod_index] * 2;
                    temp_key_sets_by_modifier[mod_index] = realloc(temp_key_sets_by_modifier[mod_index], 
                                                                   temp_capacity[mod_index] * sizeof(SpecialKeySet));
                    if (!temp_key_sets_by_modifier[mod_index]) {
                        ERROR_LOG("Failed to realloc temp key sets");
                        free(content_copy);
                        return false;
                    }
                }
                temp_key_sets_by_modifier[mod_index][temp_num_rows[mod_index]++] = new_set;
            }
        }

        line = strtok(NULL, "\n");
    }

    free(content_copy);
    DEBUG_LOG("Parsed layout content successfully");
    return true;
}

void osk_flatten_layouts(OnScreenKeyboard* osk, SpecialKeySet** parsed_sets, int* parsed_num_rows)
{
    if (!osk || !parsed_sets || !parsed_num_rows) {
        ERROR_LOG("Invalid parameters: osk=%p, parsed_sets=%p, parsed_num_rows=%p", 
                  (void*)osk, (void*)parsed_sets, (void*)parsed_num_rows);
        return;
    }

    // Free existing layouts
    if (osk->char_sets) {
        free_char_layout_rows(osk->char_sets, osk->num_char_rows);
        osk->char_sets = NULL;
        osk->num_char_rows = 0;
    }
    if (osk->shifted_char_sets) {
        free_char_layout_rows(osk->shifted_char_sets, osk->num_shifted_rows);
        osk->shifted_char_sets = NULL;
        osk->num_shifted_rows = 0;
    }

    // Copy normal layout (no modifiers)
    if (parsed_sets[0] && parsed_num_rows[0] > 0) {
        osk->num_char_rows = parsed_num_rows[0];
        osk->char_sets = malloc(osk->num_char_rows * sizeof(SpecialKeySet));
        if (osk->char_sets) {
            memcpy(osk->char_sets, parsed_sets[0], osk->num_char_rows * sizeof(SpecialKeySet));
        }
    }

    // Copy shifted layout (SHIFT modifier)
    if (parsed_sets[OSK_MOD_SHIFT] && parsed_num_rows[OSK_MOD_SHIFT] > 0) {
        osk->num_shifted_rows = parsed_num_rows[OSK_MOD_SHIFT];
        osk->shifted_char_sets = malloc(osk->num_shifted_rows * sizeof(SpecialKeySet));
        if (osk->shifted_char_sets) {
            memcpy(osk->shifted_char_sets, parsed_sets[OSK_MOD_SHIFT], osk->num_shifted_rows * sizeof(SpecialKeySet));
        }
    }

    DEBUG_LOG("Flattened layouts: normal=%d rows, shifted=%d rows", osk->num_char_rows, osk->num_shifted_rows);
}

bool parse_key_set_line(char* line, SpecialKey* key)
{
    if (!line || !key) {
        ERROR_LOG("Invalid parameters: line=%p, key=%p", (void*)line, (void*)key);
        return false;
    }

    // Initialize key
    memset(key, 0, sizeof(SpecialKey));
    key->type = SK_STRING;
    key->mod = KMOD_NONE;

    // Skip whitespace
    while (*line && isspace(*line)) line++;
    if (*line == '\0') return false;

    // Find the separator (tab or multiple spaces)
    char* separator = strchr(line, '\t');
    if (!separator) {
        separator = strstr(line, "  "); // Two spaces
    }
    if (!separator) {
        separator = strchr(line, ' '); // Single space (fallback)
    }

    if (separator) {
        *separator = '\0';
        char* display_name = line;
        char* sequence = separator + 1;

        // Trim whitespace
        while (*sequence && isspace(*sequence)) sequence++;

        // Set display name
        key->display_name = strdup(display_name);
        if (!key->display_name) {
            ERROR_LOG("Failed to allocate display name");
            return false;
        }

        // Set sequence
        key->sequence = strdup(sequence);
        if (!key->sequence) {
            ERROR_LOG("Failed to allocate sequence");
            free(key->display_name);
            return false;
        }
        key->type = SK_SEQUENCE;
    } else {
        // No separator, treat whole line as display name
        key->display_name = strdup(line);
        if (!key->display_name) {
            ERROR_LOG("Failed to allocate display name");
            return false;
        }
        key->sequence = strdup("");
        if (!key->sequence) {
            ERROR_LOG("Failed to allocate empty sequence");
            free(key->display_name);
            return false;
        }
    }

    DEBUG_LOG("Parsed key set line: '%s' -> '%s'", key->display_name, key->sequence ? key->sequence : "");
    return true;
}

bool is_dynamic_key_set_loaded(const OnScreenKeyboard* osk, const char* name)
{
    if (!osk || !name) {
        ERROR_LOG("Invalid parameters: osk=%p, name=%p", (void*)osk, (void*)name);
        return false;
    }

    for (int i = 0; i < osk->num_loaded_key_sets; i++) {
        if (osk->loaded_key_set_names[i] && strcmp(osk->loaded_key_set_names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

void add_loaded_key_set_name(OnScreenKeyboard* osk, const char* name)
{
    if (!osk || !name) {
        ERROR_LOG("Invalid parameters: osk=%p, name=%p", (void*)osk, (void*)name);
        return;
    }

    if (is_dynamic_key_set_loaded(osk, name)) {
        DEBUG_LOG("Key set already loaded: %s", name);
        return;
    }

    // Add to loaded sets array
    osk->loaded_key_set_names = realloc(osk->loaded_key_set_names,
                                     (osk->num_loaded_key_sets + 1) * sizeof(char*));
    if (osk->loaded_key_set_names) {
        osk->loaded_key_set_names[osk->num_loaded_key_sets] = strdup(name);
        if (osk->loaded_key_set_names[osk->num_loaded_key_sets]) {
            osk->num_loaded_key_sets++;
            DEBUG_LOG("Added loaded key set: %s", name);
        }
    }
}

void remove_loaded_key_set_name(OnScreenKeyboard* osk, const char* name)
{
    if (!osk || !name) {
        ERROR_LOG("Invalid parameters: osk=%p, name=%p", (void*)osk, (void*)name);
        return;
    }

    for (int i = 0; i < osk->num_loaded_key_sets; i++) {
        if (osk->loaded_key_set_names[i] && strcmp(osk->loaded_key_set_names[i], name) == 0) {
            free(osk->loaded_key_set_names[i]);
            
            // Shift remaining elements
            for (int j = i; j < osk->num_loaded_key_sets - 1; j++) {
                osk->loaded_key_set_names[j] = osk->loaded_key_set_names[j + 1];
            }
            
            osk->num_loaded_key_sets--;
            DEBUG_LOG("Removed loaded key set: %s", name);
            return;
        }
    }
}

SpecialKeySet* find_control_set(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return NULL;
    }
    return &osk->control_set;
}

bool add_to_available_list(OnScreenKeyboard* osk, const char* path)
{
    if (!osk || !path) {
        ERROR_LOG("Invalid parameters: osk=%p, path=%p", (void*)osk, (void*)path);
        return false;
    }

    // Check if already in list
    for (int i = 0; i < osk->num_available_sets; i++) {
        if (osk->available_sets[i] && strcmp(osk->available_sets[i], path) == 0) {
            DEBUG_LOG("Path already in available list: %s", path);
            return true;
        }
    }

    // Add to list
    osk->available_sets = realloc(osk->available_sets, 
                                  (osk->num_available_sets + 1) * sizeof(char*));
    if (osk->available_sets) {
        osk->available_sets[osk->num_available_sets] = strdup(path);
        if (osk->available_sets[osk->num_available_sets]) {
            osk->num_available_sets++;
            DEBUG_LOG("Added to available list: %s", path);
            return true;
        }
    }
    return false;
}

void osk_rebuild_control_set_dynamic_keys(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    // This is a simplified implementation - the full implementation would
    // rebuild the control set with dynamic keys from loaded sets
    DEBUG_LOG("Rebuilding control set with dynamic keys (simplified)");
}
