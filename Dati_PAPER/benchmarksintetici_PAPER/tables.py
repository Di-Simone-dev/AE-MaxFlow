#!/usr/bin/env python3
"""
Genera 3 tabelle LaTeX (PR, AL, CS) a partire dai CSV aggregati.

PR  : doubling su n (d fisso per gruppo graph_type/cap_type)
AL  : erdag/layered -> doubling su d (n fisso)
      grid          -> doubling su n (d fisso)
CS  : Vista 1 – doubling su d (per erdag/layered) o su n (per grid), hi fisso
      Vista 2 – doubling su hi (n/d fissi) — solo cap_type=="int" che ha 5 valori di hi
"""

import pandas as pd
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
UPLOADS = BASE_DIR
OUT_DIR = BASE_DIR
os.makedirs(OUT_DIR, exist_ok=True)

GRAPH_LABEL = {
    "erdag":   "ERDAG",
    "grid":    "Grid",
    "layered": "Layered",
}
CAP_LABEL = {
    "int":        "int",
    "unit":       "unit",
    "rational":   "rational",
    "irrational": "irrational",
}

GRAPH_ORDER = ["erdag", "grid", "layered"]
CAP_ORDER   = ["int", "rational", "irrational", "unit"]


def esc(s):
    return str(s).replace("_", r"\_")


def fmt_time(x):
    return f"{x:.6f}"


def fmt_ratio(x):
    return "---" if pd.isna(x) else f"{x:.3f}"


def add_ratio(df, group_cols, sort_col):
    """Ordina per group_cols + sort_col e calcola il rapporto mean_time[i]/mean_time[i-1]."""
    df = df.copy()
    df = df.sort_values(group_cols + [sort_col]).reset_index(drop=True)
    df["rapporto"] = df.groupby(group_cols)["mean_time"].transform(
        lambda s: s / s.shift(1)
    )
    return df


def sort_by_order(df, col, order):
    """Ordina le righe di df secondo la lista 'order' per la colonna col."""
    cat = pd.CategoricalDtype(categories=order, ordered=True)
    df = df.copy()
    df[col] = df[col].astype(cat)
    return df


# ──────────────────────────────────────────────────────────────────────────
# PR — doubling su n (d fisso), gruppo = (graph_type, cap_type)
# ──────────────────────────────────────────────────────────────────────────
def build_pr():
    df = pd.read_csv(f"{UPLOADS}/pr_aggregated.csv")
    df = add_ratio(df, ["graph_type", "cap_type"], "n")

    # Ordine leggibile
    df = sort_by_order(df, "graph_type", GRAPH_ORDER)
    df = sort_by_order(df, "cap_type",   CAP_ORDER)
    df = df.sort_values(["graph_type", "cap_type", "n"])

    lines = []
    lines.append(r"\begin{longtable}{l l r r r r}")
    lines.append(
        r"\caption{Risultati aggregati -- Push-Relabel (PR),"
        r" doubling su $n$ ($d$ fisso).}"
    )
    lines.append(r"\label{tab:pr_results} \\")
    lines.append(r"\toprule")
    header = (
        r"Tipo grafo & Capacit\`a & $n$ & $d$ & "
        r"Tempo medio (s) & Rapporto \\"
    )
    lines.append(header)
    lines.append(r"\midrule")
    lines.append(r"\endfirsthead")
    lines.append(r"\toprule")
    lines.append(header)
    lines.append(r"\midrule")
    lines.append(r"\endhead")
    lines.append(r"\bottomrule")
    lines.append(r"\endfoot")

    prev_group = None
    for _, row in df.iterrows():
        cur_group = (row["graph_type"], row["cap_type"])
        if prev_group is not None and cur_group != prev_group:
            lines.append(r"\addlinespace")
        prev_group = cur_group
        lines.append(
            f"{GRAPH_LABEL[row['graph_type']]} & {CAP_LABEL[row['cap_type']]} & "
            f"{int(row['n'])} & {row['d']:g} & "
            f"{fmt_time(row['mean_time'])} & "
            f"{fmt_ratio(row['rapporto'])} \\\\"
        )
    lines.append(r"\end{longtable}")
    return "\n".join(lines)


