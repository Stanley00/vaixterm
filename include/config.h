#ifndef INPUT_CONFIG_H
#define INPUT_CONFIG_H

#include <SDL_gamecontroller.h>

/**
 * @file config.h
 * @brief Compile-time configuration for controller input mappings and default paths.
 *
 * This file allows you to easily change which controller buttons and triggers
 * correspond to which actions and modifiers without modifying the core logic.
 * It also defines default paths for key set files, which
 * can be overridden at compile time using -D flags (e.g., -DDEFAULT_KEY_SET_LIST_PATH="\"/etc/vaixterm/key-set.list\"").
 */

// --- Controller Modifier Mappings ---
// These map physical controller buttons/triggers to held modifier states.
// The default layout is: L1=Shift, R1=Ctrl, L2=Alt, R2=GUI

#define HELD_MODIFIER_SHIFT_BUTTON SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define HELD_MODIFIER_CTRL_BUTTON  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define HELD_MODIFIER_ALT_TRIGGER  SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define HELD_MODIFIER_GUI_TRIGGER  SDL_CONTROLLER_AXIS_TRIGGERRIGHT

// --- Trigger Press Threshold ---
// The value (0-32767) at which a trigger is considered "pressed".
#define TRIGGER_THRESHOLD 16384 // ~50% pressed

// --- Controller Action Mappings ---
// These map physical controller buttons to abstract terminal actions.
// This is used when the OSK is off, or for actions that are consistent
// across OSK modes (like 'Back' or 'Enter').

#define ACTION_BUTTON_UP          SDL_CONTROLLER_BUTTON_DPAD_UP
#define ACTION_BUTTON_DOWN        SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define ACTION_BUTTON_LEFT        SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define ACTION_BUTTON_RIGHT       SDL_CONTROLLER_BUTTON_DPAD_RIGHT

#define ACTION_BUTTON_SELECT      SDL_CONTROLLER_BUTTON_A
#define ACTION_BUTTON_BACK        SDL_CONTROLLER_BUTTON_B
#define ACTION_BUTTON_TOGGLE_OSK  SDL_CONTROLLER_BUTTON_X
#define ACTION_BUTTON_SPACE       SDL_CONTROLLER_BUTTON_Y

#define ACTION_BUTTON_TAB         SDL_CONTROLLER_BUTTON_BACK
#define ACTION_BUTTON_ENTER       SDL_CONTROLLER_BUTTON_START
// #define ACTION_BUTTON_EXIT        SDL_CONTROLLER_BUTTON_GUIDE // This is now handled by a BACK+START combo in main.c

// --- Optional Action Mappings ---
// These are not essential for basic operation but can be useful.
#define ACTION_BUTTON_SCROLL_UP   SDL_CONTROLLER_BUTTON_LEFTSTICK
#define ACTION_BUTTON_SCROLL_DOWN SDL_CONTROLLER_BUTTON_RIGHTSTICK


// These are config for scroll up/down with buttons. I want to scroll up fast but scroll down slower
#define CONFIG_SCROLL_UP_AMOUNT 8
#define CONFIG_SCROLL_DOWN_AMOUNT 2


// --- Default Terminal Configuration ---
#define DEFAULT_WINDOW_WIDTH 640
#define DEFAULT_WINDOW_HEIGHT 480
#define DEFAULT_FONT_SIZE_POINTS 12
#define DEFAULT_SCROLLBACK_LINES 1000
#define DEFAULT_FONT_FILE_PATH "res/Martian.ttf"
#define DEFAULT_BACKGROUND_IMAGE_PATH NULL // Or "" if you prefer an empty string

// --- Button Repeat Timing ---
// The initial delay before a held button starts repeating.
#define BUTTON_REPEAT_INITIAL_DELAY_MS 250
// The interval between subsequent repeats of a held button.
#define BUTTON_REPEAT_INTERVAL_MS 75
#endif // INPUT_CONFIG_H
