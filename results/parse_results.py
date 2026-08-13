from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd

# ---- schema contract, mirrors docs/csv_format.md exactly ------------------

EXPECTED_COLUMNS = [
    "algorithm",
    "scenario",
    "seed",
    "turnaround",
    "context_switches",
    "slowdown",
    "jain_slowdown",
]

VALID_ALGORITHMS = {"FCFS", "RR", "Priority", "AJIE"}
VALID_SCENARIOS = {
    "balanced_random",
    "io_bound",
    "cpu_bound",
    "priority_unbalanced",
}

EXPECTED_DTYPES = {
    "algorithm": "category",
    "scenario": "category",
    "seed": "int64",
    "turnaround": "float64",
    "context_switches": "int64",
    "slowdown": "float64",
    "jain_slowdown": "float64",
}

MIN_SEEDS_PER_GROUP = 100  # assignment minimum (Section 7)


class CsvValidationError(ValueError):
    """Raised when the input CSV does not conform to docs/csv_format.md."""


def load_experiments(csv_path: str | Path) -> pd.DataFrame:
    """Load, validate and type the experiment CSV.

    Returns a DataFrame indexed by (scenario, algorithm, seed), sorted,
    with `algorithm`/`scenario` as pandas categoricals restricted to the
    domains declared in docs/csv_format.md.

    Raises CsvValidationError with a descriptive message if the file does
    not match the expected schema — fail loudly here rather than let a
    malformed CSV silently corrupt later statistics.
    """
    csv_path = Path(csv_path)
    if not csv_path.exists():
        raise CsvValidationError(f"file not found: {csv_path}")

    df = pd.read_csv(csv_path)

    _validate_columns(df, csv_path)
    _validate_domains(df, csv_path)
    _validate_no_missing_values(df, csv_path)
    _validate_unique_runs(df, csv_path)

    df["algorithm"] = pd.Categorical(df["algorithm"], categories=sorted(VALID_ALGORITHMS))
    df["scenario"] = pd.Categorical(df["scenario"], categories=sorted(VALID_SCENARIOS))
    df["seed"] = df["seed"].astype("int64")
    df["context_switches"] = df["context_switches"].astype("int64")
    for col in ("turnaround", "slowdown", "jain_slowdown"):
        df[col] = df[col].astype("float64")

    df = df.set_index(["scenario", "algorithm", "seed"]).sort_index()
    return df


def _validate_columns(df: pd.DataFrame, csv_path: Path) -> None:
    missing = [c for c in EXPECTED_COLUMNS if c not in df.columns]
    extra = [c for c in df.columns if c not in EXPECTED_COLUMNS]
    if missing:
        raise CsvValidationError(
            f"{csv_path}: missing required column(s): {missing}. "
            f"Expected exactly: {EXPECTED_COLUMNS} (see docs/csv_format.md)."
        )
    if extra:
        raise CsvValidationError(
            f"{csv_path}: unexpected column(s): {extra}. "
            "If this is intentional (e.g. a new metric), update "
            "EXPECTED_COLUMNS here AND docs/csv_format.md together."
        )


def _validate_domains(df: pd.DataFrame, csv_path: Path) -> None:
    bad_algorithms = set(df["algorithm"].unique()) - VALID_ALGORITHMS
    if bad_algorithms:
        raise CsvValidationError(
            f"{csv_path}: unknown algorithm label(s) {bad_algorithms}; "
            f"expected one of {sorted(VALID_ALGORITHMS)}."
        )
    bad_scenarios = set(df["scenario"].unique()) - VALID_SCENARIOS
    if bad_scenarios:
        raise CsvValidationError(
            f"{csv_path}: unknown scenario label(s) {bad_scenarios}; "
            f"expected one of {sorted(VALID_SCENARIOS)}."
        )


def _validate_no_missing_values(df: pd.DataFrame, csv_path: Path) -> None:
    na_counts = df[EXPECTED_COLUMNS].isna().sum()
    if na_counts.any():
        offending = na_counts[na_counts > 0].to_dict()
        raise CsvValidationError(
            f"{csv_path}: missing values found: {offending}. "
            "Per docs/csv_format.md, a failed run must not produce a row "
            "at all — check src/experiment_runner.c's failure logging "
            "instead of shipping NaNs/sentinels here."
        )


