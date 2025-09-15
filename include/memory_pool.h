#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>

typedef struct {
    void* memory;           // The actual memory pool
    void** free_list;       // Array of pointers to free blocks
    size_t block_size;      // Size of each block
    size_t num_blocks;      // Total number of blocks
    size_t free_blocks;     // Number of free blocks remaining
} MemoryPool;

/**
 * @brief Creates a memory pool for efficient allocation of fixed-size blocks
 * @param block_size Size of each block in bytes
 * @param num_blocks Number of blocks in the pool
 * @return Pointer to the memory pool or NULL on failure
 */
MemoryPool* memory_pool_create(size_t block_size, size_t num_blocks);

/**
 * @brief Allocates a block from the memory pool
 * @param pool The memory pool
 * @return Pointer to allocated block or NULL if pool is full
 */
void* memory_pool_alloc(MemoryPool* pool);

/**
 * @brief Returns a block to the memory pool
 * @param pool The memory pool
 * @param ptr Pointer to the block to free
 */
void memory_pool_free(MemoryPool* pool, void* ptr);

/**
 * @brief Destroys a memory pool and frees all resources
 * @param pool The memory pool to destroy
 */
void memory_pool_destroy(MemoryPool* pool);

#endif // MEMORY_POOL_H
