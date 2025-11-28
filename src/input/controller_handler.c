/**
 * @file controller_handler.c
 * @brief Game controller input handling functionality implementation.
 *
 * This module implements game controller input processing for VaixTerm,
 * optimized for handheld gaming devices.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "controller_handler.h"
#include "input_mapper.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

static SDL_GameController* g_controller = NULL;
static bool g_controller_subsystem_initialized = false;

bool init_controller_subsystem(void)
{
    if (g_controller_subsystem_initialized) {
        DEBUG_LOG("Controller subsystem already initialized");
        return true;
    }

    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            ERROR_LOG("Failed to initialize controller subsystem: %s", SDL_GetError());
            return false;
        }
    }

    // Add game controller mappings database
    char* mapping_path = SDL_GetPrefPath("vaixterm", "");
    if (mapping_path) {
        char db_path[512];
        snprintf(db_path, sizeof(db_path), "%sgamecontrollerdb.txt", mapping_path);
        if (SDL_GameControllerAddMappingsFromFile(db_path) == -1) {
            DEBUG_LOG("No custom controller mappings found at: %s", db_path);
        }
        SDL_free(mapping_path);
    }

    g_controller_subsystem_initialized = true;
    INFO_LOG("Controller subsystem initialized");
    return true;
}

void cleanup_controller_subsystem(void)
{
    if (g_controller) {
        close_controller(g_controller);
        g_controller = NULL;
    }

    if (g_controller_subsystem_initialized) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        g_controller_subsystem_initialized = false;
        DEBUG_LOG("Controller subsystem cleaned up");
    }
}

SDL_GameController* open_controller(int joystick_index)
{
    if (!g_controller_subsystem_initialized) {
        if (!init_controller_subsystem()) {
            return NULL;
        }
    }

    if (g_controller) {
        close_controller(g_controller);
        g_controller = NULL;
    }

    if (!SDL_IsGameController(joystick_index)) {
        ERROR_LOG("Joystick %d is not a game controller", joystick_index);
        return NULL;
    }

    g_controller = SDL_GameControllerOpen(joystick_index);
    if (!g_controller) {
        ERROR_LOG("Failed to open game controller %d: %s", joystick_index, SDL_GetError());
        return NULL;
    }

    INFO_LOG("Opened game controller: %s", get_controller_name(g_controller));
    return g_controller;
}

void close_controller(SDL_GameController* controller)
{
    if (controller) {
        SDL_GameControllerClose(controller);
        if (controller == g_controller) {
            g_controller = NULL;
        }
        DEBUG_LOG("Closed game controller");
    }
}

bool handle_controller_button(SDL_GameControllerButton button, bool state, 
                               int pty_fd, Terminal* term, OnScreenKeyboard* osk, bool* needs_render)
{
    if (!term || !osk || !needs_render) {
        ERROR_LOG("Invalid parameters: term=%p, osk=%p, needs_render=%p", (void*)term, (void*)osk, (void*)needs_render);
        return false;
    }

    if (pty_fd < 0) {
        ERROR_LOG("Invalid PTY file descriptor: %d", pty_fd);
        return false;
    }

    // Map controller button to terminal action using the centralized mapping
    TerminalAction action = map_cbutton_to_action(button);
    if (action == ACTION_NONE) {
        return false;
    }

    // Handle button press
    if (state) {
        // Process the action (this would integrate with the existing action processing system)
        DEBUG_LOG("Controller button pressed: %d -> action %d", button, action);
        
        // For now, just indicate that a render is needed
        *needs_render = true;
    }

    return true;
}

bool handle_controller_axis(SDL_GameControllerAxis axis, int value, 
                             Terminal* term, OnScreenKeyboard* osk, bool* needs_render)
{
    if (!term || !osk || !needs_render) {
        ERROR_LOG("Invalid parameters: term=%p, osk=%p, needs_render=%p", (void*)term, (void*)osk, (void*)needs_render);
        return false;
    }

    // Handle axis motion for navigation
    const int dead_zone = 8000; // 25% of full range
    bool active = (value > dead_zone || value < -dead_zone);

    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX:
        case SDL_CONTROLLER_AXIS_LEFTY:
            // Left stick - could be used for cursor movement or scrolling
            if (active) {
                // Handle left stick movement
                *needs_render = true;
            }
            break;

        case SDL_CONTROLLER_AXIS_RIGHTX:
        case SDL_CONTROLLER_AXIS_RIGHTY:
            // Right stick - could be used for scrolling or OSK navigation
            if (active) {
                // Handle right stick movement
                *needs_render = true;
            }
            break;

        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            // Triggers - could be used for modifier keys
            if (active) {
                // Handle trigger press
                *needs_render = true;
            }
            break;

        default:
            break;
    }

    return active;
}

bool is_navigation_button(SDL_GameControllerButton button)
{
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return true;
        default:
            return false;
    }
}

const char* get_controller_name(SDL_GameController* controller)
{
    if (!controller) {
        ERROR_LOG("Invalid parameter: controller=%p", (void*)controller);
        return NULL;
    }

    return SDL_GameControllerName(controller);
}

bool is_controller_connected(SDL_GameController* controller)
{
    if (!controller) {
        return false;
    }

    return SDL_GameControllerGetAttached(controller);
}

bool get_controller_button_state(SDL_GameController* controller, SDL_GameControllerButton button)
{
    if (!controller) {
        ERROR_LOG("Invalid parameter: controller=%p", (void*)controller);
        return false;
    }

    return SDL_GameControllerGetButton(controller, button) == 1;
}

int16_t get_controller_axis_value(SDL_GameController* controller, SDL_GameControllerAxis axis)
{
    if (!controller) {
        ERROR_LOG("Invalid parameter: controller=%p", (void*)controller);
        return 0;
    }

    return SDL_GameControllerGetAxis(controller, axis);
}

bool handle_controller_connection(int joystick_index, Terminal* term, OnScreenKeyboard* osk)
{
    if (!term || !osk) {
        ERROR_LOG("Invalid parameters: term=%p, osk=%p", (void*)term, (void*)osk);
        return false;
    }

    SDL_GameController* controller = open_controller(joystick_index);
    if (controller) {
        INFO_LOG("Controller connected: %s", get_controller_name(controller));
        return true;
    }

    return false;
}

bool handle_controller_disconnection(int joystick_index, Terminal* term, OnScreenKeyboard* osk) {
    (void)joystick_index;
    (void)osk;
    if (!term || !osk) {
        ERROR_LOG("Invalid parameters: term=%p, osk=%p", (void*)term, (void*)osk);
        return false;
    }

    if (g_controller && SDL_GameControllerGetAttached(g_controller) == SDL_FALSE) {
        INFO_LOG("Controller disconnected");
        close_controller(g_controller);
        return true;
    }

    return false;
}

void update_controller_state(Terminal* term, OnScreenKeyboard* osk, bool* needs_render)
{
    if (!term || !osk || !needs_render) {
        ERROR_LOG("Invalid parameters: term=%p, osk=%p, needs_render=%p", (void*)term, (void*)osk, (void*)needs_render);
        return;
    }

    if (!g_controller || !is_controller_connected(g_controller)) {
        return;
    }

    // Update controller state and handle any continuous inputs
    // This would handle things like stick-held scrolling, etc.
}

bool rumble_controller(SDL_GameController* controller, uint16_t low_frequency, 
                        uint16_t high_frequency, uint32_t duration_ms)
{
    if (!controller) {
        ERROR_LOG("Invalid parameter: controller=%p", (void*)controller);
        return false;
    }

    if (SDL_GameControllerRumble(controller, low_frequency, high_frequency, duration_ms) == 0) {
        DEBUG_LOG("Controller rumble activated: LF=%d, HF=%d, Duration=%dms", 
                  low_frequency, high_frequency, duration_ms);
        return true;
    }

    ERROR_LOG("Failed to activate controller rumble: %s", SDL_GetError());
    return false;
}
