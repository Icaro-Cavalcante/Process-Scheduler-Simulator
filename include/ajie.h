/*
 * include/ajie.h
 *
 * AJIE (Aisha, Jackson, Icaro, Elilucio) Scheduling Algorithm.
 * Priority Aging Non-Preemptive Scheduler.
 *
 * Priority convention:
 *   1 = highest priority
 *   num_priority_levels = lowest priority
 *
 * Aging:
 *   waiting processes improve their effective priority by decreasing its
 *   numerical value (-1) at each scheduling decision, capped at 1.
 */

#ifndef AJIE_H
#define AJIE_H

#include "process.h"
#include "scheduler.h"

typedef struct {
    int context_switch_cost;   /* Switch-away cost in ticks; >= 0 */
    int num_priority_levels;   /* Runtime-configured levels; 1 = highest, L = lowest */
} AJIEConfig;

typedef struct {
    long total_context_switches;
    long total_ticks;             /* Makespan / final simulation time */
    double mean_turnaround;
    double mean_slowdown;
    double jain_slowdown_percent; /* Jain's index on slowdown (0-100%) */
} AJIEMetrics;

/*
 * Runs AJIE (Priority Aging Non-Preemptive) over the given process array.
 *
 * Parameters:
 *   processes     - array of Process structs; mutated during simulation
 *   process_count - number of processes
 *   config        - runtime configuration, including the priority-level count
 *                    and context-switch cost
 *   out_metrics   - optional output metrics
 *
 * Returns:
 *   0  on success
 *  -1  on invalid parameters or allocation failure
 */
int ajie_run(Process *processes,
             int process_count,
             AJIEConfig config,
             AJIEMetrics *out_metrics);

/*
 * Uniform wrapper matching SchedulerFn from include/scheduler.h.
 */
RunMetrics run_ajie(const Process *processes,
                    size_t n,
                    const SimConfig *cfg);

#endif /* AJIE_H */
