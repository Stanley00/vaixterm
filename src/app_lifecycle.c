/**
 * @file app_lifecycle.c
 * @brief Application lifecycle management including initialization, cleanup, and main loop.
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
#include <pwd.h>

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

#define BUTTON_REPEAT_INTERVAL_MS 100

#include "app_lifecycle.h"
#include "terminal_state.h"
#include "terminal.h"
#include "rendering.h"
#include "event_handler.h"
#include "font_manager.h"
#include "config_manager.h"
#include "error_handler.h"
#include "dirty_region_tracker.h"
#include "input.h"
#include "osk.h"
#include "debug.h"

/**
 * @brief Sets up SDL video hints for cross-platform compatibility.
 */
static void setup_video_hints(void)
{
    // Allow SDL to try multiple video drivers for better compatibility
    // This helps on systems where the default driver fails
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2,opengles,opengl,software");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    
    // Enable batching for better performance
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    
    DEBUG_LOG("Video hints configured for cross-platform compatibility");
}

/**
 * @brief Configures OpenGL attributes for cross-platform compatibility.
 * Supports both OpenGL ES (embedded devices) and desktop OpenGL.
 */
static void setup_gl_attributes(void)
{
    // Try OpenGL ES 2.0 first (works on embedded devices and desktops)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    
    // Basic attributes that work across platforms
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    // Disable features that may not be available on all platforms
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    
    DEBUG_LOG("OpenGL attributes configured (ES 2.0 profile)");
}

/**
 * @brief Initializes SDL subsystems and creates the main window and renderer.
 */
