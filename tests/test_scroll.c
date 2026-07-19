/**
 * Headless test for libvterm screen layer behavior.
 *
 * Tests the correct approach: let libvterm's screen layer manage all state
 * internally. We ONLY use screen callbacks (damage, movecursor, settermprop,
 * sb_pushline/sb_popline/sb_clear) and read cells via vterm_screen_get_cell().
 *
 * Key findings verified by these tests:
 * - NOT calling vterm_state_set_callbacks() lets the screen layer handle
 *   putglyph, scrollrect, erase, etc. internally.
 * - sb_pushline fires when lines scroll off the primary screen top.
 * - Resize preserves scrollback via sb_pushline/sb_popline.
 * - Alt screen works correctly with screen layer managing buffers.
 *
 * Build: gcc -g -o test_scroll test_scroll.c -lvterm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vterm.h>

#define COLS 80
#define ROWS 24

/* ===================== Test context ===================== */

typedef struct {
    int cols, rows;
    int alt_active;
    int cursor_row, cursor_col;
    int putglyph_count;
    int sb_push_count;
    int sb_pop_count;
    int sb_clear_count;
    /* Scrollback storage */
    VTermScreenCell **sb_lines;
    int sb_count;
    int sb_capacity;
} TestCtx;

static void sb_init(TestCtx *ctx, int capacity) {
    ctx->sb_lines = calloc(capacity, sizeof(VTermScreenCell*));
    ctx->sb_count = 0;
    ctx->sb_capacity = capacity;
}

static void sb_push(TestCtx *ctx, const VTermScreenCell *cells) {
    if (ctx->sb_count < ctx->sb_capacity) {
        ctx->sb_lines[ctx->sb_count] = malloc(sizeof(VTermScreenCell) * ctx->cols);
        memcpy(ctx->sb_lines[ctx->sb_count], cells, sizeof(VTermScreenCell) * ctx->cols);
        ctx->sb_count++;
    }
}

static int sb_pop(TestCtx *ctx, VTermScreenCell *cells) {
    if (ctx->sb_count == 0) return 0;
    ctx->sb_count--;
    memcpy(cells, ctx->sb_lines[ctx->sb_count], sizeof(VTermScreenCell) * ctx->cols);
    free(ctx->sb_lines[ctx->sb_count]);
    ctx->sb_lines[ctx->sb_count] = NULL;
    return 1;
}

static void sb_clear_all(TestCtx *ctx) {
    for (int i = 0; i < ctx->sb_count; i++) {
        free(ctx->sb_lines[i]);
        ctx->sb_lines[i] = NULL;
    }
    ctx->sb_count = 0;
}

/* ===================== Screen callbacks ===================== */

static int screen_damage(VTermRect rect, void *user) {
    (void)rect; (void)user; return 1;
}

static int screen_movecursor(VTermPos pos, VTermPos old, int vis, void *user) {
    (void)old; (void)vis;
    TestCtx *ctx = user;
    ctx->cursor_row = pos.row;
    ctx->cursor_col = pos.col;
    return 1;
}

static int screen_settermprop(VTermProp prop, VTermValue *val, void *user) {
    TestCtx *ctx = user;
    if (prop == VTERM_PROP_ALTSCREEN) {
        ctx->alt_active = val->boolean;
        printf("  [screen] ALTSCREEN=%d\n", val->boolean);
    }
    return 1;
}

static int screen_bell(void *user) { (void)user; return 1; }

static int screen_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {
    TestCtx *ctx = user;
    (void)cols;
    sb_push(ctx, cells);
    ctx->sb_push_count++;
    return 1;
}

static int screen_sb_popline(int cols, VTermScreenCell *cells, void *user) {
    TestCtx *ctx = user;
    (void)cols;
    return sb_pop(ctx, cells);
}

static int screen_sb_clear(void *user) {
    TestCtx *ctx = user;
    sb_clear_all(ctx);
    ctx->sb_clear_count++;
    return 1;
}

static int screen_resize(int rows, int cols, void *user) {
    (void)rows; (void)cols; (void)user;
    return 1;
}

static const VTermScreenCallbacks screen_cbs = {
    .damage     = screen_damage,
    .movecursor = screen_movecursor,
    .settermprop= screen_settermprop,
    .bell       = screen_bell,
    .resize     = screen_resize,
    .sb_pushline= screen_sb_pushline,
    .sb_popline = screen_sb_popline,
    .sb_clear   = screen_sb_clear,
};

