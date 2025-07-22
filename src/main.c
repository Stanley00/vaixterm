/**
 * vaixterm: A lightweight, modern ANSI terminal emulator using SDL2.
 *
 * Features:
 * - Spawns a shell using a pseudoterminal (PTY).
 * - Renders a character grid using SDL2 and SDL_ttf.
 * - Parses ANSI/VT100 escape codes (colors, attributes, cursor control).
 * - Supports 256-color and True Color, configurable via colorscheme files.
 * - Custom rendering for box-drawing and Braille characters.
 * - Handles keyboard and game controller input, with configurable mappings.
 * - On-screen keyboard (OSK) with customizable key sets.
 * - Scrollback buffer with a visual scrollbar.
 * - Resizable window with automatic PTY resizing.
 * - Configurable geometry, font, colors, and background image.
 * - Read-only mode for display-only applications.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <ctype.h>
#include <pwd.h>
#include <locale.h>

// PTY headers vary by OS
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>
#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_gamecontroller.h>
#include <SDL_image.h>
#include <math.h>

// Include refactored modules
#include "terminal_state.h"
#include "terminal.h"
#include "rendering.h"
#include "osk.h"
#include "input.h"
#include "config.h"
#include "manualfont.h"

// --- Global State for Signal Handling ---
// volatile sig_atomic_t reload_config_flag = 0; // Removed: SIGUSR1 support removed

// --- Function Prototypes for Main ---
static void parse_args(int argc, char* argv[], Config* config);
static bool init_sdl(SDL_Window** win, SDL_Renderer** renderer, TTF_Font** font, const Config* config, int* char_w, int* char_h);
static void run_child_process(const Config* config);
static void main_loop(SDL_Renderer* renderer, Terminal* term, TTF_Font** font, Config* config, int* char_w, int* char_h, int master_fd, OnScreenKeyboard* osk);
static bool change_font_size(TTF_Font** font, Config* config, Terminal* term, OnScreenKeyboard* osk,
                             int* char_h, int* char_w, int master_fd, int delta);
static void execute_internal_command(InternalCommand cmd, Terminal* term, bool* needs_render, int pty_fd,
                                     TTF_Font** font, Config* config, OnScreenKeyboard* osk, int* char_w, int* char_h);
// static void reload_application_config(Config* config, Terminal* term, OnScreenKeyboard* osk, SDL_Renderer* renderer, TTF_Font** font, int* char_w, int* char_h, int master_fd, bool* needs_render); // Removed: SIGUSR1 support removed


/* --- Helper Function Prototypes --- */
static void handle_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd,
                                   TTF_Font** font, Config* config, int* char_w, int* char_h);
static void cleanup_resources(Config* config, Terminal* term, OnScreenKeyboard* osk, SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, pid_t pid, int master_fd);
static void cleanup_and_exit_from_credits(SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, pid_t pid, Config* config, Terminal* term, OnScreenKeyboard* osk, int master_fd);
static bool handle_held_modifier_button(SDL_GameControllerButton button, bool pressed, OnScreenKeyboard* osk, bool* needs_render);
static void handle_held_modifier_axis(SDL_GameControllerAxis axis, Sint16 value, OnScreenKeyboard* osk, bool* needs_render);
static TerminalAction get_action_for_button_with_mode(SDL_GameControllerButton button, bool osk_active);

static void terminal_scroll_view(Terminal* term, int amount, bool* needs_render);
static void handle_event(SDL_Event* event, bool* running, bool* needs_render,
                         Terminal* term, OnScreenKeyboard* osk, int master_fd, TTF_Font** font, Config* config, int* char_w, int* char_h, ButtonRepeatState* repeat_state);
static void process_and_repeat_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd, TTF_Font** font, Config* config, int* char_w, int* char_h, ButtonRepeatState* repeat_state);
static void stop_repeating_action(TerminalAction action, ButtonRepeatState* repeat_state);

/**
 * @brief Cleans up all allocated resources.
 */
