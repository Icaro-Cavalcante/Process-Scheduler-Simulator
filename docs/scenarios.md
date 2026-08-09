# Process Arrival Model and Benchmark Scenarios

- **Task:** ASH-03 (scenarios) + JCK-02 (process arrival model)
- **Depends on:** `docs/modelo_processo.md` (ASH-01)
- **Acceptance criteria:** numerical parameters sufficient for the workload generator (ELI-02 / ICR-05).
- **Consumed by:** ICR-05 (implementation of the seeded generator); JCK-09 (`docs/formato_csv.md`) must stay consistent with the CSV contract in Section 5.

---

## 1. Overview

This document formalizes the **Process Arrival Model** and the parameter specifications for the **4
mandatory Simulation Scenarios**, to be evaluated across First-Come-First-Served, Round Robin, Priority,
and a Custom/Original algorithm.

The deterministic seed-based workload generator will use these parameters to synthesize reproducible
workload datasets across all experiments.

> All time values below (bursts, I/O, inter-arrival) are expressed in abstract simulation ticks / time
> units, with no direct correspondence to real-world milliseconds.

---

## 2. Process Arrival Model

### 2.1. Arrival Time Definition ($T_{\text{arrival}}$)

The process arrival time $T_{\text{arrival}}$ represents the discrete clock tick at which a simulated
process is admitted into the system and enqueued into the **Ready Queue**.

To ensure statistical validity and realistic scheduling behavior:
* Process arrival times are generated **pseudo-randomly using a deterministic seed**.
* The arrival model uses a **stochastic inter-arrival distribution (Exponential / Poisson process)** or
  a **Uniform distribution across the simulation timeline $[0, T_{\text{max}}]$**.

$$\Delta t_{\text{arrival}} \sim \text{Exp}(\lambda) \quad \implies \quad T_{\text{arrival}}^{(i)} = T_{\text{arrival}}^{(i-1)} + \Delta t_{\text{arrival}}$$

---

### 2.2. Technical Justification: Why Avoid Instant 0 Arrival?

For study purposes, the team has decided **NOT to use instant 0 arrival**, opting instead for
**time-distributed stochastic arrival**.

**Limitations of Instant 0 Arrival (Why Avoided):**
1. **Unrealistic Operating System Behavior**: Production operating systems receive process creation
   requests dynamically over time.
2. **Neutralization of Real-Time Preemption**: If all processes arrive at $t = 0$, scheduler decisions
   become mostly static. Dynamic preemption upon arrival of higher-priority processes is eliminated.
3. **Queue Inflation**: Admitting 1,000+ processes simultaneously creates extreme initial queue
   contention, distorting wait-time metrics.

---

### 2.3. Reproducibility & Seed Invariance

* **Cross-Algorithm Invariance**: For a given seed $S$ and scenario $C$, the exact sequence of generated
  processes—including $T_{\text{arrival}}$, priority, CPU burst list, and I/O requests—MUST be **100%
  identical** across FCFS, Round Robin, Priority, and Custom algorithms.
* **Workload Scale**:
  * Minimum **1,000 processes** per execution run.
  * Minimum **100 independent seeds** per scenario (totaling 400 runs per algorithm).
  * **Recommended target: 1,000 seeds per scenario**, if execution time allows — the minimum of 100 is
    the mandatory floor, not the goal.
* **Generator requirement**: all distributions in this document must be sampled using the seeded
  pseudo-random generator (ELI-02), never with uncontrolled `rand()` — this is what guarantees the
  cross-algorithm invariance above.

---

## 3. Mandatory Simulation Scenarios

> Priority values below follow the same numeric convention adopted in `docs/modelo_processo.md`
> (ASH-01) — currently `[1, 10]`, where a lower integer means higher priority.

```
+-----------------------------------------------------------------------------------+
|                            SIMULATION BENCHMARK SCENARIOS                         |
+--------------------------+-----------------------+--------------------------------+
| Scenario 1: Balanced     | Scenario 2: I/O-Bound | Scenario 3: CPU-Bound          |
| (Mixed CPU & I/O)        | (Short CPU bursts,    | (Long CPU bursts,              |
|                          | frequent I/O)         | infrequent I/O)                |
+--------------------------+-----------------------+--------------------------------+
| Scenario 4: Priority Unbalanced                                                   |
| (85% High Priority, 15% Low Priority)                                             |
+-----------------------------------------------------------------------------------+
```

---

