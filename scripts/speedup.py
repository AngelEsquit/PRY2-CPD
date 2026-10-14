#!/usr/bin/env python3
"""
speedup.py
----------
Lee el CSV generado por scripts/benchmark.sh y calcula:
  - Speedup (T_secuencial / T_paralelo) para cada acercamiento y caso.
  - Eficiencia (Speedup / (np * hilos)).

Genera una tabla de resultados en stdout y, si matplotlib está disponible,
guarda gráficas comparativas en results/.

Uso:
    python3 scripts/speedup.py [resultados_benchmark.csv]

Requisitos opcionales para gráficas:
    pip install matplotlib pandas
"""

import sys
import os
import csv
from collections import defaultdict

# ---------------------------------------------------------------------------
# Lectura del CSV
# ---------------------------------------------------------------------------

def load_csv(path: str) -> list[dict]:
    rows = []
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            row["np"] = int(row["np"])
            row["hilos"] = int(row.get("hilos") or 1)
            row["tiempo_segundos"] = float(row["tiempo_segundos"])
            rows.append(row)
    return rows


def compute_speedups(rows: list[dict]) -> list[dict]:
    """
    Para cada (caso, llave), busca el tiempo de la versión secuencial (np=1)
    y calcula speedup y eficiencia para las versiones paralelas.
    """
    # Indexar tiempos secuenciales por (caso, llave)
    seq_times: dict[tuple, float] = {}
    for row in rows:
        if row["programa"] == "secuencial":
            key = (row["caso"], row["llave"])
            seq_times[key] = row["tiempo_segundos"]

    results = []
    for row in rows:
        if row["programa"] == "secuencial":
            continue
        key = (row["caso"], row["llave"])
        t_seq = seq_times.get(key)
        t_par = row["tiempo_segundos"]
        if t_seq is None or t_par <= 0:
            speedup = None
            eficiencia = None
        else:
            speedup = t_seq / t_par
            eficiencia = speedup / (row["np"] * row["hilos"])  # unidades de cómputo = procesos x hilos

        results.append({
            "programa":  row["programa"],
            "np":        row["np"],
            "hilos":     row["hilos"],
            "caso":      row["caso"],
            "llave":     row["llave"],
            "t_seq":     t_seq,
            "t_par":     t_par,
            "speedup":   speedup,
            "eficiencia": eficiencia,
        })
    return results


# ---------------------------------------------------------------------------
# Presentación en tabla
# ---------------------------------------------------------------------------

def print_table(results: list[dict]) -> None:
    col_w = [20, 4, 22, 10, 10, 8, 10]
    header = ["programa", "np", "caso", "t_seq(s)", "t_par(s)", "speedup", "eficiencia"]

    sep = "+" + "+".join("-" * (w + 2) for w in col_w) + "+"
    fmt = "| " + " | ".join(f"{{:<{w}}}" for w in col_w) + " |"

    print(sep)
    print(fmt.format(*header))
    print(sep)
    for r in results:
        speedup_str   = f"{r['speedup']:.3f}"   if r["speedup"]    is not None else "N/A"
        efic_str      = f"{r['eficiencia']:.3f}" if r["eficiencia"] is not None else "N/A"
        t_seq_str     = f"{r['t_seq']:.4f}"      if r["t_seq"]      is not None else "N/A"
        print(fmt.format(
            r["programa"][:col_w[0]],
            str(r["np"]),
            r["caso"][:col_w[2]],
            t_seq_str[:col_w[3]],
            f"{r['t_par']:.4f}"[:col_w[4]],
            speedup_str[:col_w[5]],
            efic_str[:col_w[6]],
        ))
    print(sep)


# ---------------------------------------------------------------------------
# Gráficas (opcionales, requieren matplotlib + pandas)
# ---------------------------------------------------------------------------

