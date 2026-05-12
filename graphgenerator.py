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
    print("Full path = ",full_path)
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
    #den = random.randint(1, hi) sostituito per evitare overflow in C++
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
    big = big_fn()  # noqa: F841 – kept for symmetry with original
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


# ── Batch generation ───────────────────────────────────────────────────────────

def generate_all_instances(
    n_values,
    d_values,
    cap_types,
    outdir: str,
    instances_per_combo: int,
    capacity_config: dict,
):
    """Generate layered network instances for all combinations of parameters."""
    total = len(n_values) * len(d_values) * len(cap_types) * instances_per_combo
    done  = 0

    print(f"Generating layered instances")
    print(f"  n      : {n_values}")
    print(f"  d      : {d_values}")
    print(f"  cap    : {cap_types}")
    print(f"  reps   : {instances_per_combo}")
    print(f"  total  : {total} files")
    print(f"  outdir : {outdir}\n")

    for cap_type in cap_types:
        for n in n_values:
            for d in d_values:
                subdir = os.path.join(outdir, cap_type, f"n{n}_d{d}")
                os.makedirs(subdir, exist_ok=True)

                for k in range(instances_per_combo):
                    seed     = k
                    dimacs   = generate_layered_network(n, d, seed, cap_type, capacity_config)
                    filename = f"layered_n{n}_d{d}_seed{k}.max"
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
):
    """Generate grid network instances for all combinations of parameters."""
    total = len(n_values) * len(cap_types) * instances_per_combo
    done  = 0

    print(f"Generating grid instances")
    print(f"  n      : {n_values}")
    print(f"  d      : {d}")
    print(f"  cap    : {cap_types}")
    print(f"  reps   : {instances_per_combo}")
    print(f"  total  : {total} files")
    print(f"  outdir : {outdir}\n")

    for cap_type in cap_types:
        for n in n_values:
            assert n % d == 0, f"n={n} must be divisible by d={d}"

            subdir = os.path.join(outdir, cap_type, f"grid{n}_d{d}")
            os.makedirs(subdir, exist_ok=True)

            for k in range(instances_per_combo):
                seed     = 1000 * n + k
                dimacs   = generate_grid_network(n, d, seed, cap_type, capacity_config)
                filename = f"grid_n{n}_d{d}_seed{k}.max"
                filepath = os.path.join(subdir, filename)

                with open(filepath, "w") as f:
                    f.write(dimacs)

                done += 1
                if done % 20 == 0 or done == total:
                    print(f"  [{done:>4}/{total}] {filepath}")

    print(f"\nDone. {total} grid files written under '{outdir}/'.")


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    cfg = load_config()

    cap_cfg = build_capacity_config(
        lo      = cfg["capacity"]["lo"],
        hi      = cfg["capacity"]["hi"],
        big_int = cfg["capacity"]["big_int"],
    )
    outdir = cfg["output"]["outdir"]

    # Standard layered batch
    lay = cfg["layered"]
    generate_all_instances(
        n_values            = lay["n_values"],
        d_values            = lay["d_values"],
        cap_types           = lay["cap_types"],
        outdir              = outdir,
        instances_per_combo = lay["instances_per_combo"],
        capacity_config     = cap_cfg,
    )

    # Standard grid batch
    grd = cfg["grid"]
    generate_all_grid_instances(
        n_values            = grd["n_values"],
        d                   = grd["d"],
        cap_types           = grd["cap_types"],
        outdir              = outdir,
        instances_per_combo = grd["instances_per_combo"],
        capacity_config     = cap_cfg,
    )

    # Almost-linear-time layered batch
    al_outdir = cfg["almost_linear"]["outdir"]
    al_lay = cfg["almost_linear"]["layered"]
    generate_all_instances(
        n_values            = al_lay["n_values"],
        d_values            = al_lay["d_values"],
        cap_types           = al_lay["cap_types"],
        outdir              = al_outdir,
        instances_per_combo = al_lay["instances_per_combo"],
        capacity_config     = cap_cfg,
    )

    # Almost-linear-time grid batch
    al_grd = cfg["almost_linear"]["grid"]
    generate_all_grid_instances(
        n_values            = al_grd["n_values"],
        d                   = al_grd["d"],
        cap_types           = al_grd["cap_types"],
        outdir              = al_outdir,
        instances_per_combo = al_grd["instances_per_combo"],
        capacity_config     = cap_cfg,
    )