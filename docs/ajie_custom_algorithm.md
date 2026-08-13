# AJIE Custom Scheduling Algorithm

## 1. Overview

The **AJIE (Aisha, Jackson, Icaro, Elilucio) Scheduling Algorithm** is a
custom, non-preemptive priority scheduler designed to combine the
deterministic behavior of priority scheduling with an aging mechanism
intended to reduce starvation.

The algorithm assigns each ready process to a priority queue according
to its **effective priority**. At every scheduling decision, processes
that have already been waiting are aged, improving their effective
priority. The scheduler then selects the first process available in the
highest-priority queue.

The algorithm is designed for a discrete simulation environment in which
CPU bursts, I/O operations, process states, arrival times, and
context-switch costs are represented explicitly.

> All time values used by the algorithm are expressed in abstract
> simulation ticks. They do not represent real-world milliseconds or CPU
> cycles.

------------------------------------------------------------------------

## 2. Motivation

Traditional priority scheduling gives the processor to the
highest-priority ready process. Although this is simple and
deterministic, it can lead to **starvation**: a process assigned a low
priority may remain in the ready queue indefinitely if higher-priority
processes continue to be selected.

The AJIE algorithm introduces **aging** to address this problem.

A process that remains waiting is progressively promoted. Therefore,
priority is no longer completely static:

``` text
priority_base
      |
      v
waiting
      |
      v
aging
      |
      v
higher effective priority
```

The aging mechanism is deliberately tied to **scheduling decisions**,
rather than to every simulation tick. This choice is consistent with the
non-preemptive nature of the algorithm: the scheduler does not interrupt
the running process merely because another process has become more
important.

The intended result is a scheduler that:

-   preserves priority as an important scheduling criterion;
-   reduces the possibility of starvation;
-   remains deterministic;
-   does not require burst-time prediction;
-   supports a configurable number of priority levels;
-   remains straightforward to implement and analyze.

------------------------------------------------------------------------

## 3. Scheduling Model

AJIE is a **non-preemptive priority scheduler**.

Once a ready process is selected, the scheduler does not interrupt it
because a new process with a higher priority arrives.

For example:

``` text
t = 0    P1 starts executing
t = 2    P2 arrives with higher priority
         |
         +--> P1 continues executing
```

P2 becomes eligible for consideration at the next scheduling decision.

This behavior distinguishes AJIE from preemptive priority scheduling and
ensures that aging is applied at discrete decision points.

------------------------------------------------------------------------

## 4. Priority Convention

The current project convention uses:

``` text
1                     = highest priority
2                     = lower priority
3                     = lower priority than 2
...
num_priority_levels   = lowest priority
```

Therefore:

> **A smaller numerical value represents a higher priority.**

For example, with five levels:

``` text
Priority 1  -> highest
Priority 2
Priority 3
Priority 4
Priority 5  -> lowest
```

This convention is compatible with the priority representation used by
the process model.

The parameter `num_priority_levels` is not merely descriptive. It
determines the valid priority range and the number of ready queues
created by the scheduler.

------------------------------------------------------------------------

## 5. Input Information

AJIE requires the following process information.

  -----------------------------------------------------------------------
  Information                         Purpose
  ----------------------------------- -----------------------------------
  Process identifier                  Identifies the process

  Base priority                       Stores the priority originally
                                      assigned to the process

  Effective priority                  Stores the priority currently used
                                      by the scheduler

  Process state                       Determines whether the process is
                                      eligible for scheduling

  CPU bursts                          Determines CPU execution
                                      requirements

  I/O bursts                          Determines blocking and
                                      return-to-ready events

  Arrival time                        Determines when the process becomes
                                      available

  Queue position                      Preserves FCFS ordering among
                                      processes with the same effective
                                      priority
  -----------------------------------------------------------------------

The scheduler configuration contains:

  -----------------------------------------------------------------------
  Parameter                           Purpose
  ----------------------------------- -----------------------------------
  `num_priority_levels`               Number of priority levels and ready
                                      queues

  `context_switch_cost`               Number of simulation ticks
                                      associated with a context switch
  -----------------------------------------------------------------------

The algorithm must not assume a fixed number of priority levels.

------------------------------------------------------------------------

## 6. Ready-Queue Organization

AJIE maintains one FIFO queue for each priority level.

Conceptually:

``` text
Ready Queues

Queue 1                    Highest priority
Queue 2
Queue 3
...
Queue N                    Lowest priority
```

where:

``` text
N = num_priority_levels
```

The implementation therefore allocates the queues according to the
configured value of `num_priority_levels`.

A process with effective priority `p` must be stored in:

``` text
queues[p]
```

The valid range is:

``` text
1 <= p <= num_priority_levels
```

