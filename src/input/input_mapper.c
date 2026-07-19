#include "input_mapper.h"
#include "error_codes.h"
#include "error_codes.h"
#include "terminal.h"
#include "terminal_libvterm.h"
#include "event_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>

// External mappings from original input.c
const ControllerButtonMapping controller_button_map[] = {
    {SDL_CONTROLLER_BUTTON_A, ACTION_SELECT},
    {SDL_CONTROLLER_BUTTON_B, ACTION_BACKSPACE},  // Fixed: B should be backspace
    {SDL_CONTROLLER_BUTTON_Y, ACTION_SPACE},
    {SDL_CONTROLLER_BUTTON_BACK, ACTION_TAB},
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
            terminal_scroll_view(term, SDL_max(1, term->rows / 2), needs_render);
            return CMD_NONE;
        case ACTION_SCROLL_DOWN:
            terminal_scroll_view(term, -SDL_max(1, term->rows / 2), needs_render);
            return CMD_NONE;
        default:
            process_direct_terminal_action(action, term, osk, needs_render, master_fd);
            return CMD_NONE;
    }
}
