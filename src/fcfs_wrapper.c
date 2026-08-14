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
 * void -- no RunMetrics, no context_switch_cost parameter. This
 * wrapper does not modify fcfs.c; it only works around fcfs_run's
 * existing signature so experiment_runner.c's dispatch table
 * (SCHEDULER_DISPATCH in scripts/experiment_runner.c) has a matching
 * run_fcfs symbol to link against, the same way run_priority/run_ajie
 * already provide one for their own algorithms.
 *
 * KNOWN DIVERGENCE (not fixed here, since it would require editing
 * fcfs.c, out of scope for this file): fcfs_run() does not apply
 * context_switch_cost to the simulated clock at all -- it only
 * increments each process's context_switches counter without adding
 * any ticks for it. That means FCFS results are not directly
 * comparable to Round Robin / Priority / AJIE on turnaround and
 * total_ticks whenever context_switch_cost > 0, even though the
 * context_switches count itself is still meaningful. This wrapper
 * intentionally still accepts cfg->context_switch_cost (validating it
 * is >= 0, mirroring the other wrappers) so the SchedulerFn call site
 * in experiment_runner.c stays uniform, but the value is not passed
 * to fcfs_run because fcfs_run has no parameter to receive it.
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

    fcfs_run(copy, (int)n);

    metrics = compute_run_metrics(copy, n);
    metrics.ok = 1;

    for (size_t i = 0; i < n; i++) {
        free(copy[i].bursts);
    }
    free(copy);
    return metrics;
}
