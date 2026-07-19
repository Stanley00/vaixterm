#include "terminal_libvterm.h"
#include "error_codes.h"
#include "terminal.h"
#include <string.h>
#include <SDL.h>

typedef struct {
    VTermScreenCell** lines;
    int capacity;
    int count;
    int cols;
} ScrollbackBuffer;

typedef struct {
    VTerm* vt;
    VTermState* state;
    VTermScreen* screen;
    ScrollbackBuffer sb;
    char output_buffer[4096];
    size_t output_len;
    Glyph* line_buffer;
    int line_buffer_cols;
} LibVtermBackend;

static Glyph* ensure_line_buffer(LibVtermBackend* backend, int cols)
{
    if (backend->line_buffer && backend->line_buffer_cols >= cols)
        return backend->line_buffer;
    free(backend->line_buffer);
    backend->line_buffer = calloc((size_t)cols, sizeof(Glyph));
    backend->line_buffer_cols = cols;
    return backend->line_buffer;
}

static void sb_init(ScrollbackBuffer* sb, int capacity, int cols)
{
    sb->lines = calloc((size_t)capacity, sizeof(VTermScreenCell*));
    sb->capacity = capacity;
    sb->count = 0;
    sb->cols = cols;
}

static void sb_free(ScrollbackBuffer* sb)
{
    for (int i = 0; i < sb->count; i++)
        free(sb->lines[i]);
    free(sb->lines);
    sb->lines = NULL;
    sb->count = 0;
}

static void sb_push(ScrollbackBuffer* sb, const VTermScreenCell* cells, int cols)
{
    int n = cols < sb->cols ? cols : sb->cols;
    if (sb->count < sb->capacity) {
        sb->lines[sb->count] = malloc(sizeof(VTermScreenCell) * (size_t)sb->cols);
        if (!sb->lines[sb->count]) return;
        memcpy(sb->lines[sb->count], cells, sizeof(VTermScreenCell) * (size_t)n);
        if (n < sb->cols)
            memset(sb->lines[sb->count] + n, 0, sizeof(VTermScreenCell) * (size_t)(sb->cols - n));
        sb->count++;
    } else {
        free(sb->lines[0]);
        memmove(&sb->lines[0], &sb->lines[1], sizeof(VTermScreenCell*) * (size_t)(sb->capacity - 1));
        sb->lines[sb->capacity - 1] = malloc(sizeof(VTermScreenCell) * (size_t)sb->cols);
        if (!sb->lines[sb->capacity - 1]) return;
        memcpy(sb->lines[sb->capacity - 1], cells, sizeof(VTermScreenCell) * (size_t)n);
        if (n < sb->cols)
            memset(sb->lines[sb->capacity - 1] + n, 0, sizeof(VTermScreenCell) * (size_t)(sb->cols - n));
    }
}

static bool sb_pop(ScrollbackBuffer* sb, VTermScreenCell* cells)
{
    if (sb->count == 0) return false;
    sb->count--;
    memcpy(cells, sb->lines[sb->count], sizeof(VTermScreenCell) * (size_t)sb->cols);
    free(sb->lines[sb->count]);
    sb->lines[sb->count] = NULL;
    return true;
}

static void sb_clear(ScrollbackBuffer* sb)
{
    for (int i = 0; i < sb->count; i++) {
        free(sb->lines[i]);
        sb->lines[i] = NULL;
    }
    sb->count = 0;
}

static void sb_resize(ScrollbackBuffer* sb, int new_cols, int new_rows_for_capacity)
{
    int old_cols = sb->cols;
    int new_cap = new_rows_for_capacity > 0 ? new_rows_for_capacity : sb->capacity;
    if (new_cap < 0) new_cap = 0;

    if (new_cols != old_cols) {
        for (int i = 0; i < sb->count; i++) {
            VTermScreenCell* old = sb->lines[i];
            VTermScreenCell* nc = malloc(sizeof(VTermScreenCell) * (size_t)new_cols);
            if (nc) {
                int copy = old_cols < new_cols ? old_cols : new_cols;
                memcpy(nc, old, sizeof(VTermScreenCell) * (size_t)copy);
                if (new_cols > old_cols)
                    memset(nc + old_cols, 0, sizeof(VTermScreenCell) * (size_t)(new_cols - old_cols));
                free(old);
                sb->lines[i] = nc;
            }
        }
        sb->cols = new_cols;
    }

    if (new_cap != sb->capacity) {
        VTermScreenCell** nl = realloc(sb->lines, sizeof(VTermScreenCell*) * (size_t)new_cap);
        if (nl) {
            sb->lines = nl;
            if (sb->count > new_cap) {
                for (int i = new_cap; i < sb->count; i++)
                    free(sb->lines[i]);
                sb->count = new_cap;
            }
            sb->capacity = new_cap;
        }
    }
}

