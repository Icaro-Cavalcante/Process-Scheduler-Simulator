#include "priority.h"
#include "queue.h"
#include "io_queue.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/*
 * Ready set: unlike FCFS/RR (plain FIFO order), Priority needs to pick
 * the READY process with the smallest `priority` value at each dispatch,
 * breaking ties by arrival_time (see header comment). A FIFO queue alone
 * cannot express that ordering, so ready indices are kept in a simple
 * dynamic array and scanned at each dispatch -- O(ready_count) per pick,
 * which is fine at the scale this assignment requires and mirrors the
 * same "scan for the best candidate" pattern fcfs.c already uses for
 * next_arrival_time()/next_io_completion().
 */
typedef struct {
    int *indices;
    int count;
    int capacity;
} ReadySet;

static int ready_set_init(ReadySet *set, int capacity) {
    set->indices = (int *)malloc(sizeof(int) * (size_t)capacity);
    if (set->indices == NULL) return -1;
    set->count = 0;
    set->capacity = capacity;
    return 0;
}

static void ready_set_add(ReadySet *set, int process_index) {
    /* capacity is always process_count, and no process can be in the
     * ready set twice at once, so this never overflows in correct use */
    set->indices[set->count++] = process_index;
}

/* Picks the ready process with the smallest priority value (lower =
 * higher priority, per process_model.md Section 3), breaking ties by
 * the smallest arrival_time. Removes it from the ready set (swap with
 * the last element, O(1)) and returns its process index, or -1 if the
 * ready set is empty. */
static int ready_set_pick_next(ReadySet *set, const Process *processes) {
    if (set->count == 0) return -1;

    int best_slot = 0;
    for (int slot = 1; slot < set->count; slot++) {
        int candidate = set->indices[slot];
        int best = set->indices[best_slot];

        if (processes[candidate].priority < processes[best].priority) {
            best_slot = slot;
        } else if (processes[candidate].priority == processes[best].priority &&
                   processes[candidate].arrival_time < processes[best].arrival_time) {
            best_slot = slot;
        }
    }

    int chosen = set->indices[best_slot];
    set->indices[best_slot] = set->indices[set->count - 1];
    set->count--;
    return chosen;
}

static void ready_set_free(ReadySet *set) {
    free(set->indices);
    set->indices = NULL;
    set->count = 0;
    set->capacity = 0;
}

/* Smallest arrival_time among processes not yet moved into the ready
 * set. Returns -1 if every process has already arrived. Same role as
 * fcfs.c's next_arrival_time(), kept local here since Process does not
 * expose a "not yet arrived" flag by itself. */
static int next_arrival_time(const Process *processes, int process_count,
                              const int *arrived) {
    int best = -1;
    for (int i = 0; i < process_count; i++) {
        if (!arrived[i]) {
            if (best == -1 || processes[i].arrival_time < best) {
                best = processes[i].arrival_time;
            }
        }
    }
    return best;
}

static double ideal_completion_time(const Process *p) {
    double total = 0.0;
    for (int i = 0; i < p->num_bursts; i++) {
        total += p->bursts[i].duration;
    }
    return total;
}

static void compute_priority_metrics(const Process *processes, int process_count,
                                      long total_context_switches, long total_ticks,
                                      PriorityMetrics *out) {
    double sum_turnaround = 0.0;
    double sum_slowdown = 0.0;
    double sum_sq_slowdown = 0.0;

    for (int i = 0; i < process_count; i++) {
        double turnaround = (double)(processes[i].completion_time - processes[i].arrival_time);
        sum_turnaround += turnaround;

        double ideal = ideal_completion_time(&processes[i]);
        double slowdown = (ideal > 0.0) ? (turnaround / ideal) : 1.0;
        sum_slowdown += slowdown;
        sum_sq_slowdown += slowdown * slowdown;
    }

    out->total_context_switches = total_context_switches;
    out->total_ticks = total_ticks;
    out->mean_turnaround = sum_turnaround / (double)process_count;
    out->jain_slowdown_percent = (sum_sq_slowdown > 0.0)
        ? (sum_slowdown * sum_slowdown) / ((double)process_count * sum_sq_slowdown) * 100.0
        : 100.0;
}