Queue `0` is not a valid priority level.

### FCFS behavior

Each queue is FCFS.

Therefore, when two processes have the same effective priority, the
process that entered that priority queue first is selected first.

The scheduler consequently uses:

1.  **effective priority** as the primary criterion;
2.  **FCFS order** as the secondary criterion.

------------------------------------------------------------------------

## 7. Base Priority and Effective Priority

AJIE distinguishes between two concepts.

### 7.1. Base priority

The **base priority** is the priority originally assigned to the
process.

It represents the process's original scheduling importance.

### 7.2. Effective priority

The **effective priority** is the priority currently used to select the
process.

Aging changes the effective priority while the process waits.

For example:

``` text
Base priority:       5
Initial effective:   5

After aging:         4
After aging:         3
After aging:         2
After aging:         1
```

The process can therefore become temporarily more competitive than its
original priority would indicate.

------------------------------------------------------------------------

## 8. Aging Rule

The central rule of AJIE is:

``` text
A waiting process improves its effective priority by one level
at each scheduling decision in which it remains waiting.
```

Because lower numbers represent higher priority, aging performs:

``` text
effective_priority = effective_priority - 1
```

with a lower bound of `1`.

Formally:

$$
P_{effective}^{new} =
\max(P_{effective}^{old} - 1, 1)
$$

Example:

``` text
5 -> 4 -> 3 -> 2 -> 1
```

Once a process reaches priority `1`, additional aging has no effect.

------------------------------------------------------------------------

## 9. Newly Arrived Processes

A process must not receive an aging promotion simply because it has
entered the ready queue.

The scheduler distinguishes between:

-   processes that were already waiting before the current decision;
-   processes that became ready during the current scheduling interval.

A newly arrived or newly awakened process is inserted using its
appropriate effective priority and is marked as **new for the current
scheduling round**.

During that same decision, it does not receive an additional aging
promotion.

This prevents a process from receiving an artificial priority advantage
immediately upon entering the ready queue.

------------------------------------------------------------------------

## 10. Aging Traversal

The order in which queues are traversed is important.

Since aging changes:

``` text
p -> p - 1
```

the aging pass must traverse the queues from:

``` text
2 -> 3 -> 4 -> ... -> num_priority_levels
```

The highest-priority queue, `1`, is not used as a source for aging
because a process there is already at the maximum effective priority.

This ordering prevents a process promoted from one queue from being
encountered again in the same aging pass.

For example:

``` text
Process at 3
    |
    +--> promoted to 2
```

If queue `2` were processed later during the same pass, the process
could incorrectly be promoted again:

``` text
3 -> 2 -> 1
```

The increasing traversal order prevents this:

``` text
2 -> 3 -> 4 -> ... -> N
```

A process can therefore receive **at most one aging promotion per
scheduling decision**.

------------------------------------------------------------------------

## 11. Aging Pseudocode

``` text
PROCEDURE AGE_READY_PROCESSES

    FOR level FROM 2 TO num_priority_levels

        current <- first process in queue[level]

        WHILE current exists

            next <- next process after current

            IF current was already waiting before this decision THEN

                remove current from queue[level]

                current.effective_priority <-
                    MAX(current.effective_priority - 1, 1)

                insert current at the end of
                    queue[current.effective_priority]

            ELSE

                preserve current position

            END IF

            current <- next

        END WHILE

    END FOR

END PROCEDURE
```

The procedure does not create additional priority levels and never
allows a priority below `1`.

------------------------------------------------------------------------

## 12. Selecting the Next Process

After aging, the scheduler searches the ready queues from highest
priority to lowest priority.

Because priority `1` is the highest:

``` text
1 -> 2 -> 3 -> ... -> num_priority_levels
```

The first non-empty queue provides the next process.

### Pseudocode

``` text
FUNCTION SELECT_NEXT_PROCESS

    FOR level FROM 1 TO num_priority_levels

        IF queue[level] is not empty THEN
            RETURN remove first process from queue[level]
        END IF

    END FOR

    RETURN NONE

END FUNCTION
```

Because each queue is FIFO, the first process in the selected queue is
also the earliest process among the processes sharing that effective
priority.

------------------------------------------------------------------------

## 13. Complete Scheduling Procedure

The complete AJIE scheduling cycle is:

``` text
PROCEDURE AJIE_SCHEDULER

    create num_priority_levels ready queues

    WHILE there are unfinished processes

        AGE_READY_PROCESSES()

        process <- SELECT_NEXT_PROCESS()

        IF process does not exist THEN

            advance simulation until the next process
            arrival or I/O completion event

            CONTINUE

        END IF

        set process state to RUNNING

        execute the selected CPU work
        without preemption

        update process state

        IF the process finished THEN

            record completion

        ELSE IF the process must perform I/O THEN

            set process state to BLOCKED
            schedule its return to the ready queue

        ELSE IF the process remains ready THEN

            restore its appropriate effective priority
            insert it into the corresponding ready queue

        END IF

        IF a context switch is required THEN

            increment total context-switch count
            advance simulation by context_switch_cost

        END IF

    END WHILE

END PROCEDURE
```

