import random
import math
import os
import tomllib
from fractions import Fraction
from pathlib import Path

# ── Config loading ─────────────────────────────────────────────────────────────

def load_config(path: str = "configs/config.toml") -> dict:
    base = Path(__file__).parent
    full_path = base / path
    print("Full path = ", full_path)
    with open(full_path, "rb") as f:
        return tomllib.load(f)


# ── Capacity types ─────────────────────────────────────────────────────────────

class IrrationalCapacity:
    """Symbolic irrational capacity with expression string and float value."""
    def __init__(self, expr: str, value: float):
        if value <= 0:
            raise ValueError(f"Non-positive irrational capacity: {expr!r} = {value}")
        self.expr  = expr
        self.value = value

    def __str__(self):   return self.expr
    def __float__(self): return self.value
    def __lt__(self, other): return self.value < float(other)
    def __le__(self, other): return self.value <= float(other)
    def __gt__(self, other): return self.value > float(other)
    def __ge__(self, other): return self.value >= float(other)


def _random_int_capacity(lo: int, hi: int) -> int:
    return random.randint(lo, hi)

def _random_rational_capacity(lo: int, hi: int) -> Fraction:
    num = random.randint(lo, hi)
    # den limitato a 20 per evitare overflow in C++
    den = random.randint(1, 20)
    return Fraction(num, den)

def _random_irrational_capacity(lo: int, hi: int) -> IrrationalCapacity:
    kind = random.randint(0, 9)
    if kind == 0:
        n = random.randint(2, 1000)
        while math.isqrt(n) ** 2 == n:
            n = random.randint(2, 1000)
        return IrrationalCapacity(f"sqrt({n})", math.sqrt(n))
    elif kind == 1:
        n = random.randint(2, 1000)
        return IrrationalCapacity(f"log({n})", math.log(n))
    elif kind == 2:
        n = random.randint(1, 6)
        return IrrationalCapacity(f"exp({n})", math.exp(n))
    elif kind == 3:
        n = random.randint(3, 50)
        return IrrationalCapacity(f"cos(pi/{n})", math.cos(math.pi / n))
    elif kind == 4:
        n = random.randint(3, 50)
        return IrrationalCapacity(f"sin(pi/{n})", math.sin(math.pi / n))
    elif kind == 5:
        n = random.randint(3, 24)
        return IrrationalCapacity(f"tan(pi/{n})", math.tan(math.pi / n))
    elif kind == 6:
        a = random.randint(lo, hi)
        n = random.randint(2, 1000)
        while math.isqrt(n) ** 2 == n:
            n = random.randint(2, 1000)
        return IrrationalCapacity(f"{a}*sqrt({n})", a * math.sqrt(n))
    elif kind == 7:
        a = random.randint(lo, hi)
        return IrrationalCapacity(f"{a}*pi", a * math.pi)
    elif kind == 8:
        a = random.randint(lo, hi)
        return IrrationalCapacity(f"{a}*e", a * math.e)
    else:
        a = random.randint(lo, hi)
        n = random.randint(2, 100)
        return IrrationalCapacity(f"{a}*log({n})", a * math.log(n))

def _unit_capacity(lo=None, hi=None) -> int:
    return 1


def build_capacity_config(lo: int, hi: int, big_int: int) -> dict:
    """Build the CAPACITY_CONFIG dict using values from config."""
    _big = big_int if big_int > 0 else _random_int_capacity(lo, hi)

    return {
        "int": (
            lambda: _random_int_capacity(lo, hi),
            lambda: str(_big),
        ),
        "rational": (
            lambda: _random_rational_capacity(lo, hi),
            lambda: f"{_big}/1",
        ),
        "irrational": (
            lambda: _random_irrational_capacity(lo, hi),
            lambda: f"{_big}*pi",
        ),
        "unit": (
            _unit_capacity,
            lambda: "1",
        ),
    }


# ── Capacity → DIMACS string ───────────────────────────────────────────────────

def _cap_str(cap) -> str:
    if isinstance(cap, IrrationalCapacity):
        return cap.expr
    if isinstance(cap, Fraction):
        return f"{cap.numerator}/{cap.denominator}"
    if isinstance(cap, float):
        return f"{cap:.17g}"
    return str(cap)


# ── Layered network generator ──────────────────────────────────────────────────