int priority_run(Process *processes, int process_count, PriorityConfig config,
                  PriorityMetrics *out_metrics) {
    if (processes == NULL || process_count <= 0 || config.context_switch_cost < 0) {
        return -1;
    }

    ReadySet ready;
    if (ready_set_init(&ready, process_count) != 0) {
        return -1;
    }

    /* io_queue.h defines the I/O blocking queue type as FilaIO (the
     * module's own identifiers are in Portuguese, since this file
     * reuses io_queue.h as already merged into the repository rather
     * than rewriting it). */
    FilaIO *blocked = io_queue_cria(process_count);
    if (blocked == NULL) {
        ready_set_free(&ready);
        return -1;
    }

    int *arrived = (int *)calloc((size_t)process_count, sizeof(int));
    if (arrived == NULL) {
        io_queue_destroi(blocked);
        ready_set_free(&ready);
        return -1;
    }

    int current_time = 0;
    int finished_count = 0;
    long total_context_switches = 0;

    while (finished_count < process_count) {

        /* 1. New -> Ready: move in every process that has arrived by now */
        for (int i = 0; i < process_count; i++) {
            if (!arrived[i] && processes[i].arrival_time <= current_time) {
                processes[i].current_state = PROCESS_STATE_READY;
                ready_set_add(&ready, i);
                arrived[i] = 1;
            }
        }

        /* 2. Blocked -> Ready: move in every process whose I/O burst just
         * finished (per io_modeling.md, this is a plain, priority-blind
         * FIFO device -- unblocking does not depend on priority at all). */
        while (!io_queue_vazia(blocked) && io_queue_proximo_tempo(blocked) <= current_time) {
            int unblocked_index = io_queue_desbloqueia_proximo(blocked);
            processes[unblocked_index].current_state = PROCESS_STATE_READY;
            ready_set_add(&ready, unblocked_index);
        }

        /* 3. Ready -> Running: dispatch the highest-priority ready process,
         * if any. Priority here is NON-PREEMPTIVE (process_model.md Section
         * 5): once dispatched, the process runs its full CPU burst to
         * completion -- it cannot be interrupted by a later, higher-priority
         * arrival, unlike Round Robin. */
        if (ready.count > 0) {
            int running_index = ready_set_pick_next(&ready, processes);
            Process *p = &processes[running_index];

            p->current_state = PROCESS_STATE_RUNNING;
            p->context_switches++;
            total_context_switches++;

            /* context switch overhead precedes execution, same convention
             * already used by round_robin.c and ajie.c */
            current_time += config.context_switch_cost;

            Burst *cpu_burst = &p->bursts[p->current_burst_index];
            current_time += cpu_burst->duration;
            p->current_burst_index++;

            if (p->current_burst_index >= p->num_bursts) {
                /* Running -> Finished: no next burst left */
                p->current_state = PROCESS_STATE_TERMINATED;
                p->completion_time = current_time;
                finished_count++;
            } else {
                /* Running -> Blocked: next burst is guaranteed to be I/O
                 * (process_model.md Section 4: sequences never have two
                 * consecutive I/O bursts) */
                Burst *io_burst = &p->bursts[p->current_burst_index];
                io_queue_bloqueia(blocked, processes, running_index,
                                   current_time, io_burst->duration);
                p->current_burst_index++;
            }

            continue; /* re-check arrivals/unblocks before picking again */
        }

        /* 4. CPU idle and nothing ready: jump to the next relevant event */
        int t_arrival = next_arrival_time(processes, process_count, arrived);
        int t_io = io_queue_vazia(blocked) ? -1 : io_queue_proximo_tempo(blocked);

        if (t_arrival == -1 && t_io == -1) {
            fprintf(stderr, "priority_run: stuck -- no future events but "
                             "processes remain unfinished.\n");
            break;
        }

        if (t_arrival == -1) {
            current_time = t_io;
        } else if (t_io == -1) {
            current_time = t_arrival;
        } else {
            current_time = (t_arrival < t_io) ? t_arrival : t_io;
        }
    }

    long total_ticks = current_time;

    if (out_metrics != NULL) {
        compute_priority_metrics(processes, process_count, total_context_switches,
                                  total_ticks, out_metrics);
    }

    free(arrived);
    io_queue_destroi(blocked);
    ready_set_free(&ready);

    return 0;
}

/* ---------------------------------------------------------------------
 * Wrapper matching the SchedulerFn contract in include/scheduler.h,
 * following the same pattern already used by run_ajie() in src/ajie.c.
 * --------------------------------------------------------------------- */
RunMetrics run_priority(const Process *processes, size_t n, const SimConfig *cfg) {
    RunMetrics metrics = {0.0, 0, 0.0, 0.0, 0};
    if (processes == NULL || n == 0 || cfg == NULL || n > (size_t)INT_MAX) {
        return metrics;
    }
    if (cfg->context_switch_cost < 0) {
        return metrics;
    }

    Process *copy = (Process *)calloc(n, sizeof(Process));
    if (copy == NULL) {
        return metrics;
    }
    for (size_t i = 0; i < n; i++) {
        copy[i] = processes[i];
        if (processes[i].num_bursts > 0 && processes[i].bursts != NULL) {
            size_t burst_bytes = (size_t)processes[i].num_bursts * sizeof(Burst);
            copy[i].bursts = (Burst *)malloc(burst_bytes);
            if (copy[i].bursts == NULL) {
                for (size_t j = 0; j < i; j++) free(copy[j].bursts);
                free(copy);
                return metrics;
            }
            memcpy(copy[i].bursts, processes[i].bursts, burst_bytes);
        } else {
            copy[i].bursts = NULL;
        }
    }

    PriorityConfig config;
    config.context_switch_cost = cfg->context_switch_cost;

    PriorityMetrics priority_metrics;
    int status = priority_run(copy, (int)n, config, &priority_metrics);
    if (status == 0) {
        metrics = compute_run_metrics(copy, n);
        metrics.ok = 1;
    }

    for (size_t i = 0; i < n; i++) {
        free(copy[i].bursts);
    }
    free(copy);
    return metrics;
}
