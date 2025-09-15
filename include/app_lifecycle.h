/**
 * @file app_lifecycle.h
 * @brief Application lifecycle management functions.
 */

#ifndef APP_LIFECYCLE_H
#define APP_LIFECYCLE_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <sys/types.h>

#include "terminal_state.h"

/**
 * @brief Initializes SDL subsystems and creates the main window and renderer.
 * @param win Pointer to store the created window.
 * @param renderer Pointer to store the created renderer.
 * @param font Pointer to store the loaded font.
 * @param config Application configuration.
 * @param char_w Pointer to store character width.
 * @param char_h Pointer to store character height.
 * @return true on success, false on failure.
 */
bool app_init_sdl(SDL_Window** win, SDL_Renderer** renderer, TTF_Font** font, 
                  const Config* config, int* char_w, int* char_h);

/**
 * @brief Spawns the child process (shell) using PTY.
 * @param config Application configuration.
 */
void app_run_child_process(const Config* config);

/**
 * @brief Initializes the terminal instance and related resources.
 * @param config Application configuration.
 * @param renderer SDL renderer.
 * @param char_w Character width.
 * @param char_h Character height.
 * @return Terminal instance or NULL on failure.
 */
Terminal* app_init_terminal(const Config* config, SDL_Renderer* renderer, int char_w, int char_h);

/**
 * @brief Initializes the On-Screen Keyboard.
 * @param osk OSK structure to initialize.
 * @param config Application configuration.
 * @return true on success, false on failure.
 */
bool app_init_osk(OnScreenKeyboard* osk, const Config* config);

/**
 * @brief Runs the credit screen if enabled.
 * @param renderer SDL renderer.
 * @param font Font for rendering.
 * @param config Application configuration.
 * @param pid Child process ID.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param master_fd PTY master file descriptor.
 * @return true to continue, false to exit.
 */
bool app_run_credit_screen(SDL_Renderer* renderer, TTF_Font* font, const Config* config, 
                          pid_t pid, Terminal* term, OnScreenKeyboard* osk, int master_fd);

/**
 * @brief Main application loop.
 * @param renderer SDL renderer.
 * @param term Terminal instance.
 * @param font Font pointer (may be modified for font size changes).
 * @param config Application configuration.
 * @param char_w Character width pointer (may be modified).
 * @param char_h Character height pointer (may be modified).
 * @param master_fd PTY master file descriptor.
 * @param osk OSK instance.
 */
void app_main_loop(SDL_Renderer* renderer, Terminal* term, TTF_Font** font, Config* config, 
                   int* char_w, int* char_h, int master_fd, OnScreenKeyboard* osk);

/**
 * @brief Cleans up all allocated resources.
 * @param config Application configuration.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param renderer SDL renderer.
 * @param win SDL window.
 * @param font TTF font.
 * @param pid Child process ID.
 * @param master_fd PTY master file descriptor.
 */
void app_cleanup_resources(const Config* config, Terminal* term, OnScreenKeyboard* osk, 
                           SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, 
                           pid_t pid, int master_fd);

/**
 * @brief Cleans up resources and exits the application.
 * @param config Application configuration.
 * @param term Terminal instance.
 * @param osk OSK instance.
 * @param renderer SDL renderer.
 * @param win SDL window.
 * @param font TTF font.
 * @param pid Child process ID.
 * @param master_fd PTY master file descriptor.
 */
void app_cleanup_and_exit(const Config* config, Terminal* term, OnScreenKeyboard* osk, 
                         SDL_Renderer* renderer, SDL_Window* win, TTF_Font* font, 
                         pid_t pid, int master_fd);

#endif // APP_LIFECYCLE_H
