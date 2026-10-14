#!/usr/bin/env bash
# ---------------------------------------------------------------------
# hybrid_grid.sh -- mismas 8 unidades de cómputo repartidas de distinta
# forma entre procesos MPI y hilos OpenMP (np x hilos = 8), sobre las mismas
# 30 llaves aleatorias de random_keys.sh (N = 2^24, semilla 42).
# Uso: ./scripts/hybrid_grid.sh [K=30] [semilla=42]
# Salida: resultados_hybrid_grid.csv (np,hilos,llave,tiempo_segundos)
# ---------------------------------------------------------------------
set -euo pipefail
K="${1:-30}"; SEED="${2:-42}"
N=$((2**24)); MAX_KEY=$((N-1))
CIPH="data/texto_cifrado_grid.bin"; KW="es una prueba de"; OUT="resultados_hybrid_grid.csv"
echo "np,hilos,llave,tiempo_segundos" > "$OUT"
t() { grep -i 'Tiempo' | grep -oE '[0-9]+\.[0-9]+ segundos' | head -1 | grep -oE '[0-9]+\.[0-9]+' || true; }
mapfile -t KEYS < <(python3 -c "
import random; random.seed($SEED); N=$N; K=$K
print('\n'.join(str(random.randrange(i*N//K,(i+1)*N//K)) for i in range(K)))")
for key in "${KEYS[@]}"; do
  bin/cifrar data/texto.txt "$CIPH" "$key" > /dev/null
  for cfg in "8 1" "4 2" "2 4" "1 8"; do
    set -- $cfg
    v=$(mpirun --bind-to none -np "$1" bin/hybrid_mpi_omp "$CIPH" "$KW" "$MAX_KEY" "$2" 2>&1 | t)
    echo "$1,$2,$key,$v" >> "$OUT"
  done
done
rm -f "$CIPH"
echo "Listo: $OUT"
