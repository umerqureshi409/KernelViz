/*
 * fs.c — Virtual File System implementation (in-memory inode tree)
 * KernelViz OS Simulation Engine
 *
 * CEP R1: Demonstrates real OS filesystem concepts: inodes, directory
 * trees, permissions, hierarchical namespace — all simulated in RAM.
 * The tree is navigated by parent inode IDs (like ext4's inode table).
 */

#include "fs.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Find a free inode slot. Returns index or -1 */
static int find_free_inode(VFS *vfs) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (!vfs->inodes[i].is_active) return i;
    }
    return -1;
}

/* Initialize VFS with root directory inode */
void fs_init(VFS *vfs) {
    memset(vfs, 0, sizeof(VFS));
    vfs->next_inode_id = 1;
    /* Create root directory at index 0 */
    Inode *root = &vfs->inodes[0];
    root->id = vfs->next_inode_id++;
    strncpy(root->name, "/", MAX_FILENAME - 1);
    root->type = INODE_DIR;
    root->parent_id = -1;
    root->owner_pid = 0;
    root->size = 0;
    root->perms.read = 1;
    root->perms.write = 1;
    root->perms.execute = 1;
    root->created_at = time(NULL);
    root->is_active = 1;
    root->child_count = 0;
    root->depth = 0;
    vfs->root_id = root->id;
    vfs->inode_count = 1;
}

/* Create a new file in the given parent directory */
int fs_create_file(VFS *vfs, const char *name, int parent_id, int owner_pid, Permissions perms) {
    if (vfs->inode_count >= MAX_INODES) return -1;

    /* Find parent inode */
    Inode *parent = NULL;
    int parent_idx = -1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].id == parent_id && vfs->inodes[i].type == INODE_DIR) {
            parent = &vfs->inodes[i];
            parent_idx = i;
            break;
        }
    }
    if (!parent || parent->child_count >= MAX_CHILDREN) return -1;
    if (parent->depth + 1 > MAX_DEPTH) return -1;

    int slot = find_free_inode(vfs);
    if (slot == -1) return -1;

    Inode *node = &vfs->inodes[slot];
    node->id = vfs->next_inode_id++;
    strncpy(node->name, name, MAX_FILENAME - 1);
    node->name[MAX_FILENAME - 1] = '\0';
    node->type = INODE_FILE;
    node->parent_id = parent_id;
    node->owner_pid = owner_pid;
    node->size = 0;
    node->perms = perms;
    node->created_at = time(NULL);
    node->is_active = 1;
    node->child_count = 0;
    node->depth = parent->depth + 1;

    /* Register in parent's child list */
    parent->children[parent->child_count++] = node->id;
    vfs->inode_count++;
    return node->id;
}

/* Create a new directory under parent */
int fs_make_dir(VFS *vfs, const char *name, int parent_id, int owner_pid) {
    if (vfs->inode_count >= MAX_INODES) return -1;

    Inode *parent = NULL;
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].id == parent_id && vfs->inodes[i].type == INODE_DIR) {
            parent = &vfs->inodes[i];
            break;
        }
    }
    if (!parent || parent->child_count >= MAX_CHILDREN) return -1;
    if (parent->depth >= MAX_DEPTH) return -1;

    int slot = find_free_inode(vfs);
    if (slot == -1) return -1;

    Inode *node = &vfs->inodes[slot];
    node->id = vfs->next_inode_id++;
    strncpy(node->name, name, MAX_FILENAME - 1);
    node->name[MAX_FILENAME - 1] = '\0';
    node->type = INODE_DIR;
    node->parent_id = parent_id;
    node->owner_pid = owner_pid;
    node->size = 0;
    node->perms.read = 1; node->perms.write = 1; node->perms.execute = 1;
    node->created_at = time(NULL);
    node->is_active = 1;
    node->child_count = 0;
    node->depth = parent->depth + 1;

    parent->children[parent->child_count++] = node->id;
    vfs->inode_count++;
    return node->id;
}

