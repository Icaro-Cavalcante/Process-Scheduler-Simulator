#include "../include/generator.h"
#include "../include/rng.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Parameter matrix transcribed from docs/cenarios.md Section 4.
 * Keep this table and that document in lockstep — do not edit one
 * without the other (see the doc's Section 7 review checklist).
 */
static const ScenarioParams SCENARIOS[SCENARIO_COUNT] = {
    /* SCENARIO_BALANCED_RANDOM — Section 3.1 */
    {
        .id = "balanced_random",
        .label = "Balanced Random (mixed CPU & I/O)",
        .cpu_burst_min = 5,   .cpu_burst_max = 50,
        .io_count_min = 1,    .io_count_max = 5,
        .io_burst_min = 10,   .io_burst_max = 30,
        .mean_interarrival = 5.0,
        .priority_unbalanced = false,
        .priority_min = 1, .priority_max = 10,
        .high_priority_fraction = 0.0,
        .priority_high_min = 0, .priority_high_max = 0,
        .priority_low_min = 0, .priority_low_max = 0,
    },
    /* SCENARIO_IO_BOUND — Section 3.2 */
    {
        .id = "io_bound",
        .label = "I/O-Bound (short CPU bursts, frequent I/O)",
        .cpu_burst_min = 1,   .cpu_burst_max = 8,
        .io_count_min = 6,    .io_count_max = 15,
        .io_burst_min = 20,   .io_burst_max = 60,
        .mean_interarrival = 3.0,
        .priority_unbalanced = false,
        .priority_min = 1, .priority_max = 10,
        .high_priority_fraction = 0.0,
        .priority_high_min = 0, .priority_high_max = 0,
        .priority_low_min = 0, .priority_low_max = 0,
    },
    /* SCENARIO_CPU_BOUND — Section 3.3 */
    {
        .id = "cpu_bound",
        .label = "CPU-Bound (long CPU bursts, infrequent I/O)",
        .cpu_burst_min = 40,  .cpu_burst_max = 200,
        .io_count_min = 0,    .io_count_max = 2,
        .io_burst_min = 5,    .io_burst_max = 15,
        .mean_interarrival = 12.0,
        .priority_unbalanced = false,
        .priority_min = 1, .priority_max = 10,
        .high_priority_fraction = 0.0,
        .priority_high_min = 0, .priority_high_max = 0,
        .priority_low_min = 0, .priority_low_max = 0,
    },
    /* SCENARIO_PRIORITY_UNBALANCED — Section 3.4
     * Doc says "CPU & I/O Burst Lengths: balanced mixed distribution" for
     * this scenario, i.e. reuse Scenario 1's burst ranges (Section 4's
     * matrix confirms this — the Scenario 4 column for CPU Burst/I-O
     * Request Count/I-O Duration equals Scenario 1's). */
    {
        .id = "priority_unbalanced",
        .label = "Priority Unbalanced (85% high / 15% low priority)",
        .cpu_burst_min = 5,   .cpu_burst_max = 50,
        .io_count_min = 1,    .io_count_max = 5,
        .io_burst_min = 10,   .io_burst_max = 30,
        .mean_interarrival = 4.0,
        .priority_unbalanced = true,
        .priority_min = 0, .priority_max = 0, /* unused in this mode */
        .high_priority_fraction = 0.85,
        .priority_high_min = 1, .priority_high_max = 3,
        .priority_low_min = 8, .priority_low_max = 10,
    },
};

const ScenarioParams *scenario_get_params(ScenarioType scenario) {
    if (scenario < 0 || scenario >= SCENARIO_COUNT) return NULL;
    return &SCENARIOS[scenario];
}

/* Samples a single process's priority per the scenario's rules
 * (scenarios.md Section 2.1/3, process_model.md Section 3: lower = higher priority). */
static int sample_priority(Rng *rng, const ScenarioParams *sp) {
    if (sp->priority_unbalanced) {
        if (rng_bernoulli(rng, sp->high_priority_fraction)) {
            return rng_uniform_int(rng, sp->priority_high_min, sp->priority_high_max);
        }
        return rng_uniform_int(rng, sp->priority_low_min, sp->priority_low_max);
    }
    return rng_uniform_int(rng, sp->priority_min, sp->priority_max);
}

/* Fills in the CPU/I-O burst list for one process, per process_model.md
 * Section 4: alternating CPU -> I/O -> ... -> CPU, always starting and
 * ending on a CPU burst, never two consecutive I/O bursts. */
static void fill_bursts(Rng *rng, const ScenarioParams *sp, Process *p, int io_count) {
    int num_bursts = 2 * io_count + 1;
    process_init(p, p->pid, p->arrival_time, p->priority, num_bursts);
    p->io_request_count = io_count;

    for (int i = 0; i < num_bursts; i++) {
        if (i % 2 == 0) {
            p->bursts[i].type = BURST_CPU;
            p->bursts[i].duration = rng_uniform_int(rng, sp->cpu_burst_min, sp->cpu_burst_max);
        } else {
            p->bursts[i].type = BURST_IO;
            p->bursts[i].duration = rng_uniform_int(rng, sp->io_burst_min, sp->io_burst_max);
        }
        p->bursts[i].time_left = p->bursts[i].duration;
    }
}

Process *generate_workload(ScenarioType scenario, uint64_t seed, int process_count) {
    const ScenarioParams *sp = scenario_get_params(scenario);
    if (sp == NULL || process_count <= 0) return NULL;

    Process *processes = (Process *)calloc((size_t)process_count, sizeof(Process));
    if (processes == NULL) return NULL;

    Rng rng;
    rng_seed(&rng, seed);

    double clock = 0.0;
    for (int i = 0; i < process_count; i++) {
        /* --- draw order per process is fixed: arrival, priority, io_count,
         * then each burst duration in sequence. Keeping this order stable
         * is what makes the whole generator reproducible for a given seed. --- */

        double delta = rng_exponential(&rng, sp->mean_interarrival);
        clock += delta;
        int arrival_time = (int)(clock + 0.5); /* round to nearest tick */

        int priority = sample_priority(&rng, sp);

        int io_count = rng_uniform_int(&rng, sp->io_count_min, sp->io_count_max);

        Process *p = &processes[i];
        p->pid = i;
        p->arrival_time = arrival_time;
        p->priority = priority;
        p->current_state = PROCESS_STATE_NEW;

        fill_bursts(&rng, sp, p, io_count);
        /* process_init() re-set arrival_time/priority already; keep them
         * consistent in case process_init()'s own defaults ever change. */
        p->arrival_time = arrival_time;
        p->priority = priority;
    }

    return processes;
}

void generator_free_workload(Process *processes, int process_count) {
    if (processes == NULL) return;
    for (int i = 0; i < process_count; i++) {
        process_destroy(&processes[i]);
    }
    free(processes);
}
