#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "event_handler.h"
#include "terminal_state.h"
#include "terminal.h"
#include "terminal_libvterm.h"
#include "osk_core.h"
#include "input_mapper.h"
#include "keyboard_handler.h"
#include "config.h"
#include "font_manager.h"
#include "error_codes.h"

void terminal_scroll_view(Terminal* term, int amount, bool* needs_render)
{
    // Don't scroll when in alternate screen or no history
    int sb_count = terminal_get_scrollback_count(term);
    if (term->alt_screen_active || sb_count == 0) {
        return;
    }

    int old_offset = term->view_offset;
    int new_offset = term->view_offset + amount;

    term->view_offset = SDL_max(0, SDL_min(sb_count, new_offset));

    if (term->view_offset != old_offset) {
        *needs_render = true;
        term->full_redraw_needed = true;
    }
}

static bool handle_held_modifier_button(SDL_GameControllerButton button, bool pressed, 
                                       OnScreenKeyboard* osk, bool* needs_render)
{
    bool* held_flag = NULL;

    if (button == HELD_MODIFIER_SHIFT_BUTTON) {
        held_flag = &osk->held_shift;
    } else if (button == HELD_MODIFIER_CTRL_BUTTON) {
        held_flag = &osk->held_ctrl;
    } else {
        return false; // Not a modifier button
    }

    if (pressed != *held_flag) {
        *held_flag = pressed;
        *needs_render = true;
        osk_validate_row_index(osk);
    }

    return true;
}

static void handle_held_modifier_axis(SDL_GameControllerAxis axis, Sint16 value, 
                                     OnScreenKeyboard* osk, bool* needs_render)
{
    bool pressed = (value > TRIGGER_THRESHOLD);
    bool* held_flag = NULL;

    if (axis == HELD_MODIFIER_ALT_TRIGGER) {
        held_flag = &osk->held_alt;
    } else if (axis == HELD_MODIFIER_GUI_TRIGGER) {
        held_flag = &osk->held_gui;
    } else {
        return; // Not a modifier axis
    }

    if (pressed != *held_flag) {
        *held_flag = pressed;
        *needs_render = true;
        osk_validate_row_index(osk);
    }
}

static void execute_internal_command(InternalCommand cmd, Terminal* term, bool* needs_render, 
                                   int pty_fd, TTF_Font** font, Config* config, 
                                   OnScreenKeyboard* osk, int* char_w, int* char_h)
{
    switch (cmd) {
    case CMD_FONT_INC:
        if (font_change_size(font, config, term, osk, char_w, char_h, pty_fd, 1)) {
            *needs_render = true;
        }
        break;
    case CMD_FONT_DEC:
        if (font_change_size(font, config, term, osk, char_w, char_h, pty_fd, -1)) {
            *needs_render = true;
        }
        break;
    case CMD_CURSOR_TOGGLE_VISIBILITY:
        term->cursor_visible = !term->cursor_visible;
        *needs_render = true;
        break;
    case CMD_CURSOR_TOGGLE_BLINK:
        term->cursor_style_blinking = !term->cursor_style_blinking;
        *needs_render = true;
        break;
    case CMD_CURSOR_CYCLE_STYLE:
        term->cursor_style = (term->cursor_style + 1) % 3;
        *needs_render = true;
        break;
    case CMD_TERMINAL_RESET:
        terminal_reset(term);
        *needs_render = true;
        break;
    case CMD_TERMINAL_CLEAR:
        terminal_libvterm_feed(term, "\x1b[2J\x1b[H", 7);
        term->full_redraw_needed = true;
        *needs_render = true;
        break;
    case CMD_OSK_TOGGLE_POSITION:
        osk->position_mode = (osk->position_mode == OSK_POSITION_OPPOSITE) ? OSK_POSITION_SAME : OSK_POSITION_OPPOSITE;
        *needs_render = true;
        break;
    case CMD_RELOAD_THEME:
        if (config->colorscheme_path) {
            terminal_load_colorscheme(term, config->colorscheme_path);
            *needs_render = true;
            INFO_LOG("Theme reloaded: %s", config->colorscheme_path);
        }
        break;
    case CMD_NONE:
        break;
    }
}

