/*
 * fs.h — In-memory hierarchical virtual file system
 * KernelViz OS Simulation Engine
 *
 * CEP R1 (Depth of Knowledge): A real OS VFS abstracts storage behind a
 * common interface. Here we simulate it in RAM using a tree of inodes.
 * Supports create/delete/read/write/list operations up to 5 levels deep.
 */

#ifndef FS_H
#define FS_H

#include <time.h>

/* Filesystem limits */
#define MAX_INODES     100   /* Maximum total files + directories */
#define MAX_FILENAME   64    /* Max filename length */
#define MAX_PATH       256   /* Max full path length */
#define MAX_FILE_DATA  512   /* Max file content size (bytes) */
#define MAX_DEPTH      5     /* Max directory nesting depth */
#define MAX_CHILDREN   32    /* Max children per directory */

/* Inode types */
typedef enum {
    INODE_FILE,      /* Regular file */
    INODE_DIR        /* Directory */
} InodeType;

/* File permissions as a bitmask (owner: rwx) */
typedef struct {
    int read;    /* Read permission */
    int write;   /* Write permission */
    int execute; /* Execute permission */
} Permissions;

/* Inode — represents a file or directory */
typedef struct {
    int id;                      /* Unique inode number */
    char name[MAX_FILENAME];     /* File/directory name */
    InodeType type;              /* File or directory */
    int parent_id;               /* Parent inode id (-1 for root) */
    int owner_pid;               /* PID of owning process */
    int size;                    /* File size in bytes (0 for dirs) */
    char data[MAX_FILE_DATA];    /* File contents */
    Permissions perms;           /* Access permissions */
    time_t created_at;           /* Creation timestamp */
    int is_active;               /* 1 if in use, 0 if deleted */
    int children[MAX_CHILDREN];  /* Child inode IDs (dirs only) */
    int child_count;             /* Number of children */
    int depth;                   /* Directory depth (root=0) */
} Inode;

/* Virtual file system state */
typedef struct {
    Inode inodes[MAX_INODES];  /* All inodes (files + dirs) */
    int inode_count;           /* Number of active inodes */
    int next_inode_id;         /* Next inode ID to assign */
    int root_id;               /* Root directory inode ID */
} VFS;

/* Initialize VFS with a root directory */
void fs_init(VFS *vfs);

/* Create a file under parent directory. Returns inode id or -1 */
int fs_create_file(VFS *vfs, const char *name, int parent_id, int owner_pid, Permissions perms);

/* Create a directory under parent. Returns inode id or -1 */
int fs_make_dir(VFS *vfs, const char *name, int parent_id, int owner_pid);

/* Delete a file or empty directory. Returns 0 on success */
int fs_delete(VFS *vfs, int inode_id);

/* Write data to a file. Returns bytes written or -1 */
int fs_write(VFS *vfs, int inode_id, const char *data, int len);

/* Read data from a file. Returns bytes read or -1 */
int fs_read(VFS *vfs, int inode_id, char *buf, int buf_size);

/* Find inode by name in a directory. Returns inode id or -1 */
int fs_find(VFS *vfs, const char *name, int parent_id);

/* Serialize directory listing to JSON */
void fs_list_to_json(VFS *vfs, int dir_id, char *buf, int buf_size);

/* Serialize entire filesystem tree to JSON */
void fs_tree_to_json(VFS *vfs, char *buf, int buf_size);

#endif /* FS_H */