static void cleanup_resources(Config* config, Terminal* term, OnScreenKeyboard* osk, SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, pid_t pid, int master_fd)
{
    // Free dynamically allocated config strings
    if (config->font_path) {
        free(config->font_path);
    }
    if (config->custom_command) {
        free(config->custom_command);
    }
    if (config->background_image_path) {
        free(config->background_image_path);
    }
    if (config->colorscheme_path) {
        free(config->colorscheme_path);
    }
    if (config->osk_layout_path) {
        free(config->osk_layout_path);
    }
    for (int i = 0; i < config->num_key_sets; ++i) { // Changed from num_key_set_args
        free(config->key_sets[i].path); // Changed from key_set_args
    }
    free(config->key_sets); // Changed from key_set_args

    if (osk) {
        osk_key_cache_destroy(osk->key_cache);
        osk_free_all_sets(osk); // This now handles all dynamic key set freeing
        if (osk->controller) {
            SDL_GameControllerClose(osk->controller);
            osk->controller = NULL;
        }
        if (osk->joystick) {
            SDL_JoystickClose(osk->joystick);
            osk->joystick = NULL;
        }
    }
    if (term) {
        glyph_cache_destroy(term->glyph_cache);
        SDL_DestroyTexture(term->screen_texture);
        terminal_destroy(term);
    }
    SDL_StopTextInput();
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    if (master_fd != -1) {
        close(master_fd);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (win) {
        SDL_DestroyWindow(win);
    }
    if (font) {
        TTF_CloseFont(font);
    }
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

/**
 * @brief Cleans up resources and exits when quitting from the credit screen.
 */
static void cleanup_and_exit_from_credits(SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, pid_t pid, Config* config, Terminal* term, OnScreenKeyboard* osk, int master_fd)
{
    cleanup_resources(config, term, osk, renderer, win, font, pid, master_fd); // Note: pid might be -1 if called before fork
    exit(0);
}

/**
 * @brief Updates the state of a held modifier based on a button press/release.
 * @return true if the button was a modifier button, false otherwise.
 */
static bool handle_held_modifier_button(SDL_GameControllerButton button, bool pressed, OnScreenKeyboard* osk, bool* needs_render)
{
    bool* held_flag = NULL;

    if (button == HELD_MODIFIER_SHIFT_BUTTON) {
        held_flag = &osk->held_shift;
    } else if (button == HELD_MODIFIER_CTRL_BUTTON) {
        held_flag = &osk->held_ctrl;
    } else {
        return false; // Not a modifier button
    }

    // Update the held state if it has changed. This is now unconditional.
    // The effects of this flag (layer switching, indicator visibility) are handled elsewhere.
    if (pressed != *held_flag) {
        *held_flag = pressed;
        *needs_render = true;
        osk_validate_row_index(osk);
    }

    return true; // It was a modifier button, so we consume the event.
}

/**
 * @brief Updates the state of a held modifier based on a trigger axis value.
 */
static void handle_held_modifier_axis(SDL_GameControllerAxis axis, Sint16 value, OnScreenKeyboard* osk, bool* needs_render)
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

    // Update the held state if it has changed. This is now unconditional.
    if (pressed != *held_flag) {
        *held_flag = pressed;
        *needs_render = true;
        osk_validate_row_index(osk);
    }
}

/**
 * @brief Scrolls the terminal's viewport by a given amount.
 * @param term The terminal state.
 * @param amount The number of lines to scroll (positive for up/into history, negative for down/towards present).
 * @param needs_render Pointer to a boolean, set to true if rendering is needed.
 */
static void terminal_scroll_view(Terminal* term, int amount, bool* needs_render)
{
    if (term->alt_screen_active || term->history_size == 0) {
        return;
    }

    int old_offset = term->view_offset;
    int new_offset = term->view_offset + amount;

    term->view_offset = SDL_max(0, SDL_min(term->history_size, new_offset));

    if (term->view_offset != old_offset) {
        *needs_render = true;
        term->full_redraw_needed = true;
    }
}

/**
 * @brief Parses command-line arguments and updates the Config struct.
 * Arguments override any values loaded from a config file.
 */
static void parse_args(int argc, char* argv[], Config* config)
{
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            config->win_w = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) && i + 1 < argc) {
            config->win_h = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--font") == 0) && i + 1 < argc) {
            free(config->font_path);
            config->font_path = strdup(argv[++i]);
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) && i + 1 < argc) {
            config->font_size = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--scrollback") == 0) && i + 1 < argc) {
            config->scrollback_lines = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--exec") == 0) && i + 1 < argc) {
            free(config->custom_command);
            config->custom_command = strdup(argv[++i]);
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--background") == 0) && i + 1 < argc) {
            free(config->background_image_path);
            config->background_image_path = strdup(argv[++i]);
        } else if ((strcmp(argv[i], "-cs") == 0 || strcmp(argv[i], "--colorscheme") == 0) && i + 1 < argc) {
            free(config->colorscheme_path);
            config->colorscheme_path = strdup(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            config->target_fps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--read-only") == 0) {
            config->read_only = true;
        } else if (strcmp(argv[i], "--no-credit") == 0) {
            config->no_credit = true;
        } else if (strcmp(argv[i], "--force-full-render") == 0) {
            config->force_full_render = true;
        } else if (strcmp(argv[i], "--key-set") == 0 && i + 1 < argc) {
            const char* arg = argv[++i];
            bool load = true;
            const char* path = arg;

            if (arg[0] == '-') {
                load = false;
                path = arg + 1;
            } else if (arg[0] == '+') {
                path = arg + 1;
            }

            config->num_key_sets++; // Changed from num_key_set_args
            config->key_sets = realloc(config->key_sets, sizeof(struct KeySetArg) * (size_t)config->num_key_sets); // Changed from key_set_args
            if (!config->key_sets) { // Changed from key_set_args
                fprintf(stderr, "Fatal: Could not allocate memory for key set arguments.\n");
                exit(1);
            }
            config->key_sets[config->num_key_sets - 1].path = strdup(path); // Changed from key_set_args
            config->key_sets[config->num_key_sets - 1].load_at_startup = load; // Changed from key_set_args
        } else if (strcmp(argv[i], "--osk-layout") == 0 && i + 1 < argc) {
            free(config->osk_layout_path);
            config->osk_layout_path = strdup(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0) {
            fprintf(stdout, "vaixterm - A simple, modern terminal emulator for game handhelds.\n\n");
            fprintf(stdout, "Usage: %s [options]\n\n", argv[0]);
            fprintf(stdout, "Options:\n");
            fprintf(stdout, "  -w, --width <pixels>       Set window width (default: %d)\n", DEFAULT_WINDOW_WIDTH);
            fprintf(stdout, "  -h, --height <pixels>      Set window height (default: %d)\n", DEFAULT_WINDOW_HEIGHT);
            fprintf(stdout, "  -f, --font <path>          Set font path (default: %s)\n", DEFAULT_FONT_FILE_PATH);
            fprintf(stdout, "  -s, --size <points>        Set font size (default: %d)\n", DEFAULT_FONT_SIZE_POINTS);
            fprintf(stdout, "  -l, --scrollback <lines>   Set scrollback lines (default: %d)\n", DEFAULT_SCROLLBACK_LINES);
            fprintf(stdout, "  -e, --exec <command>       Execute command instead of default shell.\n");
            fprintf(stdout, "  -b, --background <path>    Set background image (optional).\n");
            fprintf(stdout, "  -cs, --colorscheme <path>  Set colorscheme (optional).\n");
            fprintf(stdout, "  --fps <value>              Set framerate cap (default: 30 fps).\n");
            fprintf(stdout, "  --read-only                Run in read-only mode (input disabled).\n");
            fprintf(stdout, "  --no-credit                Start shell directly, skip credits.\n");
            fprintf(stdout, "  --force-full-render        Force a full re-render on every frame.\n");
            fprintf(stdout, "  --key-set [-|+]<path>      Add key set ('-': available, '+': load).\n");
            fprintf(stdout, "  --osk-layout <path>        Use a custom OSK layout file.\n");
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            exit(1);
        }
    }
}


