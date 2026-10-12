#!/usr/bin/env bash
# ---------------------------------------------------------------------
# benchmark.sh
# ---------------------------------------------------------------------
# Automatiza las pruebas pedidas en la Parte B del enunciado y deja los
# resultados en un CSV para usar como evidencia en el reporte (Anexo 2:
# bitácora de pruebas y speedups).
#
# Uso:
#   ./scripts/benchmark.sh
#
# Debe ejecutarse desde la raíz del proyecto, después de compilar con
# `make`. Requiere Open MPI (mpirun) en el PATH.
# ---------------------------------------------------------------------
set -euo pipefail

PLAIN="data/texto.txt"
CIPH="data/texto_cifrado.bin"
KEYWORD="es una prueba de"
NP=4
OUT_CSV="resultados_benchmark.csv"

echo "programa,np,caso,llave,tiempo_segundos" > "$OUT_CSV"

# Llaves de prueba definidas en el enunciado (Parte B.2 y B.5), asumiendo
# un espacio de llaves de 2^56 y np=4. Ajustar si se usa otro np.
KEYS_NOMBRES=(baseline_123456 dificil_no_termina justo_arriba facil media dificil)
KEYS_VALORES=(
  123456
  18014398509481983
  18014398509481984
  $(( (2**56)/2 + 1 ))
  $(( (2**56)/2 + (2**56)/8 ))
  $(( (2**56)/7 + (2**56)/13 + 1 ))
)

run_case() {
  local nombre="$1"
  local llave="$2"
  local programa="$3"

  echo ">> [$programa] caso '$nombre' (llave=$llave)"
  local t0 t1 elapsed
  t0=$(date +%s.%N)
  mpirun -np "$NP" "bin/$programa" "$CIPH" "$KEYWORD" "$llave" || true
  t1=$(date +%s.%N)
  elapsed=$(echo "$t1 - $t0" | bc)
  echo "$programa,$NP,$nombre,$llave,$elapsed" >> "$OUT_CSV"
}

for i in "${!KEYS_NOMBRES[@]}"; do
  nombre="${KEYS_NOMBRES[$i]}"
  llave="${KEYS_VALORES[$i]}"

  # Se cifra el texto con la llave del caso actual antes de intentar romperlo.
  bin/cifrar "$PLAIN" "$CIPH" "$llave" > /dev/null

  # ADVERTENCIA: el caso 'dificil_no_termina' (2^56/4) puede tardar
  # muchísimo o no terminar con el acercamiento naive, tal como advierte
  # el enunciado. Considere comentar esa línea o limitar el tiempo con
  # `timeout` si sólo quiere evidenciar que no termina en tiempo
  # razonable.
  run_case "$nombre" "$llave" "naive_mpi"
  run_case "$nombre" "$llave" "master_worker_mpi"
  run_case "$nombre" "$llave" "striding_mpi"
  run_case "$nombre" "$llave" "shuffled_mpi"
  run_case "$nombre" "$llave" "hybrid_mpi_omp"
done

echo "Resultados guardados en $OUT_CSV"
