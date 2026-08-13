/*
 * src/ajie.c
 *
 * AJIE (Aisha, Jackson, Icaro, Elilucio) Scheduling Algorithm.
 * Priority Aging Non-Preemptive Scheduler.
 *
 * Priority convention used by the algorithm:
 *   1 = highest priority
 *   num_priority_levels = lowest priority
 *
 * Aging therefore improves the effective priority by decreasing its
 * numerical value (-1), capped at 1.
 */

#include "ajie.h"
#include "scheduler.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------
 * CPU state
 * --------------------------------------------------------------------- */
typedef enum {
    CPU_IDLE,
    CPU_CONTEXT_SWITCHING,
    CPU_RUNNING
} CpuState;

/* ---------------------------------------------------------------------
 * Min-heap for I/O-blocked processes.
 * --------------------------------------------------------------------- */
typedef struct {
    int idx;
    int completion_tick;
} BlockedEntry;

typedef struct {
    BlockedEntry *heap;
    int capacity;
    int size;
} BlockedHeap;

static int bh_init(BlockedHeap *h, int capacity) {
    if (h == NULL || capacity <= 0) {
        return -1;
    }

    h->heap = (BlockedEntry *)malloc(sizeof(BlockedEntry) * (size_t)capacity);
    if (h->heap == NULL) {
        h->capacity = 0;
        h->size = 0;
        return -1;
    }

    h->capacity = capacity;
    h->size = 0;
    return 0;
}

static void bh_free(BlockedHeap *h) {
    if (h == NULL) {
        return;
    }

    free(h->heap);
    h->heap = NULL;
    h->capacity = 0;
    h->size = 0;
}

static int bh_empty(const BlockedHeap *h) {
    return h == NULL || h->size == 0;
}

static void bh_swap(BlockedEntry *a, BlockedEntry *b) {
    BlockedEntry tmp = *a;
    *a = *b;
    *b = tmp;
}

static int bh_push(BlockedHeap *h, int idx, int completion_tick) {
    if (h == NULL || h->size >= h->capacity) {
        return -1;
    }

    int i = h->size++;
    h->heap[i].idx = idx;
    h->heap[i].completion_tick = completion_tick;

    while (i > 0) {
        int parent = (i - 1) / 2;

        if (h->heap[parent].completion_tick <= h->heap[i].completion_tick) {
            break;
        }

        bh_swap(&h->heap[parent], &h->heap[i]);
        i = parent;
    }
    return 0;
}

static BlockedEntry bh_pop(BlockedHeap *h) {
    BlockedEntry top = h->heap[0];
    h->heap[0] = h->heap[--h->size];
    int i = 0;
    for (;;) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left < h->size &&
            h->heap[left].completion_tick < h->heap[smallest].completion_tick) {
            smallest = left;
        }
        if (right < h->size &&
            h->heap[right].completion_tick < h->heap[smallest].completion_tick) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        bh_swap(&h->heap[i], &h->heap[smallest]);
        i = smallest;
    }
    return top;
}

static int bh_peek_tick(const BlockedHeap *h) {
    return h->heap[0].completion_tick;
}

typedef struct {
    int idx;
    int current_priority;
    int arrived_in_current_round;
    int ready_arrival_tick;
    int prev;
    int next;
} AJIEReadyNode;

typedef struct {
    int head;
    int tail;
} AJIEReadyQueue;

static void ready_queue_init(AJIEReadyQueue *queue) {
    queue->head = -1;
    queue->tail = -1;
}

static void ready_push_back(AJIEReadyQueue *queue,
                            AJIEReadyNode *nodes,
                            int idx) {
    nodes[idx].prev = queue->tail;
    nodes[idx].next = -1;

    if (queue->tail != -1) {
        nodes[queue->tail].next = idx;
    } else {
        queue->head = idx;
    }
    queue->tail = idx;
}