static bool init_sdl(SDL_Window** win, SDL_Renderer** renderer, TTF_Font** font, const Config* config, int* char_w, int* char_h)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        goto sdl_quit;
    }
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        fprintf(stderr, "IMG_Init Error: %s\n", IMG_GetError());
        goto ttf_quit;
    }

    *font = TTF_OpenFont(config->font_path, config->font_size);
    if (!*font) {
        fprintf(stderr, "TTF_OpenFont Error: Failed to open font at '%s': %s\n", config->font_path, TTF_GetError());
        fprintf(stderr, "Please check the font path and permissions.\n");
        goto img_quit;
    }

    if (TTF_SizeUTF8(*font, "W", char_w, char_h) != 0) {
        fprintf(stderr, "TTF_SizeText Error: %s\n", TTF_GetError());
        goto font_close;
    }
    if (*char_w <= 0 || *char_h <= 0) {
        fprintf(stderr, "Error: Font has invalid character dimensions (w=%d, h=%d).\n", *char_w, *char_h);
        goto font_close;
    }

    *win = SDL_CreateWindow("vaixterm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config->win_w, config->win_h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!*win) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        goto font_close;
    }

    *renderer = SDL_CreateRenderer(*win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        goto window_destroy;
    }
    SDL_SetRenderDrawBlendMode(*renderer, SDL_BLENDMODE_BLEND);
    return true;

