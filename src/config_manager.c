/**
 * @file config_manager.c
 * @brief Configuration management and command-line argument parsing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_manager.h"
#include "config.h"

/**
 * @brief Initializes a Config structure with default values.
 */
void config_init_defaults(Config* config)
{
    config->win_w = DEFAULT_WINDOW_WIDTH;
    config->win_h = DEFAULT_WINDOW_HEIGHT;
    config->font_path = strdup(DEFAULT_FONT_FILE_PATH);
    config->font_size = DEFAULT_FONT_SIZE_POINTS;
    config->custom_command = NULL;
    config->scrollback_lines = DEFAULT_SCROLLBACK_LINES;
    config->force_full_render = false;
    config->background_image_path = DEFAULT_BACKGROUND_IMAGE_PATH;
    config->colorscheme_path = NULL;
    config->target_fps = 30;
    config->read_only = false;
    config->no_credit = false;
    config->osk_layout_path = NULL;
    config->key_sets = NULL;
    config->num_key_sets = 0;
}

/**
 * @brief Validates configuration values and applies corrections if needed.
 */
bool config_validate(Config* config)
{
    bool valid = true;
    
    // Validate window dimensions
    if (config->win_w < 320 || config->win_w > 4096) {
        fprintf(stderr, "Warning: Invalid window width %d, using default %d\n", 
                config->win_w, DEFAULT_WINDOW_WIDTH);
        config->win_w = DEFAULT_WINDOW_WIDTH;
        valid = false;
    }
    
    if (config->win_h < 240 || config->win_h > 4096) {
        fprintf(stderr, "Warning: Invalid window height %d, using default %d\n", 
                config->win_h, DEFAULT_WINDOW_HEIGHT);
        config->win_h = DEFAULT_WINDOW_HEIGHT;
        valid = false;
    }
    
    // Validate font size
    if (config->font_size < 6 || config->font_size > 72) {
        fprintf(stderr, "Warning: Invalid font size %d, using default %d\n", 
                config->font_size, DEFAULT_FONT_SIZE_POINTS);
        config->font_size = DEFAULT_FONT_SIZE_POINTS;
        valid = false;
    }
    
    // Validate scrollback lines
    if (config->scrollback_lines < 0 || config->scrollback_lines > 100000) {
        fprintf(stderr, "Warning: Invalid scrollback lines %d, using default %d\n", 
                config->scrollback_lines, DEFAULT_SCROLLBACK_LINES);
        config->scrollback_lines = DEFAULT_SCROLLBACK_LINES;
        valid = false;
    }
    
    // Validate FPS
    if (config->target_fps < 0 || config->target_fps > 120) {
        fprintf(stderr, "Warning: Invalid target FPS %d, using default 30\n", 
                config->target_fps);
        config->target_fps = 30;
        valid = false;
    }
    
    // Validate font path exists
    if (!config->font_path || config->font_path[0] == '\0') {
        fprintf(stderr, "Warning: Empty font path, using default\n");
        free(config->font_path);
        config->font_path = strdup(DEFAULT_FONT_FILE_PATH);
        valid = false;
    }
    
    // Clean up empty OSK layout path
    if (config->osk_layout_path && config->osk_layout_path[0] == '\0') {
        free(config->osk_layout_path);
        config->osk_layout_path = NULL;
    }
    
    return valid;
}

/**
 * @brief Parses command-line arguments and updates the Config struct.
 */
void config_parse_args(int argc, char* argv[], Config* config)
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

            config->num_key_sets++;
            config->key_sets = realloc(config->key_sets, sizeof(struct KeySetArg) * (size_t)config->num_key_sets);
            if (!config->key_sets) {
                fprintf(stderr, "Fatal: Could not allocate memory for key set arguments.\n");
                exit(1);
            }
            config->key_sets[config->num_key_sets - 1].path = strdup(path);
            config->key_sets[config->num_key_sets - 1].load_at_startup = load;
        } else if (strcmp(argv[i], "--osk-layout") == 0 && i + 1 < argc) {
            free(config->osk_layout_path);
            config->osk_layout_path = strdup(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0) {
            config_print_help(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            exit(1);
        }
    }
}

/**
 * @brief Prints help information.
 */
void config_print_help(const char* program_name)
{
    fprintf(stdout, "vaixterm - A simple, modern terminal emulator for game handhelds.\n\n");
    fprintf(stdout, "Usage: %s [options]\n\n", program_name);
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
}

/**
 * @brief Frees all dynamically allocated memory in a Config structure.
 */
void config_cleanup(Config* config)
{
    if (!config) {
        return;
    }
    
    free(config->font_path);
    free(config->custom_command);
    free(config->background_image_path);
    free(config->colorscheme_path);
    free(config->osk_layout_path);
    
    for (int i = 0; i < config->num_key_sets; ++i) {
        free(config->key_sets[i].path);
    }
    free(config->key_sets);
    
    // Reset to safe state
    config->font_path = NULL;
    config->custom_command = NULL;
    config->background_image_path = NULL;
    config->colorscheme_path = NULL;
    config->osk_layout_path = NULL;
    config->key_sets = NULL;
    config->num_key_sets = 0;
}
