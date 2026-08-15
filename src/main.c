/*
 * src/main.c
 *
 * Entry point: instantiates the ExperimentPlan required by the
 * assignment (100 seeds x 1000 processes x FCFS/RR/Priority/AJIE) and
 * invokes run_all_experiments(), writing the consolidated CSV and the
 * failures log described in docs/csv_format.md.
 */

#include <stdio.h>
#include <sys/stat.h>
#include "experiment_runner.h"

int main(void) {
    /* results/ precisa existir antes da escrita do CSV e do log de
     * falhas; cria se ainda nao existir (silencioso se ja existir). */
    mkdir("results", 0755);

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