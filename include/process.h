#ifndef PROCESS_H
#define PROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} ProcessState;

typedef enum { 
    BURST_CPU,
    BURST_IO
} BurstType;

typedef struct {
    BurstType type;
    int duration;       
    int time_left;    
} Burst;

typedef struct {
    int pid;
    int arrival_time;
    int priority;
    Burst[] bursts;
    int current_burst_index;
    int io_request_count; 
    ProcessState current_state;
    int completion_time;
    int context switches suffered;
} Process;