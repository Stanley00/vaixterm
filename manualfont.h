#ifndef MANUALFONT_H
#define MANUALFONT_H

#include "terminal_state.h" // For SDL types

/**
 * @brief Attempts to manually render a semigraphic or special character.
 * Bypasses font rendering for specific Unicode blocks (like Box Drawing,
 * Block Elements, Braille) for superior and consistent results.
 *
 * @param renderer The SDL renderer.
 * @param c The Unicode codepoint.
 * @param rx The top-left x-coordinate of the character cell.
 * @param ry The top-left y-coordinate of the character cell.
 * @param rw The width of the character cell.
 * @param rh The height of the character cell.
 * @param color The color to draw the character with.
 * @return true if the character was handled and drawn, false otherwise.
 */
bool draw_manual_char(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color);

#endif // MANUALFONT_H
