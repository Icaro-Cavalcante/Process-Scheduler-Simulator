#include "round_robin.h"
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/*
 * Wrapper matching the SchedulerFn contract in include/scheduler.h,
 * following the same pattern already used by run_ajie() in src/ajie.c.
 *
 * round_robin_run() (src/round_robin.c) already returns its own
 * RRMetrics struct with equivalent fields (total_context_switches,
 * total_ticks, mean_turnaround, jain_slowdown_percent), computed the
 * same way compute_run_metrics() computes them elsewhere in the
 * project. This wrapper only translates RRConfig/RRMetrics into
 * SimConfig/RunMetrics so experiment_runner.c's dispatch table
 * (SCHEDULER_DISPATCH in scripts/experiment_runner.c) has a matching
 * run_rr symbol to link against -- it does not modify round_robin.c.
 *
 * cfg->num_priority_levels is intentionally ignored: Round Robin does
 * not use priority at all (per Section 8 of the assignment, priority
 * differentiation is Priority Scheduling's and the custom algorithm's
 * job, not Round Robin's).
 */
RunMetrics run_rr(const Process *processes, size_t n, const SimConfig *cfg) {
    RunMetrics metrics = {0.0, 0, 0.0, 0.0, 0};
    if (processes == NULL || n == 0 || cfg == NULL || n > (size_t)INT_MAX) {
        return metrics;
    }
    if (cfg->quantum <= 0 || cfg->context_switch_cost < 0) {
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

    RRConfig rr_config;
    rr_config.quantum = cfg->quantum;
    rr_config.context_switch_cost = cfg->context_switch_cost;

    RRMetrics rr_metrics;
    int status = round_robin_run(copy, (int)n, rr_config, NULL, 0, NULL, &rr_metrics);

    if (status == 0) {
        metrics.turnaround_ms = rr_metrics.mean_turnaround;
        metrics.context_switches = rr_metrics.total_context_switches;
        metrics.jain_slowdown_pct = rr_metrics.jain_slowdown_percent;
        /* RRMetrics has no separate mean-slowdown field of its own, so
         * fall back to compute_run_metrics() over the final process
         * state to fill in `slowdown` and keep it consistent with how
         * every other algorithm's wrapper computes it. */
        RunMetrics recomputed = compute_run_metrics(copy, n);
        metrics.slowdown = recomputed.slowdown;
        metrics.ok = 1;
    }

    for (size_t i = 0; i < n; i++) {
        free(copy[i].bursts);
    }
    free(copy);
    return metrics;
}
