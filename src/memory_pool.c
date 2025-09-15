#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Creates a memory pool for efficient allocation of fixed-size blocks
 */
MemoryPool* memory_pool_create(size_t block_size, size_t num_blocks) {
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) return NULL;
    
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->free_blocks = num_blocks;
    
    // Allocate the memory pool
    pool->memory = malloc(block_size * num_blocks);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }
    
    // Initialize free list
    pool->free_list = malloc(sizeof(void*) * num_blocks);
    if (!pool->free_list) {
        free(pool->memory);
        free(pool);
        return NULL;
    }
    
    // Set up free list pointers
    for (size_t i = 0; i < num_blocks; i++) {
        pool->free_list[i] = (char*)pool->memory + (i * block_size);
    }
    
    return pool;
}

/**
 * @brief Allocates a block from the memory pool
 */
void* memory_pool_alloc(MemoryPool* pool) {
    if (!pool || pool->free_blocks == 0) {
        return NULL;
    }
    
    void* block = pool->free_list[--pool->free_blocks];
    return block;
}

/**
 * @brief Returns a block to the memory pool
 */
void memory_pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr || pool->free_blocks >= pool->num_blocks) {
        return;
    }
    
    // Verify the pointer is within our pool bounds
    char* char_ptr = (char*)ptr;
    char* pool_start = (char*)pool->memory;
    char* pool_end = pool_start + (pool->block_size * pool->num_blocks);
    
    if (char_ptr < pool_start || char_ptr >= pool_end) {
        return; // Invalid pointer
    }
    
    pool->free_list[pool->free_blocks++] = ptr;
}

/**
 * @brief Destroys a memory pool and frees all resources
 */
void memory_pool_destroy(MemoryPool* pool) {
    if (!pool) return;
    
    free(pool->free_list);
    free(pool->memory);
    free(pool);
}
