#!/usr/bin/env python3
"""Resume resultados_random.csv: media, peor caso, desv. estándar, CV del
tiempo y del speedup (T_seq/T_par) por acercamiento, y la predicción del
modelo (media N/(2 p r) para naive/striding, N/(2 (p-1) r) para master-worker).
Uso: python3 scripts/random_summary.py [resultados_random.csv]"""
import csv, sys, statistics as st
from collections import defaultdict

path = sys.argv[1] if len(sys.argv) > 1 else "resultados_random.csv"
rows = list(csv.DictReader(open(path, encoding="utf-8")))
T = defaultdict(dict)
for r in rows:
    T[r["programa"]][int(r["llave"])] = float(r["tiempo_segundos"])
seq = T["secuencial"]
N = 2 ** 24
p = int(next(r["np"] for r in rows if r["programa"] == "naive_mpi"))
rate = st.mean(k / t for k, t in seq.items() if t > 0.2)  # tasa en llaves/s (ignora llaves muy pequeñas)
print(f"K={len(seq)} llaves, N=2^24, np={p}, tasa={rate/1e6:.2f} M llaves/s")
print(f"Modelo: media naive/striding = {N/(2*p*rate):.2f} s, master-worker = {N/(2*(p-1)*rate):.2f} s, secuencial = {N/(2*rate):.2f} s\n")
hdr = f"{'programa':<18}{'T_seq/T_par':>12}{'media':>8}{'mediana':>9}{'max':>8}{'sd':>7}{'CV t':>7}{'spd medio':>11}{'spd min':>9}{'spd max':>9}{'CV spd':>8}"
print(hdr)
for prog in ["secuencial", "naive_mpi", "master_worker_mpi", "striding_mpi", "shuffled_mpi", "hybrid_mpi_omp"]:
    ks = sorted(T[prog]); t = [T[prog][k] for k in ks]
    sp = [seq[k] / T[prog][k] for k in ks if T[prog][k] > 0]
    ratio = st.mean(list(seq.values())) / st.mean(t)
    print(f"{prog:<18}{ratio:12.2f}{st.mean(t):8.2f}{st.median(t):9.2f}{max(t):8.2f}{st.stdev(t):7.2f}{st.stdev(t)/st.mean(t):7.2f}"
          f"{st.mean(sp):11.2f}{min(sp):9.2f}{max(sp):9.2f}{st.stdev(sp)/st.mean(sp):8.2f}")

# ---- Gráfica de variabilidad (cajas de tiempo y de speedup) ----
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt, os
    progs = ["naive_mpi", "master_worker_mpi", "striding_mpi", "shuffled_mpi", "hybrid_mpi_omp"]
    nombres = ["naive", "master-w.", "striding", "shuffled", "híbrido"]
    ks = sorted(seq)
    tiempos = [[T[q][k] for k in ks] for q in progs]
    speed = [[seq[k] / T[q][k] for k in ks] for q in progs]
    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    ax[0].boxplot(tiempos, tick_labels=nombres, showmeans=True)
    ax[0].set_ylabel("Tiempo paralelo (s)"); ax[0].set_title(f"Tiempo, {len(ks)} llaves aleatorias (np={p})")
    ax[1].boxplot(speed, tick_labels=nombres, showmeans=True)
    ax[1].set_yscale("log"); ax[1].axhline(p, color="gray", ls="--", label=f"ideal np={p}")
    ax[1].axhline(1, color="black", ls=":", lw=0.8)
    ax[1].set_ylabel("Speedup (escala log)"); ax[1].set_title("Speedup por llave"); ax[1].legend(fontsize=7)
    for a in ax: a.grid(True, axis="y", ls=":")
    os.makedirs("results", exist_ok=True)
    fig.savefig("results/variabilidad_random.png", dpi=130, bbox_inches="tight")
    print("\nGráfica: results/variabilidad_random.png")
except ImportError:
    print("matplotlib no disponible; se omite la gráfica.")