static int ready_pop_front(AJIEReadyQueue *queue, AJIEReadyNode *nodes) {
    int idx = queue->head;
    if (idx == -1) {
        return -1;
    }
    int next = nodes[idx].next;
    queue->head = next;
    if (next != -1) {
        nodes[next].prev = -1;
    } else {
        queue->tail = -1;
    }
    nodes[idx].prev = -1;
    nodes[idx].next = -1;
    return idx;
}

static void ready_remove(AJIEReadyQueue *queue,
                         AJIEReadyNode *nodes,
                         int idx) {
    int prev = nodes[idx].prev;
    int next = nodes[idx].next;
    if (prev != -1) {
        nodes[prev].next = next;
    } else {
        queue->head = next;
    }
    if (next != -1) {
        nodes[next].prev = prev;
    } else {
        queue->tail = prev;
    }
    nodes[idx].prev = -1;
    nodes[idx].next = -1;
}

static int ready_counted_push(AJIEReadyQueue *queues,
                              AJIEReadyNode *nodes,
                              int idx,
                              int priority,
                              int tick) {
    if (priority < 1) {
        return -1;
    }

    nodes[idx].current_priority = priority;
    nodes[idx].arrived_in_current_round = 1;
    nodes[idx].ready_arrival_tick = tick;
    nodes[idx].prev = -1;
    nodes[idx].next = -1;

    ready_push_back(&queues[priority], nodes, idx);
    return 0;
}

/*
 * Ages every ready process that was already waiting before the current
 * scheduling decision.
 *
 * Traversing from high to low priority while physically moving nodes
 * can age the same node twice. To guarantee exactly one promotion per
 * decision, the traversal is performed from low to high: when a node is
 * moved from level p to p-1, level p-1 has already been processed.
 *
 * "Aging" is therefore:
 *     current_priority = max(current_priority - 1, 1)
 *
 * because smaller values represent higher priority.
 */
static void ready_age_all_unaged(AJIEReadyQueue *queues,
                                 AJIEReadyNode *nodes,
                                 int num_priority_levels) {
    for (int level = 2; level <= num_priority_levels; ++level) {
        int idx = queues[level].head;
        while (idx != -1) {
            int next = nodes[idx].next;
            if (!nodes[idx].arrived_in_current_round) {
                ready_remove(&queues[level], nodes, idx);
                if (nodes[idx].current_priority > 1) {
                    nodes[idx].current_priority--;
                }
                ready_push_back(&queues[nodes[idx].current_priority],
                                nodes,
                                idx);
            }
            idx = next;
        }
    }
}

/* Returns the highest-priority non-empty queue. */
static int ready_find_highest_nonempty(const AJIEReadyQueue *queues,
                                       int num_priority_levels) {
    for (int level = 1; level <= num_priority_levels; ++level) {
        if (queues[level].head != -1) {
            return level;
        }
    }
    return -1;
}

/*
 * Selects the next process.
 *
 * Because each priority level is a FIFO queue, ties at the same effective
 * priority are resolved by FCFS order (ready_arrival_tick), exactly as
 * required by the PAN/AJIE specification. The explicit tick is kept as
 * metadata and the queue order is already sufficient to enforce it.
 */
static int ready_select_next(AJIEReadyQueue *queues,
                             AJIEReadyNode *nodes,
                             int num_priority_levels) {
    int highest_level =
        ready_find_highest_nonempty(queues, num_priority_levels);

    if (highest_level == -1) {
        return -1;
    }
    return ready_pop_front(&queues[highest_level], nodes);
}

/*
 * Newly arrived/unblocked processes do not age in the round in which they
 * entered the ready queues. Once a scheduling decision is made, the flags
 * for the remaining ready processes are cleared, allowing them to age at
 * the next decision.
 */
