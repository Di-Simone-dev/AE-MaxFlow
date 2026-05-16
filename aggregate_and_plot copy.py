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


# ── Aggregate and plot ─────────────────────────────────────────────────────────

def aggregate_and_plot(
    results_csv: str,
    output_csv: str,
    out_dir: str,
    style: dict,
    graph_types: list[str] | None = None,
    cap_types: list[str] | None = None,
    d_values: list[int] | None = None,
) -> None:

    # ── Aggregation ────────────────────────────────────────────────────────────
    groups: dict = defaultdict(list)

    with open(results_csv, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            graph_type = row["graph_type"]
            cap_type   = row["cap_type"]
            n          = int(row["n"])
            d          = int(row["d"])
            time       = float(row["median_time"])
            groups[(graph_type, cap_type, n, d)].append(time)

    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "graph_type", "cap_type", "n", "d",
            "mean_time", "std_dev", "min_time", "max_time", "count",
        ])

        for (graph_type, cap_type, n, d) in sorted(
            groups.keys(), key=lambda x: (x[0], x[1], x[3], x[2])
        ):
            times  = groups[(graph_type, cap_type, n, d)]
            mean_t = statistics.mean(times)
            std_t  = statistics.stdev(times) if len(times) > 1 else 0.0
            min_t  = min(times)
            max_t  = max(times)

            writer.writerow([
                graph_type, cap_type, n, d,
                f"{mean_t:.17f}",
                f"{std_t:.17f}",
                f"{min_t:.17f}",
                f"{max_t:.17f}",
                len(times),
            ])

    print("Aggregazione completata →", output_csv)

    # ── Plotting ───────────────────────────────────────────────────────────────
    df = pd.read_csv(output_csv)
    df["n"]         = pd.to_numeric(df["n"],         errors="coerce")
    df["d"]         = pd.to_numeric(df["d"],         errors="coerce")
    df["mean_time"] = pd.to_numeric(df["mean_time"], errors="coerce")
    df = df.sort_values(["graph_type", "cap_type", "d", "n"]).reset_index(drop=True)

    all_graph_types = graph_types or sorted(df["graph_type"].unique())
    all_cap_types   = cap_types   or sorted(df["cap_type"].unique())

    print(f"graph_type filtrati : {all_graph_types}")
    print(f"cap_type filtrati   : {all_cap_types}")

    for graph_type in all_graph_types:
        sub_type = df[df["graph_type"] == graph_type]
        if sub_type.empty:
            print(f"  [WARN] nessun dato per graph_type={graph_type!r}, skip.")
            continue

        gstyle = style.get(graph_type, {"color": "gray", "label_prefix": graph_type.capitalize()})

        all_d = d_values or sorted(sub_type["d"].unique())
        print(f"\n[{graph_type}] valori di d: {all_d}")

        for cap_type in all_cap_types:
            sub_cap = sub_type[sub_type["cap_type"] == cap_type]

            cap_out = os.path.join(out_dir, graph_type, cap_type)
            os.makedirs(cap_out, exist_ok=True)

            for d in all_d:
                sub = sub_cap[sub_cap["d"] == d].copy()
                if sub.empty:
                    continue

                plt.figure(figsize=(8, 5))
                plt.plot(
                    sub["n"], sub["mean_time"],
                    marker='o', markersize=7, color=gstyle["color"],
                    linewidth=1.8, label=f"d = {d}",
                )

                for _, row in sub.iterrows():
                    plt.annotate(
                        f"n={int(row['n'])}",
                        xy=(row["n"], row["mean_time"]),
                        xytext=(6, 4), textcoords="offset points",
                        fontsize=8, color=gstyle["color"],
                    )

                plt.title(
                    f"{gstyle['label_prefix']} [{cap_type}] — d = {d}",
                    fontsize=14,
                )
                plt.xlabel("n (number of nodes)", fontsize=12)
                plt.ylabel("Mean time (seconds)", fontsize=12)
                plt.grid(True, linestyle='--', alpha=0.4)
                plt.legend(fontsize=11)
                plt.tight_layout()

                fname    = f"{graph_type}_{cap_type}_d{d}.png"
                out_path = os.path.join(cap_out, fname)
                plt.savefig(out_path, dpi=150)
                plt.close()

                print(f"  Salvato: {graph_type}/{cap_type}/{fname}")

    print(f"\nFatto. I grafici sono nella cartella '{out_dir}/'.")


# ── Entry point ────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Aggrega i risultati CSV e genera i grafici per tutti i run definiti nel config.",
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