void event_handle_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, 
                                 bool* needs_render, int master_fd, TTF_Font** font, 
                                 Config* config, int* char_w, int* char_h)
{
    switch (action) {
    case ACTION_SCROLL_UP:
        terminal_scroll_view(term, SDL_max(1, term->rows / 2), needs_render);
        break;
    case ACTION_SCROLL_DOWN:
        terminal_scroll_view(term, -MOUSE_WHEEL_SCROLL_AMOUNT, needs_render);
        break;
    case ACTION_RELOAD_THEME:
        execute_internal_command(CMD_RELOAD_THEME, term, needs_render, master_fd, font, config, osk, char_w, char_h);
        break;
    default: {
        InternalCommand cmd = CMD_NONE;
        if (osk->active) {
            cmd = process_osk_action(action, term, osk, needs_render, master_fd);
        } else {
            process_direct_terminal_action(action, term, osk, needs_render, master_fd);
        }

        if (cmd != CMD_NONE) {
            execute_internal_command(cmd, term, needs_render, master_fd, font, config, osk, char_w, char_h);
        }
        break;
    }
    }
}

void event_process_and_repeat_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, 
                                    bool* needs_render, int master_fd, TTF_Font** font, 
                                    Config* config, int* char_w, int* char_h, 
                                    ButtonRepeatState* repeat_state)
{
    if (action == ACTION_NONE) {
        return;
    }

    if (repeat_state->is_held && repeat_state->action == action) {
        return;
    }

    event_handle_terminal_action(action, term, osk, needs_render, master_fd, font, config, char_w, char_h);
    repeat_state->is_held = true;
    repeat_state->action = action;
    repeat_state->next_repeat_time = SDL_GetTicks() + BUTTON_REPEAT_INITIAL_DELAY_MS;
}

void event_stop_repeating_action(TerminalAction action, ButtonRepeatState* repeat_state)
{
    if (repeat_state->is_held && repeat_state->action == action) {
        repeat_state->is_held = false;
    }
}

static int check_exit_event(SDL_Event* event, OnScreenKeyboard* osk)
{
    switch (event->type) {
    case SDL_CONTROLLERBUTTONDOWN: {
        if (event->cbutton.button == ACTION_BUTTON_TAB) { // BACK button
            osk->held_back = true;
        }
        if (event->cbutton.button == ACTION_BUTTON_ENTER) { // START button
            osk->held_start = true;
        }
        if (osk->held_back && osk->held_start) {
            return 1; // Exit immediately
        }
        break;
    }
    case SDL_CONTROLLERBUTTONUP: {
        if (event->cbutton.button == ACTION_BUTTON_TAB) { // BACK button
            osk->held_back = false;
        }
        if (event->cbutton.button == ACTION_BUTTON_ENTER) { // START button
            osk->held_start = false;
        }
        break;
    }
    }
    return 0;
}

/**
 * @brief Toggles the OSK state through its modes.
 */
static void toggle_osk_state(OnScreenKeyboard* osk, bool* needs_render)
{
    if (!osk->active) {
        // State: OFF. Action: Turn ON to CHARS mode.
        osk->active = true;
        osk->mode = OSK_MODE_CHARS;
        osk->set_idx = 0;
        osk->char_idx = 0;
        osk_validate_row_index(osk);
        osk->show_special_set_name = false;
    } else if (osk->mode == OSK_MODE_CHARS) {
        // State: ON, CHARS mode. Action: Switch to SPECIAL mode.
        osk->mode = OSK_MODE_SPECIAL;
        osk->set_idx = 0;
        osk->char_idx = 0;
        osk_validate_row_index(osk);
        osk->show_special_set_name = true;
    } else { // State: ON, SPECIAL mode.
        bool any_one_shot_modifier_active = osk->mod_ctrl || osk->mod_alt || osk->mod_shift || osk->mod_gui;
        if (any_one_shot_modifier_active) {
            // If modifiers are on, go back to CHARS mode but keep modifiers.
            osk->mode = OSK_MODE_CHARS;
            osk_validate_row_index(osk);
            osk->show_special_set_name = false;
        } else {
            // If no modifiers are on, turn the OSK OFF.
            osk->active = false;
            osk->show_special_set_name = false;
        }
    }
    *needs_render = true;
}

