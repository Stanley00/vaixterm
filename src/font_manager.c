/**
 * @file font_manager.c
 * @brief Font management and dynamic font size changing.
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <SDL_ttf.h>

#include "font_manager.h"
#include "terminal.h"
#include "error_codes.h"
#include "osk_core.h"
#include "osk_renderer.h"
#include "glyph_cache.h"

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
        ERROR_LOG("Failed to change font size to %d: %s", new_font_size, TTF_GetError());
        return false;
    }

    int new_char_w, new_char_h;
    if (TTF_SizeUTF8(new_font, "W", &new_char_w, &new_char_h) != 0 || new_char_w <= 0 || new_char_h <= 0) {
        ERROR_LOG("New font size %d has invalid character dimensions", new_font_size);
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

    if (term->glyph_cache) {
        glyph_cache_cleanup(term->glyph_cache);
        glyph_cache_init(term->glyph_cache, GLYPH_CACHE_SIZE);
    }
    osk_key_cache_destroy(osk->key_cache);
    osk->key_cache = osk_key_cache_create();
    osk_invalidate_render_cache(osk);

    struct winsize ws = {
        .ws_row = (unsigned short)new_rows,
        .ws_col = (unsigned short)new_cols,
        .ws_xpixel = (unsigned short)config->win_w,
        .ws_ypixel = (unsigned short)config->win_h
    };
    if (ioctl(master_fd, TIOCSWINSZ, &ws) == -1) {
        ERROR_LOG("ioctl(TIOCSWINSZ) failed on font resize");
    }

    return true;
}
