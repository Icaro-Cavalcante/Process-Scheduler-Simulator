# Context Switch Modeling

---

## 1. Overview and Objectives

In a process scheduling simulator written in C, a context switch represents the operating system overhead incurred when saving the execution state (CPU registers, program counter `PC`, stack pointer `SP`, etc.) of a preempted or yield process and restoring the state of the next process selected by the scheduler.

---

## 2. The 4 Fundamental Questions

### 2.1. When does a Context Switch occur? (Triggering State Transitions)

A context switch is triggered **whenever the CPU changes the executing process**. In the discrete-event/discrete-time simulator, context switches are triggered by the following process state transitions:

1. **Quantum Expiration (Round Robin)**: The currently running process exceeds its allocated CPU `quantum`. State transition: `RUNNING` $\rightarrow$ `READY` (re-inserted at the tail of the ready queue).
2. **I/O Request / Blocking Call**: The currently running process issues an I/O request (finishes a CPU burst and initiates an I/O burst). State transition: `RUNNING` $\rightarrow$ `BLOCKED` (inserted into the I/O queue).
3. **Process Termination**: The currently running process completes its final CPU burst. State transition: `RUNNING` $\rightarrow$ `TERMINATED`.
4. **Priority Preemption** (if applicable): A newly arrived or unblocked process with higher priority preempts the lower-priority process currently executing on the CPU. State transition: `RUNNING` $\rightarrow$ `READY`.

> **Modeling Convention**: Context switch overhead is debited at the exact instant the scheduler dispatches a new process to the CPU (entry transition).

---

### 2.2. How much time does a Context Switch consume?

* A context switch consumes a configurable discrete duration denoted as `context_switch_cost` (measured in simulated clock ticks).
* The value of `context_switch_cost` is **configurable** at simulation startup and is strictly **greater than zero** (`context_switch_cost > 0`) for all primary benchmark experiments.
* **Consistency Rule**: The exact same `context_switch_cost` value MUST be applied uniformly across **all** scheduling algorithms (FCFS, Round Robin, Priority, and Custom Algorithm) evaluated under identical seeds and scenarios.

---

### 2.3. Is the CPU unavailable during a Context Switch?

**Yes, the CPU is 100% unavailable for user process execution during a context switch.**

* During the `context_switch_cost` interval, the CPU transitions into a system overhead state (`CONTEXT_SWITCHING`).
* **No user process CPU burst progress occurs during this window.**
* For example, if `context_switch_cost = 2` ticks, the global simulation clock advances by 2 units without decrementing the remaining CPU burst of any process.
* **Global Clock Accounting**:
  $$\text{Total CPU Occupation Time} = \text{Total CPU Bursts} + (\text{Total Context Switches} \times \text{context\_switch\_cost})$$

---

### 2.4. Is a Context Switch counted when transitioning from IDLE to RUNNING?

**Yes, transitioning from `IDLE` $\rightarrow$ `RUNNING` incurs the `context_switch_cost` and increments the metric.**

* **Theoretical Rationale**: In real operating systems, exiting a low-power CPU `IDLE` state (or idle loop) to dispatch a newly arrived process requires hardware interrupt handling, register restoration, and page table updates (dispatcher overhead).
* **Simulator Rule**: When the CPU is `IDLE` at time $t$ and a process becomes ready and is dispatched, execution of the process's CPU burst begins at time $t + \text{context\_switch\_cost}$. The global metric `context_switches` is incremented by +1.

---

## 3. Transition Matrix and Cost Summary

| Previous CPU State | Next CPU State | Context Switch? | Duration (Ticks) | Metric Increment (`context_switches`) |
| :--- | :--- | :---: | :---: | :---: |
| `RUNNING` (Proc A) | `RUNNING` (Proc B) | **YES** | `context_switch_cost` | **+1** |
| `RUNNING` (Proc A) | `RUNNING` (Proc A) | **NO** | 0 | 0 (Round Robin without preemption) |
| `RUNNING` (Proc A) | `IDLE` (Empty Queue) | **NO** | 0 | 0 |
| `IDLE` | `RUNNING` (Proc A) | **YES** | `context_switch_cost` | **+1** |
| `IDLE` | `IDLE` | **NO** | 0 | 0 |

---

## 4. Implementation Contracts for C Developers

To ensure seamless integration across modules, the development teamused the following standardized struct and enum naming conventions in C:

```c
#ifndef CONTEXT_SWITCH_H
#define CONTEXT_SWITCH_H

typedef enum {
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} ProcessState;

typedef struct {
    int current_time;
    int context_switch_cost;     // Configurable overhead > 0
    int total_context_switches;  // Metric counter
} SimulatorConfig;

#endif
```

---

## 5. Metric Calculations & Impact Analysis

### 5.1. Turnaround Time & Slowdown Impact
* The turnaround time of process $i$ ($T_{\text{turnaround}, i} = T_{\text{completion}, i} - T_{\text{arrival}, i}$) accumulates all context switch delays experienced during its lifecycle.
* The **Ideal Minimum Time** ($T_{\text{ideal}, i}$) used in the **Slowdown** calculation **EXCLUDES** context switch overhead:
  $$T_{\text{ideal}, i} = \sum \text{CPU\_bursts}_i + \sum \text{IO\_bursts}_i$$
* **Slowdown Formula**:
  $$\text{slowdown}_i = \frac{T_{\text{turnaround}, i}}{T_{\text{ideal}, i}}$$
* This guarantees that scheduling policies causing excessive context switching (e.g., Round Robin with a very small quantum) are appropriately penalized in the slowdown and Jain's Fairness Index metrics.

---

## 6. Complementary Sensitivity Analysis

For study purposes, additional experiments with `context_switch_cost = 0` may be executed as a secondary baseline in the final paper to isolate algorithm efficiency from hardware overhead.
