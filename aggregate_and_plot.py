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


# ── Aggregation CS ─────────────────────────────────────────────────────────────

def aggregate_cs(results_csv: str, output_csv: str) -> pd.DataFrame:
    groups: dict = defaultdict(list)

    with open(results_csv, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row["graph_type"], row["cap_type"], int(row["n"]), int(row["d"]), int(row["hi"]))
            groups[key].append(float(row["median_time"]))

    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["graph_type", "cap_type", "n", "d", "hi", "mean_time", "count"])
        for (graph_type, cap_type, n, d, hi) in sorted(
            groups.keys(), key=lambda x: (x[0], x[1], x[4], x[3], x[2])
        ):
            times  = groups[(graph_type, cap_type, n, d, hi)]
            mean_t = statistics.mean(times)
            writer.writerow([graph_type, cap_type, n, d, hi, f"{mean_t:.17f}", len(times)])

    print("Aggregazione CS completata ->", output_csv)

    df = pd.read_csv(output_csv)
    for col in ("n", "d", "hi", "mean_time"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    return df.sort_values(["graph_type", "cap_type", "d", "hi"]).reset_index(drop=True)


# ── Aggregation PR/AL ──────────────────────────────────────────────────────────

def aggregate_pr_al(results_csv: str, output_csv: str) -> pd.DataFrame:
    groups: dict = defaultdict(list)

    with open(results_csv, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row["graph_type"], row["cap_type"], (row["n"]), (row["d"]))
            groups[key].append(float(row["median_time"]))

    with open(output_csv, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["graph_type", "cap_type", "n", "d", "mean_time", "min_time", "max_time", "count"])
        for (graph_type, cap_type, n, d) in sorted(
            groups.keys(), key=lambda x: (x[0], x[1], x[3], x[2])
        ):
            times  = groups[(graph_type, cap_type, n, d)]
            mean_t = statistics.mean(times)
            writer.writerow([
                graph_type, cap_type, n, d,
                f"{mean_t:.17f}", f"{min(times):.17f}", f"{max(times):.17f}",
                len(times),
            ])

    print("Aggregazione PR/AL completata ->", output_csv)

    df = pd.read_csv(output_csv)
    for col in ("n", "d", "mean_time", "min_time", "max_time"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    return df.sort_values(["graph_type", "cap_type", "d", "n"]).reset_index(drop=True)


# ── CS Plot 1 — Doubling di d (hi fisso) ──────────────────────────────────────

def plot_cs_doubling_d(df: pd.DataFrame, out_dir: str, gstyle: dict, graph_type: str, cap_type: str) -> None:
    sub = df[(df["graph_type"] == graph_type) & (df["cap_type"] == cap_type)].copy()
    if sub.empty:
        return

    color   = gstyle.get("color", "steelblue")
    cap_out = os.path.join(out_dir, graph_type, cap_type)
    os.makedirs(cap_out, exist_ok=True)

    for hi in sorted(sub["hi"].unique()):
        data = sub[sub["hi"] == hi].sort_values("d")
        if data.empty or data["d"].nunique() < 2:
            continue

        fig, ax = plt.subplots(figsize=(8, 5))
        ax.plot(data["d"], data["mean_time"], marker="o", markersize=6, color=color, linewidth=1.8)
        ax.set_xticks(sorted(data["d"].unique()))
        ax.set_xticklabels([str(int(v)) for v in sorted(data["d"].unique())])
        ax.set_title(
            f"{gstyle.get('label_prefix', graph_type.capitalize())} [{cap_type}]  --  doubling d  (hi = {int(hi)})",
            fontsize=13,
        )
        ax.set_xlabel("d (degree / layers)", fontsize=12)
        ax.set_ylabel("Mean time (seconds)", fontsize=12)
        ax.grid(True, linestyle="--", alpha=0.4)
        fig.tight_layout()

        fname = f"{graph_type}_{cap_type}_doubling_d_hi{int(hi)}.png"
        fig.savefig(os.path.join(cap_out, fname), dpi=150)
        plt.close(fig)
        print(f"  Salvato: {graph_type}/{cap_type}/{fname}")


# ── CS Plot 2 — Doubling di hi (d fisso) ──────────────────────────────────────

def plot_cs_doubling_hi(df: pd.DataFrame, out_dir: str, gstyle: dict, graph_type: str, cap_type: str) -> None:
    sub = df[(df["graph_type"] == graph_type) & (df["cap_type"] == cap_type)].copy()
    if sub.empty or sub["hi"].nunique() < 2:
        return

    color   = gstyle.get("color", "steelblue")
    cap_out = os.path.join(out_dir, graph_type, cap_type)
    os.makedirs(cap_out, exist_ok=True)

    for d in sorted(sub["d"].unique()):
        data = sub[sub["d"] == d].sort_values("hi")
        if data.empty or data["hi"].nunique() < 2:
            continue

        fig, ax = plt.subplots(figsize=(8, 5))
        ax.plot(data["hi"], data["mean_time"], marker="o", markersize=6, color=color, linewidth=1.8)
        ax.set_xticks(sorted(data["hi"].unique()))
        ax.set_xticklabels([str(int(v)) for v in sorted(data["hi"].unique())])
        ax.set_title(
            f"{gstyle.get('label_prefix', graph_type.capitalize())} [{cap_type}]  --  doubling hi  (d = {int(d)})",
            fontsize=13,
        )
        ax.set_xlabel("hi (max capacity)", fontsize=12)
        ax.set_ylabel("Mean time (seconds)", fontsize=12)
        ax.grid(True, linestyle="--", alpha=0.4)
        fig.tight_layout()

        fname = f"{graph_type}_{cap_type}_doubling_hi_d{int(d)}.png"
        fig.savefig(os.path.join(cap_out, fname), dpi=150)
        plt.close(fig)
        print(f"  Salvato: {graph_type}/{cap_type}/{fname}")


# ── PR / AL Plot — Doubling di n ──────────────────────────────────────────────
#
# One PNG per (graph_type, cap_type).
# X-axis = n.  One line per d value (PR has d fixed to a single value;
# AL may also have a single d — the code handles both cases uniformly).
# Shaded band = [min_time, max_time] across the instances_per_combo runs,
# giving an immediate visual of variance across graph instances.

def plot_doubling_n(
    df: pd.DataFrame,
    out_dir: str,
    gstyle: dict,
    graph_type: str,
    cap_type: str,
    mode_label: str = "doubling n",
) -> None:
    sub = df[(df["graph_type"] == graph_type) & (df["cap_type"] == cap_type)].copy()
    if sub.empty:
        print(f"    [SKIP] nessun dato per ({graph_type}, {cap_type})")
        return

    color    = gstyle.get("color", "steelblue")
    prefix   = gstyle.get("label_prefix", graph_type.capitalize())
    cap_out  = os.path.join(out_dir, graph_type, cap_type)
    os.makedirs(cap_out, exist_ok=True)

    # d == -1 means the benchmark could not extract d from the filename
    # (happens for erdag and grid in PR/AL runs where the cs_tag is absent).
    # Replace with a readable sentinel so the legend stays clean.
    sub["d_label"] = sub["d"].apply(lambda v: "n/a" if v == -1 else str(int(v)))

    d_groups   = sub.groupby("d_label", sort=True)
    tab_colors = plt.cm.tab10.colors
    multi_d    = d_groups.ngroups > 1

    plotted = 0
    fig, ax = plt.subplots(figsize=(9, 5))

    for i, (d_label, data) in enumerate(d_groups):
        data = data.sort_values("n")

        if data["n"].nunique() < 1:
            print(f"    [SKIP] d={d_label}: nessun punto n, skip.")
            continue

        c = tab_colors[i % len(tab_colors)] if multi_d else color
        label = f"d = {d_label}"

        ax.plot(
            data["n"], data["mean_time"],
            marker="o", markersize=6, color=c, linewidth=1.8,
            label=label,
        )

        if "min_time" in data.columns and "max_time" in data.columns:
            ax.fill_between(
                data["n"], data["min_time"], data["max_time"],
                color=c, alpha=0.15, linewidth=0,
            )

        plotted += 1

    if plotted == 0:
        plt.close(fig)
        print(f"    [SKIP] ({graph_type}, {cap_type}): nessuna curva plottabile.")
        return

    n_ticks = sorted(sub["n"].unique())
    ax.set_xticks(n_ticks)
    ax.set_xticklabels([str(int(v)) for v in n_ticks], rotation=30, ha="right")
    ax.set_title(f"{prefix} [{cap_type}]  --  {mode_label}", fontsize=13)
    ax.set_xlabel("n (number of nodes)", fontsize=12)
    ax.set_ylabel("Mean time (seconds)", fontsize=12)
    ax.grid(True, linestyle="--", alpha=0.4)
    if multi_d:
        ax.legend(fontsize=10)
    fig.tight_layout()

    fname = f"{graph_type}_{cap_type}_doubling_n.png"
    fig.savefig(os.path.join(cap_out, fname), dpi=150)
    plt.close(fig)
    print(f"  Salvato: {graph_type}/{cap_type}/{fname}")


# ── AL Plot — ER-DAG: asse X = d (letto direttamente dal CSV) ─────────────────
#
# Per AL erdag il CSV porta d come stringa decimale (es. "0.04") prodotta dal
# C++ benchmark.  La colonna viene già convertita a float da aggregate_pr_al.
# Asse X = d (probabilità p), curva unica per cap_type.

def plot_al_erdag_doubling_d(
    df: pd.DataFrame,
    out_dir: str,
    gstyle: dict,
    cap_type: str,
    p_fixed: float = 0.0,   # non più usato; mantenuto per compatibilità firma
    mode_label: str = "Almost-Linear — doubling d (ER-DAG)",
) -> None:
    sub = df[(df["graph_type"] == "erdag") & (df["cap_type"] == cap_type)].copy()
    if sub.empty:
        print(f"    [SKIP] nessun dato erdag per cap_type={cap_type}")
        return

    sub = sub.sort_values("d")

    if sub["d"].nunique() < 1:
        print(f"    [SKIP] erdag {cap_type}: nessun punto d.")
        return

    color  = gstyle.get("color", "seagreen")
    prefix = gstyle.get("label_prefix", "ER-DAG networks")
    cap_out = os.path.join(out_dir, "erdag", cap_type)
    os.makedirs(cap_out, exist_ok=True)

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(
        sub["d"], sub["mean_time"],
        marker="o", markersize=6, color=color, linewidth=1.8,
    )

    if "min_time" in sub.columns and "max_time" in sub.columns:
        ax.fill_between(
            sub["d"], sub["min_time"], sub["max_time"],
            color=color, alpha=0.15, linewidth=0,
        )

    d_ticks = sorted(sub["d"].unique())
    ax.set_xticks(d_ticks)
    ax.set_xticklabels([str(v) for v in d_ticks], rotation=30, ha="right")
    ax.set_title(f"{prefix} [{cap_type}]  --  {mode_label}", fontsize=13)
    ax.set_xlabel("d (probabilità p dell'ER-DAG)", fontsize=11)
    ax.set_ylabel("Mean time (seconds)", fontsize=12)
    ax.grid(True, linestyle="--", alpha=0.4)
    fig.tight_layout()

    fname = f"erdag_{cap_type}_doubling_d.png"
    fig.savefig(os.path.join(cap_out, fname), dpi=150)
    plt.close(fig)
    print(f"  Salvato: erdag/{cap_type}/{fname}")


# ── AL Plot — Layered: asse X = d (numero di layer) ───────────────────────────

def plot_al_layered_doubling_d(
    df: pd.DataFrame,
    out_dir: str,
    gstyle: dict,
    cap_type: str,
    mode_label: str = "Almost-Linear — doubling d (Layered)",
) -> None:
    sub = df[(df["graph_type"] == "layered") & (df["cap_type"] == cap_type)].copy()
    if sub.empty:
        print(f"    [SKIP] nessun dato layered per cap_type={cap_type}")
        return

    sub = sub.sort_values("d")

    if sub["d"].nunique() < 1:
        print(f"    [SKIP] layered {cap_type}: nessun punto d.")
        return

    color  = gstyle.get("color", "steelblue")
    prefix = gstyle.get("label_prefix", "Layered")
    cap_out = os.path.join(out_dir, "layered", cap_type)
    os.makedirs(cap_out, exist_ok=True)

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(
        sub["d"], sub["mean_time"],
        marker="o", markersize=6, color=color, linewidth=1.8,
    )

    if "min_time" in sub.columns and "max_time" in sub.columns:
        ax.fill_between(
            sub["d"], sub["min_time"], sub["max_time"],
            color=color, alpha=0.15, linewidth=0,
        )

    d_ticks = sorted(sub["d"].unique())
    ax.set_xticks(d_ticks)
    ax.set_xticklabels([str(int(v)) for v in d_ticks], rotation=30, ha="right")
    ax.set_title(f"{prefix} [{cap_type}]  --  {mode_label}", fontsize=13)
    ax.set_xlabel("d (numero di layer)", fontsize=11)
    ax.set_ylabel("Mean time (seconds)", fontsize=12)
    ax.grid(True, linestyle="--", alpha=0.4)
    fig.tight_layout()

    fname = f"layered_{cap_type}_doubling_d.png"
    fig.savefig(os.path.join(cap_out, fname), dpi=150)
    plt.close(fig)
    print(f"  Salvato: layered/{cap_type}/{fname}")


# ── Entry points ───────────────────────────────────────────────────────────────

def aggregate_and_plot_cs(
    results_csv: str,
    output_csv: str,
    out_dir: str,
    style: dict,
    graph_types: list[str] | None = None,
    cap_types:   list[str] | None = None,
    **_,
) -> None:
    df = aggregate_cs(results_csv, output_csv)

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
            plot_cs_doubling_d(df, out_dir, gstyle, graph_type, cap_type)
            plot_cs_doubling_hi(df, out_dir, gstyle, graph_type, cap_type)

    print(f"\nFatto. I grafici CS sono nella cartella '{out_dir}/'.")


def _run_doubling_n(
    results_csv: str,
    output_csv: str,
    out_dir: str,
    style: dict,
    mode_label: str,
    graph_types: list[str] | None = None,
    cap_types:   list[str] | None = None,
    **_,
) -> None:
    """Shared logic for PR and AL runs — both double n on the X-axis."""
    df = aggregate_pr_al(results_csv, output_csv)

    all_graph_types = graph_types or sorted(df["graph_type"].unique())
    all_cap_types   = cap_types   or sorted(df["cap_type"].unique())

    print(f"graph_type trovati nel CSV : {sorted(df['graph_type'].unique())}")
    print(f"graph_type usati           : {all_graph_types}")
    print(f"cap_type   trovati nel CSV : {sorted(df['cap_type'].unique())}")
    print(f"cap_type   usati           : {all_cap_types}")

    for graph_type in all_graph_types:
        sub_type = df[df["graph_type"] == graph_type]
        if sub_type.empty:
            print(f"  [WARN] nessun dato per graph_type={graph_type!r}, skip.")
            continue
        gstyle = style.get(graph_type, {"color": "gray", "label_prefix": graph_type.capitalize()})
        for cap_type in all_cap_types:
            sub_cap = sub_type[sub_type["cap_type"] == cap_type]
            if sub_cap.empty:
                print(f"  [WARN] nessun dato per ({graph_type}, {cap_type}), skip.")
                continue
            print(f"\n[{graph_type} / {cap_type}]  {len(sub_cap)} righe aggregate")
            plot_doubling_n(df, out_dir, gstyle, graph_type, cap_type, mode_label=mode_label)

    print(f"\nFatto. I grafici sono nella cartella '{out_dir}/'.")


def aggregate_and_plot_pr(
    results_csv: str, output_csv: str, out_dir: str, style: dict,
    graph_types: list[str] | None = None, cap_types: list[str] | None = None, **_,
) -> None:
    _run_doubling_n(results_csv, output_csv, out_dir, style,
                    mode_label="Push-Relabel — doubling n",
                    graph_types=graph_types, cap_types=cap_types)


def aggregate_and_plot_al(
    results_csv: str, output_csv: str, out_dir: str, style: dict,
    graph_types: list[str] | None = None, cap_types: list[str] | None = None,
    p_erdag: float = 0.3,   # mantenuto per compatibilità firma, non più usato nel plot
    **_,
) -> None:
    df = aggregate_pr_al(results_csv, output_csv)

    all_graph_types = graph_types or sorted(df["graph_type"].unique())
    all_cap_types   = cap_types   or sorted(df["cap_type"].unique())

    print(f"graph_type trovati nel CSV : {sorted(df['graph_type'].unique())}")
    print(f"graph_type usati           : {all_graph_types}")
    print(f"cap_type   trovati nel CSV : {sorted(df['cap_type'].unique())}")
    print(f"cap_type   usati           : {all_cap_types}")

    for graph_type in all_graph_types:
        sub_type = df[df["graph_type"] == graph_type]
        if sub_type.empty:
            print(f"  [WARN] nessun dato per graph_type={graph_type!r}, skip.")
            continue
        gstyle = style.get(graph_type, {"color": "gray", "label_prefix": graph_type.capitalize()})

        for cap_type in all_cap_types:
            sub_cap = sub_type[sub_type["cap_type"] == cap_type]
            if sub_cap.empty:
                print(f"  [WARN] nessun dato per ({graph_type}, {cap_type}), skip.")
                continue
            print(f"\n[{graph_type} / {cap_type}]  {len(sub_cap)} righe aggregate")

            if graph_type == "erdag":
                # Asse X = d (probabilità p, letta direttamente dal CSV)
                plot_al_erdag_doubling_d(
                    df         = df,
                    out_dir    = out_dir,
                    gstyle     = gstyle,
                    cap_type   = cap_type,
                    mode_label = "Almost-Linear — doubling d (ER-DAG)",
                )
            elif graph_type == "layered":
                # Asse X = d (numero di layer)
                plot_al_layered_doubling_d(
                    df         = df,
                    out_dir    = out_dir,
                    gstyle     = gstyle,
                    cap_type   = cap_type,
                    mode_label = "Almost-Linear — doubling d (Layered)",
                )
            else:
                # grid: asse X = n
                plot_doubling_n(
                    df, out_dir, gstyle, graph_type, cap_type,
                    mode_label="Almost-Linear — doubling n",
                )

    print(f"\nFatto. I grafici AL sono nella cartella '{out_dir}/'.")


# Keep old name as alias so existing callers don't break.
aggregate_and_plot_pr_al = aggregate_and_plot_pr


# ── Main ───────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Aggrega i risultati CSV e genera i grafici."
    )
    parser.add_argument("--config", default="configs/config.toml")
    args = parser.parse_args()

    cfg   = load_config(args.config)
    style = cfg["plots"]["style"]

    for i, run in enumerate(cfg["runs"], start=1):
        print(f"\n{'='*60}")
        print(f"Run {i}/{len(cfg['runs'])}  --  {run['results_csv']}")
        print(f"{'='*60}")

        mode = run.get("mode", "pr")   # "cs" | "pr" | "al" nel toml
        fn   = {
            "cs": aggregate_and_plot_cs,
            "pr": aggregate_and_plot_pr,
            "al": aggregate_and_plot_al,
        }.get(mode, aggregate_and_plot_pr)
        kwargs = dict(
            results_csv = run["results_csv"],
            output_csv  = run["output_csv"],
            out_dir     = run["out_dir"],
            style       = style,
            graph_types = run.get("graph_types"),
            cap_types   = run.get("cap_types"),
        )
        # AL only: p_erdag must match [almost_linear.erdag] p_values[0] in the toml.
        if mode == "al":
            p_values = cfg.get("almost_linear", {}).get("erdag", {}).get("p_values", [0.3])
            kwargs["p_erdag"] = p_values[0]
        fn(**kwargs)


if __name__ == "__main__":
    main()