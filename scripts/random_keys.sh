#!/usr/bin/env bash
# ---------------------------------------------------------------------
# random_keys.sh -- experimento con llaves aleatorias (tiempo esperado).
# Estima tiempo medio / peor caso / dispersión de cada acercamiento sobre
# llaves uniformes en [0, N-1], N = 2^24. Usa una llave aleatoria por cada
# uno de K estratos iguales (muestreo estratificado, semilla fija).
# Uso: ./scripts/random_keys.sh [K=30] [np=4] [hilos=2] [semilla=42]
# Salida: resultados_random.csv (programa,np,hilos,llave,tiempo_segundos)
# ---------------------------------------------------------------------
set -euo pipefail
K="${1:-30}"; NP="${2:-4}"; HILOS="${3:-2}"; SEED="${4:-42}"
N=$((2**24)); MAX_KEY=$((N-1))
PLAIN="data/texto.txt"; CIPH="data/texto_cifrado_random.bin"; KW="es una prueba de"
OUT="resultados_random.csv"
echo "programa,np,hilos,llave,tiempo_segundos" > "$OUT"

tiempo() { grep -i 'Tiempo' | grep -oE '[0-9]+\.[0-9]+ segundos' | head -1 | grep -oE '[0-9]+\.[0-9]+' || true; }

mapfile -t KEYS < <(python3 -c "
import random; random.seed($SEED); N=$N; K=$K
print('\n'.join(str(random.randrange(i*N//K,(i+1)*N//K)) for i in range(K)))")

for key in "${KEYS[@]}"; do
  echo ">> llave $key"
  bin/cifrar "$PLAIN" "$CIPH" "$key" > /dev/null
  t=$(bin/secuencial "$CIPH" "$KW" "$MAX_KEY" | tiempo); echo "secuencial,1,1,$key,$t" >> "$OUT"
  for prog in naive_mpi master_worker_mpi striding_mpi shuffled_mpi; do
    t=$(mpirun -np "$NP" "bin/$prog" "$CIPH" "$KW" "$MAX_KEY" 2>&1 | tiempo); echo "$prog,$NP,1,$key,$t" >> "$OUT"
  done
  t=$(mpirun --bind-to none -np "$NP" bin/hybrid_mpi_omp "$CIPH" "$KW" "$MAX_KEY" "$HILOS" 2>&1 | tiempo); echo "hybrid_mpi_omp,$NP,$HILOS,$key,$t" >> "$OUT"
done
rm -f "$CIPH"
echo "Listo: $OUT"
