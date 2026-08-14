"""Generate mean +/- 95% CI plots for the scheduling experiment results.

One figure per required metric (turnaround, context_switches, slowdown,
jain_slowdown), each showing all four scenarios as separate panels, with
one bar per algorithm (mean height, error bar = 95% confidence interval
across seeds).

Reuses parse_results.load_experiments() for CSV loading/validation instead
of re-implementing schema checks here.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # headless-safe: never opens a GUI window
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "results"))
from parse_results import (  # noqa: E402
    CsvValidationError,
    VALID_ALGORITHMS,
    VALID_SCENARIOS,
    load_experiments,
)

METRICS = [
    ("turnaround", "Mean turnaround (ticks)"),
    ("context_switches", "Context switches (count)"),
    ("slowdown", "Mean slowdown (dimensionless)"),
    ("jain_slowdown", "Jain's fairness index on slowdown (%)"),
]

Z_95 = 1.96  # two-tailed z-score for a 95% confidence interval


def mean_and_ci95(values: pd.Series) -> tuple[float, float]:
    """Returns (mean, half-width) for a 95% CI, using the formula
    mean +/- 1.96 * (sample_std / sqrt(n)).

    With n == 1 the half-width is 0.0 (no variance can be estimated from
    a single seed) rather than NaN, so a thin/incomplete run still plots
    a visible bar instead of silently disappearing.
    """
    n = values.shape[0]
    mean = float(values.mean())
    if n <= 1:
        return mean, 0.0
    std = float(values.std(ddof=1))
    half_width = Z_95 * std / np.sqrt(n)
    return mean, half_width


def build_summary(df: pd.DataFrame, metric: str) -> pd.DataFrame:
    """Returns a (scenario, algorithm) -> (mean, ci95) table for one metric."""
    rows = []
    for (scenario, algorithm), sub in df.groupby(level=["scenario", "algorithm"], observed=True):
        mean, ci95 = mean_and_ci95(sub[metric])
        rows.append({"scenario": scenario, "algorithm": algorithm, "mean": mean, "ci95": ci95})
    return pd.DataFrame(rows)


def plot_metric(df: pd.DataFrame, metric: str, ylabel: str, output_dir: Path) -> Path:
    summary = build_summary(df, metric)

    scenarios = sorted(VALID_SCENARIOS & set(summary["scenario"].unique()))
    algorithms = sorted(VALID_ALGORITHMS & set(summary["algorithm"].unique()))

    fig, axes = plt.subplots(1, len(scenarios), figsize=(4.2 * len(scenarios), 4.5), sharey=True)
    if len(scenarios) == 1:
        axes = [axes]

    x_positions = np.arange(len(algorithms))
    bar_color = "#4C72B0"

    for ax, scenario in zip(axes, scenarios):
        scenario_rows = summary[summary["scenario"] == scenario].set_index("algorithm")
        means = [scenario_rows.loc[alg, "mean"] if alg in scenario_rows.index else 0.0 for alg in algorithms]
        errors = [scenario_rows.loc[alg, "ci95"] if alg in scenario_rows.index else 0.0 for alg in algorithms]

        ax.bar(x_positions, means, yerr=errors, capsize=4, color=bar_color, edgecolor="black", linewidth=0.6)
        ax.set_xticks(x_positions)
        ax.set_xticklabels(algorithms, rotation=20, ha="right")
        ax.set_title(scenario, fontsize=10)
        ax.grid(axis="y", linestyle="--", alpha=0.4)

    axes[0].set_ylabel(ylabel)
    fig.suptitle(f"{ylabel} — mean and 95% CI across seeds, by scenario and algorithm")
    fig.tight_layout(rect=(0, 0, 1, 0.94))

    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"{metric}_by_scenario.png"
    fig.savefig(output_path, dpi=150)
    plt.close(fig)
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", help="path to the experiment CSV (e.g. results/scheduling_experiments.csv)")
    parser.add_argument("--output-dir", default="results/plots", help="directory to write PNG figures into")
    args = parser.parse_args()

    try:
        df = load_experiments(args.csv_path)
    except CsvValidationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    output_dir = Path(args.output_dir)
    written = []
    for metric, ylabel in METRICS:
        path = plot_metric(df, metric, ylabel, output_dir)
        written.append(path)
        print(f"wrote {path}")

    print(f"\n{len(written)} figure(s) written to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