window_destroy:
    SDL_DestroyWindow(*win);
font_close:
    TTF_CloseFont(*font);
img_quit:
    IMG_Quit();
ttf_quit:
    TTF_Quit();
sdl_quit:
    SDL_Quit();
    return false;
}

/**
 * @brief Executed by the child process. Sets up environment and execs into the shell/command.
 */
static void run_child_process(const Config* config)
{
    setenv("TERM", "xterm-256color", 1);

    const char* shell_path = NULL;
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        shell_path = pw->pw_shell;
    }

    if (!shell_path || shell_path[0] == '\0') {
        shell_path = "/bin/sh";
    }

    setenv("SHELL", shell_path, 1);

    const char *shell_name = strrchr(shell_path, '/');
    if (shell_name) {
        shell_name++;
    } else {
        shell_name = shell_path;
    }

    if (config->custom_command) {
        execlp(shell_path, shell_name, "-c", config->custom_command, (char *)NULL);
        fprintf(stderr, "execlp failed for command '%s' with shell '%s': %s\n", config->custom_command, shell_path, strerror(errno));
    } else {
        char argv0[256];
        snprintf(argv0, sizeof(argv0), "-%s", shell_name);
        execlp(shell_path, argv0, (char *)NULL);
        fprintf(stderr, "execlp failed for shell '%s': %s\n", shell_path, strerror(errno));
    }
    exit(1);
}

/**
 * @brief Central handler for all abstract terminal actions.
 */
static void handle_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int master_fd,
                                   TTF_Font** font, Config* config, int* char_w, int* char_h)
{
    switch (action) {
    case ACTION_SCROLL_UP:
        // Scroll up by half a page
        terminal_scroll_view(term, SDL_max(1, term->rows / 2), needs_render);
        break;
    case ACTION_SCROLL_DOWN:
        // Scroll down by the standard mouse wheel amount
        terminal_scroll_view(term, -MOUSE_WHEEL_SCROLL_AMOUNT, needs_render);
        break;
    default: {
        // For all other actions, decide whether to process them for the OSK or the terminal directly.
        InternalCommand cmd = CMD_NONE;
        if (osk->active) {
            cmd = process_osk_action(action, term, osk, needs_render, master_fd);
        } else {
            // Actions like arrows, backspace, enter from a controller when the OSK is off
            // are processed as direct terminal input.
            process_direct_terminal_action(action, term, osk, needs_render, master_fd);
        }

        if (cmd != CMD_NONE) {
            execute_internal_command(cmd, term, needs_render, master_fd, font, config, osk, char_w, char_h);
        }
        break;
    }
    }
}

/**
 * @brief Processes a terminal action and sets up for button repeating.
 */
static void process_and_repeat_action(TerminalAction action,
                                      Terminal* term, OnScreenKeyboard* osk, bool* needs_render,
                                      int master_fd, TTF_Font** font, Config* config,
                                      int* char_w, int* char_h,
                                      ButtonRepeatState* repeat_state)
{
    if (action == ACTION_NONE) {
        return;
    }

    if (repeat_state->is_held && repeat_state->action == action) {
        return;
    }

    handle_terminal_action(action, term, osk, needs_render, master_fd, font, config, char_w, char_h);
    repeat_state->is_held = true;
    repeat_state->action = action;
    repeat_state->next_repeat_time = SDL_GetTicks() + BUTTON_REPEAT_INITIAL_DELAY_MS;
}

/**
 * @brief Stops repeating an action.
 */
static void stop_repeating_action(TerminalAction action, ButtonRepeatState* repeat_state)
{
    if (repeat_state->is_held && repeat_state->action == action) {
        repeat_state->is_held = false;
    }
}