# ──────────────────────────────────────────────────────────────────────────
# AL — doubling su d per erdag/layered (n fisso), su n per grid (d fisso)
# ──────────────────────────────────────────────────────────────────────────
def build_al():
    df = pd.read_csv(f"{UPLOADS}/al_aggregated.csv")

    grid_part  = df[df["graph_type"] == "grid"].copy()
    other_part = df[df["graph_type"] != "grid"].copy()

    grid_part  = add_ratio(grid_part,  ["graph_type", "cap_type"], "n")
    other_part = add_ratio(other_part, ["graph_type", "cap_type"], "d")

    # Ordine leggibile per entrambe le parti
    for part in [grid_part, other_part]:
        pass  # l'ordinamento avviene sotto

    other_part = sort_by_order(other_part, "graph_type", GRAPH_ORDER)
    other_part = sort_by_order(other_part, "cap_type",   CAP_ORDER)
    other_part = other_part.sort_values(["graph_type", "cap_type", "d"])

    grid_part = sort_by_order(grid_part, "cap_type", CAP_ORDER)
    grid_part = grid_part.sort_values(["graph_type", "cap_type", "n"])

    full = pd.concat([other_part, grid_part], ignore_index=True)

    lines = []
    lines.append(r"\begin{longtable}{l l r r r r}")
    lines.append(
        r"\caption{Risultati aggregati -- Augmenting Layers (AL). "
        r"Doubling su $d$ per erdag/layered ($n$ fisso), "
        r"su $n$ per grid ($d$ fisso).}"
    )
    lines.append(r"\label{tab:al_results} \\")
    lines.append(r"\toprule")
    header = (
        r"Tipo grafo & Capacit\`a & $n$ & $d$ & "
        r"Tempo medio (s) & Rapporto \\"
    )
    lines.append(header)
    lines.append(r"\midrule")
    lines.append(r"\endfirsthead")
    lines.append(r"\toprule")
    lines.append(header)
    lines.append(r"\midrule")
    lines.append(r"\endhead")
    lines.append(r"\bottomrule")
    lines.append(r"\endfoot")

    prev_group = None
    for _, row in full.iterrows():
        cur_group = (row["graph_type"], row["cap_type"])
        if prev_group is not None and cur_group != prev_group:
            lines.append(r"\addlinespace")
        prev_group = cur_group
        lines.append(
            f"{GRAPH_LABEL[row['graph_type']]} & {CAP_LABEL[row['cap_type']]} & "
            f"{int(row['n'])} & {row['d']:g} & "
            f"{fmt_time(row['mean_time'])} & "
            f"{fmt_ratio(row['rapporto'])} \\\\"
        )
    lines.append(r"\end{longtable}")
    return "\n".join(lines)


