/*
 * ipc.c — IPC implementation: Pipes and Message Queues
 * KernelViz OS Simulation Engine
 *
 * CEP R4 (Infrequently Encountered): Deadlock detection via DFS cycle
 * detection on the wait-for graph. If process A waits for B, and B
 * waits for A, that is a deadlock cycle caught here.
 *
 * CEP R7 (Interdependence): IPC events feed directly into process
 * state changes — a blocked read puts the process in WAITING state.
 */

#include "ipc.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Initialize IPC subsystem: clear all pipes, queues, events */
void ipc_init(IPCManager *ipc) {
    memset(ipc, 0, sizeof(IPCManager));
    ipc->next_pipe_id = 1;
    ipc->next_mq_id = 1;
    ipc->event_count = 0;
}

/* Log an IPC event for dashboard display */
static void log_event(IPCManager *ipc, IPCEventType type, int sender, int receiver, const char *data) {
    int idx = ipc->event_count % MAX_IPC_EVENTS; /* Circular log */
    IPCEvent *e = &ipc->events[idx];
    e->type = type;
    e->sender_pid = sender;
    e->receiver_pid = receiver;
    strncpy(e->data, data, 63);
    e->data[63] = '\0';
    e->timestamp = (long)time(NULL);
    ipc->event_count++;
}

/* Create a pipe between writer and reader processes */
int pipe_create(IPCManager *ipc, int writer_pid, int reader_pid) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!ipc->pipes[i].is_active) {
            Pipe *p = &ipc->pipes[i];
            p->id = ipc->next_pipe_id++;
            p->writer_pid = writer_pid;
            p->reader_pid = reader_pid;
            p->buf_len = 0;
            p->is_active = 1;
            return p->id;
        }
    }
    return -1; /* No free pipe slots */
}

/* Write data to a pipe buffer */
int pipe_write(IPCManager *ipc, int pipe_id, const char *data, int len) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (ipc->pipes[i].is_active && ipc->pipes[i].id == pipe_id) {
            Pipe *p = &ipc->pipes[i];
            int space = PIPE_BUF_SIZE - p->buf_len;
            int write_len = (len < space) ? len : space;
            if (write_len <= 0) return 0; /* Pipe full */
            memcpy(p->buffer + p->buf_len, data, write_len);
            p->buf_len += write_len;

            char preview[32];
            snprintf(preview, sizeof(preview), "%.30s", data);
            log_event(ipc, IPC_PIPE_WRITE, p->writer_pid, p->reader_pid, preview);
            return write_len;
        }
    }
    return -1; /* Pipe not found */
}

/* Read data from a pipe buffer */
int pipe_read(IPCManager *ipc, int pipe_id, char *buf, int buf_size) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (ipc->pipes[i].is_active && ipc->pipes[i].id == pipe_id) {
            Pipe *p = &ipc->pipes[i];
            if (p->buf_len == 0) return 0; /* Empty pipe */
            int read_len = (p->buf_len < buf_size - 1) ? p->buf_len : buf_size - 1;
            memcpy(buf, p->buffer, read_len);
            buf[read_len] = '\0';
            /* Shift remaining bytes to front */
            memmove(p->buffer, p->buffer + read_len, p->buf_len - read_len);
            p->buf_len -= read_len;

            log_event(ipc, IPC_PIPE_READ, p->writer_pid, p->reader_pid, buf);
            return read_len;
        }
    }
    return -1;
}

/* Close and release a pipe */
int pipe_close(IPCManager *ipc, int pipe_id) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (ipc->pipes[i].is_active && ipc->pipes[i].id == pipe_id) {
            ipc->pipes[i].is_active = 0;
            return 0;
        }
    }
    return -1;
}

/* Create a message queue owned by a process */
int mq_create(IPCManager *ipc, int owner_pid) {
    for (int i = 0; i < MAX_MQ; i++) {
        if (!ipc->queues[i].is_active) {
            MessageQueue *mq = &ipc->queues[i];
            mq->id = ipc->next_mq_id++;
            mq->owner_pid = owner_pid;
            mq->head = 0; mq->tail = 0; mq->count = 0;
            mq->is_active = 1;
            return mq->id;
        }
    }
    return -1;
}