The exact mechanics of CPU and I/O state transitions belong to the
simulator's process model. The custom scheduling decision itself is
defined by the priority queues, aging rule, and selection rule above.

------------------------------------------------------------------------

## 14. Return from I/O

When a blocked process becomes ready again, it is inserted into the
ready queues.

The base priority remains the process's original priority.

The implementation must explicitly define whether the effective priority
is restored to the base priority on each return from I/O. AJIE's
intended behavior is to restore the base priority:

``` text
I/O completion
      |
      v
effective_priority <- base_priority
      |
      v
ready queue
```

This prevents aging accumulated during a previous ready period from
becoming a permanent priority change.

------------------------------------------------------------------------

## 15. Context-Switch Cost

AJIE is non-preemptive, so a higher-priority arrival does not
automatically force a context switch.

When the simulator determines that a context switch occurs, the
configured context-switch cost must be applied.

Conceptually:

``` text
context switch
      |
      +--> total_context_switches += 1
      |
      +--> simulation_time += context_switch_cost
```

The value of `context_switch_cost` is external configuration and must
not be hard-coded inside AJIE.

This allows experiments to distinguish between:

-   algorithmic behavior without context-switch overhead;
-   behavior under a realistic non-zero context-switch cost.

The current process model already contains a per-process context-switch
counter. Global context-switch accounting and configuration can be
integrated with the process model as those fields are finalized.

------------------------------------------------------------------------

## 16. Starvation Prevention

Suppose a process enters the ready queue with priority `b`.

Since `1` is the highest priority, the maximum number of promotions
required for it to reach the highest priority is:

$$
b - 1
$$

For example:

``` text
Initial priority = 5

Decision 1: 5 -> 4
Decision 2: 4 -> 3
Decision 3: 3 -> 2
Decision 4: 2 -> 1
```

Thus, the priority representation gives a deterministic upper bound on
the number of aging promotions required to reach priority `1`.

This does **not** mean that the process executes immediately after
reaching priority `1`. Other processes may already be waiting at
priority `1`, and they are ordered by FCFS.

The aging mechanism therefore prevents indefinite degradation caused
solely by a process having a lower base priority, but it cannot
compensate for an unsustainable system load in which the processor is
permanently overloaded.

------------------------------------------------------------------------

## 17. Correctness Properties

The implementation should preserve the following invariants.

### Priority bounds

For every ready process:

``` text
1 <= effective_priority <= num_priority_levels
```

### Queue consistency

A process with effective priority `p` must be stored in:

``` text
queue[p]
```

### Single aging promotion

A process can be promoted at most once during a scheduling decision.

### Highest-priority selection

If at least one ready process exists at priority `p`, and no ready
process exists at any level below `p`, the scheduler selects from queue
`p`.

### FCFS within a priority

For processes with equal effective priority, the process that entered
that queue first is selected first.

### Non-preemption

A process currently executing is not interrupted merely because another
process with higher priority becomes ready.

### Configurability

The scheduler must operate for any valid configured value of:

``` text
num_priority_levels >= 1
```

without recompilation or modification of the algorithm's queue count.

------------------------------------------------------------------------

## 18. Complexity

Let:

``` text
n = number of processes currently waiting
L = num_priority_levels
```

### Queue insertion

With a FIFO queue maintaining both head and tail:

$$
O(1)
$$

### Selection

The scheduler may inspect up to `L` queues:

$$
O(L)
$$

Because `L` is a configuration parameter independent of the number of
processes, this is effectively constant with respect to `n` when the
number of priority levels is bounded.

### Aging

Every waiting process may be inspected once:

$$
O(n)
$$

### Scheduling decision

The combined cost is:

$$
O(n + L)
$$

or, treating `L` as a bounded configuration parameter:

$$
O(n)
$$

### Overall execution

With a full aging pass at every scheduling decision, the cumulative
worst-case cost can reach:

$$
O(n^2)
$$

over a sequence of decisions involving a large number of processes.

This is a deliberate trade-off in the initial implementation: the
algorithm favors explicit behavior, deterministic aging, and
straightforward verification over a more complex lazy-aging structure.

------------------------------------------------------------------------

## 19. Advantages

### 19.1. Reduced starvation

Processes that remain waiting are progressively promoted.

### 19.2. Deterministic behavior

