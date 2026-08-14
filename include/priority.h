#ifndef PRIORITY_H
#define PRIORITY_H

#include "process.h"

/*
 * ELI-06 -- Non-preemptive Priority scheduling.
 *
 * Implements the algorithm required by Section 8 of the assignment
 * ("Priority Scheduling, non-preemptive"), using the process model and
 * priority convention already fixed by the rest of the project:
 *
 *   - process_model.md, Section 3: LOWER priority value = HIGHER
 *     priority (1 = highest, 10 = lowest). The ready process with the
 *     smallest `priority` is always dispatched next.
 *   - process_model.md, Section 5 ("Important note for non-preemptive
 *     algorithms"): once a process enters RUNNING, it only leaves that
 *     state via BLOCKED (I/O) or TERMINATED -- never back to READY.
 *     A process that starts running always runs its full CPU burst to
 *     completion, even if a higher-priority process arrives meanwhile.
 *   - io_modeling.md: a single I/O device, no parallelism, FIFO
 *     service order, blocking time equal to the I/O burst's duration.
 *     Reused here via include/io_queue.h instead of a separate ad-hoc
 *     array (unlike src/fcfs.c, which keeps its own local list).
 *
 * TIE-BREAKING: the assignment's modeling docs note that priority-tie
 * handling is algorithm-specific and left for separate documentation
 * that was not available when this file was written. This
 * implementation adopts arrival-time order (earliest arrival first)
 * as the tie-breaker among ready processes with equal priority --
 * the same criterion FCFS already uses -- since it is the simplest
 * choice consistent with the rest of the project and requires no
 * extra process attribute. If the team formalizes a different
 * tie-breaking rule later, only priority_pick_next() below needs to
 * change.
 */

typedef struct {
    int context_switch_cost; /* Ticks per dispatch. Must be >= 0; > 0 is
                                 required for the assignment's main
                                 experiments (see docs/context_switch.md). */
} PriorityConfig;

typedef struct {
    long total_context_switches;
    long total_ticks;             /* final simulation clock value (makespan) */
    double mean_turnaround;
    double jain_slowdown_percent; /* same formula used by round_robin.c /
                                      ajie.c, per assignment Section 9 */
} PriorityMetrics;

/*
 * Runs non-preemptive Priority scheduling over `processes` (mutates
 * each process's current_state, completion_time, context_switches,
 * and current_burst_index in place -- same contract as fcfs_run and
 * round_robin_run).
 *
 * Returns 0 on success, -1 on invalid arguments (NULL processes,
 * process_count <= 0, context_switch_cost < 0).
 */
int priority_run(Process *processes, int process_count, PriorityConfig config,
                  PriorityMetrics *out_metrics);

#endif /* PRIORITY_H */
