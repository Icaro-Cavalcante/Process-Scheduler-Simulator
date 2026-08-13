#include "round_robin.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* CPU-level state for the tick loop. Mirrors the states discussed in
 * troca_contexto.md Section 2 (IDLE / CONTEXT_SWITCHING / RUNNING). */
typedef enum {
    CPU_IDLE,
    CPU_CONTEXT_SWITCHING,
    CPU_RUNNING
} CpuState;

/* NOTE (ELI-04): the ready queue is implemented in queue.h/queue.c, shared
 * across all scheduling algorithms. This file uses the QueueIdx facade
 * (queue_idx_create/queue_idx_enqueue/queue_idx_dequeue/queue_idx_is_empty/
 * queue_idx_destroy), which stores process indices as int -- matching how
 * this file already tracks the ready set. fcfs.c uses a different facade
 * (Queue / queue_init / queue_enqueue / queue_dequeue / queue_is_empty /
 * queue_destroy) that stores Process* directly instead; both facades share
 * one generic pointer-queue core in queue.c. Only the queue_create(...) ->
 * queue_idx_create(...) call and the five queue_*(rq, ...) call sites below
 * were renamed to queue_idx_*(rq, ...) to resolve that interface conflict —
 * no scheduling logic in this file was changed. */

/* ---------------------------------------------------------------------
 * Min-heap of (process index, I/O completion tick), so we don't have to
 * linear-scan every blocked process on every tick to see who unblocks.
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

static void bh_init(BlockedHeap *h, int capacity) {
    h->heap = (BlockedEntry *)malloc(sizeof(BlockedEntry) * (size_t)capacity);
    h->capacity = capacity;
    h->size = 0;
}

static void bh_free(BlockedHeap *h) { free(h->heap); h->heap = NULL; }
static int bh_empty(const BlockedHeap *h) { return h->size == 0; }

static void bh_swap(BlockedEntry *a, BlockedEntry *b) { BlockedEntry t = *a; *a = *b; *b = t; }

static void bh_push(BlockedHeap *h, int idx, int completion_tick) {
    assert(h->size < h->capacity && "blocked heap overflow — capacity should equal process_count");
    int i = h->size++;
    h->heap[i].idx = idx;
    h->heap[i].completion_tick = completion_tick;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->heap[parent].completion_tick <= h->heap[i].completion_tick) break;
        bh_swap(&h->heap[parent], &h->heap[i]);
        i = parent;
    }
}

static BlockedEntry bh_pop(BlockedHeap *h) {
    assert(!bh_empty(h));
    BlockedEntry top = h->heap[0];
    h->heap[0] = h->heap[--h->size];
    int i = 0;
    for (;;) {
        int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
        if (left < h->size && h->heap[left].completion_tick < h->heap[smallest].completion_tick) smallest = left;
        if (right < h->size && h->heap[right].completion_tick < h->heap[smallest].completion_tick) smallest = right;
        if (smallest == i) break;
        bh_swap(&h->heap[i], &h->heap[smallest]);
        i = smallest;
    }
    return top;
}

static int bh_peek_tick(const BlockedHeap *h) { return h->heap[0].completion_tick; }

/* ---------------------------------------------------------------------
 * Metrics helpers (assignment PDF Section 9 / troca_contexto.md Section 5).
 * --------------------------------------------------------------------- */
static double process_ideal_time(const Process *p) {
    double total = 0.0;
    for (int i = 0; i < p->num_bursts; i++) total += p->bursts[i].duration;
    return total;
}

static void compute_metrics(Process *processes, int process_count, long total_context_switches,
                             long total_ticks, RRMetrics *out) {
    double turnaround_sum = 0.0;
    double *slowdown = (double *)malloc(sizeof(double) * (size_t)process_count);

    for (int i = 0; i < process_count; i++) {
        double turnaround = (double)(processes[i].completion_time - processes[i].arrival_time);
        turnaround_sum += turnaround;

        double ideal = process_ideal_time(&processes[i]);
        slowdown[i] = (ideal > 0.0) ? (turnaround / ideal) : 1.0;
    }

    double sum_sd = 0.0, sum_sd_sq = 0.0;
    for (int i = 0; i < process_count; i++) {
        sum_sd += slowdown[i];
        sum_sd_sq += slowdown[i] * slowdown[i];
    }
    double jain = (sum_sd_sq > 0.0)
        ? (sum_sd * sum_sd) / ((double)process_count * sum_sd_sq) * 100.0
        : 100.0;

    out->total_context_switches = total_context_switches;
    out->total_ticks = total_ticks;
    out->mean_turnaround = turnaround_sum / (double)process_count;
    out->jain_slowdown_percent = jain;

    free(slowdown);
}

/* ---------------------------------------------------------------------
 * The algorithm itself.
 * --------------------------------------------------------------------- */
