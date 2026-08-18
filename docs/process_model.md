# Process Model
---

- **Task:** ASH-01
- **Acceptance criteria:** document reviewed by the team, with all attributes required by Section 3
of the assignment and the state diagram.
- **Consumes/feeds:** entry requirement for ICR-04 (C struct), ELI-03 (simulation engine stub), ASH-02
and ASH-03.

---

## 1. Purpose of this document

To define, in a single and mandatory way for the whole simulator, **what a process is** within our
simulation: which attributes it carries, what each attribute means, and which states it goes through.
No algorithm (FCFS, Round Robin, Priority, or the custom algorithm) may use a process representation
different from the one described here — this is what guarantees that the comparison between them is
fair and reproducible.

This document **does not** define how I/O or context switching work in detail — that is the
responsibility of `docs/modelagem_io.md` and `docs/troca_contexto.md`. Here we deal
only with the process's data structure and its life cycle (states).

---

## 2. Mandatory process attributes

Every simulated process must be represented with, at minimum, the following fields:

| Attribute | Suggested type (C) | Description |
|---|---|---|
| `pid` | `int` | Unique process identifier within the run (sequential, starting at 0 or 1). |
| `tempo_chegada` (arrival time) | `int` (time units) | Instant at which the process enters the system (**new → ready** state). Generated from the seed, according to the arrival model defined in `docs/cenarios.md`. |
| `prioridade` (priority) | `int` | Process priority value. See the convention adopted in Section 3 below. |
| `rajadas` (bursts) | array/list of `Rajada` structs | Alternating sequence of CPU and I/O bursts that describes the process's entire CPU/I-O "lifetime". |
| `indice_rajada_atual` (current burst index) | `int` | Pointer/index indicating which burst in the `rajadas` list is currently running or pending. |
| `num_requisicoes_io` (I/O request count) | `int` | Count of how many I/O-type bursts exist in the `rajadas` list (derived information, kept explicit for metric convenience). |
| `estado` (state) | `enum EstadoProcesso` | Current process state. |
| `tempo_termino` (completion time) | `int` | Filled in when the process reaches the **finished** state; used to compute turnaround. |
| `trocas_contexto_sofridas` (context switches suffered) | `int` | Counter of how many times this specific process was the target of a context switch. |

---

## 3. Adopted priority convention

**Team decision:** **lower** `prioridade` values represent **higher** priority.

- Value range: integers from `1` (highest priority) to `10` (lowest priority).
- Justification: this is the most common convention in classic Operating Systems literature (e.g., nice
  values in Unix, where a lower value = higher priority) and it simplifies comparison in the scheduler
  — the chosen process is always the one with the minimum `prioridade` among the ready ones.
- **This convention must be used in absolutely all experiments and by all algorithms** that consider
  priority (Priority Scheduling, the custom algorithm, and any auxiliary analysis), as required by
  Section 3 of the assignment.
- In case of a priority tie, the tie-breaking criterion is defined and documented separately in
  `docs/escolhas_implementacao.md`, since it may vary by algorithm (e.g., FCFS by arrival
  time as the tiebreaker).

> **Update note:** the range was adjusted from the original `0`–`9` proposal to `1`–`10` to match the
> value range already adopted in the merged `docs/cenarios.md`, so both documents are now
> consistent. If the team later revisits this choice, both files must be updated together.

---

## 4. Burst model (CPU/I-O)

Each process is modeled as an alternating sequence of bursts, always starting and ending with a CPU
burst:

```
CPU → I/O → CPU → I/O → ... → CPU
```

- A CPU burst represents a continuous interval of processor use, uninterrupted by I/O (it can still be
  interrupted by preemption, depending on the algorithm — see `docs/troca_contexto.md`).
- An I/O burst represents an input/output request, with a duration determined at workload-generation
  time (see `docs/modelagem_io.md` for blocking and I/O queue details).
- **There are never two consecutive I/O bursts**, nor does the process terminate on an I/O burst — the
  last burst in the list is always a CPU burst, and upon completing it the process moves to the
  finished state.
- The number of CPU and I/O bursts per process, and the duration of each, are generated randomly from
  the seed, with scenario-specific distributions — the exact parameters are in `docs/cenarios.md`
  (ASH-03), not in this document.

Suggested C structure:

```c
typedef enum { RAJADA_CPU, RAJADA_IO } TipoRajada;

typedef struct {
    TipoRajada tipo;
    int duracao;          // duration in time units
    int tempo_restante;    // used by preemptive algorithms (e.g., Round Robin)
} Rajada;
```

---

## 5. Process states and transition diagram

Mandatory states (Section 3 of the assignment): **new, ready, running, blocked, finished**.

```
(add diagram later)
```

### Transition description

| From | To | Condition/trigger |
|---|---|---|
| New | Ready | The process "arrives" at the `tempo_chegada` instant generated by the arrival model (`docs/cenarios.md`). It enters the ready queue. |
| Ready | Running | The scheduler picks this process among the ones in the ready queue, according to the policy of the algorithm in use. Involves a context switch if the processor was not idle (see `docs/troca_contexto.md`). |
| Running | Blocked | The current CPU burst ends and the next burst in the list is of type I/O. The process enters the I/O queue (see `docs/modelagem_io.md`). |
| Blocked | Ready | The I/O device completes the operation. The process returns to the ready queue, competing for the processor again. |
| Running | Ready | **Only for preemptive algorithms** (e.g., Round Robin, or the custom algorithm, if preemptive): the process is interrupted before finishing its current CPU burst (quantum expired, arrival of a higher-priority process, etc.). The burst's `tempo_restante` (remaining time) is preserved. |
| Running | Finished | The current CPU burst ends and there is no next burst in the list (i.e., it was the process's last CPU burst). `tempo_termino` is recorded. |

> **Important note for non-preemptive algorithms** (FCFS, non-preemptive Priority): the *Running →
> Ready* transition due to preemption simply **does not occur**. A process that enters Running only
> leaves that state by going to Blocked (due to I/O) or Finished.

> **Reminder on 'Burst':** continuous periods of intense activity on a resource by a process. Split
> into CPU burst and I/O burst.
---

## 6. Integration points with other documents

- **Context switching** (cost, when it is accounted for, whether the CPU becomes unavailable) is
  defined in `docs/troca_contexto.md` and logically occurs at *Ready → Running* transitions.
- The **ready queue and the I/O queue** (data structure, ordering, whether there are one or more
  devices) are defined in `docs/modelagem_io.md`).
- The **numerical parameters** for generating bursts, priorities, and arrival times per scenario are in
  `docs/cenarios.md` .
- Implementing the C struct (`src/process.h` / `src/process.c`). Any divergence found during
  implementation must be reported.

---

