# 👨🏾‍💻 Process Scheduler Simulator

> **Sistemas Operacionais — Projeto da Unidade 3 (UFCA)**  
> A discrete-event CPU scheduling simulator written in C (C11) to evaluate classic scheduling policies (FCFS, Round Robin, Static Priority) and **AJIE** (*Aisha-Jackson-Icaro-Elilucio*), an original deterministic aging priority algorithm designed to eliminate starvation in non-preemptive environments without relying on future execution oracles.

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Key Features](#-key-features)
- [Algorithmic Architecture & Proposed AJIE Scheduler](#-algorithmic-architecture--proposed-ajie-scheduler)
  - [Classic Algorithms](#classic-algorithms)
  - [Proposed Algorithm: AJIE](#proposed-algorithm-ajie-aisha-jackson-icaro-elilucio)
- [Simulation Benchmark Scenarios](#-simulation-benchmark-scenarios)
- [Experimental Results & Evaluation](#-experimental-results--evaluation)
- [📦 Directory Structure](#-directory-structure)
- [🚀 Quick Start & Usage](#-quick-start--usage)
  - [Prerequisites](#prerequisites)
  - [Compilation & Execution](#compilation--execution)
  - [Generating Plots & LaTeX Tables](#generating-plots--latex-tables)
  - [Validating Results](#validating-results)
  - [🐳 Running with Docker](#-running-with-docker)
  - [Clean Build Artifacts](#clean-build-artifacts)
- [👤 Members & Team Roles](#-members--team-roles)
- [👨‍🏫 Professor](#-professor)
- [🎯 Acceptance Criteria & Evaluation Checklist](#-acceptance-criteria--evaluation-checklist)
- [📚 References & Documentation Links](#-references--documentation-links)

---

## 🔍 Overview

CPU process scheduling is a foundational mechanism in multiprogrammed operating systems, directly impacting system throughput, turnaround time, context switch overhead, and fairness. Standard non-preemptive priority algorithms suffer from **starvation**, where low-priority processes are indefinitely postponed under continuous high-priority arrivals. Conversely, time-sliced algorithms like Round Robin guarantee responsiveness but introduce substantial **context switch penalties** ($C_{\text{sw}} > 0$).

This project provides a complete experimental framework:
1. **C Discrete Simulator**: High-performance, tick-based discrete event engine supporting multi-device parallel I/O queues (Min-Heap), configurable context switch penalties, and deterministic random seed control (via `xoshiro256**` & `SplitMix64`).
2. **AJIE Scheduler**: A novel non-preemptive aging priority algorithm that mathematically bounds process starvation while preserving high CPU throughput.
3. **Statistical Pipeline**: Automated Python scripts evaluating 1,600 execution runs across 4 distinct scenarios and 100 independent seeds, generating 95% Confidence Intervals (IC95% via t-Student) and Jain's Fairness Index on process slowdown.
4. **IEEE Scientific Paper & Defense**: A complete 6-page IEEE paper and presentation slides documenting empirical findings.

---

## ✨ Key Features

- **High Statistical Rigor**: 1,000 processes per run $\times$ 100 independent seeds per scenario $\times$ 4 scenarios $\times$ 4 algorithms = **1,600 complete simulation runs**.
- **Deterministic Workload Generation**: Guarantees **100% cross-algorithm workload invariance** for any given seed $S$ and scenario $C$.
- **Explicit Overhead Modeling**: Accounts for hardware context switch costs ($C_{\text{sw}} = 2$ ticks per transition) to penalize excessive time preemption realistically.
- **Fairness Quantification**: Uses **Jain's Fairness Index** ($J_{\text{slowdown}} \in [0, 100\%]$) on normalized process slowdown ($S_i = T_{\text{turnaround}, i} / T_{\text{ideal}, i}$).
- **Automated Report Generation**: One command (`make reports`) compiles the simulator, executes all 1,600 runs, generates high-resolution figures with IC95% error bars, and formats LaTeX result tables for direct article inclusion.

---

## ⚡ Algorithmic Architecture & Proposed AJIE Scheduler

### Classic Algorithms

1. **FCFS (First-Come, First-Served)**:
   - *Policy*: Non-preemptive, FIFO ready queue ($T_{\text{arrival}}$ order).
   - *Characteristics*: Minimal context-switch overhead ($N_{\text{sw}}$), but vulnerable to the **convoy effect** under CPU-bound tasks.
2. **Round Robin (RR)**:
   - *Policy*: Time-sliced preemptive ($q = 4$ or $q = 10$ ticks).
   - *Characteristics*: Excellent interactive responsiveness, but incurs severe context switch overhead ($C_{\text{sw}}$) when executing computational workloads.
3. **Static Priority (Non-Preemptive)**:
   - *Policy*: Selects ready process with highest static priority level ($P_{\text{base}} \in [1, 10]$, lower integer = higher priority).
   - *Characteristics*: Fails under asymmetric priority distributions, suffering severe **starvation** for low-priority processes.

---

### Proposed Algorithm: AJIE (*Aisha-Jackson-Icaro-Elilucio*)

**AJIE** is designed to eliminate starvation in non-preemptive priority environments without requiring future CPU burst time prediction (oracles).

```
Ready Queues (L levels, Level 1 = Highest Priority)
┌─────────────────────────────────────────────────────────┐
│ Queue 1 (P_eff = 1): [P_a] -> [P_b] -> ... (FIFO)       │  <- Selected first
├─────────────────────────────────────────────────────────┤
│ Queue 2 (P_eff = 2): [P_c] -> ...         (FIFO)       │  <- Aged to Q1 at decision
├─────────────────────────────────────────────────────────┤
│ ...                                                     │
├─────────────────────────────────────────────────────────┤
│ Queue L (P_eff = L): [P_z] -> ...         (FIFO)       │  <- Lowest priority
└─────────────────────────────────────────────────────────┘
```

#### Core Mechanics & Rules:
1. **Multi-Queue Structure**: Maintains $L = 10$ distinct FIFO ready queues corresponding to effective priorities $P_{\text{eff}} \in [1, L]$.
2. **Deterministic Aging Pass**: At every scheduling decision point (when CPU becomes idle or current process finishes/blocks), waiting processes in queues $Q_2 \dots Q_L$ receive an aging promotion:
   $$P_{\text{eff}}^{\text{new}} = \max(P_{\text{eff}}^{\text{old}} - 1, 1)$$
   The aging pass traverses $Q_2 \to Q_L$ sequentially, ensuring a process is promoted at most **once per scheduling round**.
3. **Anti-Starvation Theorem**: For a system with $L$ levels, any process $P_i$ with base priority $P_{\text{base}}$ will be passed over at most:
   $$B_{\text{max}} = L - P_{\text{base}}$$
   times before reaching Queue 1 ($Q_1$), where it is served in strict FIFO order.
4. **No Artificial Initial Promotion**: Newly arrived or newly unblocked processes are enqueued at their base/effective level but are excluded from aging during the round of their arrival.
5. **Return from I/O**: Upon completing I/O, a process restores its effective priority to its base priority ($P_{\text{eff}} \leftarrow P_{\text{base}}$), preventing permanent priority inflation.
6. **Efficiency**: Operates with $O(N_{\text{ready}})$ aging pass overhead and $O(1)$ queue selection cost using double-ended linked list queues.

---

## 📊 Simulation Benchmark Scenarios

The workload generator synthesizes 4 mandatory stress-test scenarios (1,000 processes per run):

| Scenario | Code | CPU Burst (ticks) | I/O Reqs | I/O Duration (ticks) | Inter-arrival ($\Delta t$) | Priority Distribution | Objective |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **1. Balanced Random** | `balanced_random` | $[5, 50]$ | $[1, 5]$ | $[10, 30]$ | $\sim 5$ | $[1, 10]$ Uniform | General-purpose heterogeneous load. |
| **2. I/O-Bound** | `io_bound` | $[1, 8]$ | $[6, 15]$ | $[20, 60]$ | $\sim 3$ | $[1, 10]$ Uniform | High queue transition frequency & I/O device concurrency. |
| **3. CPU-Bound** | `cpu_bound` | $[40, 200]$ | $[0, 2]$ | $[5, 15]$ | $\sim 12$ | $[1, 10]$ Uniform | Computational intensity & context switch overhead impact. |
| **4. Priority Unbalanced** | `priority_unbalanced` | $[5, 50]$ | $[1, 5]$ | $[10, 30]$ | $\sim 4$ | $85\% \in [1,3]$, $15\% \in [8,10]$ | Starvation stress test for static priority schedulers. |

---

## 📈 Experimental Results & Evaluation

Aggregated empirical findings across 1,600 simulation runs ($N = 1000$ processes, 100 seeds/scenario, IC95%):

### Performance Summary Table

| Metric | Scenario | AJIE (Proposed) | FCFS | Static Priority | Round Robin (RR) |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Turnaround Time** $\bar{T}$ *(ticks)* | `balanced_random` | $82,113 \pm 269$ | $82,318 \pm 268$ | $56,770 \pm 181$ | $124,061 \pm 398$ |
| | `io_bound` | $63,253 \pm 117$ | $60,461 \pm 117$ | $36,222 \pm 62$ | $70,090 \pm 141$ |
| | `cpu_bound` | $141,326 \pm 649$ | $142,007 \pm 647$ | $116,147 \pm 429$ | $252,854 \pm 1055$ |
| | `priority_unbalanced` | $82,662 \pm 320$ | $82,872 \pm 322$ | $57,213 \pm 177$ | $124,414 \pm 468$ |
| **Context Switches** $N_{\text{sw}}$ | `balanced_random` | $3,004 \pm 8$ | $4,004 \pm 8$ | $4,004 \pm 8$ | $29,093 \pm 69$ |
| | `io_bound` | $10,503 \pm 16$ | $11,503 \pm 16$ | $11,503 \pm 16$ | $17,264 \pm 25$ |
| | `cpu_bound` | $998 \pm 5$ | $1,998 \pm 5$ | $1,998 \pm 5$ | $60,747 \pm 189$ |
| | `priority_unbalanced` | $2,999 \pm 9$ | $3,999 \pm 9$ | $3,999 \pm 9$ | $29,050 \pm 78$ |
| **Jain's Index** $J_{\text{slowdown}}$ *(% point)* | `balanced_random` | **90.55%** | 90.60% | 54.45% | 95.73% |
| | `io_bound` | **98.02%** | 97.68% | 67.28% | 97.27% |
| | `cpu_bound` | **77.71%** | 77.68% | 46.97% | 96.34% |
| | `priority_unbalanced` | **90.72%** | 90.79% | **54.65%** | 95.74% |

### Key Analytical Takeaways:
1. **Starvation Eradication**: In `priority_unbalanced`, Static Priority collapses to a **54.65%** Jain index. **AJIE elevates fairness to 90.72%** (+66.0% relative gain), completely eliminating starvation.
2. **Context Switch Suppression**: Round Robin experiences a context switch explosion in `cpu_bound` ($60,747.6$ switches, $60.8\times$ higher than AJIE), degrading turnaround time by +78.9%. AJIE retains non-preemptive speed ($998.9$ switches).
3. **I/O Superiority**: In `io_bound`, AJIE achieves the highest fairness index (**98.02%**), outperforming even Round Robin (**97.27%**) and FCFS (**97.68%**).

---

## 📦 Directory Structure

```
Process-Scheduler-Simulator/
├── docs/                                    # Detailed Technical Specifications & Models
│   ├── ajie_custom_algorithm.md             # Complete specification of the AJIE algorithm
│   ├── context_switch.md                    # Context switch cost modeling & accounting
│   ├── csv_format.md                        # Output CSV schema contract & data rules
│   ├── io_modeling.md                       # I/O queue structure & concurrency modeling
│   ├── process_model.md                     # Process attributes, states, & lifecycle transitions
│   └── scenarios.md                         # Numerical parameters for the 4 benchmark scenarios
├── include/                                 # C Header Files
│   ├── ajie.h                               # AJIE scheduler functions & state
│   ├── experiment_runner.h                  # Multi-seed experiment execution engine
│   ├── fcfs.h                               # FCFS scheduler headers
│   ├── generator.h                          # Workload generator interface
│   ├── io_queue.h                           # Min-Heap I/O event queue implementation
│   ├── priority.h                           # Static priority scheduler headers
│   ├── process.h                            # Process struct & metadata definitions
│   ├── queue.h                              # Double-ended FIFO queue data structure
│   ├── rng.h                                # xoshiro256** & SplitMix64 pseudo-RNG engine
│   ├── round_robin.h                        # Round Robin scheduler headers
│   ├── scheduler.h                          # Common scheduler interface & context switch ticks
│   └── seed.h                               # Seed configuration & initialization
├── report/                                  # Project Paper & Reports
│   └── relatorio.pdf                        # IEEE formatted scientific article (PDF)
├── results/                                 # Simulation Output Data & Analysis Scripts
│   ├── parse_results.py                     # Python validator for CSV data schemas & IC95%
│   ├── scheduling_experiments.csv           # Raw dataset from 1,600 experiment runs
│   ├── failed_runs.log                      # Log file for aborted simulation runs (if any)
│   └── plots/                               # Generated high-resolution charts (PNG)
├── scripts/                                 # Automation & Runner Scripts
│   ├── experiment_runner.c                  # Core simulation loop across seeds & scenarios
│   ├── generate_latex_tables.py             # Python script generating LaTeX tables
│   ├── generate_plots.py                    # Python script generating IC95% comparison charts
│   └── main_example.c                       # Minimal standalone example runner
├── src/                                     # C Implementation Source Code
│   ├── ajie.c                               # Core AJIE algorithm & aging logic
│   ├── fcfs.c / fcfs_wrapper.c              # FCFS implementation & interface wrapper
│   ├── generator.c                          # Seeded process workload generator
│   ├── io_queue.c                           # I/O device queue & event handling
│   ├── main.c                               # Simulator entry point
│   ├── priority.c                           # Static priority algorithm implementation
│   ├── process.c                            # Process creation & lifecycle logic
│   ├── queue.c                              # FIFO queue implementation
│   ├── rng.c / seed.c                       # Seed initialization & PRNG algorithms
│   └── round_robin.c / rr_wrapper.c         # Round Robin algorithm implementation
├── makefile                                 # Automated build system configuration
├── requirements.txt                         # Python dependencies for visualization scripts
├── Dockerfile                               # Docker container specification (GCC + Python environment)
├── .dockerignore                            # Excluded files for Docker build context
└── README.md                                # Repository overview & documentation
```

---

## 🚀 Quick Start & Usage

### Prerequisites

- **C Compiler**: GCC (or Clang) supporting C11 standard.
- **Build System**: GNU Make.
- **Python**: Python 3.8+ (for plots and statistical tables).
- **Docker** *(Optional)*: Docker Engine or Docker Desktop for containerized execution without local toolchains.

### Compilation & Execution

1. **Clone the repository**:
   ```bash
   git clone https://github.com/Icaro-Cavalcante/Process-Scheduler-Simulator.git
   cd Process-Scheduler-Simulator
   ```

2. **Build the C simulator**:
   ```bash
   make
   ```
   *Creates the binary target at `./build/simulator`.*

3. **Execute simulation experiments**:
   ```bash
   make run
   ```
   *Executes all 1,600 simulation runs (100 seeds $\times$ 4 scenarios $\times$ 4 algorithms) and outputs `results/scheduling_experiments.csv`.*

### Generating Plots & LaTeX Tables

1. **Install Python dependencies**:
   ```bash
   make deps
   ```
   *If using a managed Python environment (PEP 668), run `make deps PIP_FLAGS=--break-system-packages` or use a virtualenv (`python3 -m venv .venv && source .venv/bin/activate && make deps`).*

2. **Generate visualization charts & LaTeX tables**:
   ```bash
   make reports
   ```
   *Runs `make plots` (saving PNG charts in `results/plots/`) and `make tables` (saving LaTeX code in `results/tables.tex`).*

### Validating Results

Validate that the CSV output conforms strictly to the schema specification:
```bash
python3 results/parse_results.py results/scheduling_experiments.csv
```

### 🐳 Running with Docker

You can build and run the entire simulation pipeline in an isolated container without needing to configure local C compilers or Python environments.

1. **Build the Docker image**:
   ```bash
   docker build -t process-scheduler-simulator .
   ```

2. **Execute the full pipeline (Simulation + Plots + Tables)**:
   Mount the `results/` folder as a volume so that generated CSVs, figures, and LaTeX tables are saved directly to your host machine:

   - **Linux / macOS / Git Bash**:
     ```bash
     docker run --rm -v $(pwd)/results:/app/results process-scheduler-simulator
     ```
   - **Windows (PowerShell)**:
     ```powershell
     docker run --rm -v ${PWD}/results:/app/results process-scheduler-simulator
     ```
   - **Windows (Command Prompt)**:
     ```cmd
     docker run --rm -v "%cd%/results":/app/results process-scheduler-simulator
     ```

3. **Running Specific Tasks with Docker**:
   - **Run only the C simulation**:
     ```bash
     docker run --rm -v $(pwd)/results:/app/results process-scheduler-simulator make run
     ```
   - **Generate only charts and LaTeX tables**:
     ```bash
     docker run --rm -v $(pwd)/results:/app/results process-scheduler-simulator make plots tables
     ```
   - **Validate CSV results**:
     ```bash
     docker run --rm -v $(pwd)/results:/app/results process-scheduler-simulator python3 results/parse_results.py results/scheduling_experiments.csv
     ```
   - **Interactive container shell**:
     ```bash
     docker run --rm -it -v $(pwd)/results:/app/results process-scheduler-simulator bash
     ```

### Clean Build Artifacts
```bash
make clean
```

---

## 👤 Members & Team Roles

This project was developed by software engineering students as a collaborative team effort with specialized role distribution:

| Contributor 🧑‍🎓 | GitHub Profile | Primary Roles 🚀 | Strategic Focus & Responsibilities |
| :--- | :--- | :--- | :--- |
| **Icaro Cavalcante** | [@Icaro-Cavalcante](https://github.com/Icaro-Cavalcante) | Developer, Repository Manager, Automation | Setup & GitHub organization, C process struct (`process.h/.c`), seeded workload generator (`generator.c`), FCFS & Round Robin algorithms, Python result validation (`parse_results.py`). |
| **Elilúcio Teixeira** | [@Elilucio7](https://github.com/Elilucio7) | Developer, Simulation Motor, Integration | Build system (`Makefile`), simulation core engine, ready/IO queues, static priority algorithm, metric calculation scripts & IC95% plot generators (`generate_plots.py`). |
| **Samuel Jackson** | [@SJacksonML](https://github.com/SJacksonML) | Modeling, Research Paper Writer, LaTeX Lead | Context switch & arrival modeling, AJIE algorithm specification (`docs/ajie_custom_algorithm.md`), primary LaTeX IEEE article writer (`relatorio/artigo.tex`), CSV format contract. |
| **Ana Aisha** | [@aishatomaz](https://github.com/aishatomaz) | Modeling, Research Paper Revisor, Slide Designer | Process & I/O model specification (`docs/process_model.md`, `docs/io_modeling.md`), simulation scenario parameters (`docs/scenarios.md`), IEEE paper formatting/review, presentation slide design (`slides/apresentacao.pptx`). |

---

## 👨‍🏫 Professor

| Professor 👨‍🏫 | Affiliation | Institution |
| :--- | :--- | :--- |
| **[Weskley Vinicius](https://br.linkedin.com/in/weskley-vinicius-fernandes-mauricio-96288216a)** | Department of Software Engineering | Universidade Federal do Cariri (UFCA) |

---


## 📚 References & Documentation Links

### Internal Specifications
- [AJIE Custom Algorithm Specification](docs/ajie_custom_algorithm.md)
- [Simulation Benchmark Scenarios](docs/scenarios.md)
- [Process Model & Lifecycle States](docs/modelo_processo.md)
- [I/O Queue & Concurrency Model](docs/modelagem_io.md)
- [Context Switch Cost Specifications](docs/troca_contexto.md)
- [CSV Dataset Output Contract](docs/csv_format.md)

### External Scientific References
1. **Jain's Fairness Index**: R. Jain, D.-M. Chiu, and W. R. Hawe, *"A quantitative measure of fairness and permutation invariance for resource allocation in shared computer systems"*, Digital Equipment Corporation, Tech. Rep. DEC-TR-301, 1984. [PDF Link](https://www.cse.wustl.edu/~jain/atmf/ftp/af_fair.pdf)
2. **Pseudo-RNG Generators**: D. Blackman and S. Vigna, *"Scrambled linear pseudorandom number generators"*, ACM Transactions on Mathematical Software (TOMS), vol. 47, no. 4, pp. 1–32, 2021.
3. **Operating System Concepts**: A. Silberschatz, P. B. Galvin, and G. Gagne, *Operating System Concepts*, 10th ed. Wiley, 2018.
4. **Modern Operating Systems**: A. S. Tanenbaum and H. Bos, *Modern Operating Systems*, 4th ed. Pearson, 2015.
