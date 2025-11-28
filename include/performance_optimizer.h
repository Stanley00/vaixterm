/**
 * @file performance_optimizer.h
 * @brief Performance optimization and memory management framework.
 *
 * This module provides comprehensive performance monitoring, memory optimization,
 * and performance tuning capabilities for VaixTerm.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef PERFORMANCE_OPTIMIZER_H
#define PERFORMANCE_OPTIMIZER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"
#include "error_codes.h"

// Performance metrics
typedef struct {
    uint64_t frame_time_ms;
    uint64_t render_time_ms;
    uint64_t input_time_ms;
    uint64_t osk_time_ms;
    uint64_t total_memory_usage;
    uint64_t glyph_cache_size;
    uint64_t texture_memory_usage;
    int cache_hit_rate;
    int cache_miss_rate;
    int frames_per_second;
    int render_calls;
    int texture_uploads;
    int texture_downloads;
} PerformanceMetrics;

// Memory pool configuration
typedef struct {
    size_t block_size;
    int block_count;
    int growth_factor;
    bool auto_expand;
    size_t max_size;
} MemoryPoolConfig;

// Performance optimization settings
typedef struct {
    bool enable_glyph_caching;
    bool enable_dirty_regions;
    bool enable_texture_atlas;
    bool enable_memory_pools;
    bool enable_frame_rate_limiting;
    int target_fps;
    int max_cache_size;
    int memory_threshold_mb;
    bool enable_profiling;
} PerformanceSettings;

/**
 * @brief Initialize the performance optimizer
 *
 * @param settings Performance optimization settings
 * @return ErrorCode Result of initialization
 */
ErrorCode performance_optimizer_init(const PerformanceSettings* settings);

/**
 * @brief Cleanup the performance optimizer
 */
void performance_optimizer_cleanup(void);

/**
 * @brief Get current performance metrics
 *
 * @return PerformanceMetrics Current performance metrics
 */
PerformanceMetrics get_performance_metrics(void);

/**
 * @brief Reset performance metrics
 */
void reset_performance_metrics(void);

/**
 * @brief Optimize glyph cache performance
 *
 * @param cache Pointer to glyph cache
 * @param max_size Maximum cache size
 * @return ErrorCode Result of optimization
 */
ErrorCode optimize_glyph_cache(GlyphCache* cache, int max_size);

/**
 * @brief Optimize memory usage
 *
 * @return ErrorCode Result of optimization
 */
ErrorCode optimize_memory_usage(void);

/**
 * @brief Optimize rendering performance
 *
 * @param renderer SDL renderer
 * @param term Pointer to terminal structure
 * @return ErrorCode Result of optimization
 */
ErrorCode optimize_rendering_performance(SDL_Renderer* renderer, Terminal* term);

/**
 * @brief Optimize input processing performance
 *
 * @return ErrorCode Result of optimization
 */
ErrorCode optimize_input_performance(void);

/**
 * @brief Create a memory pool
 *
 * @param config Memory pool configuration
 * @return void* Pointer to memory pool or NULL
 */
void* create_memory_pool(const MemoryPoolConfig* config);

/**
 * @brief Allocate from memory pool
 *
 * @param pool Memory pool pointer
 * @param size Allocation size
 * @return void* Allocated memory or NULL
 */
void* pool_alloc(void* pool, size_t size);

/**
 * @brief Free to memory pool
 *
 * @param pool Memory pool pointer
 * @param ptr Memory to free
 */
void pool_free(void* pool, void* ptr);

/**
 * @brief Destroy memory pool
 *
 * @param pool Memory pool pointer
 */
void destroy_memory_pool(void* pool);

/**
 * @brief Profile function execution time
 *
 * @param function_name Name of the function
 * @param start_time Start timestamp
 * @param end_time End timestamp
 */
void profile_function_time(const char* function_name, uint64_t start_time, uint64_t end_time);

/**
 * @brief Get memory usage statistics
 *
 * @param total_memory Output for total memory usage
 * @param available_memory Output for available memory
 * @param cache_memory Output for cache memory usage
 * @return ErrorCode Result of operation
 */
ErrorCode get_memory_statistics(uint64_t* total_memory, uint64_t* available_memory, uint64_t* cache_memory);

/**
 * @brief Optimize texture memory usage
 *
 * @param renderer SDL renderer
 * @return ErrorCode Result of optimization
 */
ErrorCode optimize_texture_memory(SDL_Renderer* renderer);

/**
 * @brief Enable/disable performance profiling
 *
 * @param enabled Whether profiling should be enabled
 */
void set_profiling_enabled(bool enabled);

/**
 * @brief Get performance recommendations
 *
 * @param recommendations Output buffer for recommendations
 * @param buffer_size Size of recommendations buffer
 * @return ErrorCode Result of operation
 */
ErrorCode get_performance_recommendations(char* recommendations, size_t buffer_size);

/**
 * @brief Apply automatic performance optimizations
 *
 * @return ErrorCode Result of optimization
 */
ErrorCode apply_automatic_optimizations(void);

/**
 * @brief Monitor performance thresholds
 *
 * @return bool True if performance is within acceptable thresholds
 */
bool monitor_performance_thresholds(void);

/**
 * @brief Optimize for target frame rate
 *
 * @param target_fps Target frames per second
 * @return ErrorCode Result of optimization
 */
ErrorCode optimize_for_target_fps(int target_fps);

/**
 * @brief Compress glyph cache
 *
 * @param cache Pointer to glyph cache
 * @return ErrorCode Result of compression
 */
ErrorCode compress_glyph_cache(GlyphCache* cache);

/**
 * @brief Preload commonly used glyphs
 *
 * @param cache Pointer to glyph cache
 * @param font SDL font
 * @param characters Array of characters to preload
 * @param char_count Number of characters
 * @return ErrorCode Result of preloading
 */
ErrorCode preload_common_glyphs(GlyphCache* cache, TTF_Font* font, const char* characters, int char_count);

// Performance macros for profiling
#define PERF_START_TIMER() uint64_t _perf_start = SDL_GetPerformanceCounter()
#define PERF_END_TIMER(function_name) profile_function_time(function_name, _perf_start, SDL_GetPerformanceCounter())
#define PERF_PROFILE_FUNCTION() PERF_START_TIMER(); \
                              atexit([](){ PERF_END_TIMER(__func__); })

#endif // PERFORMANCE_OPTIMIZER_H