### 3.1. Scenario 1: Balanced Random (`balanced_random`)
* **Objective**: Evaluate algorithms under a heterogeneous, general-purpose workload.
* **Parameters**:
  * CPU Burst Length: Uniform in $[5, 50]$ ticks.
  * I/O Request Count: Uniform in $[1, 5]$ requests.
  * I/O Burst Duration: Uniform in $[10, 30]$ ticks.
  * Mean Inter-arrival ($\Delta t$): $5$ ticks.
  * Priorities: Uniform in $[1, 10]$ (where lower integer = higher priority).

---

### 3.2. Scenario 2: I/O-Bound (`io_bound`)
* **Objective**: Test scheduler responsiveness under heavy I/O operations and high queue transition
  frequency.
* **Parameters**:
  * CPU Burst Length: Short (Uniform in $[1, 8]$ ticks).
  * I/O Request Count: High (Uniform in $[6, 15]$ requests).
  * I/O Burst Duration: Long (Uniform in $[20, 60]$ ticks).
  * Mean Inter-arrival ($\Delta t$): $3$ ticks (high admission rate).
  * Priorities: Uniform in $[1, 10]$.

---

### 3.3. Scenario 3: CPU-Bound (`cpu_bound`)
* **Objective**: Measure throughput, turnaround time, and context-switch overhead under heavy
  computational workloads.
* **Parameters**:
  * CPU Burst Length: Long (Uniform in $[40, 200]$ ticks).
  * I/O Request Count: Low (Uniform in $[0, 2]$ requests).
  * I/O Burst Duration: Short (Uniform in $[5, 15]$ ticks).
  * Mean Inter-arrival ($\Delta t$): $12$ ticks.
  * Priorities: Uniform in $[1, 10]$.

---

### 3.4. Scenario 4: Unbalanced Priorities (`priority_unbalanced`)
* **Objective**: Assess starvation risk for low-priority processes and evaluate fairness using **Jain's
  Fairness Index** on slowdown.
* **Parameters**:
  * Priority Distribution:
    * **85% of processes**: HIGH Priority ($1 \le \text{priority} \le 3$).
    * **15% of processes**: LOW Priority ($8 \le \text{priority} \le 10$).
  * CPU & I/O Burst Lengths: Balanced mixed distribution.
  * Mean Inter-arrival ($\Delta t$): $4$ ticks.

---

## 4. Parameter Matrix for Workload Generator

| Parameter | Scenario 1 (`balanced_random`) | Scenario 2 (`io_bound`) | Scenario 3 (`cpu_bound`) | Scenario 4 (`priority_unbalanced`) |
| :--- | :---: | :---: | :---: | :---: |
| **CPU Burst (ticks)** | $[5, 50]$ | $[1, 8]$ | $[40, 200]$ | $[5, 50]$ |
| **I/O Request Count** | $[1, 5]$ | $[6, 15]$ | $[0, 2]$ | $[1, 5]$ |
| **I/O Duration (ticks)** | $[10, 30]$ | $[20, 60]$ | $[5, 15]$ | $[10, 30]$ |
| **Inter-arrival ($\Delta t$)** | $\sim 5$ | $\sim 3$ | $\sim 12$ | $\sim 4$ |
| **Priority Range** | $[1, 10]$ (uniform) | $[1, 10]$ (uniform) | $[1, 10]$ (uniform) | $85\% \in [1,3], 15\% \in [8,10]$ |

---

## 5. Output CSV Data Specification Contract

All simulation runs MUST output raw metrics adhering to the following standardized CSV header format:

```csv
algorithm,scenario,seed,process_count,mean_turnaround,context_switches,jain_slowdown
```

> This contract must remain the single source of truth for the CSV layout — any refinement should be
> made here and mirrored in `docs/formato_csv.md` (JCK-09), not diverge between the two.

---

## 6. What this document does **not** define (and where to find it)

- **Round Robin quantum**: an implementation decision, documented in `docs/escolhas_implementacao.md`
  (ASH-06), since it can be adjusted experimentally without changing the workload itself.
- **Context switch cost**: defined in `docs/troca_contexto.md` (JCK-01) — must be configurable, greater
  than zero in the main experiments, and equal across all algorithms within a given experiment.
- **I/O queue behavior** (parallelism, number of devices): defined in `docs/modelagem_io.md` (ASH-02).

---

## 7. Team review checklist

- `ICR` confirms that the 4 scenarios above have sufficient parameters to implement the generator
  (`ICR-05`), with no additional undocumented decisions needed;
- `ELI` confirms that the seeded generator (`ELI-02`) can produce these distributions reproducibly;
- `ASH` and `JCK` both confirm this document reflects the final agreed-upon parameters for scenarios
  (ASH-03) and arrival model (JCK-02);
- Everyone agrees the CSV contract in Section 5 matches `docs/formato_csv.md` (JCK-09) once it exists.
