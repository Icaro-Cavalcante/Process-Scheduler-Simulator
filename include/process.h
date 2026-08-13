#ifndef PROCESS_H
#define PROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Mandatory states per Section 5 of process_model.md (ASH-01)
typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED   // "finished" in the doc
} ProcessState;

// Burst types per Section 4: sequence always starts and ends with CPU,
// never two consecutive I/O bursts.
typedef enum {
    BURST_CPU,
    BURST_IO
} BurstType;

typedef struct {
    BurstType type;
    int duration;      // total duration of this burst (time units)
    int time_left;     // remaining time; used by preemptive algorithms (Round Robin, etc.)
} Burst;

typedef struct {
    int pid;                    // unique id, sequential (doc: "pid")

    int arrival_time;           // doc: "tempo_chegada" — instant of new -> ready transition
    int priority;                // doc: "prioridade" — LOWER value = HIGHER priority (1 = highest, 10 = lowest, per Section 3)

    Burst *bursts;               // doc: "rajadas" — dynamically allocated, size = num_bursts
    int num_bursts;              // NOT explicit in the doc's attribute table, but required to know
                                  // where the burst list ends (see Section 5, Running -> Finished).
                                  // Flag this as a divergence to reconcile in ASH-05.
    int current_burst_index;     // doc: "indice_rajada_atual"

    int io_request_count;        // doc: "num_requisicoes_io" — count of I/O-type bursts in `bursts`

    ProcessState current_state;  // doc: "estado"
    int completion_time;         // doc: "tempo_termino" — set when reaching TERMINATED

    int context_switches;        // doc: "trocas_contexto_sofridas" — fixed: no spaces allowed in C identifiers
} Process;

// Allocates `bursts` with the given count and initializes basic fields.
// Caller fills in each burst's type/duration afterward (workload generator's job, per docs/cenarios.md).
void process_init(Process *p, int pid, int arrival_time, int priority, int num_bursts);

// Frees the dynamically allocated bursts array.
void process_destroy(Process *p);

// Helper functions for string representation and debugging.
const char *process_state_to_string(ProcessState state);
const char *burst_type_to_string(BurstType type);
void process_print(const Process *p);

#endif