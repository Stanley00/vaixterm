/**
 * @file config_manager.c
 * @brief Configuration management and command-line argument parsing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_manager.h"
#include "error_codes.h"
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
    config->log_level = LOG_LEVEL_WARN;
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
        WARN_LOG("Invalid window width %d, using default %d", config->win_w, DEFAULT_WINDOW_WIDTH);
        config->win_w = DEFAULT_WINDOW_WIDTH;
        valid = false;
    }
    
    if (config->win_h < 240 || config->win_h > 4096) {
        WARN_LOG("Invalid window height %d, using default %d", config->win_h, DEFAULT_WINDOW_HEIGHT);
        config->win_h = DEFAULT_WINDOW_HEIGHT;
        valid = false;
    }
    
    if (config->font_size < 6 || config->font_size > 72) {
        WARN_LOG("Invalid font size %d, using default %d", config->font_size, DEFAULT_FONT_SIZE_POINTS);
        config->font_size = DEFAULT_FONT_SIZE_POINTS;
        valid = false;
    }
    
    if (config->scrollback_lines < 0 || config->scrollback_lines > 100000) {
        WARN_LOG("Invalid scrollback lines %d, using default %d", config->scrollback_lines, DEFAULT_SCROLLBACK_LINES);
        config->scrollback_lines = DEFAULT_SCROLLBACK_LINES;
        valid = false;
    }
    
    if (config->target_fps < 0 || config->target_fps > 120) {
        WARN_LOG("Invalid target FPS %d, using default 30", config->target_fps);
        config->target_fps = 30;
        valid = false;
    }
    
    if (!config->font_path || config->font_path[0] == '\0') {
        WARN_LOG("Empty font path, using default");
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
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* lvl = argv[++i];
            if (strcasecmp(lvl, "debug") == 0) config->log_level = LOG_LEVEL_DEBUG;
            else if (strcasecmp(lvl, "info") == 0) config->log_level = LOG_LEVEL_INFO;
            else if (strcasecmp(lvl, "warn") == 0) config->log_level = LOG_LEVEL_WARN;
            else if (strcasecmp(lvl, "error") == 0) config->log_level = LOG_LEVEL_ERROR;
            else if (strcasecmp(lvl, "fatal") == 0) config->log_level = LOG_LEVEL_FATAL;
            else { fprintf(stderr, "Invalid log level: %s (use debug/info/warn/error/fatal)\n", lvl); exit(1); }
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
                ERROR_LOG("Could not allocate memory for key set arguments");
                exit(1);
            }
            config->key_sets[config->num_key_sets - 1].path = strdup(path);
            config->key_sets[config->num_key_sets - 1].load_at_startup = load;
        } else if (strcmp(argv[i], "--osk-layout") == 0 && i + 1 < argc) {
            free(config->osk_layout_path);
            config->osk_layout_path = strdup(argv[++i]);
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            fprintf(stdout, "vaixterm %s\n", VERSION);
            exit(0);
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
    fprintf(stdout, "  --log-level <level>        Set log verbosity: debug/info/warn/error/fatal (default: warn).\n");
    fprintf(stdout, "  --force-full-render        Force a full re-render on every frame.\n");
    fprintf(stdout, "  --key-set [-|+]<path>      Add key set ('-': available, '+': load).\n");
    fprintf(stdout, "  --osk-layout <path>        Use a custom OSK layout file.\n");
    fprintf(stdout, "  --version                  Show version and exit.\n");
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

/**
 * @brief Trims whitespace from a string in-place.
 */
static void trim_whitespace(char* str)
{
    if (!str) return;
    char* end = str + strlen(str) - 1;
    while (end >= str && isspace(*end)) {
        *end-- = '\0';
    }
    char* start = str;
    while (*start && isspace(*start)) start++;
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * @brief Loads configuration from a file.
 * Config file format: simple key=value pairs, lines starting with # are comments.
 * Search order: $XDG_CONFIG_HOME/vaixterm/vaixterm.conf, then ~/.config/vaixterm/vaixterm.conf
 */
bool config_load_from_file(Config* config, const char* explicit_path)
{
    char path[4096];
    FILE* file = NULL;
    
    if (explicit_path) {
        file = fopen(explicit_path, "r");
        if (file) {
            strncpy(path, explicit_path, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        }
    } else {
        // Try XDG config directory first
        const char* xdg_config = getenv("XDG_CONFIG_HOME");
        if (xdg_config) {
            snprintf(path, sizeof(path), "%s/vaixterm/vaixterm.conf", xdg_config);
            file = fopen(path, "r");
        }
        if (!file) {
            // Fall back to ~/.config/vaixterm/vaixterm.conf
            const char* home = getenv("HOME");
            if (home) {
                snprintf(path, sizeof(path), "%s/.config/vaixterm/vaixterm.conf", home);
                file = fopen(path, "r");
            }
        }
    }
    
    if (!file) {
        return false; // No config file found, use defaults
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        trim_whitespace(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        trim_whitespace(key);
        trim_whitespace(value);
        
        // Remove surrounding quotes if present
        if (value[0] == '"' && value[strlen(value) - 1] == '"') {
            value[strlen(value) - 1] = '\0';
            value++;
        }
        
        if (strcmp(key, "width") == 0) {
            config->win_w = atoi(value);
        } else if (strcmp(key, "height") == 0) {
            config->win_h = atoi(value);
        } else if (strcmp(key, "font") == 0) {
            free(config->font_path);
            config->font_path = strdup(value);
        } else if (strcmp(key, "font_size") == 0) {
            config->font_size = atoi(value);
        } else if (strcmp(key, "scrollback") == 0) {
            config->scrollback_lines = atoi(value);
        } else if (strcmp(key, "exec") == 0) {
            free(config->custom_command);
            config->custom_command = strdup(value);
        } else if (strcmp(key, "background") == 0) {
            free(config->background_image_path);
            config->background_image_path = strdup(value);
        } else if (strcmp(key, "colorscheme") == 0) {
            free(config->colorscheme_path);
            config->colorscheme_path = strdup(value);
        } else if (strcmp(key, "fps") == 0) {
            config->target_fps = atoi(value);
        } else if (strcmp(key, "read_only") == 0) {
            config->read_only = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } else if (strcmp(key, "no_credit") == 0) {
            config->no_credit = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } else if (strcmp(key, "force_full_render") == 0) {
            config->force_full_render = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } else if (strcmp(key, "log_level") == 0) {
            if (strcasecmp(value, "debug") == 0) config->log_level = LOG_LEVEL_DEBUG;
            else if (strcasecmp(value, "info") == 0) config->log_level = LOG_LEVEL_INFO;
            else if (strcasecmp(value, "warn") == 0) config->log_level = LOG_LEVEL_WARN;
            else if (strcasecmp(value, "error") == 0) config->log_level = LOG_LEVEL_ERROR;
            else if (strcasecmp(value, "fatal") == 0) config->log_level = LOG_LEVEL_FATAL;
        } else if (strcmp(key, "osk_layout") == 0) {
            free(config->osk_layout_path);
            config->osk_layout_path = strdup(value);
        }
    }
    
    fclose(file);
    INFO_LOG("Loaded config from: %s", path);
    return true;
}