/* Delete a file or empty directory by inode id */
int fs_delete(VFS *vfs, int inode_id) {
    Inode *target = NULL;
    int target_idx = -1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].id == inode_id) {
            target = &vfs->inodes[i];
            target_idx = i;
            break;
        }
    }
    if (!target) return -1;
    if (target->type == INODE_DIR && target->child_count > 0) return -1; /* Non-empty dir */

    /* Remove from parent's child list */
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].id == target->parent_id) {
            Inode *p = &vfs->inodes[i];
            for (int j = 0; j < p->child_count; j++) {
                if (p->children[j] == inode_id) {
                    /* Shift children left */
                    for (int k = j; k < p->child_count - 1; k++)
                        p->children[k] = p->children[k + 1];
                    p->child_count--;
                    break;
                }
            }
            break;
        }
    }
    target->is_active = 0;
    vfs->inode_count--;
    return 0;
}

/* Write data into a file's data buffer */
int fs_write(VFS *vfs, int inode_id, const char *data, int len) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].id == inode_id && vfs->inodes[i].type == INODE_FILE) {
            if (!vfs->inodes[i].perms.write) return -1;
            int write_len = (len < MAX_FILE_DATA - 1) ? len : MAX_FILE_DATA - 1;
            memcpy(vfs->inodes[i].data, data, write_len);
            vfs->inodes[i].data[write_len] = '\0';
            vfs->inodes[i].size = write_len;
            return write_len;
        }
    }
    return -1;
}

/* Read data from a file into a buffer */
int fs_read(VFS *vfs, int inode_id, char *buf, int buf_size) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].id == inode_id && vfs->inodes[i].type == INODE_FILE) {
            if (!vfs->inodes[i].perms.read) return -1;
            int read_len = (vfs->inodes[i].size < buf_size - 1) ? vfs->inodes[i].size : buf_size - 1;
            memcpy(buf, vfs->inodes[i].data, read_len);
            buf[read_len] = '\0';
            return read_len;
        }
    }
    return -1;
}

/* Find an inode by name in a directory. Returns inode id or -1 */
int fs_find(VFS *vfs, const char *name, int parent_id) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (vfs->inodes[i].is_active && vfs->inodes[i].parent_id == parent_id &&
            strncmp(vfs->inodes[i].name, name, MAX_FILENAME) == 0) {
            return vfs->inodes[i].id;
        }
    }
    return -1;
}

/* Serialize a directory's contents to JSON array */
void fs_list_to_json(VFS *vfs, int dir_id, char *buf, int buf_size) {
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "[");
    int first = 1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (!vfs->inodes[i].is_active || vfs->inodes[i].parent_id != dir_id) continue;
        Inode *n = &vfs->inodes[i];
        if (!first) offset += snprintf(buf + offset, buf_size - offset, ",");
        first = 0;
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"id\":%d,\"name\":\"%s\",\"type\":\"%s\",\"size\":%d,\"perms\":\"%c%c%c\",\"owner\":%d}",
            n->id, n->name, n->type == INODE_FILE ? "file" : "dir", n->size,
            n->perms.read ? 'r' : '-', n->perms.write ? 'w' : '-', n->perms.execute ? 'x' : '-',
            n->owner_pid);
        if (offset >= buf_size - 4) break;
    }
    snprintf(buf + offset, buf_size - offset, "]");
}

/* Serialize entire VFS inode table to JSON for dashboard visualization */
void fs_tree_to_json(VFS *vfs, char *buf, int buf_size) {
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "{\"root_id\":%d,\"inodes\":[", vfs->root_id);
    int first = 1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (!vfs->inodes[i].is_active) continue;
        Inode *n = &vfs->inodes[i];
        if (!first) offset += snprintf(buf + offset, buf_size - offset, ",");
        first = 0;
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"id\":%d,\"name\":\"%s\",\"type\":\"%s\",\"parent\":%d,\"size\":%d,\"depth\":%d,\"owner\":%d}",
            n->id, n->name, n->type == INODE_FILE ? "file" : "dir",
            n->parent_id, n->size, n->depth, n->owner_pid);
        if (offset >= buf_size - 8) break;
    }
    snprintf(buf + offset, buf_size - offset, "]}");
}
