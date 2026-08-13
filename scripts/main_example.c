/*
 * src/main_example.c
 *
 * Example reference for how src/main.c should instantiate ExperimentPlan
 * and invoke run_all_experiments().
 * Aligned with repository structures and configuration conventions.
 */

#include <stdio.h>
#include "experiment_runner.h"

int main(void) {
    ExperimentPlan plan = {
        .first_seed = 1,
        .seed_count = 100,          /* assignment minimum (100 seeds) */
        .process_count = 1000,      /* assignment minimum (1000 processes) */
        .sim_config = {
            .context_switch_cost = 2,  /* ticks per context switch (> 0) */
            .quantum = 4,              /* Round Robin quantum (ticks) */
            .num_priority_levels = 10  /* priority levels range [1..10] */
        },
        .output_csv_path = "results/scheduling_experiments.csv",
        .failures_log_path = "results/failed_runs.log"
    };

    ExperimentSummary summary = run_all_experiments(&plan);

    printf("Runs attempted: %ld\n", summary.total_runs_attempted);
    printf("Runs written:   %ld\n", summary.total_runs_written);
    printf("Runs failed:    %ld\n", summary.total_runs_failed);

    if (summary.total_runs_written < 4L * 4L * plan.seed_count) {
        fprintf(stderr,
                "warning: fewer than the expected %d rows were written — "
                "check results/failed_runs.log\n",
                4 * 4 * plan.seed_count);
        return 1;
    }

    return 0;
}