/**
 * @brief Determines the correct TerminalAction for a button press based on OSK state.
 * This helper centralizes the logic for dual-mode buttons (e.g., shoulder buttons).
 * @param button The SDL_GameControllerButton that was pressed.
 * @param osk_active True if the On-Screen Keyboard is currently active.
 * @return The corresponding TerminalAction.
 */
static TerminalAction get_action_for_button_with_mode(SDL_GameControllerButton button, bool osk_active)
{
    // When OSK is inactive, shoulder buttons are for scrolling.
    if (!osk_active) {
        if (button == HELD_MODIFIER_SHIFT_BUTTON) {
            return ACTION_SCROLL_UP;
        }
        if (button == HELD_MODIFIER_CTRL_BUTTON) {
            return ACTION_SCROLL_DOWN;
        }
    }
    // Otherwise, or for any other button, use the standard map.
    // Note: When OSK is active, HELD_MODIFIER_* buttons are caught by
    // handle_held_modifier_button() before this function is called, so they
    // won't be incorrectly mapped to scroll actions here.
    return map_cbutton_to_action(button);
}

static bool change_font_size(TTF_Font** font, Config* config, Terminal* term, OnScreenKeyboard* osk,
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
    osk->cached_mod_mask = -1; // Invalidate modifier mask cache too

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

static void execute_internal_command(InternalCommand cmd, Terminal* term, bool* needs_render, int pty_fd,
                                     TTF_Font** font, Config* config, OnScreenKeyboard* osk, int* char_w, int* char_h)
{
    switch (cmd) {
    case CMD_FONT_INC:
        if (change_font_size(font, config, term, osk, char_w, char_h, pty_fd, 1)) {
            *needs_render = true;
        }
        break;
    case CMD_FONT_DEC:
        if (change_font_size(font, config, term, osk, char_w, char_h, pty_fd, -1)) {
            *needs_render = true;
        }
        break;
    case CMD_CURSOR_TOGGLE_VISIBILITY:
        term->cursor_visible = !term->cursor_visible;
        *needs_render = true;
        break;
    case CMD_CURSOR_TOGGLE_BLINK:
        term->cursor_style_blinking = !term->cursor_style_blinking;
        if (term->cursor_style_blinking) {
            term->cursor_blink_on = true;
        }
        *needs_render = true;
        break;
    case CMD_CURSOR_CYCLE_STYLE:
        term->cursor_style = (term->cursor_style + 1) % 3;
        *needs_render = true;
        break;
    case CMD_TERMINAL_RESET:
        terminal_reset(term);
        if (pty_fd != -1) {
            write(pty_fd, "\f", 1);
        }
        *needs_render = true;
        break;
    case CMD_TERMINAL_CLEAR:
        terminal_clear_visible_screen(term);
        *needs_render = true;
        break;
    case CMD_OSK_TOGGLE_POSITION:
        // Toggle between OSK_POSITION_OPPOSITE (0) and OSK_POSITION_SAME (1)
        osk->position_mode = (osk->position_mode == OSK_POSITION_OPPOSITE) ? OSK_POSITION_SAME : OSK_POSITION_OPPOSITE;
        *needs_render = true;
        break;
    case CMD_NONE:
        break;
    }
}

// --- Main ---

int main(int argc, char* argv[])
{
    setlocale(LC_CTYPE, "");

    Config config = {
        .win_w = DEFAULT_WINDOW_WIDTH,
        .win_h = DEFAULT_WINDOW_HEIGHT,
        .font_path = strdup(DEFAULT_FONT_FILE_PATH),
        .font_size = DEFAULT_FONT_SIZE_POINTS,
        .custom_command = NULL,
        .scrollback_lines = DEFAULT_SCROLLBACK_LINES,
        .force_full_render = false,
        .background_image_path = DEFAULT_BACKGROUND_IMAGE_PATH,
        .colorscheme_path = NULL,
        .target_fps = 30,
        .read_only = false,
        .no_credit = false,
        .osk_layout_path = NULL,
        .key_sets = NULL, // Changed from key_set_args
        .num_key_sets = 0 // Changed from num_key_set_args
    };

    // Parse command-line arguments (these set the initial configuration)
    parse_args(argc, argv, &config);

    // If no layout is specified, or if the specified path is empty,
    // ensure we use the default by setting the path to NULL. This makes the
    // fallback logic in osk_load_layout more robust.
    if (config.osk_layout_path && config.osk_layout_path[0] == '\0') {
        free(config.osk_layout_path);
        config.osk_layout_path = NULL;
    }
    SDL_Window* win = NULL;
    SDL_Renderer* renderer = NULL;
    TTF_Font* font = NULL;
    int char_w, char_h;
    Terminal* term = NULL;
    int master_fd = -1;

    if (!init_sdl(&win, &renderer, &font, &config, &char_w, &char_h)) {
        cleanup_resources(&config, term, NULL, renderer, win, font, -1, master_fd);
        return 1;
    }

    OnScreenKeyboard osk = {
        .active = false,
        .mode = OSK_MODE_CHARS,
        .position_mode = OSK_POSITION_OPPOSITE, // Default to opposite side
        .set_idx = 0,
        .char_idx = 0,
        .char_sets_by_modifier = {NULL}, // This initializes all 16 pointers to NULL
        .num_char_rows_by_modifier = {0},
        .controller = NULL,
        .joystick = NULL,
        .mod_ctrl = false,
        .mod_alt = false,
        .mod_shift = false,
        .mod_gui = false,
        .held_ctrl = false,
        .held_shift = false,
        .held_alt = false,
        .held_gui = false,
        .held_back = false,
        .held_start = false,
        .all_special_sets = NULL,
        .num_total_special_sets = 0,
        .cached_key_width = -1, // Initialize to -1 to force recalculation on first render
        .cached_set_idx = -1,
        .cached_mode = OSK_MODE_CHARS, // Default to char mode
        .cached_mod_mask = -1, // Initialize to -1 to force recalculation
        .show_special_set_name = false,
        .available_dynamic_key_sets = NULL,
        .num_available_dynamic_key_sets = 0,
        .loaded_key_set_names = NULL,
        .num_loaded_key_sets = 0
    };
    osk.key_cache = osk_key_cache_create();
    if (!osk.key_cache) {
        fprintf(stderr, "Failed to create OSK key cache.\n");
        cleanup_resources(&config, term, &osk, renderer, win, font, -1, master_fd);
        return 1;
    };

    osk_load_layout(&osk, config.osk_layout_path);

    osk_init_all_sets(&osk);

    init_input_devices(&osk, &config); // Modified: Pass config to init_input_devices

    int term_cols = config.win_w / char_w;
    int term_rows = config.win_h / char_h;

    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);

    if (pid < 0) {
        perror("forkpty");
        cleanup_resources(&config, term, &osk, renderer, win, font, -1, master_fd);
        return 1;
    } else if (pid == 0) {
        run_child_process(&config);
        return 1;
    }

    struct winsize ws = {.ws_row = (unsigned short)term_rows, .ws_col = (unsigned short)term_cols, .ws_xpixel = (unsigned short)config.win_w, .ws_ypixel = (unsigned short)config.win_h};
    if (ioctl(master_fd, TIOCSWINSZ, &ws) == -1) {
        perror("ioctl(TIOCSWINSZ) failed");
    }
    fcntl(master_fd, F_SETFL, O_NONBLOCK);

    if (!config.no_credit && !config.custom_command) {
        bool credit_running = true;
        while (credit_running) {
            SDL_Event event; // Note: Exit combo won't work here, only window close
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    cleanup_and_exit_from_credits(renderer, win, font, pid, &config, term, &osk, master_fd);
                }
                if (event.type == SDL_KEYDOWN || event.type == SDL_JOYBUTTONDOWN || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONDOWN) {
                    credit_running = false;
                    break;
                }
            }
            if (!credit_running) {
                break;
            }

            render_credit_screen(renderer, font, config.win_w, config.win_h);
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
        }
    }

    term = terminal_create(term_cols, term_rows, &config, renderer);
    if (!term) {
        fprintf(stderr, "Failed to create terminal instance.\n");
        cleanup_resources(&config, term, &osk, renderer, win, font, pid, master_fd);
        return 1;
    }

    term->screen_texture = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_RGBA8888,
                           SDL_TEXTUREACCESS_TARGET,
                           config.win_w, config.win_h);
    if (!term->screen_texture) {
        fprintf(stderr, "Failed to create screen texture: %s\n", SDL_GetError());
        cleanup_resources(&config, term, &osk, renderer, win, font, pid, master_fd);
        return 1;
    }

    term->glyph_cache = glyph_cache_create();
    if (!term->glyph_cache) {
        fprintf(stderr, "Failed to create glyph cache.\n");
        cleanup_resources(&config, term, &osk, renderer, win, font, pid, master_fd);
        return 1;
    }

    SDL_StartTextInput();

    main_loop(renderer, term, &font, &config, &char_w, &char_h, master_fd, &osk);

    cleanup_resources(&config, term, &osk, renderer, win, font, pid, master_fd);

    return 0;
}

