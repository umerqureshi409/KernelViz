/*
 * memory.h — Memory management with First-Fit allocation
 * KernelViz OS Simulation Engine
 *
 * CEP R2 (Conflicting Requirements):
 *   Speed of First-Fit allocation vs memory fragmentation tradeoff.
 *   First-Fit is O(n) where n = number of memory blocks. It's fast
 *   but leads to external fragmentation over time.
 */

#ifndef MEMORY_H
#define MEMORY_H

/* Total simulated RAM in memory units */
#define TOTAL_MEMORY 1024

/* Maximum number of memory blocks (allocated + free) */
#define MAX_BLOCKS 128

/* Memory block status */
typedef enum {
    BLOCK_FREE,       /* Block is available */
    BLOCK_ALLOCATED   /* Block is assigned to a process */
} BlockStatus;

/* A contiguous block of memory */
typedef struct {
    int start;          /* Starting address */
    int size;           /* Size in memory units */
    int pid;            /* Owner process PID (-1 if free) */
    BlockStatus status; /* Free or allocated */
} MemBlock;

/* Memory management state */
typedef struct {
    MemBlock blocks[MAX_BLOCKS]; /* Array of memory blocks */
    int block_count;             /* Number of blocks in use */
    int total_used;              /* Total allocated memory units */
    int total_free;              /* Total free memory units */
    float fragmentation;         /* External fragmentation percentage */
} MemoryManager;

/* Initialize memory manager with one big free block */
void memory_init(MemoryManager *mm);

/* Allocate memory using First-Fit. Returns start address or -1 */
int allocate_memory(MemoryManager *mm, int pid, int size);

/* Free all memory owned by a process. Returns 0 on success */
int free_memory(MemoryManager *mm, int pid);

/* Merge adjacent free blocks to reduce fragmentation */
void coalesce_free_blocks(MemoryManager *mm);

/* Calculate fragmentation percentage */
void update_fragmentation(MemoryManager *mm);

/* Serialize memory map to JSON */
void memory_map_to_json(MemoryManager *mm, char *buf, int buf_size);

/* Serialize memory stats to JSON */
void memory_stats_to_json(MemoryManager *mm, char *buf, int buf_size);

#endif /* MEMORY_H */
