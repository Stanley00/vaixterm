/**
 * @file font_manager.c
 * @brief Font management and dynamic font size changing.
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <SDL_ttf.h>

#include "font_manager.h"
#include "terminal.h"
#include "osk.h"
#include "cache_manager.h"

/**
 * @brief Changes the font size and updates all related components.
 */
bool font_change_size(TTF_Font** font, Config* config, Terminal* term, OnScreenKeyboard* osk,
                     int* char_w, int* char_h, int master_fd, int delta)
{
    int new_font_size = config->font_size + delta;
    if (new_font_size < 6 || new_font_size > 72) {
        return false;
    }

    TTF_Font* new_font = TTF_OpenFont(config->font_path, new_font_size);
    if (!new_font) {
        fprintf(stderr, "Failed to change font size to %d: %s\n", new_font_size, TTF_GetError());
        return false;
    }

    int new_char_w, new_char_h;
    if (TTF_SizeUTF8(new_font, "W", &new_char_w, &new_char_h) != 0 || new_char_w <= 0 || new_char_h <= 0) {
        fprintf(stderr, "New font size %d has invalid character dimensions.\n", new_font_size);
        TTF_CloseFont(new_font);
        return false;
    }

    TTF_CloseFont(*font);
    *font = new_font;
    config->font_size = new_font_size;
    *char_w = new_char_w;
    *char_h = new_char_h;

    int new_cols = config->win_w / *char_w;
    int new_rows = config->win_h / *char_h;
    terminal_resize(term, new_cols, new_rows);

    glyph_cache_destroy(term->glyph_cache);
    term->glyph_cache = glyph_cache_create();
    osk_key_cache_destroy(osk->key_cache);
    osk->key_cache = osk_key_cache_create();
    osk->cached_set_idx = -1;
    osk->cached_mod_mask = -1;

    struct winsize ws = {
        .ws_row = (unsigned short)new_rows,
        .ws_col = (unsigned short)new_cols,
        .ws_xpixel = (unsigned short)config->win_w,
        .ws_ypixel = (unsigned short)config->win_h
    };
    if (ioctl(master_fd, TIOCSWINSZ, &ws) == -1) {
        perror("ioctl(TIOCSWINSZ) failed on font resize");
    }

    return true;
}
