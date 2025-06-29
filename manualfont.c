#include "manualfont.h"
#include <math.h> // For roundf

#define SWAP_INT(a, b) do { int t = a; a = b; b = t; } while (0)

// --- Internal Helper Prototypes ---
static bool draw_box_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color);
static bool draw_braille_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color);
static bool draw_block_element_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color);
static bool draw_geometric_shape_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color);
static void fill_triangle(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3);


bool draw_manual_char(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color)
{
    if (c >= 0x2500 && c <= 0x257F) {
        return draw_box_character(renderer, c, rx, ry, rw, rh, color);
    }
    if (c >= 0x2580 && c <= 0x259F) {
        return draw_block_element_character(renderer, c, rx, ry, rw, rh, color);
    }
    if (c >= 0x25A0 && c <= 0x25FF) {
        return draw_geometric_shape_character(renderer, c, rx, ry, rw, rh, color);
    }
    if (c >= 0x2800 && c <= 0x28FF) {
        return draw_braille_character(renderer, c, rx, ry, rw, rh, color);
    }

    return false; // Character not handled by manual renderer
}

// --- Box Drawing Data-Driven Implementation ---

// Flags to describe which parts of a box character to draw
#define BOX_U (1 << 0) // Up
#define BOX_D (1 << 1) // Down
#define BOX_L (1 << 2) // Left
#define BOX_R (1 << 3) // Right

#define BOX_HU (1 << 4) // Heavy Up
#define BOX_HD (1 << 5) // Heavy Down
#define BOX_HL (1 << 6) // Heavy Left
#define BOX_HR (1 << 7) // Heavy Right

#define BOX_DU (1 << 8)  // Double-line Up
#define BOX_DD (1 << 9)  // Double-line Down
#define BOX_DL (1 << 10) // Double-line Left
#define BOX_DR (1 << 11) // Double-line Right

typedef struct {
    uint32_t c;
    uint16_t flags;
} BoxCharDef;

// Map of box-drawing characters to their drawing flags
static const BoxCharDef box_defs[] = {
    // Light
    {0x2500, BOX_L | BOX_R}, {0x2502, BOX_U | BOX_D}, {0x250C, BOX_D | BOX_R},
    {0x2510, BOX_D | BOX_L}, {0x2514, BOX_U | BOX_R}, {0x2518, BOX_U | BOX_L},
    {0x251C, BOX_U | BOX_D | BOX_R}, {0x2524, BOX_U | BOX_D | BOX_L},
    {0x252C, BOX_L | BOX_R | BOX_D}, {0x2534, BOX_L | BOX_R | BOX_U},
    {0x253C, BOX_U | BOX_D | BOX_L | BOX_R},
    // Heavy
    {0x2501, BOX_HL | BOX_HR}, {0x2503, BOX_HU | BOX_HD}, {0x250F, BOX_HD | BOX_HR},
    {0x2513, BOX_HD | BOX_HL}, {0x2517, BOX_HU | BOX_HR}, {0x251B, BOX_HU | BOX_HL},
    {0x2523, BOX_HU | BOX_HD | BOX_HR}, {0x252B, BOX_HU | BOX_HD | BOX_HL},
    {0x2533, BOX_HL | BOX_HR | BOX_HD}, {0x253B, BOX_HL | BOX_HR | BOX_HU},
    {0x254B, BOX_HU | BOX_HD | BOX_HL | BOX_HR},
    // Double
    {0x2550, BOX_DL | BOX_DR}, {0x2551, BOX_DU | BOX_DD}, {0x2554, BOX_DD | BOX_DR},
    {0x2557, BOX_DD | BOX_DL}, {0x255A, BOX_DU | BOX_DR}, {0x255D, BOX_DU | BOX_DL},
    {0x2560, BOX_DU | BOX_DD | BOX_DR}, {0x2563, BOX_DU | BOX_DD | BOX_DL},
    {0x2566, BOX_DL | BOX_DR | BOX_DD}, {0x2569, BOX_DL | BOX_DR | BOX_DU},
    {0x256C, BOX_DU | BOX_DD | BOX_DL | BOX_DR},
};