def generate_layered_network(
    n: int, d: int, seed: int, cap_type: str, capacity_config: dict
) -> str:
    random.seed(seed)

    cap_fn, big_fn = capacity_config[cap_type]

    # 1. Determine W and L such that L/W ≈ 2
    W = int(math.sqrt(n / 2))
    L = 2 * W
    assert W * L <= n

    # 2. Assign nodes to levels
    levels = [[] for _ in range(L)]
    node_id = 1
    for lvl in range(L):
        for _ in range(W):
            if node_id <= n:
                levels[lvl].append(node_id)
                node_id += 1

    s = 1
    t = n
    arcs = []

    # 3. Generate arcs with average outdegree d
    for lvl in range(L):
        for u in levels[lvl]:
            # Same-level arcs
            same_targets = random.sample(
                levels[lvl], k=min(len(levels[lvl]), max(0, d // 2))
            )
            for v in same_targets:
                if v != u:
                    arcs.append((u, v, cap_fn()))

            # Next-level arcs
            if lvl + 1 < L:
                next_targets = random.sample(
                    levels[lvl + 1], k=min(len(levels[lvl + 1]), d)
                )
                for v in next_targets:
                    arcs.append((u, v, cap_fn()))

    # 4. High-capacity arcs incident to s and t
    big = big_fn()  # noqa: F841
    for u in levels[L - 1]:
        arcs.append((u, t, cap_fn()))

    # 5. Build DIMACS output
    lines = [
        f"c Layered network n={n} d={d} seed={seed} cap={cap_type}",
        f"p max {n} {len(arcs)}",
        f"n {s} s",
        f"n {t} t",
    ]
    for u, v, cap in arcs:
        lines.append(f"a {u} {v} {_cap_str(cap)}")

    return "\n".join(lines)


# ── Grid network generator ─────────────────────────────────────────────────────

def generate_grid_network(
    n: int, d: int, seed: int, cap_type: str, capacity_config: dict
) -> str:
    """
    Generate a grid network with `rows=d` and `cols=n//d`.
    n must be divisible by d.
    Arcs go right and downward; capacities follow cap_type.
    s = node 1 (top-left), t = node n (bottom-right).
    """
    random.seed(seed)

    assert n % d == 0, f"n={n} must be divisible by d={d}"
    rows = d
    cols = n // d

    cap_fn, _ = capacity_config[cap_type]

    def node(r, c):
        return r * cols + c + 1

    arcs = []

    for r in range(rows):
        for c in range(cols):
            u = node(r, c)

            # right
            if c + 1 < cols:
                v = node(r, c + 1)
                arcs.append((u, v, cap_fn()))

            # down
            if r + 1 < rows:
                v = node(r + 1, c)
                arcs.append((u, v, cap_fn()))

    s = 1
    t = n

    lines = [
        f"c GRID network n={n} d={d} seed={seed} cap={cap_type}",
        f"p max {n} {len(arcs)}",
        f"n {s} s",
        f"n {t} t",
    ]
    for u, v, cap in arcs:
        lines.append(f"a {u} {v} {_cap_str(cap)}")

    return "\n".join(lines)


# ── Erdős–Rényi DAG generator ──────────────────────────────────────────────────

def generate_er_dag_network(
    n: int, p: float, seed: int, cap_type: str, capacity_config: dict
) -> str:
    """
    Generate an Erdős–Rényi DAG on n nodes.

    For every pair (u, v) with u < v an arc u→v is added independently
    with probability p.  This guarantees acyclicity by construction.
    s = node 1, t = node n.

    p is typically chosen as  d / (n - 1)  so that the expected
    out-degree equals d.
    """
    random.seed(seed)

    cap_fn, _ = capacity_config[cap_type]

    arcs = []
    for u in range(1, n):
        for v in range(u + 1, n + 1):
            if random.random() < p:
                arcs.append((u, v, cap_fn()))

    # Guarantee connectivity: if node 1 has no out-arc add one to node 2
    if n >= 2 and not any(u == 1 for u, _, _ in arcs):
        arcs.append((1, 2, cap_fn()))
    # Guarantee node n is reachable: if it has no in-arc add one from n-1
    if n >= 2 and not any(v == n for _, v, _ in arcs):
        arcs.append((n - 1, n, cap_fn()))

    s = 1
    t = n

    lines = [
        f"c ER-DAG network n={n} p={p:.6g} seed={seed} cap={cap_type}",
        f"p max {n} {len(arcs)}",
        f"n {s} s",
        f"n {t} t",
    ]
    for u, v, cap in arcs:
        lines.append(f"a {u} {v} {_cap_str(cap)}")

    return "\n".join(lines)


# ── Batch helpers ──────────────────────────────────────────────────────────────

def generate_all_instances(
    n_values,
    d_values,
    cap_types,
    outdir: str,
    instances_per_combo: int,
    capacity_config: dict,
    cs_tag: str = "",          # e.g. "_d8_hi1023"  — empty for PR/AL batches
):
    """Generate layered network instances for all combinations of parameters.

    cs_tag is appended to both the subfolder name and the filename when this
    function is called from the CS batch, making each (d, hi) combo uniquely
    identifiable on disk.
    """
    total = len(n_values) * len(d_values) * len(cap_types) * instances_per_combo
    done  = 0

    print("Generating layered instances")
    print(f"  n      : {n_values}")
    print(f"  d      : {d_values}")
    print(f"  cap    : {cap_types}")
    print(f"  cs_tag : {cs_tag!r}")
    print(f"  reps   : {instances_per_combo}")
    print(f"  total  : {total} files")
    print(f"  outdir : {outdir}\n")

    for cap_type in cap_types:
        for n in n_values:
            for d in d_values:
                # When cs_tag is set it already encodes the arc-degree; omit inline _d to avoid redundancy.
                folder_d  = "" if cs_tag else f"_d{d}"
                subdir = os.path.join(outdir, cap_type, f"layered_n{n}{folder_d}{cs_tag}")
                os.makedirs(subdir, exist_ok=True)

                for k in range(instances_per_combo):
                    seed     = k
                    dimacs   = generate_layered_network(n, d, seed, cap_type, capacity_config)
                    filename = f"layered_n{n}{folder_d}{cs_tag}_seed{k}.max"
                    filepath = os.path.join(subdir, filename)

                    with open(filepath, "w") as f:
                        f.write(dimacs)

                    done += 1
                    if done % 50 == 0 or done == total:
                        print(f"  [{done:>4}/{total}] {filepath}")

    print(f"\nDone. {total} layered files written under '{outdir}/'.")


def generate_all_grid_instances(
    n_values,
    d: int,
    cap_types,
    outdir: str,
    instances_per_combo: int,
    capacity_config: dict,
    cs_tag: str = "",          # e.g. "_d8_hi1023"  — empty for PR/AL batches
):
    """Generate grid network instances for all combinations of parameters.

    cs_tag is appended to both the subfolder name and the filename when this
    function is called from the CS batch, making each (d_arc, hi) combo uniquely
    identifiable on disk.  The grid's own row-count parameter is always named
    `d` inside filenames (grid d is fixed, not the same as CS d_step).
    """
    total = len(n_values) * len(cap_types) * instances_per_combo
    done  = 0

    print("Generating grid instances")
    print(f"  n      : {n_values}")
    print(f"  d      : {d}")
    print(f"  cap    : {cap_types}")
    print(f"  cs_tag : {cs_tag!r}")
    print(f"  reps   : {instances_per_combo}")
    print(f"  total  : {total} files")
    print(f"  outdir : {outdir}\n")

    for cap_type in cap_types:
        for n in n_values:
            assert n % d == 0, f"n={n} must be divisible by d={d}"

            subdir = os.path.join(outdir, cap_type, f"grid_n{n}_rows{d}{cs_tag}")
            os.makedirs(subdir, exist_ok=True)

            for k in range(instances_per_combo):
                # seed includes a hash of cs_tag so CS steps never share seeds
                seed     = 1000 * n + (hash(cs_tag) % 10000) + k
                dimacs   = generate_grid_network(n, d, seed, cap_type, capacity_config)
                filename = f"grid_n{n}_rows{d}{cs_tag}_seed{k}.max"
                filepath = os.path.join(subdir, filename)

                with open(filepath, "w") as f:
                    f.write(dimacs)

                done += 1
                if done % 20 == 0 or done == total:
                    print(f"  [{done:>4}/{total}] {filepath}")


def generate_all_er_dag_instances(
    n_values,
    p_values,
    cap_types,
    outdir: str,
    instances_per_combo: int,
    capacity_config: dict,
    cs_tag: str = "",          # e.g. "_d8_hi1023"  — empty for PR/AL batches
):
    """Generate Erdős–Rényi DAG instances for all combinations of parameters.

    cs_tag is appended to both the subfolder name and the filename when this
    function is called from the CS batch, making each (d, hi) combo uniquely
    identifiable on disk.
    """
    total = len(n_values) * len(p_values) * len(cap_types) * instances_per_combo
    done  = 0

    print("Generating ER-DAG instances")
    print(f"  n      : {n_values}")
    print(f"  p      : {p_values}")
    print(f"  cap    : {cap_types}")
    print(f"  cs_tag : {cs_tag!r}")
    print(f"  reps   : {instances_per_combo}")
    print(f"  total  : {total} files")
    print(f"  outdir : {outdir}\n")

    for cap_type in cap_types:
        for n in n_values:
            for p in p_values:
                p_tag = f"{p:.4f}".replace(".", "_")
                subdir = os.path.join(outdir, cap_type, f"erdag_n{n}_p{p_tag}{cs_tag}")
                os.makedirs(subdir, exist_ok=True)

                for k in range(instances_per_combo):
                    seed     = 2000 * n + (hash(cs_tag) % 10000) + k
                    dimacs   = generate_er_dag_network(n, p, seed, cap_type, capacity_config)
                    filename = f"erdag_n{n}_p{p_tag}{cs_tag}_seed{k}.max"
                    filepath = os.path.join(subdir, filename)

                    with open(filepath, "w") as f:
                        f.write(dimacs)

                    done += 1
                    if done % 20 == 0 or done == total:
                        print(f"  [{done:>4}/{total}] {filepath}")

    print(f"\nDone. {total} ER-DAG files written under '{outdir}/'.")


# ── PR batch: doubling on n only ───────────────────────────────────────────────

def run_pr_batch(cfg: dict, base_cap_cfg: dict):
    """
    Push-relabel batch (graphs/pr).
    Doubling series: n doubles across steps; d and capacities are fixed.
    """
    outdir = cfg["pr"]["outdir"]
    pr     = cfg["pr"]

    # ── Layered
    lay = pr["layered"]
    generate_all_instances(
        n_values            = lay["n_values"],
        d_values            = lay["d_values"],
        cap_types           = lay["cap_types"],
        outdir              = outdir,
        instances_per_combo = lay["instances_per_combo"],
        capacity_config     = base_cap_cfg,
    )

    # ── Grid
    grd = pr["grid"]
    generate_all_grid_instances(
        n_values            = grd["n_values"],
        d                   = grd["d"],
        cap_types           = grd["cap_types"],
        outdir              = outdir,
        instances_per_combo = grd["instances_per_combo"],
        capacity_config     = base_cap_cfg,
    )

    # ── ER-DAG
    er = pr["erdag"]
    generate_all_er_dag_instances(
        n_values            = er["n_values"],
        p_values            = er["p_values"],
        cap_types           = er["cap_types"],
        outdir              = outdir,
        instances_per_combo = er["instances_per_combo"],
        capacity_config     = base_cap_cfg,
    )


# ── CS batch: doubling on d (arcs) AND on capacity hi ─────────────────────────

# Fixed hi used for non-int cap types in the CS batch.
_CS_HI_FIXED = 1023

def _cs_combos(d_steps: list, hi_steps: list, cap_type: str):
    """
    Yield (d_val, hi_val) pairs for a given cap_type in the CS batch.

    - "int"                        → full Cartesian product d_steps × hi_steps  (25 combos)
    - "rational"/"irrational"/"unit" → doubling on d only, hi fixed to _CS_HI_FIXED   (5 combos)
    """
    if cap_type == "int":
        for d_val in d_steps:
            for hi_val in hi_steps:
                yield d_val, hi_val
    else:
        for d_val in d_steps:
            yield d_val, _CS_HI_FIXED


def run_cs_batch(cfg: dict):
    """
    Capacity-scaling batch (graphs/cs).

    "int" cap type: full Cartesian product d_steps × hi_steps (25 combos).
    All other cap types: doubling on d only, hi fixed to _CS_HI_FIXED (5 combos each).
    """
    outdir  = cfg["cs"]["outdir"]
    cs      = cfg["cs"]
    lo      = cfg["capacity"]["lo"]
    big_int = cfg["capacity"]["big_int"]

    d_steps  = cs["d_steps"]
    hi_steps = cs["hi_steps"]

    lay = cs["layered"]
    grd = cs["grid"]
    er  = cs["erdag"]

    # All cap types declared across the three graph configs (order preserved, no duplicates).
    all_cap_types = list(dict.fromkeys(
        lay["cap_types"] + grd["cap_types"] + er["cap_types"]
    ))

    for cap_type in all_cap_types:
        combo_idx = 0
        for d_val, hi_val in _cs_combos(d_steps, hi_steps, cap_type):
            cs_tag  = f"_d{d_val}_hi{hi_val}"
            cap_cfg = build_capacity_config(lo=lo, hi=hi_val, big_int=big_int)

            print(f"\n── CS [{cap_type}] combo {combo_idx:>2}: d={d_val}, hi={hi_val}  (tag={cs_tag}) ──")

            # Layered
            if cap_type in lay["cap_types"]:
                generate_all_instances(
                    n_values            = lay["n_values"],
                    d_values            = [d_val],
                    cap_types           = [cap_type],
                    outdir              = outdir,
                    instances_per_combo = lay["instances_per_combo"],
                    capacity_config     = cap_cfg,
                    cs_tag              = cs_tag,
                )

            # Grid (grid's own row-count d is fixed; cs_tag carries the arc-degree info)
            if cap_type in grd["cap_types"]:
                generate_all_grid_instances(
                    n_values            = grd["n_values"],
                    d                   = grd["d"],
                    cap_types           = [cap_type],
                    outdir              = outdir,
                    instances_per_combo = grd["instances_per_combo"],
                    capacity_config     = cap_cfg,
                    cs_tag              = cs_tag,
                )

            # ER-DAG (p derived from d_val so expected outdegree matches d_val)
            if cap_type in er["cap_types"]:
                er_n_values = er["n_values"]
                p_values    = [round(d_val / (n - 1), 6) for n in er_n_values]
                generate_all_er_dag_instances(
                    n_values            = er_n_values,
                    p_values            = p_values,
                    cap_types           = [cap_type],
                    outdir              = outdir,
                    instances_per_combo = er["instances_per_combo"],
                    capacity_config     = cap_cfg,
                    cs_tag              = cs_tag,
                )

            combo_idx += 1

def run_al_batch(cfg: dict, base_cap_cfg: dict):
    """
    Almost-linear-time batch (graphs/al).
    Unchanged from original: doubling on n, small sizes.
    """
    al_outdir = cfg["almost_linear"]["outdir"]

    # ── Layered
    al_lay = cfg["almost_linear"]["layered"]
    generate_all_instances(
        n_values            = al_lay["n_values"],
        d_values            = al_lay["d_values"],
        cap_types           = al_lay["cap_types"],
        outdir              = al_outdir,
        instances_per_combo = al_lay["instances_per_combo"],
        capacity_config     = base_cap_cfg,
    )

    # ── Grid
    al_grd = cfg["almost_linear"]["grid"]
    generate_all_grid_instances(
        n_values            = al_grd["n_values"],
        d                   = al_grd["d"],
        cap_types           = al_grd["cap_types"],
        outdir              = al_outdir,
        instances_per_combo = al_grd["instances_per_combo"],
        capacity_config     = base_cap_cfg,
    )

    # ── ER-DAG
    al_er = cfg["almost_linear"]["erdag"]
    generate_all_er_dag_instances(
        n_values            = al_er["n_values"],
        p_values            = al_er["p_values"],
        cap_types           = al_er["cap_types"],
        outdir              = al_outdir,
        instances_per_combo = al_er["instances_per_combo"],
        capacity_config     = base_cap_cfg,
    )


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    cfg = load_config()

    # Base capacity config (used by PR and AL; CS overrides hi per step)
    base_cap_cfg = build_capacity_config(
        lo      = cfg["capacity"]["lo"],
        hi      = cfg["capacity"]["hi"],
        big_int = cfg["capacity"]["big_int"],
    )

    print("=" * 60)
    print("  PR batch  (graphs/pr) — doubling n")
    print("=" * 60)
    run_pr_batch(cfg, base_cap_cfg)

    print("\n" + "=" * 60)
    print("  CS batch  (graphs/cs) — doubling d & hi")
    print("=" * 60)
    run_cs_batch(cfg)

    print("\n" + "=" * 60)
    print("  AL batch  (graphs/al) — unchanged")
    print("=" * 60)
    run_al_batch(cfg, base_cap_cfg)