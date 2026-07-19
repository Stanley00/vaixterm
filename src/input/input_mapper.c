#include "input_mapper.h"
#include "error_codes.h"
#include "terminal.h"
#include "terminal_libvterm.h"
#include "event_handler.h"
#include "osk_core.h"
#include "keyboard_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>

// External mappings from original input.c
// Documented design (SPEC.md / config.h):
//   D-Pad      -> arrows
//   A = type/select, B = backspace, Y = space, X = toggle OSK
//   L1/R1/triggers act as held modifiers (handled in event_handler.c),
//   so they are intentionally NOT bound here. Scrolling uses the sticks.
const ControllerButtonMapping controller_button_map[] = {
    {SDL_CONTROLLER_BUTTON_A, ACTION_SELECT},
    {SDL_CONTROLLER_BUTTON_B, ACTION_BACKSPACE},
    {SDL_CONTROLLER_BUTTON_Y, ACTION_SPACE},
    {SDL_CONTROLLER_BUTTON_BACK, ACTION_TAB},
    {SDL_CONTROLLER_BUTTON_X, ACTION_TOGGLE_OSK},
    {SDL_CONTROLLER_BUTTON_START, ACTION_ENTER},
    {SDL_CONTROLLER_BUTTON_DPAD_UP, ACTION_UP},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, ACTION_DOWN},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, ACTION_LEFT},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, ACTION_RIGHT},
    {SDL_CONTROLLER_BUTTON_LEFTSTICK, ACTION_SCROLL_UP},
    {SDL_CONTROLLER_BUTTON_RIGHTSTICK, ACTION_SCROLL_DOWN},
};

const int num_controller_button_mappings = sizeof(controller_button_map) / sizeof(controller_button_map[0]);

// Default keyboard mappings - removed since we handle keys directly

TerminalAction map_cbutton_to_action(SDL_GameControllerButton button)
{
    for (int i = 0; i < num_controller_button_mappings; ++i) {
        if (controller_button_map[i].button == button) {
            DEBUG_LOG("Controller button %d mapped to action %d", button, controller_button_map[i].action);
            return controller_button_map[i].action;
        }
    }
    DEBUG_LOG("Controller button %d has no mapping", button);
    return ACTION_NONE;
}

void init_input_devices(OnScreenKeyboard* osk, const Config* config)
{
    if (!osk || !config) {
        ERROR_LOG("Invalid parameters: osk=%p, config=%p", (void*)osk, (void*)config);
        return;
    }

    // Initialize OSK state
    osk->mod_ctrl = false;
    osk->mod_alt = false;
    osk->mod_gui = false;
    osk->mod_shift = false;

    // Initialize controller subsystem if needed
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            ERROR_LOG("Failed to initialize controller subsystem: %s", SDL_GetError());
        } else {
            INFO_LOG("Controller subsystem initialized");
        }
    }

    // Add game controller mappings for common controllers
    if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") == -1) {
        DEBUG_LOG("No custom controller database found, using defaults");
    }

    // Try to open the first available controller
    int num_joysticks = SDL_NumJoysticks();
    DEBUG_LOG("Found %d joysticks", num_joysticks);
    
    for (int i = 0; i < num_joysticks; i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* controller = SDL_GameControllerOpen(i);
            if (controller) {
                osk->controller = controller;
                osk->joystick = SDL_GameControllerGetJoystick(controller);
                INFO_LOG("Opened game controller: %s", SDL_GameControllerName(controller));
                break;
            } else {
                ERROR_LOG("Failed to open game controller %d: %s", i, SDL_GetError());
            }
        } else {
            DEBUG_LOG("Joystick %d is not a game controller", i);
        }
    }

    // If no game controller found, try to open first joystick as fallback
    if (!osk->controller && num_joysticks > 0) {
        SDL_Joystick* joystick = SDL_JoystickOpen(0);
        if (joystick) {
            osk->joystick = joystick;
            INFO_LOG("Opened fallback joystick: %s", SDL_JoystickName(joystick));
        } else {
            ERROR_LOG("Failed to open fallback joystick: %s", SDL_GetError());
        }
    }

    DEBUG_LOG("Input devices initialized");
}

bool should_process_key(const SDL_KeyboardEvent* key)
{
    if (!key) {
        ERROR_LOG("Invalid parameter: key=%p", (void*)key);
        return false;
    }

    // Don't process modifier keys themselves
    if (is_modifier_key(key->keysym.sym)) {
        return false;
    }

    // Only process key down events
    if (key->type != SDL_KEYDOWN) {
        return false;
    }

    return true;
}

bool is_modifier_key(SDL_Keycode keycode)
{
    switch (keycode) {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
        case SDLK_LCTRL:
        case SDLK_RCTRL:
        case SDLK_LALT:
        case SDLK_RALT:
        case SDLK_LGUI:
        case SDLK_RGUI:
        case SDLK_MODE:
            return true;
        default:
            return false;
    }
}

