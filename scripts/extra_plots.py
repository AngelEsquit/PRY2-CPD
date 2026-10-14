#!/usr/bin/env python3
"""Gráficas extra del informe a partir de los CSV de resultados.
  results/np_scaling.png     tiempo y speedup vs np (resultados_np_scaling.csv)
  results/hybrid_hilos.png   speedup vs hilos del híbrido y efecto del propietario
                             (resultados_hybrid_threads.csv)
Uso: python3 scripts/extra_plots.py"""
import csv, os
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

os.makedirs("results", exist_ok=True)
SEQ_DIFICIL = None
for r in csv.DictReader(open("resultados_benchmark.csv", encoding="utf-8")):
    if r["programa"] == "secuencial" and r["caso"] == "dificil":
        SEQ_DIFICIL = float(r["tiempo_segundos"])

# ---- np scaling
d = defaultdict(dict)
for r in csv.DictReader(open("resultados_np_scaling.csv", encoding="utf-8")):
    d[r["programa"]][int(r["np"])] = float(r["tiempo_segundos"])
nombres = {"naive_mpi": "naive", "master_worker_mpi": "master-worker", "striding_mpi": "striding",
           "shuffled_mpi": "shuffled", "hybrid_mpi_omp": "híbrido (2 hilos)"}
fig, ax = plt.subplots(1, 2, figsize=(10, 4))
for prog, nom in nombres.items():
    nps = sorted(d[prog]); t = [d[prog][n] for n in nps]
    ax[0].plot(nps, t, marker="o", label=nom)
    ax[1].plot(nps, [SEQ_DIFICIL / x for x in t], marker="o", label=nom)
nps = [2, 4, 8, 16]
ax[1].plot(nps, nps, "k--", lw=0.8, label="ideal (np)")
for a, y in zip(ax, ["Tiempo de búsqueda (s)", "Speedup vs. secuencial"]):
    a.set_xscale("log", base=2); a.set_yscale("log"); a.set_xticks(nps); a.set_xticklabels(nps)
    a.set_xlabel("Procesos MPI (np)"); a.set_ylabel(y); a.grid(True, ls=":")
ax[0].set_title("Caso difícil, espacio 2^27"); ax[1].legend(fontsize=7)
fig.savefig("results/np_scaling.png", dpi=130, bbox_inches="tight"); plt.close(fig)

# ---- hilos del híbrido
h = defaultdict(dict); own = []
for r in csv.DictReader(open("resultados_hybrid_threads.csv", encoding="utf-8")):
    if r["variante"] == "propietario":
        own.append(float(r["tiempo_segundos"]))
    else:
        h[r["variante"]][int(r["hilos"])] = float(r["tiempo_segundos"])
fig, ax = plt.subplots(1, 2, figsize=(10, 4))
etq = {"hibrido": "híbrido, hilos libres (--bind-to none)",
       "hibrido_atado": "híbrido, binding por defecto (1 núcleo)",
       "striding": "striding, T procesos MPI"}
for k, nom in etq.items():
    ts = sorted(h[k]); ax[0].plot(ts, [h[k][1] / h[k][x] for x in ts], marker="o", label=nom)
ts = sorted(h["hibrido"]); ax[0].plot(ts, ts, "k--", lw=0.8, label="ideal")
ax[0].axvline(12, color="gray", ls=":", lw=0.8); ax[0].text(12.2, 1.2, "12 hilos\n(6 núcleos P\ncon SMT)", fontsize=7)
ax[0].set_xlabel("Hilos / procesos (T)"); ax[0].set_ylabel("Speedup vs. T = 1")
ax[0].set_title("Escalado con np = 1, caso difícil"); ax[0].legend(fontsize=7); ax[0].grid(True, ls=":")
ax[1].bar(range(8), own, color="#4C72B0")
ax[1].set_xticks(range(8)); ax[1].set_xticklabels([f"+{i}" for i in range(8)])
ax[1].set_xlabel("Llave = llave base + d (la posee el hilo d mod 8)"); ax[1].set_ylabel("Tiempo (s), T = 8")
ax[1].set_title("Efecto del hilo propietario de la llave"); ax[1].grid(True, axis="y", ls=":")
fig.savefig("results/hybrid_hilos.png", dpi=130, bbox_inches="tight"); plt.close(fig)
print("results/np_scaling.png, results/hybrid_hilos.png")

# ---- reparto procesos x hilos con 8 unidades de cómputo (30 llaves aleatorias)
seq = {int(r["llave"]): float(r["tiempo_segundos"]) for r in csv.DictReader(open("resultados_random.csv", encoding="utf-8")) if r["programa"] == "secuencial"}
g = defaultdict(dict)
for r in csv.DictReader(open("resultados_hybrid_grid.csv", encoding="utf-8")):
    g[(int(r["np"]), int(r["hilos"]))][int(r["llave"])] = float(r["tiempo_segundos"])
import statistics as st
cfgs = [(8, 1), (4, 2), (2, 4), (1, 8)]
print("np x hilos | media | mediana | max | desv | Tseq/T | efic(/8) | speedup min-max")
for c in cfgs:
    ks = sorted(g[c]); t = [g[c][k] for k in ks]; sp = [seq[k] / g[c][k] for k in ks]
    m = st.mean(t); r_ = st.mean(seq.values()) / m
    print(f"{c[0]}x{c[1]} | {m:.3f} | {st.median(t):.3f} | {max(t):.3f} | {st.stdev(t):.3f} | {r_:.2f} | {r_/8:.2f} | {min(sp):.2f}-{max(sp):.2f}")
fig, ax = plt.subplots(figsize=(6, 4))
ax.boxplot([[g[c][k] for k in sorted(g[c])] for c in cfgs], tick_labels=[f"{a} proc x {b} hilos" for a, b in cfgs], showmeans=True)
ax.set_ylabel("Tiempo de búsqueda (s)"); ax.set_title("8 unidades de cómputo, 30 llaves aleatorias (N = 2^24)")
ax.grid(True, axis="y", ls=":"); plt.setp(ax.get_xticklabels(), fontsize=8)
fig.savefig("results/hybrid_grid.png", dpi=130, bbox_inches="tight"); plt.close(fig)