void event_handle(SDL_Event* event, bool* running, bool* needs_render, Terminal* term, 
                 OnScreenKeyboard* osk, int master_fd, TTF_Font** font, Config* config, 
                 int* char_w, int* char_h, ButtonRepeatState* repeat_state)
{
    if (!event) {
        return;
    }

    if (event->type == SDL_QUIT) {
        *running = false;
        return;
    }

    if (check_exit_event(event, osk)) {
        *running = false;
        return;
    }
    
    if (config->read_only) {
        return;
    }

    switch (event->type) {
    case SDL_TEXTINPUT: {
        const char* text = event->text.text;
        while (*text) {
            uint32_t codepoint = 0;
            unsigned char c = (unsigned char)*text;
            int bytes = 0;
            if (c < 0x80) {
                codepoint = c;
                bytes = 1;
            } else if ((c & 0xE0) == 0xC0) {
                codepoint = (c & 0x1F) << 6;
                codepoint |= (unsigned char)text[1] & 0x3F;
                bytes = 2;
            } else if ((c & 0xF0) == 0xE0) {
                codepoint = (c & 0x0F) << 12;
                codepoint |= ((unsigned char)text[1] & 0x3F) << 6;
                codepoint |= (unsigned char)text[2] & 0x3F;
                bytes = 3;
            } else if ((c & 0xF8) == 0xF0) {
                codepoint = (c & 0x07) << 18;
                codepoint |= ((unsigned char)text[1] & 0x3F) << 12;
                codepoint |= ((unsigned char)text[2] & 0x3F) << 6;
                codepoint |= (unsigned char)text[3] & 0x3F;
                bytes = 4;
            } else {
                bytes = 1;
            }
            if (codepoint >= 0x20) {
                terminal_libvterm_unichar(term, codepoint, VTERM_MOD_NONE);
            }
            text += bytes;
        }
        break;
    }
        
    case SDL_MOUSEWHEEL:
        terminal_scroll_view(term, event->wheel.y * MOUSE_WHEEL_SCROLL_AMOUNT, needs_render);
        break;
        
    /* ===== KEYBOARD INPUT (primary) ===== */
    case SDL_KEYDOWN: {
        handle_key_down(&event->key, master_fd, term);
        break;
    }
    
    /* ===== CONTROLLER INPUT (secondary) ===== */
    case SDL_CONTROLLERAXISMOTION: {
        handle_held_modifier_axis(event->caxis.axis, event->caxis.value, osk, needs_render);
        break;
    }
    
    case SDL_CONTROLLERBUTTONDOWN: {
        TerminalAction action = map_cbutton_to_action(event->cbutton.button);
        
        if (action == ACTION_TOGGLE_OSK) {
            toggle_osk_state(osk, needs_render);
            return;
        }
        
        if (osk->active && handle_held_modifier_button(event->cbutton.button, true, osk, needs_render)) {
            break;
        }
        
        event_process_and_repeat_action(action, term, osk, needs_render, master_fd, font, config, char_w, char_h, repeat_state);
        break;
    }
    
    case SDL_CONTROLLERBUTTONUP: {
        if (osk->active && handle_held_modifier_button(event->cbutton.button, false, osk, needs_render)) {
            break;
        }
        
        TerminalAction action = map_cbutton_to_action(event->cbutton.button);
        event_stop_repeating_action(action, repeat_state);
        break;
    }
    
    case SDL_CONTROLLERDEVICEADDED:
        if (!osk->controller) {
            SDL_GameController* new_controller = SDL_GameControllerOpen(event->cdevice.which);
            if (new_controller) {
                if (osk->joystick) {
                    SDL_JoystickClose(osk->joystick);
                    osk->joystick = NULL;
                }
                osk->controller = new_controller;
                INFO_LOG("Game Controller connected: %s", SDL_GameControllerName(osk->controller));
            }
        }
        break;
        
    case SDL_CONTROLLERDEVICEREMOVED:
        if (osk->controller && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(osk->controller)) == event->cdevice.which) {
            INFO_LOG("Controller disconnected");
            SDL_GameControllerClose(osk->controller);
            osk->controller = NULL;
            init_input_devices(osk, config);
        }
        break;
        
    default:
        break;
    }
}