static int screen_damage(VTermRect rect, void* user)
{
    Terminal* term = (Terminal*)user;
    term->has_dirty_regions = true;
    if (term->dirty_min_y == -1 || rect.start_row < term->dirty_min_y)
        term->dirty_min_y = rect.start_row;
    if (term->dirty_max_y == -1 || rect.end_row > term->dirty_max_y)
        term->dirty_max_y = rect.end_row;
    return 1;
}

static int screen_movecursor(VTermPos pos, VTermPos oldpos, int visible, void* user)
{
    Terminal* term = (Terminal*)user;
    (void)oldpos;
    (void)visible;
    term->cursor_x = pos.col;
    term->cursor_y = pos.row;
    return 1;
}

static int screen_settermprop(VTermProp prop, VTermValue* val, void* user)
{
    Terminal* term = (Terminal*)user;
    switch (prop) {
        case VTERM_PROP_CURSORVISIBLE:
            term->cursor_visible = val->boolean;
            break;
        case VTERM_PROP_CURSORBLINK:
            term->cursor_style_blinking = val->boolean;
            break;
        case VTERM_PROP_CURSORSHAPE:
            term->cursor_style = val->number;
            if (term->cursor_style > CURSOR_STYLE_BAR) term->cursor_style = CURSOR_STYLE_BLOCK;
            break;
        case VTERM_PROP_ALTSCREEN:
            if (term->alt_screen_active != val->boolean) {
                term->alt_screen_active = val->boolean;
                term->full_redraw_needed = true;
            }
            break;
        case VTERM_PROP_REVERSE:
            term->full_redraw_needed = true;
            break;
        default:
            break;
    }
    return 1;
}

static int screen_bell(void* user)
{
    (void)user;
    return 1;
}

static int screen_resize(int rows, int cols, void* user)
{
    Terminal* term = (Terminal*)user;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    if (!backend) return 1;

    int old_rows = term->rows;
    int scrollback_cap = term->scrollback > 0 ? term->scrollback : 5000;

    if (cols != term->cols || rows != old_rows) {
        sb_resize(&backend->sb, cols, rows + scrollback_cap);

        bool* new_dirty = realloc(term->dirty_lines, sizeof(bool) * (size_t)rows);
        if (new_dirty) {
            term->dirty_lines = new_dirty;
            for (int y = old_rows; y < rows; y++)
                term->dirty_lines[y] = true;
        }

        term->cols = cols;
        term->rows = rows;
        term->full_redraw_needed = true;
        term->has_dirty_regions = true;
        term->dirty_min_y = 0;
        term->dirty_max_y = rows - 1;
    }
    return 1;
}

static int screen_sb_pushline(int cols, const VTermScreenCell* cells, void* user)
{
    Terminal* term = (Terminal*)user;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    if (!backend) return 1;
    sb_push(&backend->sb, cells, cols);
    return 1;
}

static int screen_sb_popline(int cols, VTermScreenCell* cells, void* user)
{
    Terminal* term = (Terminal*)user;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    if (!backend) return 0;
    (void)cols;
    return sb_pop(&backend->sb, cells) ? 1 : 0;
}

static int screen_sb_clear_func(void* user)
{
    Terminal* term = (Terminal*)user;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    if (backend) sb_clear(&backend->sb);
    return 1;
}

static void output_callback(const char* s, size_t len, void* user)
{
    Terminal* term = (Terminal*)user;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    if (!backend) return;

    size_t space = sizeof(backend->output_buffer) - backend->output_len;
    if (space == 0) return;
    size_t copy = len < space ? len : space;
    memcpy(backend->output_buffer + backend->output_len, s, copy);
    backend->output_len += copy;
}