bool app_init_sdl(SDL_Window** win, SDL_Renderer** renderer, TTF_Font** font, 
                  const Config* config, int* char_w, int* char_h)
{
    // Set up video hints before initializing SDL
    setup_video_hints();
    
    DEBUG_LOG("Initializing SDL...");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        ERROR_LOG("SDL_Init Error: %s", SDL_GetError());
        return false;
    }
    DEBUG_LOG("SDL initialized successfully");

    DEBUG_LOG("Initializing SDL_ttf...");
    if (TTF_Init() == -1) {
        ERROR_LOG("TTF_Init Error: %s", TTF_GetError());
        SDL_Quit();
        return false;
    }
    DEBUG_LOG("SDL_ttf initialized successfully");
    
    DEBUG_LOG("Initializing SDL_image...");
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        ERROR_LOG("IMG_Init Error: %s", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    DEBUG_LOG("SDL_image initialized successfully");

    // Set up OpenGL attributes before creating window
    setup_gl_attributes();
    
    DEBUG_LOG("Creating window (%dx%d)...", config->win_w, config->win_h);
    *win = SDL_CreateWindow("VaixTerm", 
                           SDL_WINDOWPOS_UNDEFINED, 
                           SDL_WINDOWPOS_UNDEFINED, 
                           config->win_w, 
                           config->win_h, 
                           SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    if (!*win) {
        // Try again without OpenGL ES profile (desktop OpenGL fallback)
        DEBUG_LOG("Failed with OpenGL ES, trying desktop OpenGL...");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        
        *win = SDL_CreateWindow("VaixTerm", 
                               SDL_WINDOWPOS_UNDEFINED, 
                               SDL_WINDOWPOS_UNDEFINED, 
                               config->win_w, 
                               config->win_h, 
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        
        if (!*win) {
            // Final fallback: try without any OpenGL attributes
            DEBUG_LOG("Failed with desktop OpenGL, trying without GL attributes...");
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
            
            *win = SDL_CreateWindow("VaixTerm", 
                                   SDL_WINDOWPOS_UNDEFINED, 
                                   SDL_WINDOWPOS_UNDEFINED, 
                                   config->win_w, 
                                   config->win_h, 
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
            
            if (!*win) {
                ERROR_LOG("Window could not be created! SDL_Error: %s", SDL_GetError());
                IMG_Quit();
                TTF_Quit();
                SDL_Quit();
                return false;
            }
        }
    }
    DEBUG_LOG("Window created successfully");

    DEBUG_LOG("Creating renderer...");
    
    // Try multiple renderer configurations for maximum compatibility
    const struct {
        const char* name;
        Uint32 flags;
    } renderer_configs[] = {
        {"OpenGLES2 accelerated with vsync", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
        {"OpenGLES2 accelerated", SDL_RENDERER_ACCELERATED},
        {"Default accelerated", SDL_RENDERER_ACCELERATED},
        {"Software", SDL_RENDERER_SOFTWARE},
        {"Auto (no flags)", 0}
    };
    
    bool renderer_created = false;
    for (size_t i = 0; i < sizeof(renderer_configs) / sizeof(renderer_configs[0]); i++) {
        DEBUG_LOG("Trying renderer: %s", renderer_configs[i].name);
        *renderer = SDL_CreateRenderer(*win, -1, renderer_configs[i].flags);
        
        if (*renderer) {
            DEBUG_LOG("Successfully created renderer: %s", renderer_configs[i].name);
            
            // Query and log renderer info
            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(*renderer, &info) == 0) {
                DEBUG_LOG("Renderer name: %s", info.name);
                DEBUG_LOG("Renderer flags: 0x%x", info.flags);
            }
            
            renderer_created = true;
            break;
        } else {
            DEBUG_LOG("Failed: %s", SDL_GetError());
        }
    }
    
    if (!renderer_created) {
        ERROR_LOG("Could not create any renderer! Last SDL_Error: %s", SDL_GetError());
        SDL_DestroyWindow(*win);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    // Set renderer draw color to black (background)
    SDL_SetRenderDrawColor(*renderer, 0, 0, 0, 255);
    SDL_RenderClear(*renderer);
    SDL_RenderPresent(*renderer);
    DEBUG_LOG("Renderer initialized and cleared");

    // Load the font
    DEBUG_LOG("Loading font: %s (size: %d)", config->font_path, config->font_size);
    *font = TTF_OpenFont(config->font_path, config->font_size);
    if (!*font) {
        ERROR_LOG("Failed to load font! SDL_ttf Error: %s", TTF_GetError());
        // Try fallback font
        DEBUG_LOG("Trying fallback font...");
        *font = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", config->font_size);
        if (!*font) {
            ERROR_LOG("Failed to load fallback font! SDL_ttf Error: %s", TTF_GetError());
            SDL_DestroyRenderer(*renderer);
            SDL_DestroyWindow(*win);
            IMG_Quit();
            TTF_Quit();
            SDL_Quit();
            return false;
        }
    }
    DEBUG_LOG("Font loaded successfully");

    // Get character dimensions
    DEBUG_LOG("Getting font metrics...");
    int w, h;
    if (TTF_SizeText(*font, "W", &w, &h) != 0) {
        ERROR_LOG("Failed to get font metrics! SDL_ttf Error: %s", TTF_GetError());
        TTF_CloseFont(*font);
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*win);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    *char_w = w;
    *char_h = h;
    DEBUG_LOG("Font metrics: char_w=%d, char_h=%d", *char_w, *char_h);

    DEBUG_LOG("SDL initialization complete");
    return true;
}

/**
 * @brief Spawns the child process (shell) using PTY.
 */
void app_run_child_process(const Config* config)
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
        fprintf(stderr, "execlp failed for command '%s' with shell '%s': %s\n", 
                config->custom_command, shell_path, strerror(errno));
    } else {
        char argv0[256];
        snprintf(argv0, sizeof(argv0), "-%s", shell_name);
        execlp(shell_path, argv0, (char *)NULL);
        fprintf(stderr, "execlp failed for shell '%s': %s\n", shell_path, strerror(errno));
    }
    exit(1);
}

/**
 * @brief Initializes the terminal instance and related resources.
 */
Terminal* app_init_terminal(const Config* config, SDL_Renderer* renderer, int char_w, int char_h)
{
    int term_cols = config->win_w / char_w;
    int term_rows = config->win_h / char_h;
    
    Terminal* term = terminal_create(term_cols, term_rows, config, renderer);
    if (!term) {
        fprintf(stderr, "Failed to create terminal instance.\n");
        return NULL;
    }

    term->screen_texture = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_RGBA8888,
                           SDL_TEXTUREACCESS_TARGET,
                           config->win_w, config->win_h);
    if (!term->screen_texture) {
        fprintf(stderr, "Failed to create screen texture: %s\n", SDL_GetError());
        terminal_destroy(term);
        return NULL;
    }

    term->glyph_cache = glyph_cache_create();
    if (!term->glyph_cache) {
        fprintf(stderr, "Failed to create glyph cache.\n");
        terminal_destroy(term);
        return NULL;
    }

    return term;
}

/**
 * @brief Initializes the On-Screen Keyboard.
 */
bool app_init_osk(OnScreenKeyboard* osk, const Config* config)
{
    // Initialize OSK structure
    *osk = (OnScreenKeyboard) {
        .active = false,
        .mode = OSK_MODE_CHARS,
        .position_mode = OSK_POSITION_OPPOSITE,
        .set_idx = 0,
        .char_idx = 0,
        .char_sets_by_modifier = {NULL},
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
        .cached_key_width = -1,
        .cached_set_idx = -1,
        .cached_mode = OSK_MODE_CHARS,
        .cached_mod_mask = -1,
        .show_special_set_name = false,
        .available_dynamic_key_sets = NULL,
        .num_available_dynamic_key_sets = 0,
        .loaded_key_set_names = NULL,
        .num_loaded_key_sets = 0
    };
    
    osk->key_cache = osk_key_cache_create();
    if (!osk->key_cache) {
        fprintf(stderr, "Failed to create OSK key cache.\n");
        return false;
    }

    osk_load_layout(osk, config->osk_layout_path);
    osk_init_all_sets(osk);
    init_input_devices(osk, config);
    
    return true;
}

/**
 * @brief Runs the credit screen if enabled.
 */
bool app_run_credit_screen(SDL_Renderer* renderer, TTF_Font* font, const Config* config, 
                          pid_t pid, Terminal* term, OnScreenKeyboard* osk, int master_fd)
{
    if (config->no_credit || config->custom_command) {
        return true; // Skip credit screen
    }

    bool credit_running = true;
    while (credit_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                app_cleanup_and_exit(config, term, osk, renderer, NULL, font, pid, master_fd);
                return false; // Will not return
            }
            if (event.type == SDL_KEYDOWN || event.type == SDL_JOYBUTTONDOWN || 
                event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONDOWN) {
                credit_running = false;
                break;
            }
        }
        if (!credit_running) {
            break;
        }

        render_credit_screen(renderer, font, config->win_w, config->win_h);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return true;
}
/**
 * @brief Main application loop.
 */
