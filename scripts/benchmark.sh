#!/usr/bin/env bash
# ---------------------------------------------------------------------
# benchmark.sh
# ---------------------------------------------------------------------
# Automatiza las pruebas pedidas en la Parte B del enunciado y deja los
# resultados en un CSV para usar como evidencia en el reporte (Anexo 2:
# bitácora de pruebas y speedups).
#
# Uso:
#   ./scripts/benchmark.sh [np] [timeout_segundos] [hilos_openmp]
#
# Ejemplos:
#   ./scripts/benchmark.sh          # np=4, sin timeout en paralelos
#   ./scripts/benchmark.sh 8        # np=8
#   ./scripts/benchmark.sh 4 120    # np=4, timeout de 2 min por caso
#
# Debe ejecutarse desde la raíz del proyecto, después de compilar con
# `make`. Requiere Open MPI (mpirun) en el PATH.
# ---------------------------------------------------------------------
set -euo pipefail

PLAIN="data/texto.txt"
CIPH="data/texto_cifrado.bin"
KEYWORD="es una prueba de"
NP="${1:-4}"
TIMEOUT_SECS="${2:-0}"   # 0 = sin límite de tiempo
HILOS="${3:-2}"           # hilos OpenMP por proceso en hybrid_mpi_omp
OUT_CSV="resultados_benchmark.csv"

echo "=== Benchmark Proyecto 2 — np=$NP $([ "$TIMEOUT_SECS" -gt 0 ] && echo ", timeout=${TIMEOUT_SECS}s" || echo "") ==="
echo "programa,np,hilos,caso,llave,tiempo_segundos,tiempo_wall_segundos" > "$OUT_CSV"

# Llaves de prueba (Parte B.5), usando un espacio efectivo reducido
# EFFECTIVE_N en lugar de 2^56 real: a 2^56 ni la versión naive es
# ejecutable en tiempo humano (requeriría años, sin importar cuántos
# procesos se usen, porque el trabajo total sigue siendo O(N)). Se
# mantienen las mismas fracciones del enunciado (N/2+1, N/2+N/8,
# N/7+N/13) aplicadas sobre EFFECTIVE_N, preservando el fenómeno que se
# quiere demostrar (posición de la llave dentro del segmento) con
# tiempos de ejecución razonables. Ver discusión en el informe.
EFFECTIVE_N=$((2**27))
# Espacio de búsqueda FIJO [0, EFFECTIVE_N-1] para todos los casos: lo que
# varía entre casos es sólo la posición de la llave (como en el enunciado,
# donde las llaves son fracciones de 2^56 dentro de un mismo espacio).
MAX_KEY=$(( EFFECTIVE_N - 1 ))   # ~134M llaves; ajustar si se corre en hardware más lento/rápido

KEYS_NOMBRES=(baseline_123456 facil media dificil)
KEYS_VALORES=(
  123456
  $(( EFFECTIVE_N/2 + 1 ))
  $(( EFFECTIVE_N/2 + EFFECTIVE_N/8 ))
  $(( EFFECTIVE_N/7 + EFFECTIVE_N/13 + 1 ))
)

# Extrae el tiempo interno (MPI_Wtime / clock) que imprime cada programa en
# su línea "Tiempo ...: X segundos". Es el tiempo de cómputo de la búsqueda,
# sin el arranque de mpirun (~1.5 s), que sí está incluido en el tiempo de
# pared (columna tiempo_wall_segundos).
extract_internal_time() {
  grep -i 'Tiempo' | grep -oE '[0-9]+\.[0-9]+ segundos' | head -1 | grep -oE '[0-9]+\.[0-9]+' || true
}

# CSV: programa,np,hilos,caso,llave,tiempo_segundos,tiempo_wall_segundos
run_one() {
  local programa="$1" np="$2" hilos="$3" nombre="$4" llave="$5"
  shift 5
  local t0 t1 wall out interno
  echo ">> [$programa] caso '$nombre' (llave=$llave)"
  t0=$(date +%s.%N)
  if [ "$TIMEOUT_SECS" -gt 0 ]; then
    out=$(timeout "$TIMEOUT_SECS" "$@" 2>&1 || true)
  else
    out=$("$@" 2>&1 || true)
  fi
  t1=$(date +%s.%N)
  wall=$(echo "$t1 - $t0" | bc)
  interno=$(echo "$out" | extract_internal_time)
  [ -z "$interno" ] && interno="$wall"   # sin salida (timeout): se usa el tiempo de pared
  echo "$programa,$np,$hilos,$nombre,$llave,$interno,$wall" >> "$OUT_CSV"
}

run_seq() {
  run_one secuencial 1 1 "$1" "$2" bin/secuencial "$CIPH" "$KEYWORD" "$MAX_KEY"
}

run_parallel() {
  local nombre="$1" llave="$2" programa="$3"
  if [ "$programa" = "hybrid_mpi_omp" ]; then
    run_one "$programa" "$NP" "$HILOS" "$nombre" "$llave" \
      mpirun -np "$NP" "bin/$programa" "$CIPH" "$KEYWORD" "$MAX_KEY" "$HILOS"
  else
    run_one "$programa" "$NP" 1 "$nombre" "$llave" \
      mpirun -np "$NP" "bin/$programa" "$CIPH" "$KEYWORD" "$MAX_KEY"
  fi
}

for i in "${!KEYS_NOMBRES[@]}"; do
  nombre="${KEYS_NOMBRES[$i]}"
  llave="${KEYS_VALORES[$i]}"

  echo ""
  echo "===== Caso: $nombre (llave=$llave) ====="

  # Se cifra el texto con la llave del caso actual antes de intentar romperlo.
  bin/cifrar "$PLAIN" "$CIPH" "$llave" > /dev/null

  run_seq        "$nombre" "$llave"
  run_parallel   "$nombre" "$llave" "naive_mpi"
  run_parallel   "$nombre" "$llave" "master_worker_mpi"
  run_parallel   "$nombre" "$llave" "striding_mpi"
  run_parallel   "$nombre" "$llave" "shuffled_mpi"
  run_parallel   "$nombre" "$llave" "hybrid_mpi_omp"
done

# -----------------------------------------------------------------------
# NOTA sobre Parte B.2 (np=4, llaves 123456, 2^56/4 y 2^56/4+1): no se
# automatiza aquí. Se corrió a mano con naive_mpi y max_key = 2^56-1
# (72057594037927935): 123456 y 2^56/4+1 terminan en milisegundos; 2^56/4
# no termina en tiempo humano (ver informe, sección 3.2). Este script usa el
# espacio reducido 2^27 para las comparaciones fácil/media/difícil.
# -----------------------------------------------------------------------

echo ""
echo "Resultados guardados en $OUT_CSV"