void process_direct_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd) {
    (void)osk; (void)master_fd;
    if (!term || !needs_render) return;
    
    switch (action) {
        case ACTION_UP:
            terminal_libvterm_key(term, VTERM_KEY_UP, VTERM_MOD_NONE);
            break;
        case ACTION_DOWN:
            terminal_libvterm_key(term, VTERM_KEY_DOWN, VTERM_MOD_NONE);
            break;
        case ACTION_LEFT:
            terminal_libvterm_key(term, VTERM_KEY_LEFT, VTERM_MOD_NONE);
            break;
        case ACTION_RIGHT:
            terminal_libvterm_key(term, VTERM_KEY_RIGHT, VTERM_MOD_NONE);
            break;
        case ACTION_BACKSPACE:
            terminal_libvterm_key(term, VTERM_KEY_BACKSPACE, VTERM_MOD_NONE);
            break;
        case ACTION_ENTER:
            terminal_libvterm_key(term, VTERM_KEY_ENTER, VTERM_MOD_NONE);
            break;
        case ACTION_TAB:
            terminal_libvterm_key(term, VTERM_KEY_TAB, VTERM_MOD_NONE);
            break;
        case ACTION_ESCAPE:
            terminal_libvterm_key(term, VTERM_KEY_ESCAPE, VTERM_MOD_NONE);
            break;
        case ACTION_SPACE:
            terminal_libvterm_unichar(term, ' ', VTERM_MOD_NONE);
            break;
        case ACTION_SELECT:
            terminal_libvterm_unichar(term, ' ', VTERM_MOD_NONE);
            break;
        default:
            break;
    }
}

static void osk_navigate_left(OnScreenKeyboard* osk, bool* needs_render)
{
    if (osk->mode == OSK_MODE_CHARS) {
        const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->current_char_row);
        if (row && row->num_keys > 0) {
            osk->char_idx--;
            if (osk->char_idx < 0) osk->char_idx = row->num_keys - 1;
        }
    } else {
        const SpecialKeySet* active_set = get_active_special_set(osk);
        if (active_set && active_set->num_keys > 0) {
            osk->char_idx--;
            if (osk->char_idx < 0) osk->char_idx = active_set->num_keys - 1;
        }
    }
    *needs_render = true;
}

static void osk_navigate_right(OnScreenKeyboard* osk, bool* needs_render)
{
    if (osk->mode == OSK_MODE_CHARS) {
        const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->current_char_row);
        if (row && row->num_keys > 0) {
            osk->char_idx++;
            if (osk->char_idx >= row->num_keys) osk->char_idx = 0;
        }
    } else {
        const SpecialKeySet* active_set = get_active_special_set(osk);
        if (active_set && active_set->num_keys > 0) {
            osk->char_idx++;
            if (osk->char_idx >= active_set->num_keys) osk->char_idx = 0;
        }
    }
    *needs_render = true;
}

static void osk_navigate_up(OnScreenKeyboard* osk, bool* needs_render)
{
    if (osk->mode == OSK_MODE_SPECIAL) {
        // Cycle backwards through available key sets (no-op when only one exists)
        if (osk->num_total_special_sets > 1) {
            osk->set_idx = (osk->set_idx - 1 + osk->num_total_special_sets) % osk->num_total_special_sets;
        }
        osk->char_idx = 0;
    } else {
        int num_rows = get_current_num_char_rows(osk);
        if (num_rows > 0) {
            osk->current_char_row--;
            if (osk->current_char_row < 0) osk->current_char_row = num_rows - 1;
            osk_validate_row_index(osk);
            const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->current_char_row);
            if (row && osk->char_idx >= row->num_keys) {
                osk->char_idx = row->num_keys - 1;
            }
        }
    }
    *needs_render = true;
}

static void osk_navigate_down(OnScreenKeyboard* osk, bool* needs_render)
{
    if (osk->mode == OSK_MODE_SPECIAL) {
        // Cycle forwards through available key sets (no-op when only one exists)
        if (osk->num_total_special_sets > 1) {
            osk->set_idx = (osk->set_idx + 1) % osk->num_total_special_sets;
        }
        osk->char_idx = 0;
    } else {
        int num_rows = get_current_num_char_rows(osk);
        if (num_rows > 0) {
            osk->current_char_row++;
            if (osk->current_char_row >= num_rows) osk->current_char_row = 0;
            osk_validate_row_index(osk);
            const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->current_char_row);
            if (row && osk->char_idx >= row->num_keys) {
                osk->char_idx = row->num_keys - 1;
            }
        }
    }
    *needs_render = true;
}

