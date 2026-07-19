#ifndef TERMINAL_LIBVTERM_H
#define TERMINAL_LIBVTERM_H

#include <vterm.h>
#include "terminal_state.h"

#ifdef __cplusplus
extern "C" {
#endif

bool terminal_libvterm_init(Terminal* term, int cols, int rows);
void terminal_libvterm_free(Terminal* term);

void terminal_libvterm_reset(Terminal* term, bool hard);

void terminal_libvterm_feed(Terminal* term, const char* data, size_t len);
void terminal_libvterm_key(Terminal* term, VTermKey key, VTermModifier mod);
void terminal_libvterm_unichar(Terminal* term, uint32_t c, VTermModifier mod);

void terminal_libvterm_resize(Terminal* term, int rows, int cols);

void terminal_libvterm_set_default_colors(Terminal* term, SDL_Color fg, SDL_Color bg);
void terminal_libvterm_set_palette_color(Terminal* term, int index, SDL_Color color);
void terminal_libvterm_flush_damage(Terminal* term);

size_t terminal_libvterm_flush_output(Terminal* term, char* dst, size_t dst_len);

Glyph* terminal_libvterm_get_view_line(Terminal* term, int y);
int terminal_libvterm_get_scrollback_count(Terminal* term);

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_LIBVTERM_H