def try_plot(results: list[dict], out_dir: str) -> None:
    try:
        import matplotlib.pyplot as plt  # type: ignore
        import pandas as pd              # type: ignore
    except ImportError:
        print("\n[INFO] matplotlib/pandas no disponibles; se omiten las gráficas.")
        print("       Instalar con: pip install matplotlib pandas")
        return

    os.makedirs(out_dir, exist_ok=True)
    df = pd.DataFrame(results).dropna(subset=["speedup"])

    np_val = int(df["np"].iloc[0])
    orden = ["naive_mpi", "master_worker_mpi", "striding_mpi", "shuffled_mpi", "hybrid_mpi_omp"]
    colores = ["#4C72B0", "#DD8452", "#55A868", "#C44E52", "#8172B3"]

    # Gráfica 1: speedup por acercamiento para cada caso (barras, escala log:
    # los speedups de naive/shuffled/híbrido varían por órdenes de magnitud
    # según la posición de la llave, que es justo el fenómeno que se estudia).
    for caso in df["caso"].unique():
        sub = df[df["caso"] == caso].set_index("programa").reindex(orden).dropna(subset=["speedup"])
        fig, ax = plt.subplots(figsize=(8, 4.2))
        bars = ax.bar(sub.index, sub["speedup"], color=colores[: len(sub)])
        ax.bar_label(bars, fmt="%.2f", fontsize=8)
        ax.axhline(np_val, color="gray", linestyle="--", label=f"speedup ideal con np={np_val}")
        ax.axhline(1.0, color="black", linestyle=":", linewidth=0.8, label="sin mejora (1.0)")
        ax.set_yscale("log")
        ax.set_title(f"Speedup por acercamiento — caso: {caso} (np={np_val})")
        ax.set_ylabel("Speedup (T_seq / T_par), escala log")
        ax.legend(fontsize=7)
        ax.grid(True, axis="y", linestyle=":")
        plt.setp(ax.get_xticklabels(), rotation=20, ha="right", fontsize=8)
        fname = os.path.join(out_dir, f"speedup_{caso}.png")
        fig.savefig(fname, dpi=120, bbox_inches="tight")
        plt.close(fig)
        print(f"  Gráfica guardada: {fname}")

    # Gráfica 2: eficiencia por acercamiento y caso (grupos de barras, log).
    # Eficiencia = speedup / (np * hilos). No se promedia entre casos: un
    # solo caso con suerte posicional (p. ej. naive 222x) dominaría el
    # promedio y ocultaría el comportamiento típico.
    casos = [c for c in ["facil", "media", "dificil"] if c in set(df["caso"])]
    fig, ax = plt.subplots(figsize=(8.5, 4.5))
    ancho = 0.8 / max(len(casos), 1)
    for j, caso in enumerate(casos):
        sub = df[df["caso"] == caso].set_index("programa").reindex(orden)
        xs = [i + j * ancho for i in range(len(orden))]
        ax.bar(xs, sub["eficiencia"], width=ancho, label=caso)
    ax.set_xticks([i + ancho * (len(casos) - 1) / 2 for i in range(len(orden))])
    ax.set_xticklabels(orden, rotation=20, ha="right", fontsize=8)
    ax.axhline(1.0, color="gray", linestyle="--", label="eficiencia ideal (1.0)")
    ax.set_yscale("log")
    ax.set_ylabel("Eficiencia = speedup / (np x hilos), escala log")
    ax.set_title(f"Eficiencia por acercamiento y caso (np={np_val})")
    ax.legend(fontsize=8)
    ax.grid(True, axis="y", linestyle=":")
    fname = os.path.join(out_dir, "eficiencia_por_caso.png")
    fig.savefig(fname, dpi=120, bbox_inches="tight")
    plt.close(fig)
    print(f"  Gráfica guardada: {fname}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "resultados_benchmark.csv"
    if not os.path.exists(csv_path):
        print(f"[ERROR] No se encontró el archivo '{csv_path}'.")
        print("        Genéralo primero con:  ./scripts/benchmark.sh")
        sys.exit(1)

    rows = load_csv(csv_path)
    results = compute_speedups(rows)

    print(f"\n=== Speedup y Eficiencia (fuente: {csv_path}) ===\n")
    print_table(results)

    print("\nGenerando gráficas en results/ ...")
    try_plot(results, "results")


if __name__ == "__main__":
    main()
