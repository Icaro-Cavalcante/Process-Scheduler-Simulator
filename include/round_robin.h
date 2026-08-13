#ifndef ROUND_ROBIN_H
#define ROUND_ROBIN_H

#include "process.h"

typedef struct {
    int quantum;               /* Ticks. Must be > 0 (ASH-06's job to pick
                                   the experimental value; this module just
                                   enforces and applies it). */
    int context_switch_cost;   /* Ticks. Must be >= 0. > 0 required for the
                                   project's main experiments; 0 is allowed
                                   only for the secondary sensitivity
                                   analysis described in troca_contexto.md
                                   Section 6. */
} RRConfig;

/* Why a given execution slice ended. */
typedef enum {
    RR_SLICE_PREEMPTED,   /* quantum expired before the burst finished */
    RR_SLICE_BLOCKED,     /* burst finished and the process issued I/O */
    RR_SLICE_TERMINATED   /* burst finished and it was the process's last */
} RRSliceReason;

/* One contiguous stretch of real CPU execution for one process (context
 * switch overhead ticks are NOT part of the slice — they precede it).
 * Optional instrumentation, used to verify the quantum/preemption
 * acceptance criteria; production runs can pass slice_log = NULL to skip
 * the bookkeeping entirely. */
typedef struct {
    int pid;
    int start_tick;        /* inclusive */
    int end_tick;           /* exclusive; end_tick - start_tick <= quantum, always */
    RRSliceReason reason;
} RRExecutionSlice;

typedef struct {
    long total_context_switches;
    long total_ticks;             /* final simulation clock value (makespan) */
    double mean_turnaround;
    double jain_slowdown_percent; /* per assignment PDF Section 9 / troca_contexto.md Section 5 */
} RRMetrics;

/*
 * Runs Round Robin over `processes` (mutates each process's current_state,
 * completion_time, context_switches, and every burst's time_left in place
 * — same contract ICR-04's struct and process_model.md Section 5 define).
 *
 * `slice_log` / `slice_log_capacity` / `slice_log_count`: optional. If
 * slice_log is non-NULL, up to slice_log_capacity execution slices are
 * recorded (oldest-first) and *slice_log_count is set to how many were
 * written. Pass NULL/0/NULL to skip tracing.
 *
 * Returns 0 on success, -1 on invalid arguments (NULL processes,
 * process_count <= 0, quantum <= 0, context_switch_cost < 0).
 */
int round_robin_run(Process *processes, int process_count, RRConfig config,
                     RRExecutionSlice *slice_log, int slice_log_capacity, int *slice_log_count,
                     RRMetrics *out_metrics);

#endif
