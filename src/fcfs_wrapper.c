#include "fcfs.h"
#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/*
 * Wrapper matching the SchedulerFn contract in include/scheduler.h,
 * following the same pattern already used by run_ajie() in src/ajie.c.
 *
 * fcfs_run() (src/fcfs.c) mutates its input array in place and returns
 * void -- no RunMetrics. This wrapper computes RunMetrics from the
 * mutated copy afterwards, the same way run_priority/run_ajie do for
 * their own algorithms.
 */
RunMetrics run_fcfs(const Process *processes, size_t n, const SimConfig *cfg) {
    RunMetrics metrics = {0.0, 0, 0.0, 0.0, 0};
    if (processes == NULL || n == 0 || cfg == NULL || n > (size_t)INT_MAX) {
        return metrics;
    }
    if (cfg->context_switch_cost < 0) {
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

    fcfs_run(copy, (int)n, cfg->context_switch_cost);

    metrics = compute_run_metrics(copy, n);
    metrics.ok = 1;

    for (size_t i = 0; i < n; i++) {
        free(copy[i].bursts);
    }
    free(copy);
    return metrics;
}