static InternalCommand osk_type_selected_key(Terminal* term, OnScreenKeyboard* osk, bool* needs_render)
{
    const SpecialKey* key = NULL;

    if (osk->mode == OSK_MODE_CHARS) {
        key = osk_get_effective_char_ptr(osk, osk->current_char_row, osk->char_idx);
    } else {
        const SpecialKeySet* active_set = &osk->control_set;
        if (osk->num_total_special_sets > 0 && osk->all_special_sets &&
            osk->set_idx >= 0 && osk->set_idx < osk->num_total_special_sets) {
            active_set = &osk->all_special_sets[osk->set_idx];
        }
        if (osk->char_idx >= 0 && osk->char_idx < active_set->num_keys) {
            key = &active_set->keys[osk->char_idx];
        }
    }

    if (!key) return CMD_NONE;

    switch (key->type) {
        case SK_STRING:
        case SK_SEQUENCE:
        case SK_MACRO:
            if (key->sequence) {
                VTermKey vk = sdl_key_to_vterm(key->keycode);
                if (vk != VTERM_KEY_NONE) {
                    terminal_libvterm_key(term, vk, VTERM_MOD_NONE);
                } else if (key->keycode == SDLK_SPACE) {
                    terminal_libvterm_unichar(term, ' ', VTERM_MOD_NONE);
                } else {
                    terminal_libvterm_feed(term, key->sequence, strlen(key->sequence));
                }
            } else if (key->keycode != SDLK_UNKNOWN) {
                // Encode the key. A literal character from a .kb layout is
                // stored as keycode == the codepoint itself (e.g. 'q'),
                // for which sdl_key_to_vterm() returns VTERM_KEY_NONE.
                // In that case send it as a Unicode char; for real
                // special keys (Backspace, Tab, Enter, Esc, arrows,
                // F-keys...) sdl_key_to_vterm() maps them correctly.
                VTermKey vk = sdl_key_to_vterm(key->keycode);
                if (vk != VTERM_KEY_NONE) {
                    terminal_libvterm_key(term, vk, VTERM_MOD_NONE);
                } else {
                    terminal_libvterm_unichar(term, (uint32_t)key->keycode, VTERM_MOD_NONE);
                }
            }
            break;
        case SK_MOD_CTRL:
            osk->mod_ctrl = !osk->mod_ctrl;
            break;
        case SK_MOD_ALT:
            osk->mod_alt = !osk->mod_alt;
            break;
        case SK_MOD_SHIFT:
            osk->mod_shift = !osk->mod_shift;
            break;
        case SK_MOD_GUI:
            osk->mod_gui = !osk->mod_gui;
            break;
        case SK_LOAD_FILE:
            if (key->sequence) {
                osk_add_custom_set(osk, key->sequence);
            }
            *needs_render = true;
            return CMD_NONE;
        case SK_UNLOAD_FILE:
            if (key->sequence) {
                const char* name = strrchr(key->sequence, '/');
                name = name ? name + 1 : key->sequence;
                osk_remove_custom_set(osk, name);
            }
            *needs_render = true;
            return CMD_NONE;
        case SK_INTERNAL_CMD:
            *needs_render = true;
            return key->command;
        default:
            break;
    }
    *needs_render = true;
    return CMD_NONE;
}

InternalCommand process_osk_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd)
{
    if (!term || !osk || !needs_render) {
        return CMD_NONE;
    }

    switch (action) {
        case ACTION_SCROLL_UP:
            terminal_scroll_view(term, SDL_max(1, term->rows / 2), needs_render);
            return CMD_NONE;
        case ACTION_SCROLL_DOWN:
            terminal_scroll_view(term, -SDL_max(1, term->rows / 2), needs_render);
            return CMD_NONE;
        case ACTION_LEFT:
            osk_navigate_left(osk, needs_render);
            return CMD_NONE;
        case ACTION_RIGHT:
            osk_navigate_right(osk, needs_render);
            return CMD_NONE;
        case ACTION_UP:
            osk_navigate_up(osk, needs_render);
            return CMD_NONE;
        case ACTION_DOWN:
            osk_navigate_down(osk, needs_render);
            return CMD_NONE;
        case ACTION_SELECT:
            return osk_type_selected_key(term, osk, needs_render);
        case ACTION_ENTER:
            terminal_libvterm_key(term, VTERM_KEY_ENTER, VTERM_MOD_NONE);
            *needs_render = true;
            return CMD_NONE;
        case ACTION_SPACE:
            terminal_libvterm_unichar(term, ' ', VTERM_MOD_NONE);
            *needs_render = true;
            return CMD_NONE;
        case ACTION_BACKSPACE:
            terminal_libvterm_key(term, VTERM_KEY_BACKSPACE, VTERM_MOD_NONE);
            *needs_render = true;
            return CMD_NONE;
        case ACTION_TAB:
            terminal_libvterm_key(term, VTERM_KEY_TAB, VTERM_MOD_NONE);
            *needs_render = true;
            return CMD_NONE;
        case ACTION_ESCAPE:
            terminal_libvterm_key(term, VTERM_KEY_ESCAPE, VTERM_MOD_NONE);
            *needs_render = true;
            return CMD_NONE;
        default:
            process_direct_terminal_action(action, term, osk, needs_render, master_fd);
            return CMD_NONE;
    }
}