static void main_loop(SDL_Renderer* renderer, Terminal* term, TTF_Font** font, Config* config, int* char_w, int* char_h, int master_fd, OnScreenKeyboard* osk)
{
    bool running = true;
    bool needs_render = true;

    ButtonRepeatState repeat_state = { .is_held = false, .action = ACTION_NONE };
    char buf[4096];

    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        ssize_t bytes_read = read(master_fd, buf, sizeof(buf));
        if (bytes_read > 0) {
            if (term->view_offset != 0) {
                term->view_offset = 0;
            }
            terminal_handle_input(term, buf, bytes_read);
            needs_render = true;
        } else if (bytes_read == 0) {
            fprintf(stderr, "PTY closed. Shell likely exited.\n");
            running = false;
        } else if (bytes_read < 0 && errno != EAGAIN) {
            fprintf(stderr, "PTY read error or closed: %s\n", strerror(errno));
            running = false;
        }

        if (term->response_len > 0) {
            if (write(master_fd, term->response_buffer, term->response_len) < 0) {
                perror("write to pty");
            }
            term->response_len = 0;
        }

        Uint32 current_time = SDL_GetTicks();

        if (repeat_state.is_held && current_time >= repeat_state.next_repeat_time) {
            handle_terminal_action(repeat_state.action, term, osk, &needs_render, master_fd, font, config, char_w, char_h);
            repeat_state.next_repeat_time = current_time + BUTTON_REPEAT_INTERVAL_MS;
        }

        if (current_time - term->last_blink_toggle_time >= CURSOR_BLINK_INTERVAL_MS) {
            term->cursor_blink_on = !term->cursor_blink_on;
            term->last_blink_toggle_time = current_time;
            if (term->view_offset == 0 && term->cursor_y >= 0 && term->cursor_y < term->rows) {
                term->dirty_lines[term->cursor_y] = true;
            }
            if (term->cursor_visible && term->view_offset == 0) {
                needs_render = true;
            }
        }

        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, BUTTON_REPEAT_INTERVAL_MS)) {
            do {
                handle_event(&event, &running, &needs_render, term, osk, master_fd, font, config, char_w, char_h, &repeat_state);
            } while (SDL_PollEvent(&event));
        }

        if (needs_render) {
            terminal_render(renderer, term, *font, *char_w, *char_h, osk, config->force_full_render, config->win_w, config->win_h);

            SDL_RenderPresent(renderer);
            needs_render = false;
        }

        if (config->target_fps > 0) {
            const Uint32 frame_delay = 1000 / config->target_fps;
            Uint32 frame_time = SDL_GetTicks() - frame_start;
            if (frame_time < frame_delay) {
                SDL_Delay(frame_delay - frame_time);
            }
        }
    }
}

