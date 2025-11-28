/**
 * @file main_refactored.c
 * @brief Refactored main entry point for VaixTerm terminal emulator.
 * 
 * This is a clean, focused main function that delegates responsibilities
 * to specialized modules for better maintainability and organization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <SDL.h>

// PTY headers vary by OS
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>
#endif

#include "terminal_state.h"
#include "app_lifecycle.h"
#include "config_manager.h"
#include "error_handling.h"

/**
 * @brief Sets up the PTY and returns the master file descriptor and child PID.
 */
static bool setup_pty(const Config* config, int* master_fd, pid_t* pid)
{
    int term_cols = config->win_w / 12; // Approximate, will be updated later
    int term_rows = config->win_h / 16; // Approximate, will be updated later
    
    *pid = forkpty(master_fd, NULL, NULL, NULL);
    
    if (*pid < 0) {
        REPORT_SDL_ERROR("forkpty failed");
        return false;
    } else if (*pid == 0) {
        // Child process
        app_run_child_process(config);
        return false; // Will not return
    }
    
    // Parent process - set up PTY
    struct winsize ws = {
        .ws_row = (unsigned short)term_rows,
        .ws_col = (unsigned short)term_cols,
        .ws_xpixel = (unsigned short)config->win_w,
        .ws_ypixel = (unsigned short)config->win_h
    };
    
    if (ioctl(*master_fd, TIOCSWINSZ, &ws) == -1) {
        REPORT_SDL_ERROR("ioctl(TIOCSWINSZ) failed");
    }
    
    fcntl(*master_fd, F_SETFL, O_NONBLOCK);
    return true;
}

/**
 * @brief Main entry point for VaixTerm.
 */
int main(int argc, char* argv[])
{
    // Initialize locale for UTF-8 support
    setlocale(LC_CTYPE, "");
    
    // Initialize configuration with defaults
    Config config;
    config_init_defaults(&config);
    
    // Parse command-line arguments
    config_parse_args(argc, argv, &config);
    
    // Validate configuration
    if (!config_validate(&config)) {
        REPORT_ERROR(ERR_INVALID_ARGUMENT, ERROR_SEVERITY_ERROR, "Configuration validation failed, using corrected values");
    }
    
    // Initialize SDL and create window/renderer
    SDL_Window* win = NULL;
    SDL_Renderer* renderer = NULL;
    TTF_Font* font = NULL;
    int char_w, char_h;
    
    if (!app_init_sdl(&win, &renderer, &font, &config, &char_w, &char_h)) {
        REPORT_ERROR(ERR_SDL, ERROR_SEVERITY_ERROR, "Failed to initialize SDL");
        config_cleanup(&config);
        return 1;
    }
    
    // Set up PTY
    int master_fd = -1;
    pid_t pid = -1;
    
    if (!setup_pty(&config, &master_fd, &pid)) {
        REPORT_ERROR(ERR_PTY, ERROR_SEVERITY_ERROR, "Failed to set up PTY");
        app_cleanup_resources(&config, NULL, NULL, renderer, win, font, pid, master_fd);
        return 1;
    }
    
    // Initialize OSK
    OnScreenKeyboard osk;
    if (!app_init_osk(&osk, &config)) {
        REPORT_ERROR(ERR_NOT_IMPLEMENTED, ERROR_SEVERITY_ERROR, "Failed to initialize OSK");
        app_cleanup_resources(&config, NULL, &osk, renderer, win, font, pid, master_fd);
        return 1;
    }
    
    // Run credit screen if enabled
    if (!app_run_credit_screen(renderer, font, &config, pid, NULL, &osk, master_fd)) {
        // User quit during credit screen
        app_cleanup_resources(&config, NULL, &osk, renderer, win, font, pid, master_fd);
        return 0;
    }
    
    // Initialize terminal
    Terminal* term = app_init_terminal(&config, renderer, char_w, char_h);
    if (!term) {
        REPORT_ERROR(ERR_NOT_IMPLEMENTED, ERROR_SEVERITY_ERROR, "Failed to initialize terminal");
        app_cleanup_resources(&config, term, &osk, renderer, win, font, pid, master_fd);
        return 1;
    }
    
    // Update PTY window size with correct terminal dimensions
    // This ensures the shell knows the correct terminal width/height
    struct winsize ws = {
        .ws_row = (unsigned short)term->rows,
        .ws_col = (unsigned short)term->cols,
        .ws_xpixel = (unsigned short)config.win_w,
        .ws_ypixel = (unsigned short)config.win_h
    };
    if (ioctl(master_fd, TIOCSWINSZ, &ws) == -1) {
        REPORT_SDL_ERROR("Failed to update PTY window size");
    }
    
    // Start text input
    SDL_StartTextInput();
    
    // Run main application loop
    app_main_loop(renderer, term, &font, &config, &char_w, &char_h, master_fd, &osk);
    
    // Cleanup and exit
    app_cleanup_resources(&config, term, &osk, renderer, win, font, pid, master_fd);
    return 0;
}