void app_main_loop(SDL_Renderer* renderer, Terminal* term, TTF_Font** font, Config* config, 
                   int* char_w, int* char_h, int master_fd, OnScreenKeyboard* osk)
{
    bool running = true;
    bool needs_render = true;
    ButtonRepeatState repeat_state = { .is_held = false, .action = ACTION_NONE };
    char buf[4096];
    
    // Initial render
    DEBUG_LOG("Performing initial render...");
    terminal_render(renderer, term, *font, *char_w, *char_h, osk, true, config->win_w, config->win_h);
    SDL_RenderPresent(renderer);
    
    DEBUG_LOG("Entering main loop...");

    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        
        // Process all pending events first
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            DEBUG_LOG("Processing event: %d", event.type);
            
            switch (event.type) {
                case SDL_QUIT:
                    DEBUG_LOG("Received SDL_QUIT event, exiting...");
                    running = false;
                    break;
                    
                case SDL_WINDOWEVENT:
                    DEBUG_LOG("Window event: %d", event.window.event);
                    if (event.window.event == SDL_WINDOWEVENT_EXPOSED ||
                        event.window.event == SDL_WINDOWEVENT_SHOWN) {
                        needs_render = true;
                    }
                    break;
                    
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                case SDL_MOUSEMOTION:
                    // Handle input events
                    event_handle(&event, &running, &needs_render, term, osk, master_fd, 
                               font, config, char_w, char_h, &repeat_state);
                    break;
                    
                default:
                    // Handle other events
                    event_handle(&event, &running, &needs_render, term, osk, master_fd,
                               font, config, char_w, char_h, &repeat_state);
                    break;
            }
        }

        // Read from PTY if there's data available
        struct timeval tv = { .tv_sec = 0, .tv_usec = 16000 }; // 16ms timeout
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(master_fd, &fds);
        
        int ret = select(master_fd + 1, &fds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(master_fd, &fds)) {
            ssize_t bytes_read = read(master_fd, buf, sizeof(buf) - 1);
            if (bytes_read > 0) {
                buf[bytes_read] = '\0';
                DEBUG_LOG("Read %zd bytes from PTY: [%.*s]", 
                        bytes_read, (int)bytes_read, buf);
                
                if (term->view_offset != 0) {
                    term->view_offset = 0;
                }
                terminal_handle_input(term, buf, bytes_read);
                needs_render = true;
            } else if (bytes_read == 0) {
                INFO_LOG("PTY closed. Shell likely exited.");
                running = false;
            } else if (bytes_read < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ERROR_LOG("PTY read error: %s", strerror(errno));
                    running = false;
                }
            }
        } else if (ret < 0) {
            ERROR_LOG("select() error: %s", strerror(errno));
        }

        // Write terminal responses
        if (term->response_len > 0) {
            if (write(master_fd, term->response_buffer, term->response_len) < 0) {
                perror("write to pty");
            }
            term->response_len = 0;
        }

        Uint32 current_time = SDL_GetTicks();

        // Handle button repeat
        if (repeat_state.is_held && current_time >= repeat_state.next_repeat_time) {
            event_handle_terminal_action(repeat_state.action, term, osk, &needs_render, 
                                       master_fd, font, config, char_w, char_h);
            repeat_state.next_repeat_time = current_time + BUTTON_REPEAT_INTERVAL_MS;
        }

        // Handle cursor blinking
        if (current_time - term->last_blink_toggle_time >= CURSOR_BLINK_INTERVAL_MS) {
            term->cursor_blink_on = !term->cursor_blink_on;
            term->last_blink_toggle_time = current_time;
            if (term->view_offset == 0 && term->cursor_y >= 0 && term->cursor_y < term->rows) {
                terminal_mark_line_dirty(term, term->cursor_y);
                needs_render = true;
            }
        }

        // Handle rendering
        if (needs_render || (current_time - term->last_render_time) >= 33) { // Cap at ~30 FPS
            Uint32 render_start = SDL_GetTicks();
            
            // Clear the screen
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            
            // Render the terminal content
            terminal_render(renderer, term, *font, *char_w, *char_h, osk, 
                          needs_render || config->force_full_render, 
                          config->win_w, config->win_h);
            
            // Update the screen
            SDL_RenderPresent(renderer);
            
            Uint32 render_time = SDL_GetTicks() - render_start;
            if (render_time > 16) {  // Warn about slow rendering
                DEBUG_LOG("Slow render: %u ms", render_time);
            }
            
            term->last_render_time = render_start;
            needs_render = false;
        }

        // Frame rate limiting - cap at 60 FPS
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < 16) {  // ~60 FPS
            SDL_Delay(16 - frame_time);
        }
    }
}

