/**
 * @file config_manager.h
 * @brief Configuration management and command-line argument parsing functions.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>
#include "terminal_state.h"

/**
 * @brief Initializes a Config structure with default values.
 * @param config Configuration structure to initialize.
 */
void config_init_defaults(Config* config);

/**
 * @brief Validates configuration values and applies corrections if needed.
 * @param config Configuration structure to validate.
 * @return true if all values were valid, false if corrections were made.
 */
bool config_validate(Config* config);

/**
 * @brief Parses command-line arguments and updates the Config struct.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param config Configuration structure to update.
 */
void config_parse_args(int argc, char* argv[], Config* config);

/**
 * @brief Prints help information.
 * @param program_name Name of the program executable.
 */
void config_print_help(const char* program_name);

/**
 * @brief Frees all dynamically allocated memory in a Config structure.
 * @param config Configuration structure to cleanup.
 */
void config_cleanup(Config* config);

#endif // CONFIG_MANAGER_H