int round_robin_run(Process *processes, int process_count, RRConfig config,
                     RRExecutionSlice *slice_log, int slice_log_capacity, int *slice_log_count,
                     RRMetrics *out_metrics) {
    if (processes == NULL || process_count <= 0 || config.quantum <= 0 || config.context_switch_cost < 0) {
        return -1;
    }

    /* Workload is assumed sorted by arrival_time ascending — true of
     * ICR-05's generate_workload() output (arrivals are a monotonic
     * cumulative sum). Round Robin doesn't reorder processes, only reads
     * arrival_time, so this holds regardless of who produced the array. */
    for (int i = 1; i < process_count; i++) {
        assert(processes[i].arrival_time >= processes[i - 1].arrival_time &&
               "round_robin_run expects processes[] sorted by arrival_time ascending");
    }

    QueueIdx *rq = queue_idx_create(process_count);
    if (rq == NULL) return -1;
    BlockedHeap bh;
    bh_init(&bh, process_count);

    for (int i = 0; i < process_count; i++) {
        processes[i].current_state = PROCESS_STATE_NEW;
        processes[i].current_burst_index = 0;
        processes[i].context_switches = 0;
        for (int b = 0; b < processes[i].num_bursts; b++) {
            processes[i].bursts[b].time_left = processes[i].bursts[b].duration;
        }
    }

    int arrival_ptr = 0;
    int completed_count = 0;
    long total_context_switches = 0;
    int slices_written = 0;

    CpuState cpu_state = CPU_IDLE;
    int running_index = -1;
    int quantum_used = 0;
    int cs_remaining = 0;
    int cs_target = -1;

    int slice_open = 0;
    int slice_start_tick = 0;
    int slice_pid = -1;

    int current_time = 0;

    while (completed_count < process_count) {
        /* 1. Admit arrivals due at this tick (new -> ready). */
        while (arrival_ptr < process_count && processes[arrival_ptr].arrival_time <= current_time) {
            processes[arrival_ptr].current_state = PROCESS_STATE_READY;
            queue_idx_enqueue(rq, arrival_ptr);
            arrival_ptr++;
        }

        /* 2. Admit I/O completions due at this tick (blocked -> ready).
         *    Assumption: arrivals are enqueued before I/O completions when
         *    both happen on the same tick — process_model.md doesn't
         *    specify a tie-break here, so this is a documented choice. */
        while (!bh_empty(&bh) && bh_peek_tick(&bh) <= current_time) {
            BlockedEntry e = bh_pop(&bh);
            processes[e.idx].current_state = PROCESS_STATE_READY;
            queue_idx_enqueue(rq, e.idx);
        }

        /* 3. Dispatch if the CPU is idle and someone is ready. */
        if (cpu_state == CPU_IDLE && !queue_idx_is_empty(rq)) {
            int target = queue_idx_dequeue(rq);
            if (config.context_switch_cost > 0) {
                cpu_state = CPU_CONTEXT_SWITCHING;
                cs_remaining = config.context_switch_cost;
                cs_target = target;
            } else {
                /* Zero-cost dispatch (secondary sensitivity analysis only,
                 * troca_contexto.md Section 6): skip straight to RUNNING. */
                cpu_state = CPU_RUNNING;
                running_index = target;
                quantum_used = 0;
                total_context_switches++;
                processes[target].context_switches++;
                processes[target].current_state = PROCESS_STATE_RUNNING;
            }
        }

        /* 4. Advance the CPU for this tick. */
        if (cpu_state == CPU_CONTEXT_SWITCHING) {
            cs_remaining--;
            if (cs_remaining == 0) {
                cpu_state = CPU_RUNNING;
                running_index = cs_target;
                quantum_used = 0;
                total_context_switches++;
                processes[cs_target].context_switches++;
                processes[cs_target].current_state = PROCESS_STATE_RUNNING;
            }
        } else if (cpu_state == CPU_RUNNING) {
            Process *p = &processes[running_index];
            Burst *b = &p->bursts[p->current_burst_index];

            if (!slice_open) {
                slice_open = 1;
                slice_start_tick = current_time;
                slice_pid = p->pid;
            }

            b->time_left--;
            quantum_used++;

            if (b->time_left == 0) {
                /* Burst finished this tick — finishing takes priority over
                 * quantum expiry if both would happen on the same tick. */
                RRSliceReason reason;
                if (p->current_burst_index == p->num_bursts - 1) {
                    p->current_state = PROCESS_STATE_TERMINATED;
                    p->completion_time = current_time + 1;
                    completed_count++;
                    reason = RR_SLICE_TERMINATED;
                } else {
                    p->current_burst_index++; /* now at the I/O burst */
                    Burst *io = &p->bursts[p->current_burst_index];
                    p->current_state = PROCESS_STATE_BLOCKED;
                    bh_push(&bh, running_index, current_time + 1 + io->duration);
                    p->current_burst_index++; /* pre-advance to the next CPU burst,
                                                  ready for when I/O completes */
                    reason = RR_SLICE_BLOCKED;
                }
                cpu_state = CPU_IDLE;

                if (slice_log != NULL && slices_written < slice_log_capacity) {
                    slice_log[slices_written].pid = slice_pid;
                    slice_log[slices_written].start_tick = slice_start_tick;
                    slice_log[slices_written].end_tick = current_time + 1;
                    slice_log[slices_written].reason = reason;
                    slices_written++;
                }
                slice_open = 0;

            } else if (quantum_used == config.quantum) {
                /* Quantum expired before the burst finished: preempt.
                 * time_left is left exactly as decremented above — this IS
                 * the "remaining time preserved" requirement
                 * (process_model.md Section 5, Running -> Ready row). */
                assert(quantum_used <= config.quantum);
                p->current_state = PROCESS_STATE_READY;
                queue_idx_enqueue(rq, running_index);
                cpu_state = CPU_IDLE;

                if (slice_log != NULL && slices_written < slice_log_capacity) {
                    slice_log[slices_written].pid = slice_pid;
                    slice_log[slices_written].start_tick = slice_start_tick;
                    slice_log[slices_written].end_tick = current_time + 1;
                    slice_log[slices_written].reason = RR_SLICE_PREEMPTED;
                    slices_written++;
                }
                slice_open = 0;
            }
            /* else: quantum not yet expired and burst not finished —
             * keep running, no state change, no log entry yet. */
        }

        current_time++;
    }

    if (slice_log_count != NULL) *slice_log_count = slices_written;
    if (out_metrics != NULL) {
        compute_metrics(processes, process_count, total_context_switches, current_time, out_metrics);
    }

    queue_idx_destroy(rq);
    bh_free(&bh);
    return 0;
}
