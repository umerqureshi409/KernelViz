/*
 * memory.c — First-Fit Memory Allocation Implementation
 * KernelViz OS Simulation Engine
 *
 * CEP R2 (Conflicting Requirements):
 *   Speed of First-Fit vs fragmentation tradeoff.
 *   First-Fit scans from the beginning and picks the first block that fits.
 *   Fast (O(n) blocks) but causes external fragmentation as small gaps form
 *   between allocated blocks. We mitigate this with coalescing adjacent frees.
 */

#include "memory.h"
#include <string.h>
#include <stdio.h>

/* Initialize memory with a single free block spanning all RAM */
void memory_init(MemoryManager *mm) {
    memset(mm, 0, sizeof(MemoryManager));
    mm->blocks[0].start = 0;
    mm->blocks[0].size = TOTAL_MEMORY;
    mm->blocks[0].pid = -1;
    mm->blocks[0].status = BLOCK_FREE;
    mm->block_count = 1;
    mm->total_used = 0;
    mm->total_free = TOTAL_MEMORY;
    mm->fragmentation = 0.0f;
}

/* Allocate memory using First-Fit algorithm */
int allocate_memory(MemoryManager *mm, int pid, int size) {
    if (size <= 0 || size > TOTAL_MEMORY) return -1;

    /* Scan blocks from start, find first free block >= requested size */
    for (int i = 0; i < mm->block_count; i++) {
        if (mm->blocks[i].status == BLOCK_FREE && mm->blocks[i].size >= size) {
            int remaining = mm->blocks[i].size - size;

            /* Assign this block to the process */
            mm->blocks[i].status = BLOCK_ALLOCATED;
            mm->blocks[i].pid = pid;
            int start_addr = mm->blocks[i].start;

            /* Split block if there's leftover space */
            if (remaining > 0 && mm->block_count < MAX_BLOCKS) {
                /* Shift subsequent blocks to make room */
                for (int j = mm->block_count; j > i + 1; j--) {
                    mm->blocks[j] = mm->blocks[j - 1];
                }
                /* Create new free block from remainder */
                mm->blocks[i].size = size;
                mm->blocks[i + 1].start = start_addr + size;
                mm->blocks[i + 1].size = remaining;
                mm->blocks[i + 1].pid = -1;
                mm->blocks[i + 1].status = BLOCK_FREE;
                mm->block_count++;
            }

            mm->total_used += size;
            mm->total_free -= size;
            update_fragmentation(mm);
            return start_addr;
        }
    }
    return -1; /* No suitable block found */
}

/* Free all memory blocks belonging to a process */
int free_memory(MemoryManager *mm, int pid) {
    int found = 0;
    for (int i = 0; i < mm->block_count; i++) {
        if (mm->blocks[i].status == BLOCK_ALLOCATED && mm->blocks[i].pid == pid) {
            mm->blocks[i].status = BLOCK_FREE;
            mm->blocks[i].pid = -1;
            mm->total_used -= mm->blocks[i].size;
            mm->total_free += mm->blocks[i].size;
            found = 1;
        }
    }
    if (found) {
        coalesce_free_blocks(mm);
        update_fragmentation(mm);
    }
    return found ? 0 : -1;
}

/* Merge adjacent free blocks to reduce external fragmentation */
void coalesce_free_blocks(MemoryManager *mm) {
    int merged;
    do {
        merged = 0;
        for (int i = 0; i < mm->block_count - 1; i++) {
            if (mm->blocks[i].status == BLOCK_FREE && mm->blocks[i + 1].status == BLOCK_FREE) {
                /* Merge block i and i+1 */
                mm->blocks[i].size += mm->blocks[i + 1].size;
                /* Shift remaining blocks left */
                for (int j = i + 1; j < mm->block_count - 1; j++) {
                    mm->blocks[j] = mm->blocks[j + 1];
                }
                mm->block_count--;
                merged = 1;
            }
        }
    } while (merged);
}

/* Calculate external fragmentation percentage */
void update_fragmentation(MemoryManager *mm) {
    if (mm->total_free == 0) {
        mm->fragmentation = 0.0f;
        return;
    }
    /* Find largest free block */
    int largest_free = 0;
    int free_block_count = 0;
    for (int i = 0; i < mm->block_count; i++) {
        if (mm->blocks[i].status == BLOCK_FREE) {
            free_block_count++;
            if (mm->blocks[i].size > largest_free)
                largest_free = mm->blocks[i].size;
        }
    }
    /* Fragmentation = 1 - (largest_free / total_free) */
    if (free_block_count <= 1) {
        mm->fragmentation = 0.0f;
    } else {
        mm->fragmentation = (1.0f - (float)largest_free / mm->total_free) * 100.0f;
    }
}

/* Serialize memory map to JSON array */
void memory_map_to_json(MemoryManager *mm, char *buf, int buf_size) {
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "[");
    for (int i = 0; i < mm->block_count; i++) {
        if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"start\":%d,\"size\":%d,\"pid\":%d,\"status\":\"%s\"}",
            mm->blocks[i].start, mm->blocks[i].size, mm->blocks[i].pid,
            mm->blocks[i].status == BLOCK_FREE ? "free" : "allocated");
        if (offset >= buf_size - 2) break;
    }
    snprintf(buf + offset, buf_size - offset, "]");
}

/* Serialize memory stats to JSON */
void memory_stats_to_json(MemoryManager *mm, char *buf, int buf_size) {
    snprintf(buf, buf_size,
        "{\"total\":%d,\"used\":%d,\"free\":%d,\"fragmentation\":%.2f,\"block_count\":%d}",
        TOTAL_MEMORY, mm->total_used, mm->total_free, mm->fragmentation, mm->block_count);
}