static bool draw_box_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const int mid_x = rx + rw / 2;
    const int mid_y = ry + rh / 2;
    const int end_x = rx + rw - 1;
    const int end_y = ry + rh - 1;

    const int heavy_thickness = (rw > 12 && rh > 12) ? 3 : 2;
    const int heavy_offset = heavy_thickness / 2;
    const int dbl_offset = (rw > 8 && rh > 8) ? 2 : 1;
    const int dbl_x1 = mid_x - dbl_offset, dbl_x2 = mid_x + dbl_offset;
    const int dbl_y1 = mid_y - dbl_offset, dbl_y2 = mid_y + dbl_offset;
    uint16_t flags = 0;
    for (size_t i = 0; i < sizeof(box_defs) / sizeof(box_defs[0]); ++i) {
        if (box_defs[i].c == c) {
            flags = box_defs[i].flags;
            break;
        }
    }

    if (flags == 0) return false; // Not a recognized box character

    // Draw light lines
    if (flags & BOX_U) SDL_RenderDrawLine(renderer, mid_x, ry, mid_x, mid_y);
    if (flags & BOX_D) SDL_RenderDrawLine(renderer, mid_x, mid_y, mid_x, end_y);
    if (flags & BOX_L) SDL_RenderDrawLine(renderer, rx, mid_y, mid_x, mid_y);
    if (flags & BOX_R) SDL_RenderDrawLine(renderer, mid_x, mid_y, end_x, mid_y);

    // Draw heavy lines
    if (flags & BOX_HU) { SDL_Rect r = {mid_x - heavy_offset, ry, heavy_thickness, rh / 2 + 1}; SDL_RenderFillRect(renderer, &r); }
    if (flags & BOX_HD) { SDL_Rect r = {mid_x - heavy_offset, mid_y, heavy_thickness, rh / 2 + 1}; SDL_RenderFillRect(renderer, &r); }
    if (flags & BOX_HL) { SDL_Rect r = {rx, mid_y - heavy_offset, rw / 2 + 1, heavy_thickness}; SDL_RenderFillRect(renderer, &r); }
    if (flags & BOX_HR) { SDL_Rect r = {mid_x, mid_y - heavy_offset, rw / 2 + 1, heavy_thickness}; SDL_RenderFillRect(renderer, &r); }

    // Draw double lines
    if (flags & BOX_DU) { SDL_RenderDrawLine(renderer, dbl_x1, ry, dbl_x1, mid_y); SDL_RenderDrawLine(renderer, dbl_x2, ry, dbl_x2, mid_y); }
    if (flags & BOX_DD) { SDL_RenderDrawLine(renderer, dbl_x1, mid_y, dbl_x1, end_y); SDL_RenderDrawLine(renderer, dbl_x2, mid_y, dbl_x2, end_y); }
    if (flags & BOX_DL) { SDL_RenderDrawLine(renderer, rx, dbl_y1, mid_x, dbl_y1); SDL_RenderDrawLine(renderer, rx, dbl_y2, mid_x, dbl_y2); }
    if (flags & BOX_DR) { SDL_RenderDrawLine(renderer, mid_x, dbl_y1, end_x, dbl_y1); SDL_RenderDrawLine(renderer, mid_x, dbl_y2, end_x, dbl_y2); }

    return true;
}

static bool draw_braille_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    unsigned char dots = c & 0xFF;

    const int x1 = rx + rw / 4;
    const int x2 = rx + rw * 3 / 4;
    const int y1 = ry + rh / 8;
    const int y2 = ry + rh * 3 / 8;
    const int y3 = ry + rh * 5 / 8;
    const int y4 = ry + rh * 7 / 8;

    const int h_spacing = x2 - x1;
    const int v_spacing = y2 - y1;
    const int dot_size = SDL_max(2, SDL_min(h_spacing, v_spacing) / 2);

    const SDL_Point dot_positions[8] = {
        {x1, y1}, {x1, y2}, {x1, y3}, {x2, y1},
        {x2, y2}, {x2, y3}, {x1, y4}, {x2, y4}
    };

    for (int i = 0; i < 8; ++i) {
        if (dots & (1 << i)) {
            SDL_Rect dot_rect = {dot_positions[i].x - dot_size / 2, dot_positions[i].y - dot_size / 2, dot_size, dot_size};
            SDL_RenderFillRect(renderer, &dot_rect);
        }
    }
    return true;
}

