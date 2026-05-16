import argparse
import csv
import os
import statistics
import tomllib
from collections import defaultdict

import matplotlib.pyplot as plt
import pandas as pd


# ── Config loading ─────────────────────────────────────────────────────────────

def load_config(path: str = "configs/config.toml") -> dict:
    with open(path, "rb") as f:
        return tomllib.load(f)


# ── Aggregation helper ─────────────────────────────────────────────────────────

def aggregate(results_csv: str, output_csv: str) -> pd.DataFrame:
    groups: dict = defaultdict(list)

    with open(results_csv, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row["graph_type"], row["cap_type"], int(row["n"]), int(row["d"]), int(row["hi"]))
            groups[key].append(float(row["median_time"]))

    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "graph_type", "cap_type", "n", "d", "hi",
            "mean_time", "std_dev", "min_time", "max_time", "count",
        ])
        for (graph_type, cap_type, n, d, hi) in sorted(
            groups.keys(), key=lambda x: (x[0], x[1], x[4], x[3], x[2])
        ):
            times  = groups[(graph_type, cap_type, n, d, hi)]
            mean_t = statistics.mean(times)
            std_t  = statistics.stdev(times) if len(times) > 1 else 0.0
            writer.writerow([
                graph_type, cap_type, n, d, hi,
                f"{mean_t:.17f}", f"{std_t:.17f}",
                f"{min(times):.17f}", f"{max(times):.17f}",
                len(times),
            ])

    print("Aggregazione completata →", output_csv)

    df = pd.read_csv(output_csv)
    for col in ("n", "d", "hi", "mean_time", "std_dev"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    return df.sort_values(["graph_type", "cap_type", "d", "hi"]).reset_index(drop=True)


# ── Plot 1 — Doubling of d ─────────────────────────────────────────────────────
#   One file per (graph_type, cap_type, hi).
#   x-axis : d  —  single curve (hi is fixed for that file).

def plot_doubling_d(df: pd.DataFrame, out_dir: str, gstyle: dict, graph_type: str, cap_type: str) -> None:
    sub = df[(df["graph_type"] == graph_type) & (df["cap_type"] == cap_type)].copy()
    if sub.empty:
        return

    color   = gstyle.get("color", "steelblue")
    cap_out = os.path.join(out_dir, graph_type, cap_type)
    os.makedirs(cap_out, exist_ok=True)

    for hi in sorted(sub["hi"].unique()):
        data = sub[sub["hi"] == hi].sort_values("d")
        if data.empty:
            continue

        fig, ax = plt.subplots(figsize=(8, 5))
        ax.plot(
            data["d"], data["mean_time"],
            marker="o", markersize=6, color=color,
            linewidth=1.8,
        )
        ax.fill_between(
            data["d"],
            data["mean_time"] - data["std_dev"],
            data["mean_time"] + data["std_dev"],
            color=color, alpha=0.15,
        )
        ax.set_xscale("log", base=2)
        ax.set_xticks(sorted(data["d"].unique()))
        ax.set_xticklabels([str(int(v)) for v in sorted(data["d"].unique())])
        ax.set_title(
            f"{gstyle.get('label_prefix', graph_type.capitalize())} [{cap_type}]  —  doubling d  (hi = {int(hi)})",
            fontsize=13,
        )
        ax.set_xlabel("d (degree / layers)", fontsize=12)
        ax.set_ylabel("Mean time (seconds)", fontsize=12)
        ax.grid(True, linestyle="--", alpha=0.4)
        fig.tight_layout()

        fname    = f"{graph_type}_{cap_type}_doubling_d_hi{int(hi)}.png"
        out_path = os.path.join(cap_out, fname)
        fig.savefig(out_path, dpi=150)
        plt.close(fig)
        print(f"  Salvato: {graph_type}/{cap_type}/{fname}")


# ── Plot 2 — Doubling of hi ────────────────────────────────────────────────────
#   One file per (graph_type, cap_type, d).
#   x-axis : hi  —  single curve (d is fixed for that file).
#   Skipped when fewer than 2 hi values exist for the combination.

def plot_doubling_hi(df: pd.DataFrame, out_dir: str, gstyle: dict, graph_type: str, cap_type: str) -> None:
    sub = df[(df["graph_type"] == graph_type) & (df["cap_type"] == cap_type)].copy()
    if sub.empty or sub["hi"].nunique() < 2:
        return

    color   = gstyle.get("color", "steelblue")
    cap_out = os.path.join(out_dir, graph_type, cap_type)
    os.makedirs(cap_out, exist_ok=True)

    for d in sorted(sub["d"].unique()):
        data = sub[sub["d"] == d].sort_values("hi")
        if data.empty:
            continue

        fig, ax = plt.subplots(figsize=(8, 5))
        ax.plot(
            data["hi"], data["mean_time"],
            marker="o", markersize=6, color=color,
            linewidth=1.8,
        )
        ax.fill_between(
            data["hi"],
            data["mean_time"] - data["std_dev"],
            data["mean_time"] + data["std_dev"],
            color=color, alpha=0.15,
        )
        ax.set_xscale("log", base=2)
        ax.set_xticks(sorted(data["hi"].unique()))
        ax.set_xticklabels([str(int(v)) for v in sorted(data["hi"].unique())])
        ax.set_title(
            f"{gstyle.get('label_prefix', graph_type.capitalize())} [{cap_type}]  —  doubling hi  (d = {int(d)})",
            fontsize=13,
        )
        ax.set_xlabel("hi (max capacity)", fontsize=12)
        ax.set_ylabel("Mean time (seconds)", fontsize=12)
        ax.grid(True, linestyle="--", alpha=0.4)
        fig.tight_layout()

        fname    = f"{graph_type}_{cap_type}_doubling_hi_d{int(d)}.png"
        out_path = os.path.join(cap_out, fname)
        fig.savefig(out_path, dpi=150)
        plt.close(fig)
        print(f"  Salvato: {graph_type}/{cap_type}/{fname}")


# ── Main aggregation + plotting entry point ────────────────────────────────────

def aggregate_and_plot(
    results_csv: str,
    output_csv: str,
    out_dir: str,
    style: dict,
    graph_types: list[str] | None = None,
    cap_types: list[str] | None = None,
    d_values: list[int] | None = None,
) -> None:

    df = aggregate(results_csv, output_csv)

    all_graph_types = graph_types or sorted(df["graph_type"].unique())
    all_cap_types   = cap_types   or sorted(df["cap_type"].unique())

    print(f"graph_type : {all_graph_types}")
    print(f"cap_type   : {all_cap_types}")

    for graph_type in all_graph_types:
        sub_type = df[df["graph_type"] == graph_type]
        if sub_type.empty:
            print(f"  [WARN] nessun dato per graph_type={graph_type!r}, skip.")
            continue

        gstyle = style.get(graph_type, {"color": "gray", "label_prefix": graph_type.capitalize()})

        for cap_type in all_cap_types:
            if sub_type[sub_type["cap_type"] == cap_type].empty:
                continue

            print(f"\n[{graph_type} / {cap_type}]")
            plot_doubling_d(df, out_dir, gstyle, graph_type, cap_type)
            plot_doubling_hi(df, out_dir, gstyle, graph_type, cap_type)

    print(f"\nFatto. I grafici sono nella cartella '{out_dir}/'.")


# ── Entry point ────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Aggrega i risultati CSV e genera i grafici capacity scaling.",
    )
    parser.add_argument(
        "--config", default="configs/config.toml",
        help="Percorso del file di configurazione (default: config.toml)",
    )
    args = parser.parse_args()

    cfg   = load_config(args.config)
    style = cfg["plots"]["style"]

    for i, run in enumerate(cfg["runs"], start=1):
        print(f"\n{'='*60}")
        print(f"Run {i}/{len(cfg['runs'])}  —  {run['results_csv']}")
        print(f"{'='*60}")
        aggregate_and_plot(
            results_csv = run["results_csv"],
            output_csv  = run["output_csv"],
            out_dir     = run["out_dir"],
            style       = style,
            graph_types = run.get("graph_types"),
            cap_types   = run.get("cap_types"),
            d_values    = run.get("d_values"),
        )


if __name__ == "__main__":
    main()