The scheduler does not rely on random selection or burst-time
prediction.

### 19.3. Configurable priority range

`num_priority_levels` directly controls the scheduler's number of
priority queues.

### 19.4. Simple tie-breaking

FCFS provides a clear and deterministic rule for processes sharing a
priority.

### 19.5. Compatible with non-preemptive scheduling

The algorithm does not require preemption to perform aging.

### 19.6. Easy to analyze

The priority transition is explicit:

``` text
p -> max(p - 1, 1)
```

which makes the behavior straightforward to test and reason about.

------------------------------------------------------------------------

## 20. Limitations

### 20.1. Aging overhead

The basic implementation may inspect every waiting process at every
scheduling decision.

### 20.2. Priority saturation

Once a process reaches priority `1`, further aging cannot increase its
priority further.

### 20.3. Priority-1 contention

Several aged processes may reach priority `1` and then compete using
FCFS.

### 20.4. Non-preemptive response delay

A long-running process can delay a higher-priority process that arrives
while the CPU is occupied.

### 20.5. No burst prediction

Unlike algorithms such as SJF or HRRN, AJIE does not use estimated
CPU-burst duration as a scheduling criterion.

### 20.6. Initial implementation trade-off

The explicit aging pass favors clarity but can be less efficient than
lazy or event-based priority calculations for very large workloads.

------------------------------------------------------------------------

## 21. Comparison with Related Scheduling Strategies

### Priority Scheduling

Priority scheduling keeps the priority fixed.

AJIE extends this model:

``` text
Priority Scheduling:
base priority -> scheduling decision

AJIE:
base priority -> waiting -> aging -> effective priority
```

The additional aging mechanism is intended to reduce starvation.

### FCFS

FCFS orders processes primarily by arrival.

AJIE instead prioritizes effective priority and uses FCFS only as a
tie-breaker within a priority level.

### SJF

SJF selects according to estimated CPU-burst length.

AJIE does not require burst-length prediction.

### Round Robin

Round Robin allocates CPU time using a quantum and is generally
associated with preemption.

AJIE does not use a time quantum and is non-preemptive.

### HRRN

HRRN combines waiting time and estimated burst time through a
response-ratio calculation.

AJIE does not require estimated burst duration. Its fairness mechanism
is based on discrete priority aging.

### MLFQ

MLFQ commonly changes priorities according to execution behavior and is
often implemented with preemption.

AJIE changes effective priority according to waiting history and is
specifically defined as non-preemptive.

------------------------------------------------------------------------

## 22. Experimental Considerations

AJIE should be evaluated using the same generated workloads used to
compare the other scheduling algorithms.

For a given workload and seed, the following should remain unchanged
across algorithms:

-   process arrivals;
-   process priorities;
-   CPU bursts;
-   I/O requests;
-   I/O durations.

Only the scheduling policy should change.

Relevant measurements include:

-   mean turnaround time;
-   waiting time;
-   context-switch count;
-   slowdown;
-   fairness;
-   final simulation time.

Priority-unbalanced workloads are particularly relevant because they
allow the aging mechanism to be observed under conditions where
starvation would be more likely with static priority scheduling.

------------------------------------------------------------------------

## 23. Future Improvements

Possible improvements include:

1.  replacing full aging scans with lazy priority calculation;
2.  using timestamps or scheduling-decision counters to derive effective
    priority on demand;
3.  improving queue removal to guarantee constant-time operations;
4.  evaluating alternative tie-breaking policies;
5.  measuring the effect of different values of `num_priority_levels`;
6.  measuring the effect of different context-switch costs;
7.  comparing aging frequency based on decisions against aging based on
    elapsed simulation time;
8.  performing larger benchmark experiments to determine the workload
    conditions under which AJIE provides the greatest benefit.

------------------------------------------------------------------------

## 24. Summary

AJIE is a **non-preemptive priority scheduler with deterministic
aging**.

Its central rule is:

``` text
Higher priority = smaller numerical value
```

and its aging rule is:

``` text
effective_priority <- max(effective_priority - 1, 1)
```

for processes that have already waited at the current scheduling
decision.

The complete scheduling policy can be summarized as:

``` text
WHILE unfinished processes exist:

    age processes that were already waiting;

    find the first non-empty queue from
        priority 1 through num_priority_levels;

    select the first process in that queue;

    execute it without preemption;

    update its CPU/I/O state;

    if it becomes ready again:
        place it in the appropriate ready queue;

    if a context switch occurs:
        account for the configured context-switch cost;

END
```

The defining idea of AJIE is therefore:

> **Priority determines the initial preference, while repeated waiting
> progressively improves a process's effective priority, reducing
> starvation without requiring preemption or CPU-burst prediction.**
