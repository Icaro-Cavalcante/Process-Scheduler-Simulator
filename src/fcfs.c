#include <stdio.h>
#include <stdbool.h>
#include "fcfs.h"
#include "queue.h"

// A process currently doing I/O, and when its I/O burst will finish.
typedef struct {
    Process *process;
    int io_completion_time;
} BlockedEntry;

// Smallest arrival_time among processes not yet added to the ready queue.
// Returns -1 if all processes have already arrived.
static int next_arrival_time(Process processes[], int n, bool added[]) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (!added[i]) {
            if (best == -1 || processes[i].arrival_time < best) {
                best = processes[i].arrival_time;
            }
        }
    }
    return best;
}

// Smallest io_completion_time among currently blocked processes.
// Returns -1 if nothing is blocked.
static int next_io_completion(BlockedEntry blocked[], int blocked_count) {
    int best = -1;
    for (int i = 0; i < blocked_count; i++) {
        if (best == -1 || blocked[i].io_completion_time < best) {
            best = blocked[i].io_completion_time;
        }
    }
    return best;
}

void fcfs_run(Process processes[], int n) {
    Queue ready_queue;
    queue_init(&ready_queue);

    BlockedEntry blocked[n];
    int blocked_count = 0;

    bool added[n];
    for (int i = 0; i < n; i++) {
        added[i] = false;

        // current_burst_index, current_state, context_switches, completion_time
        // are assumed already set by process_init(); we don't touch io_request_count
        // here because per Section 2 of the doc it's fixed metadata (count of I/O
        // bursts in the process's burst list), computed at generation time —
        // not something the scheduler increments as it runs.
    }

    int current_time = 0;
    int finished_count = 0;

    while (finished_count < n) {

        // 1. New -> Ready: add every process that has arrived by current_time
        for (int i = 0; i < n; i++) {
            if (!added[i] && processes[i].arrival_time <= current_time) {
                processes[i].current_state = PROCESS_STATE_READY;
                queue_enqueue(&ready_queue, &processes[i]);
                added[i] = true;
            }
        }

        // 2. Blocked -> Ready: move any process whose I/O just finished
        for (int i = 0; i < blocked_count; /* conditional increment below */) {
            if (blocked[i].io_completion_time <= current_time) {
                Process *p = blocked[i].process;
                p->current_state = PROCESS_STATE_READY;
                queue_enqueue(&ready_queue, p);

                // remove this entry (swap with last, O(1))
                blocked[i] = blocked[blocked_count - 1];
                blocked_count--;
            } else {
                i++;
            }
        }

        // 3. Ready -> Running: if CPU is free and someone is ready, dispatch it.
        //    FCFS is non-preemptive, so once picked it runs until Blocked or Finished
        //    (per the doc's note: "Running -> Ready due to preemption simply does not occur").
        if (!queue_is_empty(&ready_queue)) {
            Process *p = queue_dequeue(&ready_queue);
            p->current_state = PROCESS_STATE_RUNNING;
            p->context_switches++;   // counts this dispatch as a context switch (JCK-01 defines the exact cost/rules)

            Burst *cpu_burst = &p->bursts[p->current_burst_index];
            // Per Section 4: the burst at this index is guaranteed to be BURST_CPU here,
            // since running only starts right after New->Ready or Blocked->Ready,
            // and sequences never have two consecutive I/O bursts.

            current_time += cpu_burst->duration;
            p->current_burst_index++;

            if (p->current_burst_index >= p->num_bursts) {
                // Running -> Finished: no next burst, so this was the last CPU burst
                p->current_state = PROCESS_STATE_TERMINATED;
                p->completion_time = current_time;
                finished_count++;

                printf("Process %d terminated at time %d\n", p->pid, current_time);
            } else {
                // Running -> Blocked: next burst must be I/O (Section 4 guarantees this)
                Burst *io_burst = &p->bursts[p->current_burst_index];
                p->current_state = PROCESS_STATE_BLOCKED;

                blocked[blocked_count].process = p;
                blocked[blocked_count].io_completion_time = current_time + io_burst->duration;
                blocked_count++;

                p->current_burst_index++; // this I/O burst is "consumed" once it completes below

                printf("Process %d blocked for I/O at time %d (until %d)\n",
                       p->pid, current_time, current_time + io_burst->duration);
            }

            continue; // re-check arrivals/I-O completions before picking the next process
        }

        // 4. CPU idle and ready_queue empty: jump to the next event in time
        int t_arrival = next_arrival_time(processes, n, added);
        int t_io = next_io_completion(blocked, blocked_count);

        if (t_arrival == -1 && t_io == -1) {
            // Should not happen while finished_count < n; guards against an infinite loop
            fprintf(stderr, "Scheduler stuck: no future events but processes unfinished.\n");
            break;
        }

        if (t_arrival == -1) {
            current_time = t_io;
        } else if (t_io == -1) {
            current_time = t_arrival;
        } else {
            current_time = (t_arrival < t_io) ? t_arrival : t_io;
        }
    }

    queue_destroy(&ready_queue);
}