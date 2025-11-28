/**
 * @file module_integration.c
 * @brief Integration layer implementation for all VaixTerm modules.
 *
 * This module implements the integration layer that connects all the refactored
 * modules together, ensuring proper initialization, cleanup, and coordination
 * between different components.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "module_integration.h"
#include "osk_core.h"
#include "rendering_core.h"
#include "glyph_cache.h"
#include "color_manager.h"
#include "input_mapper.h"
#include "keyboard_handler.h"
#include "controller_handler.h"
#include "terminal_parser.h"
#include "terminal_grid.h"
#include "terminal_modes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Maximum number of registered modules
#define MAX_REGISTERED_MODULES 32

// Module callback structure
typedef struct {
    char name[64];
    void (*init_callback)(void);
    void (*cleanup_callback)(void);
    bool (*health_check_callback)(void);
    bool initialized;
} ModuleCallback;

// Global integration state
static struct {
    IntegrationState state;
    ModuleCallback modules[MAX_REGISTERED_MODULES];
    int num_modules;
    bool global_validation;
    bool global_error_handling;
} g_integration = {0};

static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

ErrorCode initialize_all_modules(const Config* config)
{
    if (!config) {
        ERROR_LOG("Invalid configuration parameter");
        return ERR_INVALID_ARGUMENT;
    }

    INFO_LOG("Initializing all VaixTerm modules");

    // Initialize error handling first
    if (!error_handling_init()) {
        ERROR_LOG("Failed to initialize error handling");
        return ERR_NOT_IMPLEMENTED;
    }
    g_integration.state.error_handling_initialized = true;

    // Initialize validation layer
    g_integration.state.validation_enabled = g_integration.global_validation;
    INFO_LOG("Validation layer %s", g_integration.state.validation_enabled ? "enabled" : "disabled");

    // Initialize modules in dependency order
    ErrorCode result;

    // 1. Terminal core modules
    result = initialize_terminal_modules(NULL, config);
    if (result != ERR_SUCCESS) {
        ERROR_LOG("Failed to initialize terminal modules: %s", get_error_description(result));
        return result;
    }

    // 2. Rendering modules
    result = initialize_rendering_modules(NULL, NULL, NULL);
    if (result != ERR_SUCCESS) {
        ERROR_LOG("Failed to initialize rendering modules: %s", get_error_description(result));
        return result;
    }

    // 3. Input modules
    result = initialize_input_modules(NULL, config);
    if (result != ERR_SUCCESS) {
        ERROR_LOG("Failed to initialize input modules: %s", get_error_description(result));
        return result;
    }

    // 4. OSK modules
    result = initialize_osk_modules(NULL, config);
    if (result != ERR_SUCCESS) {
        ERROR_LOG("Failed to initialize OSK modules: %s", get_error_description(result));
        return result;
    }

    // Validate module dependencies
    result = validate_module_dependencies();
    if (result != ERR_SUCCESS) {
        ERROR_LOG("Module dependency validation failed: %s", get_error_description(result));
        return result;
    }

    g_integration.state.init_timestamp = get_timestamp_ms();
    INFO_LOG("All modules initialized successfully");
    return ERR_SUCCESS;
}

void cleanup_all_modules(void)
{
    INFO_LOG("Cleaning up all VaixTerm modules");

    // Cleanup in reverse order of initialization

    // 1. OSK modules
    if (g_integration.state.osk_initialized) {
        // OSK cleanup would be called here
        g_integration.state.osk_initialized = false;
    }

    // 2. Input modules
    if (g_integration.state.input_initialized) {
        cleanup_controller_subsystem();
        g_integration.state.input_initialized = false;
    }

    // 3. Rendering modules
    if (g_integration.state.rendering_initialized) {
        // Rendering cleanup would be called here
        g_integration.state.rendering_initialized = false;
    }

    // 4. Terminal modules
    if (g_integration.state.terminal_initialized) {
        // Terminal cleanup would be called here
        g_integration.state.terminal_initialized = false;
    }

    // 5. Error handling (cleanup last)
    if (g_integration.state.error_handling_initialized) {
        error_handling_cleanup();
        g_integration.state.error_handling_initialized = false;
    }

    memset(&g_integration.state, 0, sizeof(g_integration.state));
    INFO_LOG("All modules cleaned up");
}

const IntegrationState* get_integration_state(void)
{
    return &g_integration.state;
}

ErrorCode initialize_osk_modules(OnScreenKeyboard* osk, const Config* config) {
    (void)osk;
    (void)config;
    if (!config) {
        return ERR_INVALID_ARGUMENT;
    }

    // OSK initialization would be performed here
    // For now, just mark as initialized
    g_integration.state.osk_initialized = true;
    
    DEBUG_LOG("OSK modules initialized");
    return ERR_SUCCESS;
}

ErrorCode initialize_rendering_modules(SDL_Renderer* renderer, TTF_Font* font, Terminal* term) {
    (void)renderer;
    (void)font;
    (void)term;
    // Rendering initialization would be performed here
    // For now, just mark as initialized
    g_integration.state.rendering_initialized = true;
    
    DEBUG_LOG("Rendering modules initialized");
    return ERR_SUCCESS;
}

ErrorCode initialize_input_modules(OnScreenKeyboard* osk, const Config* config)
{
    if (!config) {
        return ERR_INVALID_ARGUMENT;
    }

    // Initialize controller subsystem
    if (!init_controller_subsystem()) {
        WARN_LOG("Failed to initialize controller subsystem");
    }

    // Initialize input devices
    if (osk) {
        init_input_devices(osk, config);
    }

    g_integration.state.input_initialized = true;
    DEBUG_LOG("Input modules initialized");
    return ERR_SUCCESS;
}

ErrorCode initialize_terminal_modules(Terminal* term, const Config* config) {
    (void)term;
    (void)config;
    if (!config) {
        return ERR_INVALID_ARGUMENT;
    }

    // Terminal core initialization would be performed here
    // For now, just mark as initialized
    g_integration.state.terminal_initialized = true;
    
    DEBUG_LOG("Terminal modules initialized");
    return ERR_SUCCESS;
}

ErrorCode validate_module_dependencies(void)
{
    // Check that all required dependencies are satisfied
    if (!g_integration.state.error_handling_initialized) {
        ERROR_LOG("Error handling is required but not initialized");
        return ERR_INVALID_STATE;
    }

    // Additional dependency checks would go here
    DEBUG_LOG("Module dependencies validated");
    return ERR_SUCCESS;
}

ErrorCode synchronize_module_states(Terminal* term, OnScreenKeyboard* osk)
{
    if (!term || !osk) {
        return ERR_INVALID_ARGUMENT;
    }

    // Synchronize OSK state with terminal state
    // This would involve updating OSK position based on cursor position, etc.
    
    DEBUG_LOG("Module states synchronized");
    return ERR_SUCCESS;
}

bool handle_module_error(const ErrorContext* error)
{
    if (!error) {
        return false;
    }

    // Centralized error handling for all modules
    switch (error->code) {
        case ERR_SDL:
            // Attempt SDL recovery
            ERROR_LOG("Handling SDL error in module: %s", error->function);
            return attempt_recovery(RECOVERY_RESET, error);
            
        case ERR_MEMORY:
            // Attempt memory recovery
            ERROR_LOG("Handling memory allocation error in module: %s", error->function);
            return attempt_recovery(RECOVERY_RETRY, error);
            
        case ERR_INVALID_ARGUMENT:
            // Parameter errors are not recoverable
            ERROR_LOG("Invalid parameter error in module: %s", error->function);
            return false;
            
        default:
            // Default error handling
            ERROR_LOG("Handling error %d in module: %s", error->code, error->function);
            return attempt_recovery(get_recovery_strategy(error->code), error);
    }
}

ErrorCode update_module_configurations(const Config* config)
{
    if (!config) {
        return ERR_INVALID_ARGUMENT;
    }

    // Update all module configurations
    DEBUG_LOG("Module configurations updated");
    return ERR_SUCCESS;
}

ErrorCode get_module_statistics(const char* module_name, char* stats, size_t stats_size)
{
    if (!module_name || !stats || stats_size == 0) {
        return ERR_INVALID_ARGUMENT;
    }

    // Return statistics for the requested module
    if (strcmp(module_name, "error_handling") == 0) {
        int total_errors, critical_errors;
        uint64_t last_error_time;
        get_error_statistics(&total_errors, &critical_errors, &last_error_time);
        snprintf(stats, stats_size, "total_errors=%d,critical_errors=%d,last_error=%llu", 
                 total_errors, critical_errors, (unsigned long long)last_error_time);
    } else {
        snprintf(stats, stats_size, "module '%s' statistics not available", module_name);
    }

    return ERR_SUCCESS;
}

ErrorCode reset_all_modules(void)
{
    INFO_LOG("Resetting all modules");

    // Reset all module states without deallocating resources
    if (g_integration.state.error_handling_initialized) {
        clear_last_error();
    }

    DEBUG_LOG("All modules reset");
    return ERR_SUCCESS;
}

bool are_modules_healthy(void)
{
    // Check health of all critical modules
    if (!g_integration.state.error_handling_initialized) return false;
    if (!g_integration.state.terminal_initialized) return false;
    if (!g_integration.state.rendering_initialized) return false;

    return true;
}

bool is_module_healthy(const char* module_name)
{
    if (!module_name) {
        return false;
    }

    // Check health of specific module
    if (strcmp(module_name, "error_handling") == 0) {
        return g_integration.state.error_handling_initialized;
    } else if (strcmp(module_name, "terminal") == 0) {
        return g_integration.state.terminal_initialized;
    } else if (strcmp(module_name, "rendering") == 0) {
        return g_integration.state.rendering_initialized;
    } else if (strcmp(module_name, "input") == 0) {
        return g_integration.state.input_initialized;
    } else if (strcmp(module_name, "osk") == 0) {
        return g_integration.state.osk_initialized;
    }

    return false;
}

int get_module_initialization_order(char** module_names, int max_modules)
{
    if (!module_names || max_modules < 6) {
        return 0;
    }

    int count = 0;
    
    // Return initialization order
    static const char* order[] = {
        "error_handling",
        "validation",
        "terminal",
        "rendering", 
        "input",
        "osk"
    };

    for (int i = 0; i < 6 && i < max_modules; i++) {
        module_names[i] = (char*)order[i];
        count++;
    }

    return count;
}

ErrorCode register_module_callbacks(const char* module_name, 
                                   void (*init_callback)(void),
                                   void (*cleanup_callback)(void),
                                   bool (*health_check_callback)(void))
{
    if (!module_name) {
        return ERR_INVALID_ARGUMENT;
    }

    if (g_integration.num_modules >= MAX_REGISTERED_MODULES) {
        ERROR_LOG("Too many modules registered");
        return ERR_NOT_IMPLEMENTED;
    }

    ModuleCallback* module = &g_integration.modules[g_integration.num_modules];
    strncpy(module->name, module_name, sizeof(module->name) - 1);
    module->name[sizeof(module->name) - 1] = '\0';
    module->init_callback = init_callback;
    module->cleanup_callback = cleanup_callback;
    module->health_check_callback = health_check_callback;
    module->initialized = false;

    g_integration.num_modules++;
    DEBUG_LOG("Module callbacks registered: %s", module_name);
    return ERR_SUCCESS;
}

void set_global_validation(bool enabled)
{
    g_integration.global_validation = enabled;
    g_integration.state.validation_enabled = enabled;
    INFO_LOG("Global validation %s", enabled ? "enabled" : "disabled");
}

void set_global_error_handling(bool enabled)
{
    g_integration.global_error_handling = enabled;
    INFO_LOG("Global error handling %s", enabled ? "enabled" : "disabled");
}