# ──────────────────────────────────────────────────────────────────────────
# CS — Vista 1: doubling su d (erdag/layered) o n (grid), hi fisso
#      Vista 2: doubling su hi, n/d fisso  (solo dove hi ha ≥2 valori)
# ──────────────────────────────────────────────────────────────────────────
def build_cs():
    df = pd.read_csv(f"{UPLOADS}/cs_aggregated.csv")

    # ---- Vista 1 --------------------------------------------------------
    # Per erdag/layered: gruppo fisso = (graph_type, cap_type, hi), doubling su d
    # Per grid:          gruppo fisso = (graph_type, cap_type, hi), doubling su n
    #   (nel grid d == n, quindi è equivalente)

    grid_v1  = df[df["graph_type"] == "grid"].copy()
    other_v1 = df[df["graph_type"] != "grid"].copy()

    other_v1 = add_ratio(other_v1, ["graph_type", "cap_type", "hi"], "d")
    grid_v1  = add_ratio(grid_v1,  ["graph_type", "cap_type", "hi"], "n")

    other_v1 = sort_by_order(other_v1, "graph_type", GRAPH_ORDER)
    other_v1 = sort_by_order(other_v1, "cap_type",   CAP_ORDER)
    other_v1 = other_v1.sort_values(["graph_type", "cap_type", "hi", "d"])

    grid_v1 = sort_by_order(grid_v1, "cap_type", CAP_ORDER)
    grid_v1 = grid_v1.sort_values(["graph_type", "cap_type", "hi", "n"])

    view1 = pd.concat([other_v1, grid_v1], ignore_index=True)

    # ---- Vista 2 --------------------------------------------------------
    # Doubling su hi (gruppo fisso = graph_type, cap_type, n/d)
    # Valido solo dove esistono ≥2 valori di hi per il gruppo
    # Per erdag/layered: doubling su hi con d fisso (n fisso di default=1000)
    # Per grid: doubling su hi con n fisso

    grid_v2  = df[df["graph_type"] == "grid"].copy()
    other_v2 = df[df["graph_type"] != "grid"].copy()

    other_v2 = add_ratio(other_v2, ["graph_type", "cap_type", "d"], "hi")
    grid_v2  = add_ratio(grid_v2,  ["graph_type", "cap_type", "n"], "hi")

    # Filtra solo gruppi con ≥2 valori di hi
    def filter_multi_hi(part, group_cols):
        counts = part.groupby(group_cols)["hi"].nunique()
        valid  = counts[counts >= 2].reset_index()
        return part.merge(valid[group_cols], on=group_cols)

    other_v2 = filter_multi_hi(other_v2, ["graph_type", "cap_type", "d"])
    grid_v2  = filter_multi_hi(grid_v2,  ["graph_type", "cap_type", "n"])

    other_v2 = sort_by_order(other_v2, "graph_type", GRAPH_ORDER)
    other_v2 = sort_by_order(other_v2, "cap_type",   CAP_ORDER)
    other_v2 = other_v2.sort_values(["graph_type", "cap_type", "d", "hi"])

    grid_v2 = sort_by_order(grid_v2, "cap_type", CAP_ORDER)
    grid_v2 = grid_v2.sort_values(["graph_type", "cap_type", "n", "hi"])

    view2 = pd.concat([other_v2, grid_v2], ignore_index=True)

    # ---- Render ---------------------------------------------------------
    def render(view_df, caption, label, doubling_col):
        lines = []
        lines.append(r"\begin{longtable}{l l r r r r r}")
        lines.append(f"\\caption{{{caption}}}")
        lines.append(f"\\label{{{label}}} \\\\")
        lines.append(r"\toprule")
        header = (
            r"Tipo grafo & Capacit\`a & $n$ & $d$ & $hi$ & "
            r"Tempo medio (s) & Rapporto \\"
        )
        lines.append(header)
        lines.append(r"\midrule")
        lines.append(r"\endfirsthead")
        lines.append(r"\toprule")
        lines.append(header)
        lines.append(r"\midrule")
        lines.append(r"\endhead")
        lines.append(r"\bottomrule")
        lines.append(r"\endfoot")

        # Gruppo di separazione: varia in base al doubling
        if doubling_col == "d":
            def group_key(row):
                return (row["graph_type"], row["cap_type"], row["hi"])
        elif doubling_col == "n":
            def group_key(row):
                return (row["graph_type"], row["cap_type"], row["hi"])
        else:  # hi
            def group_key(row):
                if row["graph_type"] == "grid":
                    return (row["graph_type"], row["cap_type"], row["n"])
                else:
                    return (row["graph_type"], row["cap_type"], row["d"])

        prev_group = None
        for _, row in view_df.iterrows():
            cur_group = group_key(row)
            if prev_group is not None and cur_group != prev_group:
                lines.append(r"\addlinespace")
            prev_group = cur_group

            # d display: per grid d non è significativo (==n), mostriamo 5 fisso
            d_display = "5" if row["graph_type"] == "grid" else f"{row['d']:g}"

            lines.append(
                f"{GRAPH_LABEL[row['graph_type']]} & {CAP_LABEL[row['cap_type']]} & "
                f"{int(row['n'])} & {d_display} & {int(row['hi'])} & "
                f"{fmt_time(row['mean_time'])} & "
                f"{fmt_ratio(row['rapporto'])} \\\\"
            )
        lines.append(r"\end{longtable}")
        return "\n".join(lines)

    part1 = render(
        view1,
        r"Risultati aggregati -- Capacity Scaling (CS), "
        r"doubling su $d$ (erdag/layered) o $n$ (grid), $hi$ fisso.",
        "tab:cs_doubling_nd",
        "d",
    )
    part2 = render(
        view2,
        r"Risultati aggregati -- Capacity Scaling (CS), "
        r"doubling su $hi$, $n$/$d$ fissi.",
        "tab:cs_doubling_hi",
        "hi",
    )
    return part1 + "\n\n\\bigskip\n\n" + part2


# ──────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    pr_tex = build_pr()
    al_tex = build_al()
    cs_tex = build_cs()

    with open(f"{OUT_DIR}/tabella_pr.tex", "w") as f:
        f.write(pr_tex + "\n")
    with open(f"{OUT_DIR}/tabella_al.tex", "w") as f:
        f.write(al_tex + "\n")
    with open(f"{OUT_DIR}/tabella_cs.tex", "w") as f:
        f.write(cs_tex + "\n")

    print("Generati: tabella_pr.tex, tabella_al.tex, tabella_cs.tex")