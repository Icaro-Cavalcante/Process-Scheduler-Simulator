/*
 * include/scheduler.h
 *
 * Shared data types and integration contract used across the simulator.
 * Integrates directly with the repository's base data structures (include/process.h
 * and include/generator.h).
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include <stdint.h>
#include "process.h"
#include "generator.h"

/* ---- Algorithms and scenarios -------------------------------------- */

typedef enum {
    ALG_FCFS = 0,
    ALG_RR,
    ALG_PRIORITY,
    ALG_AJIE,
    ALG_COUNT /* sentinel: number of algorithms */
} AlgorithmId;

/* Alias ScenarioType from generator.h for convenience */
typedef ScenarioType ScenarioId;

/* Labels MUST match docs/csv_format.md and src/generator.c exactly */
extern const char *ALGORITHM_LABEL[ALG_COUNT];
extern const char *SCENARIO_LABEL[SCENARIO_COUNT];

/* ---- Simulation configuration ---------------------------------------- */

typedef struct {
    int context_switch_cost; /* Ticks per context switch (must match RRConfig.context_switch_cost) */
    int quantum;             /* Round Robin quantum in ticks */
    int num_priority_levels; /* Number of priority levels (e.g., 10) */
} SimConfig;

/* ---- Result of a single (algorithm, scenario, seed) run --------------- */

typedef struct {
    double turnaround_ms;     /* Average turnaround time across processes */
    long   context_switches;  /* Total number of context switches in the run */
    double slowdown;          /* Average slowdown across processes */
    double jain_slowdown_pct; /* Jain's fairness index applied to slowdown (0-100%) */
    int    ok;                /* 1 on success, 0 on failure */
} RunMetrics;

/*
 * Contract every algorithm module wrapper must implement:
 *
 *   RunMetrics run_fcfs    (const Process *processes, size_t n, const SimConfig *cfg);
 *   RunMetrics run_rr      (const Process *processes, size_t n, const SimConfig *cfg);
 *   RunMetrics run_priority(const Process *processes, size_t n, const SimConfig *cfg);
 *   RunMetrics run_ajie    (const Process *processes, size_t n, const SimConfig *cfg);
 */
typedef RunMetrics (*SchedulerFn)(const Process *processes, size_t n, const SimConfig *cfg);

/* Helper function to compute RunMetrics from an array of completed processes */
RunMetrics compute_run_metrics(const Process *processes, size_t n);

#endif /* SCHEDULER_H */