def _validate_unique_runs(df: pd.DataFrame, csv_path: Path) -> None:
    key = ["algorithm", "scenario", "seed"]
    duplicates = df[df.duplicated(subset=key, keep=False)]
    if not duplicates.empty:
        raise CsvValidationError(
            f"{csv_path}: duplicate (algorithm, scenario, seed) rows found "
            f"({len(duplicates)} rows) — each combination must appear exactly "
            f"once.\n{duplicates[key].to_string(index=False)}"
        )


def group_by_scenario_algorithm(df: pd.DataFrame) -> dict[tuple[str, str], pd.DataFrame]:
    """Split the indexed DataFrame into one sub-DataFrame per
    (scenario, algorithm) pair, each still indexed by seed.
    """
    groups: dict[tuple[str, str], pd.DataFrame] = {}
    for (scenario, algorithm), sub in df.groupby(level=["scenario", "algorithm"], observed=True):
        groups[(str(scenario), str(algorithm))] = sub.droplevel(["scenario", "algorithm"])
    return groups


def check_coverage(df: pd.DataFrame, min_seeds: int = MIN_SEEDS_PER_GROUP) -> list[str]:
    """Return a list of human-readable warnings for any (scenario,
    algorithm) group with fewer than `min_seeds` rows.
    """
    warnings: list[str] = []
    counts = df.groupby(level=["scenario", "algorithm"], observed=True).size()
    for (scenario, algorithm), count in counts.items():
        if count < min_seeds:
            warnings.append(
                f"{scenario} / {algorithm}: only {count} seed(s) "
                f"(expected >= {min_seeds})"
            )
    return warnings


def build_quick_summary(df: pd.DataFrame) -> pd.DataFrame:
    """Lightweight sanity-check summary (mean per metric, per scenario x algorithm).
    """
    metrics = ["turnaround", "context_switches", "slowdown", "jain_slowdown"]
    summary = (
        df.groupby(level=["scenario", "algorithm"], observed=True)[metrics]
        .mean()
        .round(3)
        .sort_index()
    )
    return summary


def _print_report(df: pd.DataFrame, coverage_warnings: list[str], summary: pd.DataFrame) -> None:
    print(f"Loaded {len(df)} rows.")
    print(
        f"Scenarios: {sorted(df.index.get_level_values('scenario').unique())}\n"
        f"Algorithms: {sorted(df.index.get_level_values('algorithm').unique())}"
    )

    if coverage_warnings:
        print("\nCoverage warnings:")
        for w in coverage_warnings:
            print(f"  - {w}")
    else:
        print(f"\nCoverage OK: every (scenario, algorithm) pair has >= {MIN_SEEDS_PER_GROUP} seeds.")

    print("\nQuick summary (mean per scenario x algorithm):")
    print(summary.to_string())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", help="path to the experiment CSV (e.g. results/scheduling_experiments.csv)")
    parser.add_argument("--scenario", choices=sorted(VALID_SCENARIOS), help="filter the report to a single scenario")
    parser.add_argument("--algorithm", choices=sorted(VALID_ALGORITHMS), help="filter the report to a single algorithm")
    parser.add_argument("--summary-out", help="optional path to also save the quick summary as CSV")
    args = parser.parse_args()

    try:
        df = load_experiments(args.csv_path)
    except CsvValidationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.scenario:
        df = df[df.index.get_level_values("scenario") == args.scenario]
    if args.algorithm:
        df = df[df.index.get_level_values("algorithm") == args.algorithm]

    if df.empty:
        print("error: no rows left after filtering", file=sys.stderr)
        return 1

    coverage_warnings = check_coverage(df)
    summary = build_quick_summary(df)
    _print_report(df, coverage_warnings, summary)

    if args.summary_out:
        summary.to_csv(args.summary_out)
        print(f"\nSummary saved to {args.summary_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
