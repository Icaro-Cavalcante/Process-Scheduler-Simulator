# Experiment Output CSV Format


## 1. Granularity

Each row of the CSV corresponds to **one complete simulation run**, i.e. a unique combination of `(algorithm, scenario, seed)`. This is not a per-process row — per-process metrics are collected internally by the C simulator and **aggregated** before being written to this row.

With 4 algorithms × 4 scenarios × ≥100 seeds, the final file for the minimum required configuration will have **≥ 1,600 rows** (matches the acceptance criterion: "Runner automatically executes 1,600+ tests, generating the final CSV").

## 2. Columns

| # | Column | Type | Domain / unit | Description |
|---|---|---|---|---|
| 1 | `algorithm` | string | `FCFS`, `RR`, `Priority`, `PAN` | Identifier of the algorithm evaluated in this run. Use exactly these labels (case-sensitive) across all scripts. |
| 2 | `scenario` | string | `balanced_random`, `io_bound`, `cpu_bound`, `priority_unbalanced` | One of the 4 mandatory scenarios. |
| 3 | `seed` | integer | ≥ 1 | Seed used to generate the deterministic workload for this run. The same seed, in the same scenario, must reproduce the exact same set of processes in any future run. |
| 4 | `turnaround` | float | milliseconds (ms), 2 decimal places | **Average turnaround** across all processes in that run — mean time between arrival and completion. |
| 5 | `context_switches` | integer | absolute count | Total number of context switches that occurred during the entire run (sum, not average). |
| 6 | `slowdown` | float | dimensionless, 3 decimal places | **Average slowdown** across all processes in that run (`turnaround_i / ideal_min_time_i`). |
| 7 | `jain_slowdown` | float | percentage (0–100), 2 decimal places | **Jain's fairness index applied to slowdown**, computed inside the C simulator from the full per-process slowdown distribution of that run (see Section 4). Values close to 100 indicate processes had similar slowdowns; lower values indicate some processes were proportionally more penalized than others. |

## 3. General conventions

- **Field separator:** comma (`,`).
- **Decimal separator:** period (`.`), never comma — avoids clashing with the field separator.
- **Encoding:** UTF-8, no BOM.
- **Header:** mandatory first line, with column names exactly as listed above (lowercase, snake_case).
- **Recommended ordering:** `scenario → algorithm → seed`, for easier visual inspection and Git diffs — but the consolidation scripts (ICR-08) must not depend on row order, only on column names.
- **A failed/corrupted seed run** should not produce a row at all (avoid silent zeros or sentinel values, which would distort averages and the 95% CI); the failure should be logged separately, not written to the CSV.

## 4. Open issue for the team

The assignment (Section 9) requires **Jain's fairness index applied to slowdown** as a mandatory metric. Jain's formula,

```
Jain_slowdown(%) = (Σ slowdown_i)² / (n · Σ slowdown_i²) × 100
```

depends on the **full distribution** of per-process slowdown within a run (sum and sum of squares), not just the average. Since this CSV only stores the **average** `slowdown` per `(algorithm, scenario, seed)`, it is **not sufficient** on its own for it to recompute Jain's index later in Python.

**Decision:** compute Jain's slowdown index inside the C simulator (where per-process metrics already exist), and expose it as the `jain_slowdown` column (Section 2). This keeps the CSV small and puts the metric's responsibility where the raw data already lives, instead of shipping a second, per-process file just to recompute it later in Python.

## 5. Example (first rows)

```
algorithm,scenario,seed,turnaround,context_switches,slowdown,jain_slowdown
FCFS,balanced_random,1,999.31,45,2.063,78.42
RR,balanced_random,1,898.92,3021,1.924,91.15
Priority,balanced_random,1,980.95,52,2.448,64.30
PAN,balanced_random,1,912.40,48,1.812,93.87
```

> The attached `scheduling_experiments.csv` file contains an **example/mock** dataset (80 rows — 4 algorithms × 4 scenarios × 5 illustrative seeds), generated only to visually validate the format before approval. These are not real simulation results: the C simulator still needs to be implemented and run to produce the definitive CSV with ≥100 seeds per scenario.
