/**
 * @file module_integration.h
 * @brief Integration layer for all VaixTerm modules.
 *
 * This module provides the integration layer that connects all the refactored
 * modules together, ensuring proper initialization, cleanup, and coordination
 * between different components.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef MODULE_INTEGRATION_H
#define MODULE_INTEGRATION_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"
#include "error_codes.h"
#include "validation.h"
#include "error_handling.h"

// Integration state
typedef struct {
    bool error_handling_initialized;
    bool validation_enabled;
    bool osk_initialized;
    bool rendering_initialized;
    bool input_initialized;
    bool terminal_initialized;
    uint64_t init_timestamp;
} IntegrationState;

/**
 * @brief Initialize all VaixTerm modules
 *
 * This function initializes all modules in the correct order and establishes
 * the necessary connections between them.
 *
 * @param config Application configuration
 * @return ErrorCode Result of initialization
 */
ErrorCode initialize_all_modules(const Config* config);

/**
 * @brief Cleanup all VaixTerm modules
 *
 * This function cleans up all modules in the correct order, ensuring
 * proper resource deallocation.
 */
void cleanup_all_modules(void);

/**
 * @brief Get the current integration state
 *
 * @return const IntegrationState* Current integration state
 */
const IntegrationState* get_integration_state(void);

/**
 * @brief Initialize OSK modules
 *
 * @param osk Pointer to OSK structure
 * @param config Application configuration
 * @return ErrorCode Result of initialization
 */
ErrorCode initialize_osk_modules(OnScreenKeyboard* osk, const Config* config);

/**
 * @brief Initialize rendering modules
 *
 * @param renderer SDL renderer
 * @param font SDL font
 * @param term Pointer to terminal structure
 * @return ErrorCode Result of initialization
 */
ErrorCode initialize_rendering_modules(SDL_Renderer* renderer, TTF_Font* font, Terminal* term);

/**
 * @brief Initialize input modules
 *
 * @param osk Pointer to OSK structure
 * @param config Application configuration
 * @return ErrorCode Result of initialization
 */
ErrorCode initialize_input_modules(OnScreenKeyboard* osk, const Config* config);

/**
 * @brief Initialize terminal core modules
 *
 * @param term Pointer to terminal structure
 * @param config Application configuration
 * @return ErrorCode Result of initialization
 */
ErrorCode initialize_terminal_modules(Terminal* term, const Config* config);

/**
 * @brief Validate module dependencies
 *
 * This function checks that all required dependencies between modules
 * are properly satisfied.
 *
 * @return ErrorCode Result of validation
 */
ErrorCode validate_module_dependencies(void);

/**
 * @brief Synchronize module states
 *
 * This function ensures that all modules have consistent state information.
 *
 * @param term Pointer to terminal structure
 * @param osk Pointer to OSK structure
 * @return ErrorCode Result of synchronization
 */
ErrorCode synchronize_module_states(Terminal* term, OnScreenKeyboard* osk);

/**
 * @brief Handle module errors
 *
 * This function provides centralized error handling for all modules,
 * implementing appropriate recovery strategies.
 *
 * @param error Error context
 * @return bool True if error was handled successfully
 */
bool handle_module_error(const ErrorContext* error);

/**
 * @brief Update module configurations
 *
 * This function updates configuration for all modules when settings change.
 *
 * @param config New configuration
 * @return ErrorCode Result of update
 */
ErrorCode update_module_configurations(const Config* config);

/**
 * @brief Get module performance statistics
 *
 * @param module_name Name of the module
 * @param stats Output buffer for statistics
 * @param stats_size Size of statistics buffer
 * @return ErrorCode Result of operation
 */
ErrorCode get_module_statistics(const char* module_name, char* stats, size_t stats_size);

/**
 * @brief Reset all modules to initial state
 *
 * This function resets all modules to their initial state without
 * deallocating resources.
 *
 * @return ErrorCode Result of reset
 */
ErrorCode reset_all_modules(void);

/**
 * @brief Check if all modules are healthy
 *
 * @return bool True if all modules are in healthy state
 */
bool are_modules_healthy(void);

/**
 * @brief Perform module health check
 *
 * @param module_name Name of the module to check
 * @return bool True if module is healthy
 */
bool is_module_healthy(const char* module_name);

/**
 * @brief Get module initialization order
 *
 * @param module_names Output buffer for module names
 * @param max_modules Maximum number of modules to return
 * @return int Number of modules returned
 */
int get_module_initialization_order(char** module_names, int max_modules);

/**
 * @brief Register module lifecycle callbacks
 *
 * @param module_name Name of the module
 * @param init_callback Initialization callback
 * @param cleanup_callback Cleanup callback
 * @param health_check_callback Health check callback
 * @return ErrorCode Result of registration
 */
ErrorCode register_module_callbacks(const char* module_name, 
                                   void (*init_callback)(void),
                                   void (*cleanup_callback)(void),
                                   bool (*health_check_callback)(void));

/**
 * @brief Enable/disable validation for all modules
 *
 * @param enabled Whether validation should be enabled
 */
void set_global_validation(bool enabled);

/**
 * @brief Enable/disable enhanced error handling for all modules
 *
 * @param enabled Whether enhanced error handling should be enabled
 */
void set_global_error_handling(bool enabled);

// Integration macros for consistent module usage
#define INTEGRATION_VALIDATE_AND_RETURN(ptr, param) \
    do { \
        if (get_integration_state()->validation_enabled) { \
            VALIDATE_AND_RETURN(ptr, param); \
        } \
    } while(0)

#define INTEGRATION_REPORT_ERROR(code, severity, message) \
    do { \
        if (get_integration_state()->error_handling_initialized) { \
            REPORT_ERROR(code, severity, message); \
        } \
    } while(0)

#define INTEGRATION_REPORT_SDL_ERROR(operation) \
    do { \
        if (get_integration_state()->error_handling_initialized) { \
            REPORT_SDL_ERROR(operation); \
        } \
    } while(0)

#endif // MODULE_INTEGRATION_H
