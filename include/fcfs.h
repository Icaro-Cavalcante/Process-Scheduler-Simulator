#ifndef FCFS_H
#define FCFS_H

#include "process.h"

// Runs non-preemptive FCFS scheduling over the given array of processes,
// following the state model in docs/process_model.md (Section 5).
void fcfs_run(Process processes[], int n, int context_switch_cost);

#endif