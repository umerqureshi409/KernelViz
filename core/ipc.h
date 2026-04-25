/*
 * ipc.h — Inter-Process Communication: Pipes + Message Queues
 * KernelViz OS Simulation Engine
 *
 * CEP R4 (Infrequently Encountered): IPC deadlock detection via
 * wait-for graph cycle detection — an advanced OS concept rarely
 * implemented in student projects but critical for production OSes.
 *
 * CEP R7 (Interdependence): IPC events directly affect process states.
 * A process blocked on a pipe read enters WAITING state, which feeds
 * back into the scheduler's ready queue decisions.
 */

#ifndef IPC_H
#define IPC_H

#include <time.h>

/* IPC limits */
#define MAX_PIPES      16    /* Maximum concurrent pipes */
#define MAX_MQ         8     /* Maximum message queues */
#define PIPE_BUF_SIZE  256   /* Pipe buffer capacity */
#define MAX_MQ_MSGS    16    /* Max messages per queue */
#define MAX_MSG_SIZE   128   /* Max message payload size */
#define MAX_IPC_EVENTS 64    /* Max recorded IPC events */

/* IPC event types for logging */
typedef enum {
    IPC_PIPE_WRITE,
    IPC_PIPE_READ,
    IPC_MQ_SEND,
    IPC_MQ_RECEIVE
} IPCEventType;

/* A logged IPC event for dashboard display */
typedef struct {
    IPCEventType type;   /* What happened */
    int sender_pid;      /* Source process */
    int receiver_pid;    /* Destination process */
    char data[64];       /* Short data preview */
    long timestamp;      /* Unix timestamp */
} IPCEvent;

/* Pipe — unidirectional byte buffer between two processes */
typedef struct {
    int id;                      /* Pipe identifier */
    int writer_pid;              /* Process that writes to this pipe */
    int reader_pid;              /* Process that reads from this pipe */
    char buffer[PIPE_BUF_SIZE];  /* Data buffer */
    int buf_len;                 /* Bytes currently in buffer */
    int is_active;               /* 1 if pipe exists */
} Pipe;

/* Message in a queue */
typedef struct {
    int sender_pid;           /* Who sent it */
    char payload[MAX_MSG_SIZE]; /* Message content */
    int len;                  /* Payload length */
} Message;

/* Message queue */
typedef struct {
    int id;                     /* Queue identifier */
    int owner_pid;              /* Process that created this queue */
    Message msgs[MAX_MQ_MSGS];  /* Message buffer (circular) */
    int head;                   /* Read index */
    int tail;                   /* Write index */
    int count;                  /* Number of messages */
    int is_active;              /* 1 if queue exists */
} MessageQueue;

/* IPC subsystem state */
typedef struct {
    Pipe pipes[MAX_PIPES];          /* All pipes */
    MessageQueue queues[MAX_MQ];    /* All message queues */
    IPCEvent events[MAX_IPC_EVENTS];/* Recent IPC events log */
    int event_count;                /* Total events logged */
    int next_pipe_id;               /* Next pipe ID */
    int next_mq_id;                 /* Next queue ID */

    /* Wait-for graph for deadlock detection (CEP R4) */
    /* wait_graph[i][j]=1 means process i is waiting for process j */
    int wait_graph[50][50];
} IPCManager;

/* Initialize IPC subsystem */
void ipc_init(IPCManager *ipc);

/* Create a pipe between two processes. Returns pipe id or -1 */
int pipe_create(IPCManager *ipc, int writer_pid, int reader_pid);

/* Write data to a pipe. Returns bytes written or -1 */
int pipe_write(IPCManager *ipc, int pipe_id, const char *data, int len);

/* Read data from a pipe. Returns bytes read or -1 */
int pipe_read(IPCManager *ipc, int pipe_id, char *buf, int buf_size);

/* Close and destroy a pipe */
int pipe_close(IPCManager *ipc, int pipe_id);

/* Create a message queue. Returns queue id or -1 */
int mq_create(IPCManager *ipc, int owner_pid);

/* Send a message to a queue. Returns 0 on success */
int mq_send(IPCManager *ipc, int mq_id, int sender_pid, const char *payload, int len);

/* Receive a message from a queue. Returns bytes read or -1 */
int mq_receive(IPCManager *ipc, int mq_id, char *buf, int buf_size, int *sender_pid);

/* Check for deadlocks in wait-for graph. Returns 1 if deadlock detected */
int ipc_detect_deadlock(IPCManager *ipc, int num_processes);

/* Serialize recent IPC events to JSON */
void ipc_events_to_json(IPCManager *ipc, char *buf, int buf_size);

/* Serialize IPC stats to JSON */
void ipc_stats_to_json(IPCManager *ipc, char *buf, int buf_size);

#endif /* IPC_H */
