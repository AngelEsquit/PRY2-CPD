#!/usr/bin/env python3
"""
speedup.py
----------
Lee el CSV generado por scripts/benchmark.sh y calcula:
  - Speedup (T_secuencial / T_paralelo) para cada acercamiento y caso.
  - Eficiencia (Speedup / np).

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
            eficiencia = speedup / row["np"]

        results.append({
            "programa":  row["programa"],
            "np":        row["np"],
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

    # Gráfica 1: Speedup por programa para cada caso
    for caso in df["caso"].unique():
        sub = df[df["caso"] == caso]
        fig, ax = plt.subplots(figsize=(8, 4))
        for prog in sub["programa"].unique():
            psub = sub[sub["programa"] == prog].sort_values("np")
            ax.plot(psub["np"], psub["speedup"], marker="o", label=prog)
        ax.axline((0, 0), slope=1, color="gray", linestyle="--", label="speedup ideal")
        ax.set_title(f"Speedup — caso: {caso}")
        ax.set_xlabel("Número de procesos (np)")
        ax.set_ylabel("Speedup")
        ax.legend(fontsize=7)
        ax.grid(True, linestyle=":")
        fname = os.path.join(out_dir, f"speedup_{caso}.png")
        fig.savefig(fname, dpi=120, bbox_inches="tight")
        plt.close(fig)
        print(f"  Gráfica guardada: {fname}")

    # Gráfica 2: Eficiencia promedio por acercamiento (barras).
    # NOTA: se usa un gráfico de barras, no una línea por np, porque todas
    # las corridas de este proyecto se hicieron a un único np (ver
    # 'resultados_benchmark.csv'); un gráfico de línea por np aquí
    # implicaría falsamente una variación de np que no se midió.
    eff_by_prog = df.groupby("programa")["eficiencia"].mean().sort_values(ascending=False)
    fig, ax = plt.subplots(figsize=(8, 4.5))
    bars = ax.bar(eff_by_prog.index, eff_by_prog.values, color="#4C72B0")
    ax.bar_label(bars, fmt="%.2f", fontsize=8)
    ax.axhline(1.0, color="gray", linestyle="--", label="eficiencia ideal (1.0)")
    ax.set_title(f"Eficiencia promedio por acercamiento (np={df['np'].iloc[0]}, "
                 f"promedio sobre {df['caso'].nunique()} casos)")
    ax.set_xlabel("Acercamiento")
    ax.set_ylabel("Eficiencia (Speedup / np)")
    ax.legend(fontsize=8)
    ax.grid(True, axis="y", linestyle=":")
    plt.setp(ax.get_xticklabels(), rotation=20, ha="right")
    fname = os.path.join(out_dir, "eficiencia_promedio.png")
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