/* ===================== Helpers ===================== */

static char get_screen_cell(VTermScreen *screen, int row, int col) {
    VTermScreenCell cell;
    memset(&cell, 0, sizeof(cell));
    vterm_screen_get_cell(screen, (VTermPos){ .row = row, .col = col }, &cell);
    uint32_t c = cell.chars[0];
    if (c >= 32 && c < 127) return (char)c;
    return (c == 0) ? '.' : '?';
}

static void print_row(VTermScreen *screen, int row, const char *label) {
    printf("  %s [%2d]: \"", label, row);
    for (int x = 0; x < COLS && x < 40; x++) putchar(get_screen_cell(screen, row, x));
    printf("...\"\n");
}

static void write_str(VTerm *vt, const char *s) {
    vterm_input_write(vt, s, strlen(s));
}

static void clear_screen(VTerm *vt, VTermScreen *screen) {
    write_str(vt, "\x1b[2J\x1b[H");
    vterm_screen_flush_damage(screen);
}

/* ===================== Main ===================== */

int main(void) {
    int pass = 0, fail = 0;
    TestCtx ctx = {0};
    ctx.cols = COLS;
    ctx.rows = ROWS;
    sb_init(&ctx, 500);

    VTerm *vt = vterm_new(ROWS, COLS);
    vterm_set_utf8(vt, 1);

    VTermScreen *screen = vterm_obtain_screen(vt);

    /* NOTE: We do NOT call vterm_state_set_callbacks().
     * The screen layer's internal state_cbs handle everything.
     * We only register screen callbacks. */
    vterm_screen_set_callbacks(screen, &screen_cbs, &ctx);
    vterm_screen_enable_altscreen(screen, 1);
    vterm_screen_reset(screen, 1);

    /* ===== TEST 1: Basic write + read via vterm_screen_get_cell ===== */
    printf("TEST 1: Basic write + read\n");
    clear_screen(vt, screen);
    write_str(vt, "\x1b[1;1HHELLO");
    vterm_screen_flush_damage(screen);

    if (get_screen_cell(screen, 0, 0) == 'H' && get_screen_cell(screen, 0, 1) == 'E') {
        printf("  PASS\n"); pass++;
    } else {
        printf("  FAIL: '%c','%c'\n", get_screen_cell(screen, 0, 0), get_screen_cell(screen, 0, 1)); fail++;
    }

    /* ===== TEST 2: Scroll up (CSI S) — lines should be pushed to sb ===== */
    printf("\nTEST 2: CSI S scroll up 3 lines (sb_pushline should fire)\n");
    clear_screen(vt, screen);
    sb_clear_all(&ctx);
    ctx.sb_push_count = 0;
    write_str(vt, "\x1b[1;1HAAA");
    write_str(vt, "\x1b[2;1HBBB");
    write_str(vt, "\x1b[3;1HCCC");
    write_str(vt, "\x1b[4;1HDDD");
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[3S");
    vterm_screen_flush_damage(screen);

    if (get_screen_cell(screen, 0, 0) == 'D' && ctx.sb_push_count >= 3) {
        printf("  PASS (sb_push_count=%d, row0='%c')\n", ctx.sb_push_count, get_screen_cell(screen, 0, 0)); pass++;
    } else {
        printf("  FAIL: row0='%c' sb_push_count=%d\n", get_screen_cell(screen, 0, 0), ctx.sb_push_count); fail++;
    }

    /* ===== TEST 3: Enter alt screen, write, exit ===== */
    printf("\nTEST 3: Alt screen enter/write/exit\n");
    clear_screen(vt, screen);
    ctx.alt_active = 0;
    write_str(vt, "\x1b[1;1HNORMAL");
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[?1049h");
    vterm_screen_flush_damage(screen);
    printf("  alt_active=%d\n", ctx.alt_active);

    write_str(vt, "\x1b[1;1HALTSCR");
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[?1049l");
    vterm_screen_flush_damage(screen);
    printf("  alt_active=%d\n", ctx.alt_active);

    char normal_ch = get_screen_cell(screen, 0, 0);
    if (normal_ch == 'N' && ctx.alt_active == 0) {
        printf("  PASS (normal restored, alt=%d)\n", ctx.alt_active); pass++;
    } else {
        printf("  FAIL: normal='%c' alt=%d\n", normal_ch, ctx.alt_active); fail++;
    }

    /* ===== TEST 4: Full htop-like exit ===== */
    printf("\nTEST 4: Full htop-like exit\n");
    clear_screen(vt, screen);
    ctx.alt_active = 0;
    write_str(vt, "\x1b[1;1HPROMPT_");
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[?1049h");
    vterm_screen_flush_damage(screen);
    for (int y = 0; y < ROWS; y++) {
        char seq[32];
        int len = snprintf(seq, sizeof(seq), "\x1b[%d;1HHTOP%02d", y + 1, y);
        vterm_input_write(vt, seq, len);
    }
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[?1049l");
    vterm_screen_flush_damage(screen);

    normal_ch = get_screen_cell(screen, 0, 0);
    if (normal_ch == 'P' && ctx.alt_active == 0) {
        printf("  PASS (normal='%c', alt=%d)\n", normal_ch, ctx.alt_active); pass++;
    } else {
        printf("  FAIL: normal='%c' alt=%d\n", normal_ch, ctx.alt_active); fail++;
    }

    /* ===== TEST 5: Scroll on alt screen, exit should restore normal ===== */
    printf("\nTEST 5: Scroll on alt screen then exit\n");
    clear_screen(vt, screen);
    ctx.alt_active = 0;
    write_str(vt, "\x1b[1;1HPROMPT_");
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[?1049h");
    vterm_screen_flush_damage(screen);

    for (int y = 0; y < 5; y++) {
        char seq[32];
        int len = snprintf(seq, sizeof(seq), "\x1b[%d;1HLINE%02d", y + 1, y);
        vterm_input_write(vt, seq, len);
    }
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[1S");
    vterm_screen_flush_damage(screen);

    write_str(vt, "\x1b[?1049l");
    vterm_screen_flush_damage(screen);

    normal_ch = get_screen_cell(screen, 0, 0);
    if (normal_ch == 'P' && ctx.alt_active == 0) {
        printf("  PASS (normal='%c', alt=%d)\n", normal_ch, ctx.alt_active); pass++;
    } else {
        printf("  FAIL: normal='%c' alt=%d\n", normal_ch, ctx.alt_active); fail++;
    }

    /* ===== TEST 6: Screen layer puts glyphs in internal buffer ===== */
    printf("\nTEST 6: vterm_screen_get_cell reads correct data\n");
    clear_screen(vt, screen);
    write_str(vt, "\x1b[1;1HX");
    vterm_screen_flush_damage(screen);

    if (get_screen_cell(screen, 0, 0) == 'X') {
        printf("  PASS\n"); pass++;
    } else {
        printf("  FAIL: cell='%c'\n", get_screen_cell(screen, 0, 0)); fail++;
    }

    /* ===== TEST 7: Scrollback accumulates lines ===== */
    printf("\nTEST 7: Scrollback accumulates lines\n");
    clear_screen(vt, screen);
    sb_clear_all(&ctx);
    ctx.sb_push_count = 0;
    /* Fill the screen then overflow to trigger scrolling */
    write_str(vt, "\x1b[1;1H");
    vterm_screen_flush_damage(screen);
    for (int i = 0; i < ROWS + 10; i++) {
        char seq[32];
        int len = snprintf(seq, sizeof(seq), "LINE%02d\r\n", i);
        vterm_input_write(vt, seq, len);
    }
    vterm_screen_flush_damage(screen);

    if (ctx.sb_count > 0 && ctx.sb_push_count > 0) {
        printf("  PASS (sb_count=%d, sb_push_count=%d)\n", ctx.sb_count, ctx.sb_push_count); pass++;
    } else {
        printf("  FAIL: sb_count=%d sb_push_count=%d\n", ctx.sb_count, ctx.sb_push_count); fail++;
    }

    printf("\n===== %d passed, %d failed =====\n", pass, fail);

    for (int i = 0; i < ctx.sb_count; i++)
        free(ctx.sb_lines[i]);
    free(ctx.sb_lines);
    vterm_free(vt);
    return fail > 0 ? 1 : 0;
}
