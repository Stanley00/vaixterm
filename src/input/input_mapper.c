/**
 * @file input_mapper.c
 * @brief Input mapping functionality implementation.
 *
 * This module implements mapping of raw input events to terminal actions
 * with proper error handling and validation.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "input_mapper.h"
#include "error_codes.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>

// External mappings from original input.c
extern const ControllerButtonMapping controller_button_map[];
extern const KeyMapping key_map[];
extern const int num_controller_button_mappings;
extern const int num_key_mappings;

// Default controller button mappings
const ControllerButtonMapping controller_button_map[] = {
    {SDL_CONTROLLER_BUTTON_A, ACTION_SELECT},
    {SDL_CONTROLLER_BUTTON_B, ACTION_BACK},
    {SDL_CONTROLLER_BUTTON_Y, ACTION_SPACE},
    {SDL_CONTROLLER_BUTTON_BACK, ACTION_TAB},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, ACTION_SCROLL_UP},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, ACTION_SCROLL_DOWN},
    {SDL_CONTROLLER_BUTTON_X, ACTION_TOGGLE_OSK},
    {SDL_CONTROLLER_BUTTON_START, ACTION_ENTER},
    {SDL_CONTROLLER_BUTTON_DPAD_UP, ACTION_UP},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, ACTION_DOWN},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, ACTION_LEFT},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, ACTION_RIGHT},
};

const int num_controller_button_mappings = sizeof(controller_button_map) / sizeof(controller_button_map[0]);

// Default keyboard mappings - removed since we handle keys directly

TerminalAction map_cbutton_to_action(SDL_GameControllerButton button)
{
    for (int i = 0; i < num_controller_button_mappings; ++i) {
        if (controller_button_map[i].button == button) {
            return controller_button_map[i].action;
        }
    }
    return ACTION_NONE;
}

SDL_Keymod get_combined_modifiers(const OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return KMOD_NONE;
    }

    SDL_Keymod combined = KMOD_NONE;
    if (osk->mod_ctrl) combined |= KMOD_CTRL;
    if (osk->mod_alt) combined |= KMOD_ALT;
    if (osk->mod_gui) combined |= KMOD_GUI;
    if (osk->mod_shift) combined |= KMOD_SHIFT;

    return combined;
}

SDL_Keymod get_effective_send_modifiers(const OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return KMOD_NONE;
    }

    SDL_Keymod effective = get_combined_modifiers(osk);
    
    // Clear one-shot modifiers after use
    if (osk->mod_alt) {
        // Alt is one-shot, clear it
        // Note: This would need to be handled in the calling function
    }
    if (osk->mod_gui) {
        // GUI is one-shot, clear it
        // Note: This would need to be handled in the calling function
    }

    return effective;
}

void clear_one_shot_modifiers(OnScreenKeyboard* osk, bool* needs_render_ptr)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    bool cleared = false;
    if (osk->mod_alt) {
        osk->mod_alt = false;
        cleared = true;
    }
    if (osk->mod_gui) {
        osk->mod_gui = false;
        cleared = true;
    }

    if (cleared && needs_render_ptr) {
        *needs_render_ptr = true;
    }

    DEBUG_LOG("Cleared one-shot modifiers");
}

SDL_Keymod osk_mask_to_sdl_mod(int osk_mask)
{
    SDL_Keymod sdl_mod = KMOD_NONE;
    if (osk_mask & OSK_MOD_CTRL) sdl_mod |= KMOD_CTRL;
    if (osk_mask & OSK_MOD_ALT) sdl_mod |= KMOD_ALT;
    if (osk_mask & OSK_MOD_GUI) sdl_mod |= KMOD_GUI;
    if (osk_mask & OSK_MOD_SHIFT) sdl_mod |= KMOD_SHIFT;
    return sdl_mod;
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

TerminalAction get_key_combination_action(SDL_Keycode keycode, SDL_Keymod mod)
{
    // Check for specific key combinations
    if (mod & KMOD_CTRL) {
        switch (keycode) {
            case SDLK_c: return ACTION_COPY;
            case SDLK_v: return ACTION_PASTE;
            case SDLK_z: return ACTION_UNDO;
            case SDLK_y: return ACTION_REDO;
            case SDLK_f: return ACTION_FIND;
            case SDLK_s: return ACTION_SAVE;
            case SDLK_o: return ACTION_OPEN;
            case SDLK_q: return ACTION_QUIT;
            default: break;
        }
    }

    if (mod & KMOD_ALT) {
        switch (keycode) {
            case SDLK_TAB: return ACTION_WINDOW_NEXT;
            case SDLK_F4: return ACTION_WINDOW_CLOSE;
            default: break;
        }
    }

    if (mod & (KMOD_CTRL | KMOD_SHIFT)) {
        switch (keycode) {
            case SDLK_c: return ACTION_COPY_COLUMN;
            case SDLK_v: return ACTION_PASTE_COLUMN;
            case SDLK_z: return ACTION_REDO;
            default: break;
        }
    }

    return ACTION_NONE;
}

bool is_modifier_pressed(SDL_Keymod mod, SDL_Keymod modifier)
{
    return (mod & modifier) != 0;
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
    (void)osk;
    if (!term || !needs_render) {
        return;
    }
    
    char seq[32];
    switch (action) {
        case ACTION_UP:
            strcpy(seq, "\033[A");
            write(master_fd, seq, strlen(seq));
            break;
        case ACTION_DOWN:
            strcpy(seq, "\033[B");
            write(master_fd, seq, strlen(seq));
            break;
        case ACTION_LEFT:
            strcpy(seq, "\033[D");
            write(master_fd, seq, strlen(seq));
            break;
        case ACTION_RIGHT:
            strcpy(seq, "\033[C");
            write(master_fd, seq, strlen(seq));
            break;
        case ACTION_BACKSPACE:
            write(master_fd, "\177", 1);
            break;
        case ACTION_ENTER:
            write(master_fd, "\r", 1);
            break;
        case ACTION_TAB:
            write(master_fd, "\t", 1);
            break;
        case ACTION_ESCAPE:
            write(master_fd, "\033", 1);
            break;
        case ACTION_SPACE:
            write(master_fd, " ", 1);
            break;
        default:
            break;
    }
}

InternalCommand process_osk_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd)
{
    if (!term || !osk || !needs_render) {
        return CMD_NONE;
    }
    
    switch (action) {
        case ACTION_TOGGLE_OSK:
            osk->active = !osk->active;
            *needs_render = true;
            return CMD_NONE;
        case ACTION_SCROLL_UP:
            terminal_scroll_up(term);
            *needs_render = true;
            return CMD_NONE;
        case ACTION_SCROLL_DOWN:
            // terminal_scroll_down doesn't exist, use scroll_up with negative logic
            // For now, just mark as needs_render
            *needs_render = true;
            return CMD_NONE;
        default:
            process_direct_terminal_action(action, term, osk, needs_render, master_fd);
            return CMD_NONE;
    }
}
