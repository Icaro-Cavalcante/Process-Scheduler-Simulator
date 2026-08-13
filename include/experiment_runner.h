/*
 * src/experiment_runner.h                                        (ICR-10)
 *
 * Executes every (algorithm x scenario x seed) combination required by
 * the assignment and writes the consolidated results CSV described in
 * docs/csv_format.md.
 * Uses repository structures defined in include/process.h and include/generator.h.
 */

#ifndef EXPERIMENT_RUNNER_H
#define EXPERIMENT_RUNNER_H

#include <stdint.h>
#include "scheduler.h"

typedef struct {
    uint64_t first_seed;        /* inclusive start seed, e.g. 1 */
    int seed_count;             /* seeds per scenario, e.g. 100 (assignment minimum) */
    int process_count;          /* processes per run, e.g. 1000 (assignment minimum) */
    SimConfig sim_config;       /* simulation configuration (context_switch_cost, quantum, etc.) */
    const char *output_csv_path;   /* e.g. "results/scheduling_experiments.csv" */
    const char *failures_log_path; /* e.g. "results/failed_runs.log" */
} ExperimentPlan;

typedef struct {
    long total_runs_attempted; /* seed_count * SCENARIO_COUNT * ALG_COUNT */
    long total_runs_written;   /* rows actually written to the CSV */
    long total_runs_failed;    /* workload-generation or algorithm failures */
} ExperimentSummary;

/*
 * Runs every combination defined by `plan`:
 *
 *   for each scenario in SCENARIO_COUNT:
 *     for each seed in [first_seed, first_seed + seed_count):
 *       generate the workload ONCE for (scenario, seed)
 *       for each algorithm in ALG_COUNT:
 *         run it against a fresh copy of that same workload
 *         write one row to output_csv_path, OR log a failure and skip it
 *
 * Returns run counters for reporting/validation.
 */
ExperimentSummary run_all_experiments(const ExperimentPlan *plan);

#endif /* EXPERIMENT_RUNNER_H */
