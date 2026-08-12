#include "process.h"
#include <stdio.h>
#include <stdlib.h>

void process_init(Process *p, int pid, int arrival_time, int priority, int num_bursts) {
    if (p == NULL) {
        return;
    }

    p->pid = pid;
    p->arrival_time = arrival_time;
    p->priority = priority;
    p->num_bursts = num_bursts;
    p->current_burst_index = 0;
    p->io_request_count = 0;
    p->current_state = PROCESS_STATE_NEW;
    p->completion_time = -1;
    p->context_switches = 0;

    if (num_bursts > 0) {
        p->bursts = (Burst *)calloc((size_t)num_bursts, sizeof(Burst));
        if (p->bursts == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for bursts of process %d\n", pid);
            p->num_bursts = 0;
        }
    } else {
        p->bursts = NULL;
    }
}

void process_destroy(Process *p) {
    if (p == NULL) {
        return;
    }

    if (p->bursts != NULL) {
        free(p->bursts);
        p->bursts = NULL;
    }

    p->num_bursts = 0;
    p->current_burst_index = 0;
    p->io_request_count = 0;
}

const char *process_state_to_string(ProcessState state) {
    switch (state) {
        case PROCESS_STATE_NEW:
            return "NEW";
        case PROCESS_STATE_READY:
            return "READY";
        case PROCESS_STATE_RUNNING:
            return "RUNNING";
        case PROCESS_STATE_BLOCKED:
            return "BLOCKED";
        case PROCESS_STATE_TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}

const char *burst_type_to_string(BurstType type) {
    switch (type) {
        case BURST_CPU:
            return "CPU";
        case BURST_IO:
            return "IO";
        default:
            return "UNKNOWN";
    }
}

void process_print(const Process *p) {
    if (p == NULL) {
        printf("Process (NULL)\n");
        return;
    }

    printf("Process PID: %d | Priority: %d | Arrival Time: %d | State: %s | Switches: %d | Completion: %d\n",
           p->pid, p->priority, p->arrival_time, process_state_to_string(p->current_state),
           p->context_switches, p->completion_time);
    printf("  Bursts (%d total, I/O count: %d, current index: %d):\n",
           p->num_bursts, p->io_request_count, p->current_burst_index);

    if (p->bursts != NULL) {
        for (int i = 0; i < p->num_bursts; i++) {
            printf("    [%d] Type: %-3s | Duration: %d | Time Left: %d\n",
                   i, burst_type_to_string(p->bursts[i].type),
                   p->bursts[i].duration, p->bursts[i].time_left);
        }
    }
}
