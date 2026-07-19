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
#include "keyboard_handler.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Default layout tokens
const LayoutToken s_layout_tokens[] = {
    {"ESC", "Esc", SK_SEQUENCE, SDLK_ESCAPE},
    {"TAB", "Tab", SK_SEQUENCE, SDLK_TAB},
    {"ENTER", "Ent", SK_SEQUENCE, SDLK_RETURN},
    {"BACKSPACE", "Bksp", SK_SEQUENCE, SDLK_BACKSPACE},
    {"BS", "Bksp", SK_SEQUENCE, SDLK_BACKSPACE},
    {"SPACE", "Space", SK_STRING, SDLK_SPACE},
    {"DEL", "Del", SK_SEQUENCE, SDLK_DELETE},
    {"DELETE", "Del", SK_SEQUENCE, SDLK_DELETE},
    {"HOME", "Home", SK_SEQUENCE, SDLK_HOME},
    {"END", "End", SK_SEQUENCE, SDLK_END},
    {"PGUP", "PUp", SK_SEQUENCE, SDLK_PAGEUP},
    {"PAGEUP", "PUp", SK_SEQUENCE, SDLK_PAGEUP},
    {"PGDN", "PDn", SK_SEQUENCE, SDLK_PAGEDOWN},
    {"PAGEDOWN", "PDn", SK_SEQUENCE, SDLK_PAGEDOWN},
    {"INS", "Ins", SK_SEQUENCE, SDLK_INSERT},
    {"INSERT", "Ins", SK_SEQUENCE, SDLK_INSERT},
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
    {"N/A", NULL, SK_STRING, SDLK_UNKNOWN},
    {"DEFAULT", NULL, SK_STRING, SDLK_UNKNOWN},
};

