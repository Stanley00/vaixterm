/**
 * @file keyboard_handler.c
 * @brief Keyboard input handling — all encoding delegated to libvterm.
 *
 * Special keys (arrows, function keys, etc.) go through vterm_keyboard_key().
 * All other input (letters, numbers, symbols, Ctrl/Shift/Alt combos) goes
 * through vterm_keyboard_unichar() which handles control character encoding,
 * DECCKM, and escape sequence generation internally.
 */

#include "keyboard_handler.h"
#include "input_mapper.h"
#include "terminal_libvterm.h"
#include "error_codes.h"
#include <SDL.h>

int utf8_char_len(const char* s)
{
    if (!s) return 0;
    unsigned char b = (unsigned char)*s;
    if ((b & 0x80) == 0x00) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;
}

static VTermModifier sdl_mod_to_vterm(SDL_Keymod mod)
{
    VTermModifier vmod = VTERM_MOD_NONE;
    if (mod & KMOD_SHIFT) vmod |= VTERM_MOD_SHIFT;
    if (mod & KMOD_ALT)   vmod |= VTERM_MOD_ALT;
    if (mod & KMOD_CTRL)  vmod |= VTERM_MOD_CTRL;
    return vmod;
}

VTermKey sdl_key_to_vterm(SDL_Keycode sym)
{
    switch (sym) {
        case SDLK_RETURN:    return VTERM_KEY_ENTER;
        case SDLK_TAB:       return VTERM_KEY_TAB;
        case SDLK_BACKSPACE: return VTERM_KEY_BACKSPACE;
        case SDLK_ESCAPE:    return VTERM_KEY_ESCAPE;
        case SDLK_UP:        return VTERM_KEY_UP;
        case SDLK_DOWN:      return VTERM_KEY_DOWN;
        case SDLK_LEFT:      return VTERM_KEY_LEFT;
        case SDLK_RIGHT:     return VTERM_KEY_RIGHT;
        case SDLK_INSERT:    return VTERM_KEY_INS;
        case SDLK_DELETE:    return VTERM_KEY_DEL;
        case SDLK_HOME:      return VTERM_KEY_HOME;
        case SDLK_END:       return VTERM_KEY_END;
        case SDLK_PAGEUP:    return VTERM_KEY_PAGEUP;
        case SDLK_PAGEDOWN:  return VTERM_KEY_PAGEDOWN;
        case SDLK_KP_ENTER:  return VTERM_KEY_KP_ENTER;
        case SDLK_KP_0: return VTERM_KEY_KP_0;
        case SDLK_KP_1: return VTERM_KEY_KP_1;
        case SDLK_KP_2: return VTERM_KEY_KP_2;
        case SDLK_KP_3: return VTERM_KEY_KP_3;
        case SDLK_KP_4: return VTERM_KEY_KP_4;
        case SDLK_KP_5: return VTERM_KEY_KP_5;
        case SDLK_KP_6: return VTERM_KEY_KP_6;
        case SDLK_KP_7: return VTERM_KEY_KP_7;
        case SDLK_KP_8: return VTERM_KEY_KP_8;
        case SDLK_KP_9: return VTERM_KEY_KP_9;
        case SDLK_KP_MINUS:   return VTERM_KEY_KP_MINUS;
        case SDLK_KP_PLUS:    return VTERM_KEY_KP_PLUS;
        case SDLK_KP_MULTIPLY: return VTERM_KEY_KP_MULT;
        case SDLK_KP_DIVIDE:  return VTERM_KEY_KP_DIVIDE;
        case SDLK_KP_PERIOD:  return VTERM_KEY_KP_PERIOD;
        case SDLK_KP_COMMA:   return VTERM_KEY_KP_COMMA;
        case SDLK_KP_EQUALS:  return VTERM_KEY_KP_EQUAL;
        default:
            if (sym >= SDLK_F1 && sym <= SDLK_F12)
                return VTERM_KEY_FUNCTION(sym - SDLK_F1 + 1);
            return VTERM_KEY_NONE;
    }
}

void handle_key_down(const SDL_KeyboardEvent* key, Terminal* term)
{
    if (!key || !term) return;
    if (!should_process_key(key)) return;

    VTermKey vkey = sdl_key_to_vterm(key->keysym.sym);
    VTermModifier vmod = sdl_mod_to_vterm(key->keysym.mod);

    if (vkey != VTERM_KEY_NONE) {
        terminal_libvterm_key(term, vkey, vmod);
    } else if (vmod & (VTERM_MOD_CTRL | VTERM_MOD_ALT)) {
        SDL_Keycode sym = key->keysym.sym;
        if (sym >= 'a' && sym <= 'z') {
            terminal_libvterm_unichar(term, (uint32_t)sym, vmod);
        } else if (sym >= ' ' && sym < 128) {
            terminal_libvterm_unichar(term, (uint32_t)sym, vmod);
        }
    }

    DEBUG_LOG("Key: sym=%d mod=%d vkey=%d", key->keysym.sym, key->keysym.mod, vkey);
}
