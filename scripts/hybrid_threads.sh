#!/usr/bin/env bash
# ---------------------------------------------------------------------
# hybrid_threads.sh -- escalado del híbrido con el número de hilos.
# Caso "difícil" (llave N/7+N/13+1) en [0, 2^27-1] con np=1, así el bloque
# completo lo recorren sólo los hilos de OpenMP. Compara:
#   hibrido        : mpirun --bind-to none (hilos libres en todos los núcleos)
#   hibrido_atado  : mpirun con el binding por defecto de Open MPI 5 (el rank
#                    queda atado a UN núcleo y sus hilos lo comparten)
#   striding       : T procesos MPI (sin OpenMP), mismo reparto intercalado
#   propietario    : T=8 con 8 llaves consecutivas (cada una la posee otro hilo)
# Uso: ./scripts/hybrid_threads.sh [lista_hilos="1 2 3 4 6 8 10 12 14 16 20"]
# Salida: resultados_hybrid_threads.csv (variante,hilos,llave,tiempo_segundos)
# ---------------------------------------------------------------------
set -euo pipefail
THREADS="${1:-1 2 3 4 6 8 10 12 14 16 20}"
N=$((2**27)); MAX_KEY=$((N-1)); KEY=$(( N/7 + N/13 + 1 ))
CIPH="data/texto_cifrado_hilos.bin"; KW="es una prueba de"; OUT="resultados_hybrid_threads.csv"
echo "variante,hilos,llave,tiempo_segundos" > "$OUT"
t() { grep -i 'Tiempo' | grep -oE '[0-9]+\.[0-9]+ segundos' | head -1 | grep -oE '[0-9]+\.[0-9]+' || true; }
cifrar() { bin/cifrar data/texto.txt "$CIPH" "$1" > /dev/null; }

cifrar "$KEY"
for T in $THREADS; do
  v=$(mpirun --bind-to none -np 1 bin/hybrid_mpi_omp "$CIPH" "$KW" "$MAX_KEY" "$T" 2>&1 | t); echo "hibrido,$T,$KEY,$v" >> "$OUT"
  v=$(mpirun -np 1 bin/hybrid_mpi_omp "$CIPH" "$KW" "$MAX_KEY" "$T" 2>&1 | t); echo "hibrido_atado,$T,$KEY,$v" >> "$OUT"
  v=$(mpirun --oversubscribe -np "$T" bin/striding_mpi "$CIPH" "$KW" "$MAX_KEY" 2>&1 | t); echo "striding,$T,$KEY,$v" >> "$OUT"
done
for d in 0 1 2 3 4 5 6 7; do
  k=$((KEY + d)); cifrar "$k"
  v=$(mpirun --bind-to none -np 1 bin/hybrid_mpi_omp "$CIPH" "$KW" "$MAX_KEY" 8 2>&1 | t); echo "propietario,8,$k,$v" >> "$OUT"
done
rm -f "$CIPH"
echo "Listo: $OUT"
