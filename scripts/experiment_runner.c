/*
 * src/experiment_runner.c                                       
 *
 * Executes every (algorithm x scenario x seed) combination required by
 * the assignment and writes the consolidated results CSV described in
 * docs/csv_format.md.
 * Integrates directly with repository structures (include/process.h and include/generator.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "scheduler.h"
#include "experiment_runner.h"

/* Wrappers provided by src/fcfs.c, src/round_robin.c, src/priority.c, src/ajie.c */
extern RunMetrics run_fcfs(const Process *processes, size_t n, const SimConfig *cfg);
extern RunMetrics run_rr(const Process *processes, size_t n, const SimConfig *cfg);
extern RunMetrics run_priority(const Process *processes, size_t n, const SimConfig *cfg);
extern RunMetrics run_ajie(const Process *processes, size_t n, const SimConfig *cfg);

const char *ALGORITHM_LABEL[ALG_COUNT] = {
    "FCFS", "RR", "Priority", "AJIE"
};

const char *SCENARIO_LABEL[SCENARIO_COUNT] = {
    "balanced_random", "io_bound", "cpu_bound", "priority_unbalanced"
};

static const SchedulerFn SCHEDULER_DISPATCH[ALG_COUNT] = {
    run_fcfs,
    run_rr,
    run_priority,
    run_ajie
};

/* ---- metric calculation helper --------------------------------------- */

RunMetrics compute_run_metrics(const Process *processes, size_t n) {
    RunMetrics m = {0.0, 0, 0.0, 0.0, 1};
    if (!processes || n == 0) {
        m.ok = 0;
        return m;
    }

    double sum_turnaround = 0.0;
    double sum_slowdown = 0.0;
    double sum_sq_slowdown = 0.0;
    long total_cs = 0;

    for (size_t i = 0; i < n; i++) {
        const Process *p = &processes[i];
        int turnaround = p->completion_time - p->arrival_time;
        sum_turnaround += turnaround;
        total_cs += p->context_switches;

        int ideal_min = 0;
        for (int b = 0; b < p->num_bursts; b++) {
            ideal_min += p->bursts[b].duration;
        }

        double p_slowdown = (ideal_min > 0) ? ((double)turnaround / ideal_min) : 1.0;
        sum_slowdown += p_slowdown;
        sum_sq_slowdown += (p_slowdown * p_slowdown);
    }

    m.turnaround_ms = sum_turnaround / (double)n;
    m.context_switches = total_cs;
    m.slowdown = sum_slowdown / (double)n;

    if (sum_sq_slowdown > 0.0) {
        double jain = (sum_slowdown * sum_slowdown) / ((double)n * sum_sq_slowdown);
        m.jain_slowdown_pct = jain * 100.0;
    } else {
        m.jain_slowdown_pct = 100.0;
    }

    return m;
}

/* ---- internal helpers ------------------------------------------------ */

static Process *duplicate_workload(const Process *src, int n) {
    if (!src || n <= 0) return NULL;
    Process *dst = (Process *)calloc((size_t)n, sizeof(Process));
    if (!dst) return NULL;
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
        if (src[i].num_bursts > 0 && src[i].bursts) {
            dst[i].bursts = (Burst *)malloc((size_t)src[i].num_bursts * sizeof(Burst));
            if (!dst[i].bursts) {
                for (int j = 0; j < i; j++) free(dst[j].bursts);
                free(dst);
                return NULL;
            }
            memcpy(dst[i].bursts, src[i].bursts, (size_t)src[i].num_bursts * sizeof(Burst));
        } else {
            dst[i].bursts = NULL;
        }
    }
    return dst;
}

static void free_duplicated_workload(Process *p, int n) {
    if (!p) return;
    for (int i = 0; i < n; i++) {
        if (p[i].bursts) free(p[i].bursts);
    }
    free(p);
}

static FILE *open_csv_with_header(const char *path) {
    FILE *f = fopen(path, "w");
    if (f == NULL) return NULL;
    fprintf(f, "algorithm,scenario,seed,turnaround,context_switches,slowdown,jain_slowdown\n");
    return f;
}

static void write_csv_row(FILE *f, AlgorithmId alg, ScenarioId scen, uint64_t seed,
                           const RunMetrics *m) {
    fprintf(f, "%s,%s,%llu,%.2f,%ld,%.3f,%.2f\n",
            ALGORITHM_LABEL[alg], SCENARIO_LABEL[scen], (unsigned long long)seed,
            m->turnaround_ms, m->context_switches, m->slowdown, m->jain_slowdown_pct);
}

static void log_failure(FILE *log, const char *reason, ScenarioId scen, uint64_t seed,
                         AlgorithmId alg) {
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));

    if (log == NULL) return;
    if (alg == ALG_COUNT) {
        fprintf(log, "[%s] scenario=%s seed=%llu algorithm=- reason=%s\n",
                timestamp, SCENARIO_LABEL[scen], (unsigned long long)seed, reason);
    } else {
        fprintf(log, "[%s] scenario=%s seed=%llu algorithm=%s reason=%s\n",
                timestamp, SCENARIO_LABEL[scen], (unsigned long long)seed, ALGORITHM_LABEL[alg], reason);
    }
    fflush(log);
}

/* ---- public entry point ----------------------------------------------- */

ExperimentSummary run_all_experiments(const ExperimentPlan *plan) {
    ExperimentSummary summary = {0, 0, 0};

    FILE *csv = open_csv_with_header(plan->output_csv_path);
    if (csv == NULL) {
        fprintf(stderr, "experiment_runner: could not open output CSV '%s'\n",
                plan->output_csv_path);
        return summary;
    }

    FILE *log = fopen(plan->failures_log_path, "w");
    if (log == NULL) {
        fprintf(stderr,
                "experiment_runner: warning — could not open failures log '%s'\n",
                plan->failures_log_path);
    }

    for (int scen = 0; scen < SCENARIO_COUNT; scen++) {
        for (uint64_t seed = plan->first_seed; seed < plan->first_seed + (uint64_t)plan->seed_count; seed++) {

            /* Use repository's generate_workload signature */
            Process *workload = generate_workload((ScenarioType)scen, seed, plan->process_count);

            if (workload == NULL) {
                log_failure(log, "workload_generation_failed", (ScenarioId)scen, seed, ALG_COUNT);
                summary.total_runs_attempted += ALG_COUNT;
                summary.total_runs_failed += ALG_COUNT;
                continue;
            }

            for (int alg = 0; alg < ALG_COUNT; alg++) {
                summary.total_runs_attempted++;

                Process *workload_copy = duplicate_workload(workload, plan->process_count);
                if (workload_copy == NULL) {
                    log_failure(log, "workload_copy_failed", (ScenarioId)scen, seed, (AlgorithmId)alg);
                    summary.total_runs_failed++;
                    continue;
                }

                RunMetrics result = SCHEDULER_DISPATCH[alg](
                    workload_copy, (size_t)plan->process_count, &plan->sim_config);

                if (!result.ok) {
                    log_failure(log, "scheduler_returned_error", (ScenarioId)scen, seed,
                                (AlgorithmId)alg);
                    summary.total_runs_failed++;
                    free_duplicated_workload(workload_copy, plan->process_count);
                    continue;
                }

                write_csv_row(csv, (AlgorithmId)alg, (ScenarioId)scen, seed, &result);
                summary.total_runs_written++;

                free_duplicated_workload(workload_copy, plan->process_count);
            }

            generator_free_workload(workload, plan->process_count);
        }
    }

    fclose(csv);
    if (log != NULL) {
        fclose(log);
    }

    return summary;
}