static bool draw_block_element_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int y_divs[9], x_divs[9];
    for (int i = 0; i <= 8; ++i) {
        y_divs[i] = ry + (int)roundf((float)rh * i / 8.0f);
        x_divs[i] = rx + (int)roundf((float)rw * i / 8.0f);
    }

    switch (c) {
    // Block Elements
    case 0x2580: {
        SDL_Rect r = {rx, y_divs[0], rw, y_divs[4] - y_divs[0]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2581: {
        SDL_Rect r = {rx, y_divs[7], rw, y_divs[8] - y_divs[7]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2582: {
        SDL_Rect r = {rx, y_divs[6], rw, y_divs[8] - y_divs[6]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2583: {
        SDL_Rect r = {rx, y_divs[5], rw, y_divs[8] - y_divs[5]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2584: {
        SDL_Rect r = {rx, y_divs[4], rw, y_divs[8] - y_divs[4]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2585: {
        SDL_Rect r = {rx, y_divs[3], rw, y_divs[8] - y_divs[3]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2586: {
        SDL_Rect r = {rx, y_divs[2], rw, y_divs[8] - y_divs[2]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2587: {
        SDL_Rect r = {rx, y_divs[1], rw, y_divs[8] - y_divs[1]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2588: {
        SDL_Rect r = {rx, ry, rw, rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2589: {
        SDL_Rect r = {rx, ry, x_divs[7] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x258A: {
        SDL_Rect r = {rx, ry, x_divs[6] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x258B: {
        SDL_Rect r = {rx, ry, x_divs[5] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x258C: {
        SDL_Rect r = {rx, ry, x_divs[4] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x258D: {
        SDL_Rect r = {rx, ry, x_divs[3] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x258E: {
        SDL_Rect r = {rx, ry, x_divs[2] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x258F: {
        SDL_Rect r = {rx, ry, x_divs[1] - x_divs[0], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2590: {
        SDL_Rect r = {x_divs[4], ry, x_divs[8] - x_divs[4], rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }

    // Shades
    case 0x2591:
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 64);
        {
            SDL_Rect r = {rx, ry, rw, rh};
            SDL_RenderFillRect(renderer, &r);
        }
        return true; // Light
    case 0x2592:
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 128);
        {
            SDL_Rect r = {rx, ry, rw, rh};
            SDL_RenderFillRect(renderer, &r);
        }
        return true; // Medium
    case 0x2593:
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 192);
        {
            SDL_Rect r = {rx, ry, rw, rh};
            SDL_RenderFillRect(renderer, &r);
        }
        return true; // Dark

    // Quadrants
    case 0x2596: {
        SDL_Rect r = {x_divs[0], y_divs[4], x_divs[4] - x_divs[0], y_divs[8] - y_divs[4]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2597: {
        SDL_Rect r = {x_divs[4], y_divs[4], x_divs[8] - x_divs[4], y_divs[8] - y_divs[4]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x2598: {
        SDL_Rect r = {x_divs[0], y_divs[0], x_divs[4] - x_divs[0], y_divs[4] - y_divs[0]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x259D: {
        SDL_Rect r = {x_divs[4], y_divs[0], x_divs[8] - x_divs[4], y_divs[4] - y_divs[0]};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    default:
        fprintf(stderr, "Debug: Unhandled block element character: U+%04X\n", c);
        return false;
    }
}

static bool draw_geometric_shape_character(SDL_Renderer* renderer, uint32_t c, int rx, int ry, int rw, int rh, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const int mid_x = rx + rw / 2;
    const int mid_y = ry + rh / 2;
    const int end_x = rx + rw;
    const int end_y = ry + rh;

    switch (c) {
    case 0x25A0: {
        SDL_Rect r = {rx, ry, rw, rh};
        SDL_RenderFillRect(renderer, &r);
        return true;
    }
    case 0x25A1: {
        SDL_Rect r = {rx, ry, rw - 1, rh - 1};
        SDL_RenderDrawRect(renderer, &r);
        return true;
    }
    case 0x25B2:
        fill_triangle(renderer, mid_x, ry, rx, end_y - 1, end_x - 1, end_y - 1);
        return true; // ▲ BLACK UP-POINTING TRIANGLE
    case 0x25BC:
        fill_triangle(renderer, rx, ry, end_x - 1, ry, mid_x, end_y - 1);
        return true; // ▼ BLACK DOWN-POINTING TRIANGLE
    case 0x25C0:
        fill_triangle(renderer, end_x - 1, ry, end_x - 1, end_y - 1, rx, mid_y);
        return true; // ◀ BLACK LEFT-POINTING TRIANGLE
    case 0x25B6:
        fill_triangle(renderer, rx, ry, rx, end_y - 1, end_x - 1, mid_y);
        return true; // ▶ BLACK RIGHT-POINTING TRIANGLE
    default:
        return false;
    }
}

/**
 * @brief Fills a triangle using horizontal lines (scanline algorithm).
 */
static void fill_triangle(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3)
{
    // Sort vertices by y-coordinate (y1 <= y2 <= y3)
    if (y1 > y2) {
        SWAP_INT(x1, x2);
        SWAP_INT(y1, y2);
    }
    if (y1 > y3) {
        SWAP_INT(x1, x3);
        SWAP_INT(y1, y3);
    }
    if (y2 > y3) {
        SWAP_INT(x2, x3);
        SWAP_INT(y2, y3);
    }

    int total_height = y3 - y1;
    if (total_height == 0) {
        return;
    }

    int second_half_height = y3 - y2;

    // Top half
    for (int y = y1; y <= y2; y++) {
        int segment_height = y2 - y1 + 1;
        int ax = x1 + (x3 - x1) * (y - y1) / total_height;
        int bx = x1 + (x2 - x1) * (y - y1) / segment_height;
        if (ax > bx) {
            SWAP_INT(ax, bx);
        }
        SDL_RenderDrawLine(renderer, ax, y, bx, y);
    }

    // Bottom half
    for (int y = y2; y <= y3; y++) {
        int ax = x1 + (x3 - x1) * (y - y1) / total_height;
        int bx = x2 + (x3 - x2) * (y - y2) / (second_half_height + 1);
        if (ax > bx) {
            SWAP_INT(ax, bx);
        }
        SDL_RenderDrawLine(renderer, ax, y, bx, y);
    }
}
