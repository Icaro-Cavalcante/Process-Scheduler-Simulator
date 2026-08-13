#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h" /* same directory (include/), no path prefix needed */

/* Task: ICR-05 — implementation of the seeded workload generator.
 * Parameters below are transcribed 1:1 from docs/cenarios.md (ASH-03/JCK-02)
 * Sections 3-4. If those docs change, update this table to match — it must
 * not diverge, per the doc's own review checklist (Section 7). */

typedef enum {
    SCENARIO_BALANCED_RANDOM = 0,   /* Section 3.1 */
    SCENARIO_IO_BOUND        = 1,   /* Section 3.2 */
    SCENARIO_CPU_BOUND       = 2,   /* Section 3.3 */
    SCENARIO_PRIORITY_UNBALANCED = 3, /* Section 3.4 */
    SCENARIO_COUNT
} ScenarioType;

typedef struct {
    const char *id;              /* machine-readable name, e.g. "balanced_random" */
    const char *label;           /* human-readable description */

    int cpu_burst_min, cpu_burst_max;   /* CPU burst length, ticks */
    int io_count_min, io_count_max;     /* number of I/O bursts per process */
    int io_burst_min, io_burst_max;     /* I/O burst duration, ticks */
    double mean_interarrival;           /* mean Delta t for the arrival model */

    /* Priority model. Scenarios 1-3 use a flat uniform range.
     * Scenario 4 overrides with an 85/15 high/low split (see priority_unbalanced). */
    bool priority_unbalanced;
    int priority_min, priority_max;                 /* used when !priority_unbalanced */
    double high_priority_fraction;                  /* used when priority_unbalanced (0.85) */
    int priority_high_min, priority_high_max;        /* e.g. [1,3] */
    int priority_low_min, priority_low_max;          /* e.g. [8,10] */
} ScenarioParams;

/* Returns the (read-only, statically defined) parameter set for a scenario. */
const ScenarioParams *scenario_get_params(ScenarioType scenario);

/*
 * Generates `process_count` processes for the given scenario and seed.
 *
 * Reproducibility contract (scenarios.md Section 2.3): calling this function
 * again with the same (scenario, seed, process_count) MUST yield an
 * identical array — arrival times, priorities, and full burst lists byte
 * for byte. This is what makes the same workload usable, unmodified, across
 * FCFS / Round Robin / Priority / the custom algorithm: generate it once per
 * (scenario, seed) and feed the identical array into each algorithm's run.
 *
 * Returns a heap-allocated array of `process_count` Process structs, or
 * NULL on allocation failure. Caller owns the result: call process_destroy()
 * on each element, then free() the array (see generator_free_workload()).
 */
Process *generate_workload(ScenarioType scenario, uint64_t seed, int process_count);

/* Convenience: destroys every process in the array and frees the array itself. */
void generator_free_workload(Process *processes, int process_count);

#endif