static const VTermScreenCallbacks screen_callbacks = {
    .damage = screen_damage,
    .movecursor = screen_movecursor,
    .settermprop = screen_settermprop,
    .bell = screen_bell,
    .resize = screen_resize,
    .sb_pushline = screen_sb_pushline,
    .sb_popline = screen_sb_popline,
    .sb_clear = screen_sb_clear_func,
};

bool terminal_libvterm_init(Terminal* term, int cols, int rows)
{
    if (!term || cols <= 0 || rows <= 0) return false;

    term->cols = cols;
    term->rows = rows;
    term->screen_texture = NULL;

    LibVtermBackend* backend = calloc(1, sizeof(LibVtermBackend));
    if (!backend) return false;

    backend->vt = vterm_new(rows, cols);
    if (!backend->vt) { free(backend); return false; }

    backend->state = vterm_obtain_state(backend->vt);
    backend->screen = vterm_obtain_screen(backend->vt);

    vterm_screen_set_callbacks(backend->screen, &screen_callbacks, term);
    vterm_screen_enable_altscreen(backend->screen, 1);
    vterm_output_set_callback(backend->vt, output_callback, term);
    vterm_set_utf8(backend->vt, 1);

    int scrollback_cap = term->scrollback > 0 ? term->scrollback : 5000;
    sb_init(&backend->sb, scrollback_cap + rows, cols);

    backend->line_buffer = NULL;
    backend->line_buffer_cols = 0;

    term->backend = backend;

    return true;
}

void terminal_libvterm_free(Terminal* term)
{
    if (!term || !term->backend) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    sb_free(&backend->sb);
    free(backend->line_buffer);
    if (backend->vt) vterm_free(backend->vt);
    free(backend);
    term->backend = NULL;
}

void terminal_libvterm_reset(Terminal* term, bool hard)
{
    if (!term || !term->backend) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    sb_clear(&backend->sb);
    vterm_screen_reset(backend->screen, hard);
    vterm_state_reset(backend->state, hard);
    vterm_screen_flush_damage(backend->screen);

    for (int y = 0; y < term->rows; y++)
        if (term->dirty_lines) term->dirty_lines[y] = true;
    term->has_dirty_regions = true;
    term->dirty_min_y = 0;
    term->dirty_max_y = term->rows - 1;
    term->cursor_x = 0;
    term->cursor_y = 0;
}

void terminal_libvterm_feed(Terminal* term, const char* data, size_t len)
{
    if (!term || !term->backend || !data) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    vterm_input_write(backend->vt, data, len);
}

void terminal_libvterm_key(Terminal* term, VTermKey key, VTermModifier mod)
{
    if (!term || !term->backend) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    vterm_keyboard_key(backend->vt, key, mod);
}

void terminal_libvterm_unichar(Terminal* term, uint32_t c, VTermModifier mod)
{
    if (!term || !term->backend) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    vterm_keyboard_unichar(backend->vt, c, mod);
}

void terminal_libvterm_resize(Terminal* term, int rows, int cols)
{
    if (!term || !term->backend) return;
    if (rows <= 0 || cols <= 0) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;

    // vterm_set_size triggers screen_resize callback which handles
    // sb_resize, dirty_lines realloc, and term->rows/cols update.
    vterm_set_size(backend->vt, rows, cols);

    vterm_screen_flush_damage(backend->screen);

    for (int y = 0; y < term->rows; y++)
        if (term->dirty_lines) term->dirty_lines[y] = true;
    term->has_dirty_regions = true;
    term->dirty_min_y = 0;
    term->dirty_max_y = term->rows - 1;
}

void terminal_libvterm_set_default_colors(Terminal* term, SDL_Color fg, SDL_Color bg)
{
    if (!term || !term->backend) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    VTermColor vfg, vbg;
    vterm_color_rgb(&vfg, fg.r, fg.g, fg.b);
    vterm_color_rgb(&vbg, bg.r, bg.g, bg.b);
    vterm_state_set_default_colors(backend->state, &vfg, &vbg);
    vterm_screen_set_default_colors(backend->screen, &vfg, &vbg);
    term->default_fg = fg;
    term->default_bg = bg;
}