/* Send a message to a message queue */
int mq_send(IPCManager *ipc, int mq_id, int sender_pid, const char *payload, int len) {
    for (int i = 0; i < MAX_MQ; i++) {
        if (ipc->queues[i].is_active && ipc->queues[i].id == mq_id) {
            MessageQueue *mq = &ipc->queues[i];
            if (mq->count >= MAX_MQ_MSGS) return -1; /* Queue full */
            Message *msg = &mq->msgs[mq->tail];
            msg->sender_pid = sender_pid;
            int copy_len = (len < MAX_MSG_SIZE - 1) ? len : MAX_MSG_SIZE - 1;
            memcpy(msg->payload, payload, copy_len);
            msg->payload[copy_len] = '\0';
            msg->len = copy_len;
            mq->tail = (mq->tail + 1) % MAX_MQ_MSGS;
            mq->count++;

            log_event(ipc, IPC_MQ_SEND, sender_pid, mq->owner_pid, payload);
            return 0;
        }
    }
    return -1;
}

/* Receive a message from a queue. Returns bytes read or -1 */
int mq_receive(IPCManager *ipc, int mq_id, char *buf, int buf_size, int *sender_pid) {
    for (int i = 0; i < MAX_MQ; i++) {
        if (ipc->queues[i].is_active && ipc->queues[i].id == mq_id) {
            MessageQueue *mq = &ipc->queues[i];
            if (mq->count == 0) return 0; /* Empty queue */
            Message *msg = &mq->msgs[mq->head];
            if (sender_pid) *sender_pid = msg->sender_pid;
            int copy_len = (msg->len < buf_size - 1) ? msg->len : buf_size - 1;
            memcpy(buf, msg->payload, copy_len);
            buf[copy_len] = '\0';
            mq->head = (mq->head + 1) % MAX_MQ_MSGS;
            mq->count--;

            log_event(ipc, IPC_MQ_RECEIVE, msg->sender_pid, mq->owner_pid, buf);
            return copy_len;
        }
    }
    return -1;
}

/* DFS-based cycle detection for deadlock in wait-for graph (CEP R4) */
static int dfs_cycle(int node, int visited[], int rec_stack[], int n, int graph[][50]) {
    visited[node] = 1;
    rec_stack[node] = 1;
    for (int j = 0; j < n; j++) {
        if (!graph[node][j]) continue;
        if (!visited[j] && dfs_cycle(j, visited, rec_stack, n, graph)) return 1;
        if (rec_stack[j]) return 1; /* Back edge = cycle */
    }
    rec_stack[node] = 0;
    return 0;
}

/* Detect deadlock using cycle detection in the wait-for graph */
int ipc_detect_deadlock(IPCManager *ipc, int num_processes) {
    int visited[50] = {0};
    int rec_stack[50] = {0};
    for (int i = 0; i < num_processes; i++) {
        if (!visited[i]) {
            if (dfs_cycle(i, visited, rec_stack, num_processes, ipc->wait_graph))
                return 1; /* Deadlock found */
        }
    }
    return 0;
}

/* Serialize the last N IPC events to JSON for dashboard log panel */
void ipc_events_to_json(IPCManager *ipc, char *buf, int buf_size) {
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "[");
    static const char *type_names[] = {"PIPE_WRITE", "PIPE_READ", "MQ_SEND", "MQ_RECV"};
    int count = (ipc->event_count < MAX_IPC_EVENTS) ? ipc->event_count : MAX_IPC_EVENTS;
    /* Show last 20 events */
    int start = (ipc->event_count > 20) ? ipc->event_count - 20 : 0;
    int first = 1;
    for (int i = start; i < ipc->event_count && i < start + 20; i++) {
        IPCEvent *e = &ipc->events[i % MAX_IPC_EVENTS];
        if (!first) offset += snprintf(buf + offset, buf_size - offset, ",");
        first = 0;
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"type\":\"%s\",\"sender\":%d,\"receiver\":%d,\"data\":\"%s\",\"time\":%ld}",
            type_names[e->type], e->sender_pid, e->receiver_pid, e->data, e->timestamp);
        if (offset >= buf_size - 8) break;
    }
    snprintf(buf + offset, buf_size - offset, "]");
    (void)count;
}

/* Serialize IPC summary stats to JSON */
void ipc_stats_to_json(IPCManager *ipc, char *buf, int buf_size) {
    int active_pipes = 0, active_mqs = 0;
    for (int i = 0; i < MAX_PIPES; i++) if (ipc->pipes[i].is_active) active_pipes++;
    for (int i = 0; i < MAX_MQ; i++) if (ipc->queues[i].is_active) active_mqs++;
    snprintf(buf, buf_size,
        "{\"active_pipes\":%d,\"active_queues\":%d,\"total_events\":%d}",
        active_pipes, active_mqs, ipc->event_count);
}
