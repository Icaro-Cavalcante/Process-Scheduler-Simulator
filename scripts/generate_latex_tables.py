"""Generate consolidated LaTeX tables (mean +/- 95% CI) from the scheduling
experiment results, ready to paste into the paper.

One table per required metric (turnaround, context_switches, slowdown,
jain_slowdown), with scenarios as rows and algorithms as columns, each cell
showing "mean +/- ci95".

Reuses parse_results.load_experiments() for CSV loading/validation and the
same mean/CI95 formula as generate_plots.py, so the numbers in the tables
and in the figures always match.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

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
    ("turnaround", "Mean turnaround (ticks)", 2),
    ("context_switches", "Context switches (count)", 1),
    ("slowdown", "Mean slowdown", 3),
    ("jain_slowdown", "Jain's fairness index on slowdown (\\%)", 2),
]

Z_95 = 1.96


def mean_and_ci95(values: pd.Series) -> tuple[float, float]:
    """Same formula as generate_plots.py: mean +/- 1.96 * (std / sqrt(n)),
    with half-width 0.0 when only one seed is available.
    """
    n = values.shape[0]
    mean = float(values.mean())
    if n <= 1:
        return mean, 0.0
    std = float(values.std(ddof=1))
    half_width = Z_95 * std / np.sqrt(n)
    return mean, half_width


def escape_latex(text: str) -> str:
    """Minimal escaping for the identifiers this project actually uses
    (scenario/algorithm names) -- not a general-purpose LaTeX escaper.
    """
    return text.replace("_", r"\_")


def build_table(df: pd.DataFrame, metric: str, caption: str, decimals: int, label_suffix: str) -> str:
    scenarios = sorted(VALID_SCENARIOS & set(df.index.get_level_values("scenario").unique()))
    algorithms = sorted(VALID_ALGORITHMS & set(df.index.get_level_values("algorithm").unique()))

    cells: dict[tuple[str, str], str] = {}
    for (scenario, algorithm), sub in df.groupby(level=["scenario", "algorithm"], observed=True):
        mean, ci95 = mean_and_ci95(sub[metric])
        cells[(str(scenario), str(algorithm))] = f"{mean:.{decimals}f} $\\pm$ {ci95:.{decimals}f}"

    column_spec = "l" + "c" * len(algorithms)
    header = " & ".join(["Scenario"] + [escape_latex(a) for a in algorithms])

    lines = [
        r"\begin{table}[htbp]",
        r"\centering",
        f"\\caption{{{caption} (mean $\\pm$ 95\\% CI across seeds)}}",
        f"\\label{{tab:{label_suffix}}}",
        f"\\begin{{tabular}}{{{column_spec}}}",
        r"\toprule",
        header + r" \\",
        r"\midrule",
    ]

    for scenario in scenarios:
        row_cells = [cells.get((scenario, alg), "--") for alg in algorithms]
        row = " & ".join([escape_latex(scenario)] + row_cells)
        lines.append(row + r" \\")

    lines += [
        r"\bottomrule",
        r"\end{tabular}",
        r"\end{table}",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", help="path to the experiment CSV (e.g. results/scheduling_experiments.csv)")
    parser.add_argument("--output", default="results/tables.tex", help="path to write the consolidated .tex file")
    args = parser.parse_args()

    try:
        df = load_experiments(args.csv_path)
    except CsvValidationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    tables = [
        build_table(df, metric, caption, decimals, label_suffix=metric)
        for metric, caption, decimals in METRICS
    ]

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n\n".join(tables) + "\n", encoding="utf-8")

    print(f"wrote {len(tables)} table(s) to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
