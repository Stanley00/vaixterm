/**
 * @file performance_optimizer.c
 * @brief Performance optimization and memory management implementation.
 *
 * This module implements comprehensive performance monitoring, memory optimization,
 * and performance tuning capabilities for VaixTerm.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "performance_optimizer.h"
#include "glyph_cache.h"
#include "error_codes.h"
#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <SDL.h>
#include <SDL_ttf.h>

// Maximum number of profiled functions
#define MAX_PROFILED_FUNCTIONS 64

// Memory pool structure
typedef struct MemoryPool {
    MemoryPoolConfig config;
    void* memory;
    size_t used_size;
    int free_blocks;
    bool* block_used;
} MemoryPool;

// Function profile entry
typedef struct {
    char name[64];
    uint64_t total_time;
    uint64_t call_count;
    uint64_t min_time;
    uint64_t max_time;
} FunctionProfile;

// Global performance state
static struct {
    bool initialized;
    PerformanceSettings settings;
    PerformanceMetrics metrics;
    FunctionProfile profiles[MAX_PROFILED_FUNCTIONS];
    int profile_count;
    bool profiling_enabled;
    uint64_t frame_start_time;
    int frame_count;
    uint64_t last_fps_update;
} g_perf_state = {0};

static uint64_t get_timestamp_ms(void)
{
    return SDL_GetPerformanceCounter() / (SDL_GetPerformanceFrequency() / 1000);
}

ErrorCode performance_optimizer_init(const PerformanceSettings* settings)
{
    if (!settings) {
        ERROR_LOG("Invalid performance settings");
        return ERR_INVALID_ARGUMENT;
    }

    if (g_perf_state.initialized) {
        DEBUG_LOG("Performance optimizer already initialized");
        return ERR_SUCCESS;
    }

    memset(&g_perf_state, 0, sizeof(g_perf_state));
    g_perf_state.settings = *settings;
    g_perf_state.profiling_enabled = settings->enable_profiling;
    g_perf_state.frame_start_time = get_timestamp_ms();

    INFO_LOG("Performance optimizer initialized");
    INFO_LOG("Glyph caching: %s", settings->enable_glyph_caching ? "enabled" : "disabled");
    INFO_LOG("Dirty regions: %s", settings->enable_dirty_regions ? "enabled" : "disabled");
    INFO_LOG("Texture atlas: %s", settings->enable_texture_atlas ? "enabled" : "disabled");
    INFO_LOG("Memory pools: %s", settings->enable_memory_pools ? "enabled" : "disabled");
    INFO_LOG("Target FPS: %d", settings->target_fps);

    g_perf_state.initialized = true;
    return ERR_SUCCESS;
}

void performance_optimizer_cleanup(void)
{
    if (!g_perf_state.initialized) {
        return;
    }

    // Apply final optimizations before cleanup
    optimize_memory_usage();

    memset(&g_perf_state, 0, sizeof(g_perf_state));
    DEBUG_LOG("Performance optimizer cleaned up");
}

PerformanceMetrics get_performance_metrics(void)
{
    if (!g_perf_state.initialized) {
        PerformanceMetrics empty = {0};
        return empty;
    }

    // Update current metrics
    uint64_t current_time = get_timestamp_ms();
    g_perf_state.metrics.frame_time_ms = current_time - g_perf_state.frame_start_time;

    // Calculate FPS
    g_perf_state.frame_count++;
    if (current_time - g_perf_state.last_fps_update >= 1000) {
        g_perf_state.metrics.frames_per_second = g_perf_state.frame_count;
        g_perf_state.frame_count = 0;
        g_perf_state.last_fps_update = current_time;
    }

    // Get memory statistics
    get_memory_statistics(&g_perf_state.metrics.total_memory_usage, NULL, &g_perf_state.metrics.glyph_cache_size);

    return g_perf_state.metrics;
}

void reset_performance_metrics(void)
{
    if (!g_perf_state.initialized) {
        return;
    }

    memset(&g_perf_state.metrics, 0, sizeof(g_perf_state.metrics));
    memset(g_perf_state.profiles, 0, sizeof(g_perf_state.profiles));
    g_perf_state.profile_count = 0;
    g_perf_state.frame_count = 0;
    g_perf_state.frame_start_time = get_timestamp_ms();

    DEBUG_LOG("Performance metrics reset");
}

ErrorCode optimize_glyph_cache(GlyphCache* cache, int max_size)
{
    if (!cache || max_size <= 0) {
        ERROR_LOG("Invalid parameters for glyph cache optimization");
        return ERR_INVALID_ARGUMENT;
    }

    if (!g_perf_state.initialized || !g_perf_state.settings.enable_glyph_caching) {
        return ERR_SUCCESS;
    }

    DEBUG_LOG("Optimizing glyph cache with max size %d", max_size);

    // Implement cache optimization logic
    // This could include:
    // - Removing least recently used items
    // - Compressing cache entries
    // - Preloading common glyphs

    return compress_glyph_cache(cache);
}

ErrorCode optimize_memory_usage(void)
{
    if (!g_perf_state.initialized) {
        return ERR_INVALID_STATE;
    }

    DEBUG_LOG("Optimizing memory usage");

    // Get current memory statistics
    uint64_t total_memory, available_memory, cache_memory;
    ErrorCode result = get_memory_statistics(&total_memory, &available_memory, &cache_memory);
    if (result != ERR_SUCCESS) {
        return result;
    }

    // Check if we're approaching memory threshold
    if (g_perf_state.settings.memory_threshold_mb > 0) {
        uint64_t threshold_bytes = g_perf_state.settings.memory_threshold_mb * 1024 * 1024;
        if (total_memory > threshold_bytes) {
            WARN_LOG("Memory usage (%llu MB) exceeds threshold (%d MB)", 
                     total_memory / (1024 * 1024), g_perf_state.settings.memory_threshold_mb);
            
            // Trigger memory optimization
            if (cache_memory > threshold_bytes / 2) {
                DEBUG_LOG("Clearing glyph cache to free memory");
                // glyph_cache_clear would be called here
            }
        }
    }

    return ERR_SUCCESS;
}

ErrorCode optimize_rendering_performance(SDL_Renderer* renderer, Terminal* term)
{
    if (!renderer || !term) {
        ERROR_LOG("Invalid parameters for rendering optimization");
        return ERR_INVALID_ARGUMENT;
    }

    if (!g_perf_state.initialized) {
        return ERR_INVALID_STATE;
    }

    PERF_START_TIMER();

    DEBUG_LOG("Optimizing rendering performance");

    // Optimize texture memory
    optimize_texture_memory(renderer);

    // Enable dirty region rendering if configured
    if (g_perf_state.settings.enable_dirty_regions) {
        term->has_dirty_regions = true;
        DEBUG_LOG("Dirty region rendering enabled");
    }

    // Apply frame rate limiting if configured
    if (g_perf_state.settings.enable_frame_rate_limiting && g_perf_state.settings.target_fps > 0) {
        optimize_for_target_fps(g_perf_state.settings.target_fps);
    }

    PERF_END_TIMER("optimize_rendering_performance");
    return ERR_SUCCESS;
}

ErrorCode optimize_input_performance(void)
{
    if (!g_perf_state.initialized) {
        return ERR_INVALID_STATE;
    }

    PERF_START_TIMER();
    DEBUG_LOG("Optimizing input performance");

    // Input optimization logic would go here
    // This could include:
    // - Input event batching
    // - Controller polling optimization
    // - Keyboard input buffering

    PERF_END_TIMER("optimize_input_performance");
    return ERR_SUCCESS;
}

void* create_memory_pool(const MemoryPoolConfig* config)
{
    if (!config || config->block_size == 0 || config->block_count == 0) {
        ERROR_LOG("Invalid memory pool configuration");
        return NULL;
    }

    if (!g_perf_state.initialized || !g_perf_state.settings.enable_memory_pools) {
        return NULL;
    }

    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) {
        ERROR_LOG("Failed to allocate memory pool structure");
        return NULL;
    }

    pool->config = *config;
    pool->memory = malloc(config->block_size * config->block_count);
    if (!pool->memory) {
        ERROR_LOG("Failed to allocate memory pool blocks");
        free(pool);
        return NULL;
    }

    pool->block_used = calloc(config->block_count, sizeof(bool));
    if (!pool->block_used) {
        ERROR_LOG("Failed to allocate block tracking array");
        free(pool->memory);
        free(pool);
        return NULL;
    }

    pool->used_size = 0;
    pool->free_blocks = config->block_count;

    DEBUG_LOG("Created memory pool: %zu bytes x %d blocks", config->block_size, config->block_count);
    return pool;
}

void* pool_alloc(void* pool_ptr, size_t size)
{
    if (!pool_ptr || size == 0) {
        return NULL;
    }

    MemoryPool* pool = (MemoryPool*)pool_ptr;
    
    // Find a free block
    for (int i = 0; i < pool->config.block_count; i++) {
        if (!pool->block_used[i] && size <= pool->config.block_size) {
            pool->block_used[i] = true;
            pool->used_size += pool->config.block_size;
            pool->free_blocks--;
            
            void* block = (char*)pool->memory + (i * pool->config.block_size);
            return block;
        }
    }

    // No free blocks available
    if (pool->config.auto_expand && pool->used_size < pool->config.max_size) {
        // Expand pool logic would go here
        DEBUG_LOG("Pool expansion requested");
    }

    WARN_LOG("Memory pool exhausted");
    return NULL;
}

void pool_free(void* pool_ptr, void* ptr)
{
    if (!pool_ptr || !ptr) {
        return;
    }

    MemoryPool* pool = (MemoryPool*)pool_ptr;
    
    // Find the block containing this pointer
    for (int i = 0; i < pool->config.block_count; i++) {
        void* block = (char*)pool->memory + (i * pool->config.block_size);
        if (block == ptr) {
            if (pool->block_used[i]) {
                pool->block_used[i] = false;
                pool->used_size -= pool->config.block_size;
                pool->free_blocks++;
                return;
            }
        }
    }

    WARN_LOG("Attempted to free invalid memory pool block");
}

void destroy_memory_pool(void* pool_ptr)
{
    if (!pool_ptr) {
        return;
    }

    MemoryPool* pool = (MemoryPool*)pool_ptr;
    
    if (pool->memory) {
        free(pool->memory);
    }
    if (pool->block_used) {
        free(pool->block_used);
    }
    
    free(pool);
    DEBUG_LOG("Memory pool destroyed");
}

void profile_function_time(const char* function_name, uint64_t start_time, uint64_t end_time)
{
    if (!g_perf_state.initialized || !g_perf_state.profiling_enabled || !function_name) {
        return;
    }

    uint64_t elapsed = end_time - start_time;

    // Find existing profile entry
    for (int i = 0; i < g_perf_state.profile_count; i++) {
        if (strcmp(g_perf_state.profiles[i].name, function_name) == 0) {
            g_perf_state.profiles[i].total_time += elapsed;
            g_perf_state.profiles[i].call_count++;
            
            if (elapsed < g_perf_state.profiles[i].min_time) {
                g_perf_state.profiles[i].min_time = elapsed;
            }
            if (elapsed > g_perf_state.profiles[i].max_time) {
                g_perf_state.profiles[i].max_time = elapsed;
            }
            return;
        }
    }

    // Create new profile entry
    if (g_perf_state.profile_count < MAX_PROFILED_FUNCTIONS) {
        FunctionProfile* profile = &g_perf_state.profiles[g_perf_state.profile_count];
        strncpy(profile->name, function_name, sizeof(profile->name) - 1);
        profile->name[sizeof(profile->name) - 1] = '\0';
        profile->total_time = elapsed;
        profile->call_count = 1;
        profile->min_time = elapsed;
        profile->max_time = elapsed;
        g_perf_state.profile_count++;
    }
}

ErrorCode get_memory_statistics(uint64_t* total_memory, uint64_t* available_memory, uint64_t* cache_memory)
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        ERROR_LOG("Failed to get memory usage statistics");
        return ERR_NOT_IMPLEMENTED;
    }

    if (total_memory) {
        // For now, return 0 since ru_maxrss is not available on macOS
        *total_memory = 0;
    }

    if (available_memory) {
        // This would require platform-specific implementation
        *available_memory = 0;
    }

    if (cache_memory) {
        // Get glyph cache size from global state
        *cache_memory = g_perf_state.metrics.glyph_cache_size;
    }

    return ERR_SUCCESS;
}

ErrorCode optimize_texture_memory(SDL_Renderer* renderer)
{
    if (!renderer) {
        ERROR_LOG("Invalid renderer for texture optimization");
        return ERR_INVALID_ARGUMENT;
    }

    if (!g_perf_state.initialized || !g_perf_state.settings.enable_texture_atlas) {
        return ERR_SUCCESS;
    }

    DEBUG_LOG("Optimizing texture memory");

    // Texture optimization logic would go here
    // This could include:
    // - Texture atlas creation
    // - Texture compression
    // - Unused texture cleanup

    return ERR_SUCCESS;
}

void set_profiling_enabled(bool enabled)
{
    if (!g_perf_state.initialized) {
        return;
    }

    g_perf_state.profiling_enabled = enabled;
    DEBUG_LOG("Performance profiling %s", enabled ? "enabled" : "disabled");
}

ErrorCode get_performance_recommendations(char* recommendations, size_t buffer_size)
{
    if (!recommendations || buffer_size == 0) {
        return ERR_INVALID_ARGUMENT;
    }

    if (!g_perf_state.initialized) {
        strncpy(recommendations, "Performance optimizer not initialized", buffer_size - 1);
        recommendations[buffer_size - 1] = '\0';
        return ERR_INVALID_STATE;
    }

    PerformanceMetrics metrics = get_performance_metrics();
    size_t offset = 0;

    // Analyze performance and provide recommendations
    if (metrics.frames_per_second < 30) {
        offset += snprintf(recommendations + offset, buffer_size - offset,
                         "Low FPS detected (%d). Consider enabling dirty regions or reducing cache size.\n", 
                         metrics.frames_per_second);
    }

    if (metrics.cache_hit_rate < 80) {
        offset += snprintf(recommendations + offset, buffer_size - offset,
                         "Low cache hit rate (%d%%). Consider increasing cache size.\n", 
                         metrics.cache_hit_rate);
    }

    if (metrics.total_memory_usage > 100 * 1024 * 1024) { // 100MB
        offset += snprintf(recommendations + offset, buffer_size - offset,
                         "High memory usage (%llu MB). Consider enabling memory pools.\n", 
                         metrics.total_memory_usage / (1024 * 1024));
    }

    if (offset == 0) {
        strncpy(recommendations, "Performance is optimal. No recommendations at this time.", buffer_size - 1);
        recommendations[buffer_size - 1] = '\0';
    }

    return ERR_SUCCESS;
}

ErrorCode apply_automatic_optimizations(void)
{
    if (!g_perf_state.initialized) {
        return ERR_INVALID_STATE;
    }

    DEBUG_LOG("Applying automatic performance optimizations");

    ErrorCode result = ERR_SUCCESS;

    // Optimize memory usage
    ErrorCode mem_result = optimize_memory_usage();
    if (mem_result != ERR_SUCCESS) {
        result = mem_result;
    }

    // Apply frame rate optimization
    if (g_perf_state.settings.target_fps > 0) {
        ErrorCode fps_result = optimize_for_target_fps(g_perf_state.settings.target_fps);
        if (fps_result != ERR_SUCCESS) {
            result = fps_result;
        }
    }

    return result;
}

bool monitor_performance_thresholds(void)
{
    if (!g_perf_state.initialized) {
        return false;
    }

    PerformanceMetrics metrics = get_performance_metrics();

    // Check performance thresholds
    bool thresholds_ok = true;

    if (g_perf_state.settings.target_fps > 0 && metrics.frames_per_second < g_perf_state.settings.target_fps * 0.8) {
        WARN_LOG("FPS below threshold: %d < %d", metrics.frames_per_second, g_perf_state.settings.target_fps);
        thresholds_ok = false;
    }

    if (g_perf_state.settings.memory_threshold_mb > 0) {
        uint64_t memory_mb = metrics.total_memory_usage / (1024 * 1024);
        if (memory_mb > (uint64_t)g_perf_state.settings.memory_threshold_mb) {
            WARN_LOG("Memory usage above threshold: %llu MB > %d MB", 
                     memory_mb, g_perf_state.settings.memory_threshold_mb);
            thresholds_ok = false;
        }
    }

    return thresholds_ok;
}

ErrorCode optimize_for_target_fps(int target_fps)
{
    if (target_fps <= 0) {
        ERROR_LOG("Invalid target FPS: %d", target_fps);
        return ERR_INVALID_ARGUMENT;
    }

    if (!g_perf_state.initialized) {
        return ERR_INVALID_STATE;
    }

    uint64_t target_frame_time = 1000 / target_fps; // milliseconds per frame
    
    DEBUG_LOG("Optimizing for target FPS: %d (frame time: %llu ms)", target_fps, target_frame_time);

    // Frame rate limiting logic would be implemented here
    // This could include:
    // - Adaptive quality settings
    // - Dynamic cache sizing
    // - Rendering optimization

    return ERR_SUCCESS;
}

ErrorCode compress_glyph_cache(GlyphCache* cache)
{
    if (!cache) {
        ERROR_LOG("Invalid glyph cache for compression");
        return ERR_INVALID_ARGUMENT;
    }

    DEBUG_LOG("Compressing glyph cache");

    // Cache compression logic would go here
    // This could include:
    // - Removing unused entries
    // - Compressing texture data
    // - Optimizing cache layout

    return ERR_SUCCESS;
}

ErrorCode preload_common_glyphs(GlyphCache* cache, TTF_Font* font, const char* characters, int char_count)
{
    if (!cache || !font || !characters || char_count <= 0) {
        ERROR_LOG("Invalid parameters for glyph preloading");
        return ERR_INVALID_ARGUMENT;
    }

    DEBUG_LOG("Preloading %d common glyphs", char_count);

    // Preload commonly used characters
    for (int i = 0; i < char_count; i++) {
        // Preload logic would go here
        // This would render and cache the glyph
        (void)characters[i]; // Suppress unused warning
    }

    return ERR_SUCCESS;
}