void terminal_libvterm_set_palette_color(Terminal* term, int index, SDL_Color color)
{
    if (!term || !term->backend || index < 0 || index >= 256) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    VTermColor vcol;
    vterm_color_rgb(&vcol, color.r, color.g, color.b);
    vterm_state_set_palette_color(backend->state, index, &vcol);
    term->palette[index] = color;
}

void terminal_libvterm_flush_damage(Terminal* term)
{
    if (!term || !term->backend) return;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    vterm_screen_flush_damage(backend->screen);
}

size_t terminal_libvterm_flush_output(Terminal* term, char* dst, size_t dst_len)
{
    if (!term || !term->backend || !dst || dst_len == 0) return 0;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;

    size_t copy = backend->output_len < dst_len ? backend->output_len : dst_len;
    if (copy > 0) {
        memcpy(dst, backend->output_buffer, copy);
        if (copy < backend->output_len) {
            memmove(backend->output_buffer, backend->output_buffer + copy,
                    backend->output_len - copy);
        }
        backend->output_len -= copy;
    }
    return copy;
}

static void convert_cell_to_glyph(VTermScreen* screen, const VTermScreenCell* cell, Glyph* g)
{
    g->character = cell->chars[0] ? cell->chars[0] : ' ';
    g->width = (cell->width == 2) ? 2 : 1;

    VTermColor fg = cell->fg;
    vterm_screen_convert_color_to_rgb(screen, &fg);
    g->fg.r = fg.rgb.red;
    g->fg.g = fg.rgb.green;
    g->fg.b = fg.rgb.blue;
    g->fg.a = 255;

    VTermColor bg = cell->bg;
    vterm_screen_convert_color_to_rgb(screen, &bg);
    g->bg.r = bg.rgb.red;
    g->bg.g = bg.rgb.green;
    g->bg.b = bg.rgb.blue;
    g->bg.a = 255;

    g->attributes = 0;
    if (cell->attrs.bold) g->attributes |= ATTR_BOLD;
    if (cell->attrs.italic) g->attributes |= ATTR_ITALIC;
    if (cell->attrs.underline) g->attributes |= ATTR_UNDERLINE;
    if (cell->attrs.blink) g->attributes |= ATTR_BLINK;
    if (cell->attrs.reverse) g->attributes |= ATTR_INVERSE;
    g->attr = 0;
}

Glyph* terminal_libvterm_get_view_line(Terminal* term, int y)
{
    if (!term || !term->backend || y < 0 || y >= term->rows) return NULL;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    ScrollbackBuffer* sb = &backend->sb;

    Glyph* buf = ensure_line_buffer(backend, term->cols);
    if (!buf) return NULL;

    VTermScreenCell cell;
    memset(&cell, 0, sizeof(cell));

    if (term->view_offset == 0) {
        vterm_screen_get_cell(backend->screen, (VTermPos){ .row = y, .col = 0 }, &cell);
        for (int x = 0; x < term->cols; x++) {
            VTermPos pos = { .row = y, .col = x };
            vterm_screen_get_cell(backend->screen, pos, &cell);
            convert_cell_to_glyph(backend->screen, &cell, &buf[x]);
        }
    } else {
        int sb_line = sb->count - term->view_offset + y;
        if (sb_line >= 0 && sb_line < sb->count) {
            const VTermScreenCell* sc = sb->lines[sb_line];
            for (int x = 0; x < term->cols; x++) {
                convert_cell_to_glyph(backend->screen, &sc[x], &buf[x]);
            }
        } else if (sb_line >= sb->count) {
            int screen_row = sb_line - sb->count;
            for (int x = 0; x < term->cols; x++) {
                VTermPos pos = { .row = screen_row, .col = x };
                vterm_screen_get_cell(backend->screen, pos, &cell);
                convert_cell_to_glyph(backend->screen, &cell, &buf[x]);
            }
        } else {
            for (int x = 0; x < term->cols; x++) {
                buf[x] = (Glyph){.character=' ', .width=1, .fg=term->default_bg, .bg=term->default_bg};
            }
        }
    }

    return buf;
}

int terminal_libvterm_get_scrollback_count(Terminal* term)
{
    if (!term || !term->backend) return 0;
    LibVtermBackend* backend = (LibVtermBackend*)term->backend;
    return backend->sb.count;
}
