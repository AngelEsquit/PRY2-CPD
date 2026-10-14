#!/usr/bin/env bash
# ---------------------------------------------------------------------
# np_scaling.sh -- variación del tiempo con el número de procesos.
# Caso "difícil" (llave N/7+N/13+1) en el espacio fijo [0, 2^27-1].
# Uso: ./scripts/np_scaling.sh [lista_np="2 4 8 16"] [hilos=2]
# Salida: resultados_np_scaling.csv (programa,np,llave,tiempo_segundos)
# ---------------------------------------------------------------------
set -euo pipefail
NPS="${1:-2 4 8 16}"; HILOS="${2:-2}"
N=$((2**27)); MAX_KEY=$((N-1)); KEY=$(( N/7 + N/13 + 1 ))
CIPH="data/texto_cifrado_np.bin"; KW="es una prueba de"; OUT="resultados_np_scaling.csv"
bin/cifrar data/texto.txt "$CIPH" "$KEY" > /dev/null
echo "programa,np,llave,tiempo_segundos" > "$OUT"
for prog in naive_mpi master_worker_mpi striding_mpi shuffled_mpi hybrid_mpi_omp; do
  for np in $NPS; do
    extra=""; [ "$prog" = hybrid_mpi_omp ] && extra="$HILOS"
    bind=""; [ "$prog" = hybrid_mpi_omp ] && bind="--bind-to none"
    t=$(timeout 120 mpirun --oversubscribe $bind -np "$np" "bin/$prog" "$CIPH" "$KW" "$MAX_KEY" $extra 2>&1 \
        | grep -i 'Tiempo' | grep -oE '[0-9]+\.[0-9]+ segundos' | head -1 | grep -oE '[0-9]+\.[0-9]+' || true)
    echo "$prog,$np,$KEY,$t" >> "$OUT"
  done
done
rm -f "$CIPH"
echo "Listo: $OUT"