/**
 * @brief Cleans up all allocated resources.
 */
void app_cleanup_resources(const Config* config, Terminal* term, OnScreenKeyboard* osk, 
                           SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, 
                           pid_t pid, int master_fd)
{
    (void)config; // Suppress unused parameter warning
    // Note: Config strings are freed by config_cleanup() in main

    // Clean up OSK
    if (osk) {
        if (osk->key_cache) {
            osk_key_cache_destroy(osk->key_cache);
        }
        osk_free_all_sets(osk);
        if (osk->controller) {
            SDL_GameControllerClose(osk->controller);
        }
        if (osk->joystick) {
            SDL_JoystickClose(osk->joystick);
        }
    }
    
    // Clean up terminal
    if (term) {
        if (term->glyph_cache) {
            glyph_cache_destroy(term->glyph_cache);
        }
        if (term->screen_texture) {
            SDL_DestroyTexture(term->screen_texture);
        }
        terminal_destroy(term);
    }
    
    SDL_StopTextInput();
    
    // Clean up child process
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    
    if (master_fd != -1) {
        close(master_fd);
    }
    
    // Clean up SDL resources
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
 * @brief Cleans up resources and exits the application.
 */
void app_cleanup_and_exit(const Config* config, Terminal* term, OnScreenKeyboard* osk, 
                         SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, 
                         pid_t pid, int master_fd)
{
    app_cleanup_resources(config, term, osk, renderer, win, font, pid, master_fd);
    exit(0);
}