static void ready_clear_arrival_flags(AJIEReadyQueue *queues,
                                       AJIEReadyNode *nodes,
                                       int num_priority_levels) {
    for (int level = 1; level <= num_priority_levels; ++level) {
        for (int idx = queues[level].head; idx != -1; idx = nodes[idx].next) {
            nodes[idx].arrived_in_current_round = 0;
        }
    }
}

/* ---------------------------------------------------------------------
 * Validation
 * --------------------------------------------------------------------- */
static int validate_processes(const Process *processes,
                              int process_count,
                              int num_priority_levels) {
    if (processes == NULL || process_count <= 0 || num_priority_levels <= 0) {
        return -1;
    }
    for (int i = 0; i < process_count; ++i) {
        if (processes[i].priority < 1 ||
            processes[i].priority > num_priority_levels) {
            return -1;
        }
        if (processes[i].num_bursts <= 0 || processes[i].bursts == NULL) {
            return -1;
        }
        if (processes[i].bursts[0].type != BURST_CPU ||
            processes[i].bursts[processes[i].num_bursts - 1].type != BURST_CPU) {
            return -1;
        }
        int io_count = 0;
        for (int b = 0; b < processes[i].num_bursts; ++b) {
            if (processes[i].bursts[b].duration <= 0) {
                return -1;
            }
            if (b > 0 && processes[i].bursts[b].type == processes[i].bursts[b - 1].type) {
                return -1;
            }
            if (processes[i].bursts[b].type == BURST_IO) {
                ++io_count;
            }
        }
        if (processes[i].io_request_count != 0 &&
            processes[i].io_request_count != io_count) {
            return -1;
        }
        if (i > 0 &&
            processes[i].arrival_time < processes[i - 1].arrival_time) {
            /*
             * The simulator admits arrivals with a single monotonic pointer,
             * so the workload must be ordered by arrival_time.
             */
            return -1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * Primary AJIE simulator.
 * --------------------------------------------------------------------- */
int ajie_run(Process *processes,
              int process_count,
              AJIEConfig config,
              AJIEMetrics *out_metrics) {
    if (config.context_switch_cost < 0 ||
        validate_processes(processes,
                           process_count,
                           config.num_priority_levels) != 0) {
        return -1;
    }

    AJIEReadyQueue *ready_queues =
        (AJIEReadyQueue *)malloc(sizeof(AJIEReadyQueue) *
                                 (size_t)(config.num_priority_levels + 1));
    AJIEReadyNode *ready_nodes =
        (AJIEReadyNode *)calloc((size_t)process_count,
                                sizeof(AJIEReadyNode));

    if (ready_queues == NULL || ready_nodes == NULL) {
        free(ready_queues);
        free(ready_nodes);
        return -1;
    }
    for (int level = 0; level <= config.num_priority_levels; ++level) {
        ready_queue_init(&ready_queues[level]);
    }
    BlockedHeap blocked;
    if (bh_init(&blocked, process_count) != 0) {
        free(ready_queues);
        free(ready_nodes);
        return -1;
    }
    for (int i = 0; i < process_count; ++i) {
        ready_nodes[i].idx = i;
        ready_nodes[i].current_priority = processes[i].priority;
        ready_nodes[i].arrived_in_current_round = 0;
        ready_nodes[i].ready_arrival_tick = processes[i].arrival_time;
        ready_nodes[i].prev = -1;
        ready_nodes[i].next = -1;

        processes[i].current_state = PROCESS_STATE_NEW;
        processes[i].current_burst_index = 0;
        processes[i].context_switches = 0;
        processes[i].completion_time = 0;

        for (int b = 0; b < processes[i].num_bursts; ++b) {
            processes[i].bursts[b].time_left =
                processes[i].bursts[b].duration;
        }
    }

    int arrival_ptr = 0;
    int completed_count = 0;

    /*
     * This counter represents real context-switch events, not dispatches.
     * Under the non-preemptive rule from the source document, a switch is
     * charged when the CPU drops a process before that process terminates
     * (for AJIE this occurs when a CPU burst ends and the process blocks for
     * I/O). The first dispatch is therefore NOT a context switch.
     */
    long total_context_switches = 0;
    CpuState cpu_state = CPU_IDLE;
    int running_index = -1;

    /*
     * When a process blocks, the switch-away overhead is charged before the
     * next process can run. New arrivals and I/O completions may occur
     * during this overhead, but they cannot change the already-decided fact
     * that the running process was relinquished.
     */
    int cs_remaining = 0;
    int current_time = 0;

    while (completed_count < process_count) {
        /* 1. Admit external arrivals. */
        while (arrival_ptr < process_count &&
               processes[arrival_ptr].arrival_time <= current_time) {
            int idx = arrival_ptr;
            processes[idx].current_state = PROCESS_STATE_READY;
            if (ready_counted_push(ready_queues,
                                   ready_nodes,
                                   idx,
                                   processes[idx].priority,
                                   current_time) != 0) {
                bh_free(&blocked);
                free(ready_queues);
                free(ready_nodes);
                return -1;
            }
            ++arrival_ptr;
        }

        /* 2. Admit I/O completions. Returning processes restart aging from their configured base priority. */
        while (!bh_empty(&blocked) &&
               bh_peek_tick(&blocked) <= current_time) {
            BlockedEntry entry = bh_pop(&blocked);
            int idx = entry.idx;
            processes[idx].current_state = PROCESS_STATE_READY;
            if (ready_counted_push(ready_queues,
                                   ready_nodes,
                                   idx,
                                   processes[idx].priority,
                                   current_time) != 0) {
                bh_free(&blocked);
                free(ready_queues);
                free(ready_nodes);
                return -1;
            }
        }

        /* 3. A context switch consumes real simulation ticks. The tick in which the final unit   
         *    of switching overhead is consumed is not also a CPU tick for the next process. */
        if (cpu_state == CPU_CONTEXT_SWITCHING) {
            --cs_remaining;
            if (cs_remaining == 0) {
                cpu_state = CPU_IDLE;
            }
            ++current_time;
            continue;
        }

        /* 4. If the processor is free, perform one scheduling decision. */
        if (cpu_state == CPU_IDLE) {
            /*
             * One aging round happens only at a real scheduling decision. A process that entered the ready queues during the current
             * round is protected by arrived_in_current_round and therefore does not receive an immediate promotion.
             */
            if (ready_find_highest_nonempty(ready_queues,
                                            config.num_priority_levels) != -1) {
                ready_age_all_unaged(ready_queues,
                                     ready_nodes,
                                     config.num_priority_levels);
            }
            int selected = ready_select_next(ready_queues,
                                             ready_nodes,
                                             config.num_priority_levels);
            if (selected != -1) {
                ready_clear_arrival_flags(ready_queues,
                                          ready_nodes,
                                          config.num_priority_levels);
                running_index = selected;
                cpu_state = CPU_RUNNING;
                processes[selected].current_state = PROCESS_STATE_RUNNING;
            }
        }

        /* 5. Run exactly one tick of the selected CPU burst. */
        if (cpu_state == CPU_RUNNING) {
            Process *p = &processes[running_index];

            /*
             * CPU bursts are assumed to have positive duration. A zero-length
             * burst is rejected by the validation below to avoid an invalid
             * decrement.
             */
            Burst *b = &p->bursts[p->current_burst_index];
            --b->time_left;
            if (b->time_left == 0) {
                if (p->current_burst_index == p->num_bursts - 1) {
                    p->current_state = PROCESS_STATE_TERMINATED;
                    p->completion_time = current_time + 1;
                    ++completed_count;
                    cpu_state = CPU_IDLE;
                    running_index = -1;
                } else {
                    /*
                     * CPU burst finished; the next burst is I/O.
                     */
                    ++p->current_burst_index;
                    Burst *io = &p->bursts[p->current_burst_index];
                    p->current_state = PROCESS_STATE_BLOCKED;
                    if (bh_push(&blocked,
                                running_index,
                                current_time + 1 + io->duration) != 0) {
                        bh_free(&blocked);
                        free(ready_queues);
                        free(ready_nodes);
                        return -1;
                    }
                    ++p->current_burst_index;
                    ++total_context_switches;
                    ++p->context_switches;
                    if (config.context_switch_cost > 0) {
                        cpu_state = CPU_CONTEXT_SWITCHING;
                        cs_remaining = config.context_switch_cost;
                    } else {
                        cpu_state = CPU_IDLE;
                    }
                    running_index = -1;
                }
            }
        }

        /*
         * 6. If the processor is genuinely idle, jump directly to the next arrival or I/O completion. This avoids busy waiting.
         */
        if (cpu_state == CPU_IDLE &&
            completed_count < process_count) {
            int next_arrival =
                (arrival_ptr < process_count)
                    ? processes[arrival_ptr].arrival_time
                    : INT_MAX;
            int next_io =
                (!bh_empty(&blocked))
                    ? bh_peek_tick(&blocked)
                    : INT_MAX;
            int next_event =
                (next_arrival < next_io) ? next_arrival : next_io;
            if (next_event != INT_MAX && next_event > current_time) {
                current_time = next_event;
                continue;
            }
        }
        ++current_time;
    }
    if (out_metrics != NULL) {
        RunMetrics metrics =
            compute_run_metrics(processes, (size_t)process_count);

        out_metrics->total_context_switches = total_context_switches;
        out_metrics->total_ticks = current_time;
        out_metrics->mean_turnaround = metrics.turnaround_ms;
        out_metrics->mean_slowdown = metrics.slowdown;
        out_metrics->jain_slowdown_percent = metrics.jain_slowdown_pct;
    }

    bh_free(&blocked);
    free(ready_queues);
    free(ready_nodes);
    return 0;
}

/* ---------------------------------------------------------------------
 * Wrapper matching SchedulerFn contract in include/scheduler.h.
 * --------------------------------------------------------------------- */
RunMetrics run_ajie(const Process *processes, size_t n, const SimConfig *cfg) {
    RunMetrics metrics = {0.0, 0, 0.0, 0.0, 0};
    if (processes == NULL || n == 0 || cfg == NULL || n > (size_t)INT_MAX) {
        return metrics;
    }

    /*
     * No fallback priority count is used here. The quantity of priority levels is a real user/system configuration parameter.
     */
    if (cfg->num_priority_levels <= 0 || cfg->context_switch_cost < 0) {
        return metrics;
    }
    Process *copy =
        (Process *)calloc(n, sizeof(Process));
    if (copy == NULL) {
        return metrics;
    }
    for (size_t i = 0; i < n; ++i) {
        copy[i] = processes[i];
        if (processes[i].num_bursts > 0 && processes[i].bursts != NULL) {
            size_t burst_bytes =
                (size_t)processes[i].num_bursts * sizeof(Burst);
            copy[i].bursts = (Burst *)malloc(burst_bytes);
            if (copy[i].bursts == NULL) {
                for (size_t j = 0; j < i; ++j) {
                    free(copy[j].bursts);
                }
                free(copy);
                return metrics;
            }
            memcpy(copy[i].bursts,
                   processes[i].bursts,
                   burst_bytes);
        } else {
            copy[i].bursts = NULL;
        }
    }

    AJIEConfig config;
    config.context_switch_cost = cfg->context_switch_cost;
    config.num_priority_levels = cfg->num_priority_levels;

    AJIEMetrics ajie_metrics;
    int status =
        ajie_run(copy, (int)n, config, &ajie_metrics);
    if (status == 0) {
        metrics = compute_run_metrics(copy, n);
        metrics.ok = 1;
    }
    for (size_t i = 0; i < n; ++i) {
        free(copy[i].bursts);
    }
    free(copy);
    return metrics;
}