const int s_num_layout_tokens = sizeof(s_layout_tokens) / sizeof(s_layout_tokens[0]);

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
                if (strncmp(p + 1, s_layout_tokens[t].token, token_len) == 0 && p[1 + token_len] == '}') {
                    // Placeholder tokens (N/A, DEFAULT) — skip without creating a key
                    if (s_layout_tokens[t].display == NULL) {
                        p += token_len + 2;
                        break;
                    }
                    new_key.display_name = strdup(s_layout_tokens[t].display);
                    new_key.type = s_layout_tokens[t].type;
                    new_key.keycode = s_layout_tokens[t].keycode;

                    if (new_key.type == SK_STRING) {
                        new_key.sequence = strdup("");
                    }

                    p += token_len + 2; // skip '{' + token + '}'
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

    // Split by '+' to get individual modifiers.
    // NOTE: do NOT use strtok() here — parse_layout_content() drives this
    // function from inside its own strtok(content_copy, "\n") loop, and a
    // nested strtok() call corrupts that outer tokenizer's internal state.
    char* p = section_name;
    while (*p) {
        char* plus = strchr(p, '+');
        if (plus) *plus = '\0';
        int mod = get_modifier_mask_from_name_part(p);
        if (mod != OSK_MOD_NONE) {
            *show_mask |= mod;
            *active_mask |= mod;
        }
        if (!plus) break;
        p = plus + 1;
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

        // Skip comment lines
        if (*line == '#') {
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

// Key name table for .keys files
typedef struct {
    const char* name;
    SDL_Keycode keycode;
} KeyNameEntry;

static const KeyNameEntry s_key_names[] = {
    {"ESC", SDLK_ESCAPE}, {"ESCAPE", SDLK_ESCAPE},
    {"TAB", SDLK_TAB},
    {"ENTER", SDLK_RETURN}, {"RETURN", SDLK_RETURN},
    {"BS", SDLK_BACKSPACE}, {"BACKSPACE", SDLK_BACKSPACE},
    {"DEL", SDLK_DELETE}, {"DELETE", SDLK_DELETE},
    {"HOME", SDLK_HOME}, {"END", SDLK_END},
    {"PAGEUP", SDLK_PAGEUP}, {"PGUP", SDLK_PAGEUP},
    {"PAGEDOWN", SDLK_PAGEDOWN}, {"PGDN", SDLK_PAGEDOWN},
    {"INSERT", SDLK_INSERT}, {"INS", SDLK_INSERT},
    {"SPACE", SDLK_SPACE},
    {"UP", SDLK_UP}, {"DOWN", SDLK_DOWN},
    {"LEFT", SDLK_LEFT}, {"RIGHT", SDLK_RIGHT},
    {"F1", SDLK_F1}, {"F2", SDLK_F2}, {"F3", SDLK_F3}, {"F4", SDLK_F4},
    {"F5", SDLK_F5}, {"F6", SDLK_F6}, {"F7", SDLK_F7}, {"F8", SDLK_F8},
    {"F9", SDLK_F9}, {"F10", SDLK_F10}, {"F11", SDLK_F11}, {"F12", SDLK_F12},
};

// Command name table for .keys files
typedef struct {
    const char* name;
    InternalCommand cmd;
} CommandNameEntry;

static const CommandNameEntry s_commands[] = {
    {"CMD_FONT_INC", CMD_FONT_INC},
    {"CMD_FONT_DEC", CMD_FONT_DEC},
    {"CMD_CURSOR_TOGGLE_VISIBILITY", CMD_CURSOR_TOGGLE_VISIBILITY},
    {"CMD_CURSOR_TOGGLE_BLINK", CMD_CURSOR_TOGGLE_BLINK},
    {"CMD_CURSOR_CYCLE_STYLE", CMD_CURSOR_CYCLE_STYLE},
    {"CMD_TERMINAL_RESET", CMD_TERMINAL_RESET},
    {"CMD_TERMINAL_CLEAR", CMD_TERMINAL_CLEAR},
    {"CMD_OSK_TOGGLE_POSITION", CMD_OSK_TOGGLE_POSITION},
    {"CMD_RELOAD_THEME", CMD_RELOAD_THEME},
};

static char* find_unescaped_colon(char* str)
{
    for (char* p = str; *p; p++) {
        if (*p == '\\' && *(p + 1)) {
            p++; // skip escaped char
        } else if (*p == ':') {
            return p;
        }
    }
    return NULL;
}

static void unescape_display_name(char* str)
{
    char* read = str;
    char* write = str;
    while (*read) {
        if (*read == '\\' && *(read + 1)) {
            read++;
            *write++ = *read++;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

bool parse_key_set_line(char* line, SpecialKey* key)
{
    if (!line || !key) {
        ERROR_LOG("Invalid parameters: line=%p, key=%p", (void*)line, (void*)key);
        return false;
    }

    memset(key, 0, sizeof(SpecialKey));
    key->type = SK_STRING;
    key->mod = KMOD_NONE;

    // Skip whitespace
    while (*line && isspace(*line)) line++;
    if (*line == '\0') return false;

    // Find first unescaped colon — DISPLAY_NAME:VALUE[:EXTRA_DATA]
    char* first_colon = find_unescaped_colon(line);
    if (!first_colon) return false;

    *first_colon = '\0';
    char* display_name = line;
    char* rest = first_colon + 1;

    // Trim display name
    char* dn_end = first_colon - 1;
    while (dn_end >= display_name && isspace(*dn_end)) *dn_end-- = '\0';

    key->display_name = strdup(display_name);
    unescape_display_name(key->display_name);

    // Find second unescaped colon for EXTRA_DATA
    char* second_colon = find_unescaped_colon(rest);
    char* value_str = rest;
    char* extra_str = NULL;

    if (second_colon) {
        *second_colon = '\0';
        extra_str = second_colon + 1;
    }

    // Trim value
    while (*value_str && isspace(*value_str)) value_str++;
    char* v_end = value_str + strlen(value_str) - 1;
    while (v_end > value_str && isspace(*v_end)) *v_end-- = '\0';

    // Parse VALUE
    if (value_str[0] == '"') {
        // Quoted literal string — send verbatim
        value_str++;
        char* q_end = strchr(value_str, '"');
        if (q_end) *q_end = '\0';
        key->type = SK_STRING;
        key->sequence = strdup(value_str);
    } else if (strncmp(value_str, "LOAD_FILE", 9) == 0) {
        key->type = SK_LOAD_FILE;
        key->sequence = extra_str ? strdup(extra_str) : strdup("");
    } else if (strncmp(value_str, "UNLOAD_FILE", 11) == 0) {
        key->type = SK_UNLOAD_FILE;
        key->sequence = extra_str ? strdup(extra_str) : strdup("");
    } else {
        // Check internal commands
        bool found_cmd = false;
        for (size_t i = 0; i < sizeof(s_commands) / sizeof(s_commands[0]); i++) {
            if (strcmp(value_str, s_commands[i].name) == 0) {
                key->type = SK_INTERNAL_CMD;
                key->command = s_commands[i].cmd;
                found_cmd = true;
                break;
            }
        }

        if (!found_cmd) {
            // Try key name lookup
            char upper_name[64];
            size_t name_len = strlen(value_str);
            if (name_len < sizeof(upper_name)) {
                for (size_t i = 0; i < name_len; i++)
                    upper_name[i] = toupper((unsigned char)value_str[i]);
                upper_name[name_len] = '\0';
            }

            bool found_key = false;
            for (size_t i = 0; i < sizeof(s_key_names) / sizeof(s_key_names[0]); i++) {
                if (strcmp(upper_name, s_key_names[i].name) == 0) {
                    key->type = SK_SEQUENCE;
                    key->keycode = s_key_names[i].keycode;
                    key->sequence = NULL;
                    found_key = true;
                    break;
                }
            }

            if (!found_key) {
                // Treat as literal string
                key->type = SK_STRING;
                key->sequence = strdup(value_str);
            }
        }
    }

    // Parse EXTRA_DATA (comma-separated modifiers)
    if (extra_str && key->type == SK_SEQUENCE) {
        char* mod_token = strtok(extra_str, ",");
        while (mod_token) {
            while (*mod_token && isspace(*mod_token)) mod_token++;
            char mod_lower[32];
            size_t ml_len = strlen(mod_token);
            if (ml_len < sizeof(mod_lower)) {
                for (size_t i = 0; i < ml_len; i++)
                    mod_lower[i] = tolower((unsigned char)mod_token[i]);
                mod_lower[ml_len] = '\0';
            }

            if (strcmp(mod_lower, "ctrl") == 0) key->mod |= KMOD_CTRL;
            else if (strcmp(mod_lower, "alt") == 0) key->mod |= KMOD_ALT;
            else if (strcmp(mod_lower, "shift") == 0) key->mod |= KMOD_SHIFT;
            else if (strcmp(mod_lower, "gui") == 0 || strcmp(mod_lower, "win") == 0 || strcmp(mod_lower, "super") == 0)
                key->mod |= KMOD_GUI;

            mod_token = strtok(NULL, ",");
        }
    }

    DEBUG_LOG("Parsed key set line: '%s' -> type=%d", key->display_name, key->type);
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

    // Match by base name so callers can pass either the full path or the
    // file base name (e.g. "nano.keys"). The registry stores full paths.
    const char* name_base = strrchr(name, '/');
    name_base = name_base ? name_base + 1 : name;

    for (int i = 0; i < osk->num_loaded_key_sets; i++) {
        const char* reg = osk->loaded_key_set_names[i];
        if (!reg) continue;
        const char* reg_base = strrchr(reg, '/');
        reg_base = reg_base ? reg_base + 1 : reg;
        if (strcmp(reg_base, name_base) == 0) {
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

SpecialKeySet osk_parse_key_set_file(const OnScreenKeyboard* osk, const char* path)
{
    SpecialKeySet set = {0};
    (void)osk;

    if (!path) {
        ERROR_LOG("Invalid parameter: path=%p", (void*)path);
        return set;
    }

    // Derive display name from the file base name (strip dir + .keys/.key).
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    size_t len = strlen(base);
    while (len > 0 && base[len - 1] != '.') len--;
    if (len > 0) len--; // drop the trailing dot
    if (len == 0) len = strlen(base);
    char* name = malloc(len + 1);
    if (name) {
        memcpy(name, base, len);
        name[len] = '\0';
        set.name = name;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        ERROR_LOG("Failed to open key set file: %s", path);
        free_special_key_set_contents(&set);
        return set;
    }

    char line[256];
    int capacity = 0;
    bool found = false;

    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        SpecialKey key;
        if (!parse_key_set_line(line, &key)) continue;

        if (set.num_keys >= capacity) {
            capacity = capacity == 0 ? 8 : capacity * 2;
            SpecialKey* tmp = realloc(set.keys, capacity * sizeof(SpecialKey));
            if (!tmp) {
                ERROR_LOG("Failed to realloc key set keys");
                free(key.display_name);
                free(key.sequence);
                free_special_key_set_contents(&set);
                fclose(file);
                return set;
            }
            set.keys = tmp;
        }
        set.keys[set.num_keys++] = key;
        found = true;
    }
    fclose(file);

    if (!found) {
        DEBUG_LOG("No valid keys found in key set file: %s", path);
        free_special_key_set_contents(&set);
        return set;
    }

    set.is_dynamic = true;
    set.file_path = strdup(path);
    set.active_mod_mask = OSK_MOD_NONE;
    DEBUG_LOG("Parsed key set '%s' with %d keys from %s", set.name, set.num_keys, path);
    return set;
}

void osk_rebuild_control_set_dynamic_keys(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    if (osk->control_set.keys) {
        free_special_key_set_contents(&osk->control_set);
        osk->control_set.keys = NULL;
        osk->control_set.num_keys = 0;
    }

    static const SpecialKey osk_special_set_action_keys[] = {
        {.display_name = "OSK Pos", .type = SK_INTERNAL_CMD, .sequence = NULL, .keycode = 0, .mod = KMOD_NONE, .command = CMD_OSK_TOGGLE_POSITION},
        {.display_name = "Ctrl",  .type = SK_MOD_CTRL,  .sequence = NULL, .keycode = SDLK_LCTRL, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Alt",   .type = SK_MOD_ALT,   .sequence = NULL, .keycode = SDLK_LALT, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "GUI",   .type = SK_MOD_GUI,   .sequence = NULL, .keycode = SDLK_LGUI,  .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Esc",   .type = SK_SEQUENCE,  .sequence = "\x1b", .keycode = SDLK_ESCAPE, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Tab",   .type = SK_SEQUENCE,  .sequence = "\t", .keycode = SDLK_TAB, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Enter", .type = SK_SEQUENCE,  .sequence = "\r", .keycode = SDLK_RETURN, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Space", .type = SK_SEQUENCE,  .sequence = " ", .keycode = SDLK_SPACE, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Bksp",  .type = SK_SEQUENCE,  .sequence = "\x7f", .keycode = SDLK_BACKSPACE, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Del",   .type = SK_SEQUENCE, .sequence = "\x1b[3~", .keycode = SDLK_DELETE, .mod = KMOD_NONE, .command = CMD_NONE},
        {.display_name = "Shift", .type = SK_MOD_SHIFT, .sequence = NULL, .keycode = SDLK_LSHIFT, .mod = KMOD_NONE, .command = CMD_NONE},
    };

    osk->control_set.keys = malloc(sizeof(osk_special_set_action_keys));
    if (osk->control_set.keys) {
        memcpy(osk->control_set.keys, osk_special_set_action_keys, sizeof(osk_special_set_action_keys));
        osk->control_set.num_keys = sizeof(osk_special_set_action_keys) / sizeof(osk_special_set_action_keys[0]);
        for (int i = 0; i < osk->control_set.num_keys; i++) {
            if (osk->control_set.keys[i].display_name)
                osk->control_set.keys[i].display_name = strdup(osk->control_set.keys[i].display_name);
            if (osk->control_set.keys[i].sequence)
                osk->control_set.keys[i].sequence = strdup(osk->control_set.keys[i].sequence);
        }
    }

    for (int i = 0; i < osk->num_available_sets; i++) {
        const char* path = osk->available_sets[i];
        if (!path) continue;

        const char* base = strrchr(path, '/');
        base = base ? base + 1 : path;
        bool loaded = is_dynamic_key_set_loaded(osk, path);

        char prefix = loaded ? '-' : '+';
        size_t basename_len = strlen(base);
        while (basename_len > 0 && base[basename_len - 1] != '.') basename_len--;
        if (basename_len > 0) basename_len--;
        if (basename_len == 0) basename_len = strlen(base);
        char* display = malloc(basename_len + 2);
        if (!display) continue;

        display[0] = prefix;
        memcpy(display + 1, base, basename_len);
        display[basename_len + 1] = '\0';

        int new_num = osk->control_set.num_keys + 1;
        SpecialKey* tmp = realloc(osk->control_set.keys, new_num * sizeof(SpecialKey));
        if (!tmp) {
            free(display);
            continue;
        }

        osk->control_set.keys = tmp;
        SpecialKey* key = &osk->control_set.keys[osk->control_set.num_keys];
        memset(key, 0, sizeof(SpecialKey));
        key->display_name = display;
        key->type = loaded ? SK_UNLOAD_FILE : SK_LOAD_FILE;
        key->sequence = strdup(path);
        key->keycode = SDLK_UNKNOWN;
        key->mod = KMOD_NONE;
        key->command = CMD_NONE;
        osk->control_set.num_keys = new_num;
    }

    if (osk->char_idx >= osk->control_set.num_keys) {
        osk->char_idx = osk->control_set.num_keys - 1;
    }
    if (osk->char_idx < 0) osk->char_idx = 0;

    // Free any previously built list (not the built-in control_set itself, which
    // lives at osk->control_set and is freed separately).
    if (osk->all_special_sets) {
        // Only free the dynamically parsed sets; index 0 is the control_set
        // (owned by osk) and must not be freed here.
        for (int i = 1; i < osk->num_total_special_sets; i++) {
            free_special_key_set_contents(&osk->all_special_sets[i]);
        }
        free(osk->all_special_sets);
        osk->all_special_sets = NULL;
        osk->num_total_special_sets = 0;
    }

    int num_dynamic = osk->num_loaded_key_sets;
    int total = num_dynamic + 1; // +1 for the built-in control set

    osk->all_special_sets = calloc(total, sizeof(SpecialKeySet));
    if (!osk->all_special_sets) {
        ERROR_LOG("Failed to allocate special set list");
        return;
    }

    // Index 0 is always the built-in control set (displayed when no dynamic
    // set is selected).
    osk->all_special_sets[0] = osk->control_set;
    osk->num_total_special_sets = 1;

    for (int i = 0; i < num_dynamic; i++) {
        const char* path = osk->loaded_key_set_names[i];
        if (!path) continue;
        SpecialKeySet parsed = osk_parse_key_set_file(osk, path);
        if (parsed.keys && parsed.num_keys > 0) {
            osk->all_special_sets[osk->num_total_special_sets++] = parsed;
        } else {
            free_special_key_set_contents(&parsed);
        }
    }

    // Clamp the current selection into range.
    if (osk->set_idx < 0 || osk->set_idx >= osk->num_total_special_sets) {
        osk->set_idx = 0;
    }

    DEBUG_LOG("Rebuilt control set: %d total special set(s)", osk->num_total_special_sets);
}