static int check_exit_event(SDL_Event* event, OnScreenKeyboard* osk)
{
    switch (event->type) {
    case SDL_CONTROLLERBUTTONDOWN: {
        // Handle exit combo: BACK + START
        if (event->cbutton.button == ACTION_BUTTON_TAB) { // BACK button
            osk->held_back = true;
        }
        if (event->cbutton.button == ACTION_BUTTON_ENTER) { // START button
            osk->held_start = true;
        }
        if (osk->held_back && osk->held_start) {
            return 1; // Exit immediately, don't process other actions
        }
        break;
    }
    case SDL_CONTROLLERBUTTONUP: {
        // Update state for exit combo
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
 * @brief Handles the logic for toggling the OSK state.
 * This cycles through: OFF -> CHARS -> SPECIAL -> (back to CHARS or OFF).
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

static void handle_event(SDL_Event* event, bool* running, bool* needs_render,
                         Terminal* term, OnScreenKeyboard* osk, int master_fd,
                         TTF_Font** font, Config* config, int* char_w, int* char_h, ButtonRepeatState* repeat_state)
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

    // Handle OSK Toggle as a special case, as it changes the input mode
    TerminalAction mapped_action = ACTION_NONE;
    if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        mapped_action = map_cbutton_to_action(event->cbutton.button);
    } else if (event->type == SDL_KEYDOWN) {
        mapped_action = map_keyboard_to_action(&event->key);
    }

    if (mapped_action == ACTION_TOGGLE_OSK) {
        toggle_osk_state(osk, needs_render);
        return; // Consume the event
    }
    if (config->read_only) {
        return;
    }

    switch (event->type) {
    case SDL_TEXTINPUT:
        write(master_fd, event->text.text, strlen(event->text.text));
        break;
    case SDL_MOUSEWHEEL:
        // Positive y is scroll up (into history), negative is scroll down.
        // The scroll amount is inverted to match the sign convention of our helper.
        terminal_scroll_view(term, event->wheel.y * MOUSE_WHEEL_SCROLL_AMOUNT, needs_render);
        break;
    case SDL_KEYDOWN: {
        if (osk->active) {
            // Keyboard is navigating the OSK
            TerminalAction action = ACTION_NONE;
            switch (event->key.keysym.sym) {
            case SDLK_UP:
                action = ACTION_UP;
                break;
            case SDLK_DOWN:
                action = ACTION_DOWN;
                break;
            case SDLK_LEFT:
                action = ACTION_LEFT;
                break;
            case SDLK_RIGHT:
                action = ACTION_RIGHT;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                action = ACTION_SELECT; // In OSK, enter is select
                break;
            case SDLK_BACKSPACE:
                action = ACTION_BACK;
                break;
            case SDLK_TAB:
                action = ACTION_TAB;
                break;
            case SDLK_ESCAPE:
                action = ACTION_BACK;
                break; // ESC also acts as back in OSK
            }
            if (action != ACTION_NONE) {
                InternalCommand cmd = process_osk_action(action, term, osk, needs_render, master_fd);
                if (cmd != CMD_NONE) {
                    execute_internal_command(cmd, term, needs_render, master_fd, font, config, osk, char_w, char_h);
                }
            }
        } else {
            // Keyboard is for the terminal directly
            TerminalAction action = map_keyboard_to_action(&event->key);
            if (action != ACTION_NONE) {
                // This is for non-character actions like PageUp/PageDown scrolling
                handle_terminal_action(action, term, osk, needs_render, master_fd, font, config, char_w, char_h);
            } else {
                // This is for regular key presses that send sequences
                handle_key_down(&event->key, master_fd, term);
            }
        }
        break;
    }
    case SDL_CONTROLLERAXISMOTION: {
        handle_held_modifier_axis(event->caxis.axis, event->caxis.value, osk, needs_render);
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
        // Handle exit combo: BACK + START
        if (osk->active && handle_held_modifier_button(event->cbutton.button, true, osk, needs_render)) {
            // Modifier handled, consume event.
        } else {
            TerminalAction action = get_action_for_button_with_mode(event->cbutton.button, osk->active);
            process_and_repeat_action(action, term, osk, needs_render, master_fd, font, config, char_w, char_h, repeat_state);
        }
        break;
    }
    case SDL_CONTROLLERBUTTONUP: {
        if (osk->active && handle_held_modifier_button(event->cbutton.button, false, osk, needs_render)) {
            // Modifier handled, consume event.
        } else {
            TerminalAction action = get_action_for_button_with_mode(event->cbutton.button, osk->active);
            stop_repeating_action(action, repeat_state);
        }
        break;
    }
    case SDL_CONTROLLERDEVICEADDED:
        if (!osk->controller) {
            SDL_GameController* new_controller = SDL_GameControllerOpen(event->cdevice.which);
            if (new_controller) {
                if (osk->joystick) {
                    printf("Replacing fallback joystick with Game Controller.\n");
                    SDL_JoystickClose(osk->joystick);
                    osk->joystick = NULL;
                }
                osk->controller = new_controller;
                printf("Game Controller connected: %s\n", SDL_GameControllerName(osk->controller));
            }
        }
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (osk->controller && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(osk->controller)) == event->cdevice.which) {
            printf("Controller disconnected.\n");
            SDL_GameControllerClose(osk->controller);
            osk->controller = NULL;
            init_input_devices(osk, config); // Modified: Pass config to init_input_devices
        }
        break;
    default:
        break;
    }
}
