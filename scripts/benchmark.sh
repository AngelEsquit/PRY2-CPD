#!/usr/bin/env bash
# ---------------------------------------------------------------------
# benchmark.sh
# ---------------------------------------------------------------------
# Automatiza las pruebas pedidas en la Parte B del enunciado y deja los
# resultados en un CSV para usar como evidencia en el reporte (Anexo 2:
# bitácora de pruebas y speedups).
#
# Uso:
#   ./scripts/benchmark.sh [np] [timeout_segundos]
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
OUT_CSV="resultados_benchmark.csv"

echo "=== Benchmark Proyecto 2 — np=$NP $([ "$TIMEOUT_SECS" -gt 0 ] && echo ", timeout=${TIMEOUT_SECS}s" || echo "") ==="
echo "programa,np,caso,llave,tiempo_segundos" > "$OUT_CSV"

# Llaves de prueba (Parte B.5), usando un espacio efectivo reducido
# EFFECTIVE_N en lugar de 2^56 real: a 2^56 ni la versión naive es
# ejecutable en tiempo humano (requeriría años, sin importar cuántos
# procesos se usen, porque el trabajo total sigue siendo O(N)). Se
# mantienen las mismas fracciones del enunciado (N/2+1, N/2+N/8,
# N/7+N/13) aplicadas sobre EFFECTIVE_N, preservando el fenómeno que se
# quiere demostrar (posición de la llave dentro del segmento) con
# tiempos de ejecución razonables. Ver discusión en el informe.
EFFECTIVE_N=$((2**27))   # ~134M llaves; ajustar si se corre en hardware más lento/rápido

KEYS_NOMBRES=(baseline_123456 facil media dificil)
KEYS_VALORES=(
  123456
  $(( EFFECTIVE_N/2 + 1 ))
  $(( EFFECTIVE_N/2 + EFFECTIVE_N/8 ))
  $(( EFFECTIVE_N/7 + EFFECTIVE_N/13 + 1 ))
)

run_seq() {
  local nombre="$1"
  local llave="$2"

  echo ">> [secuencial] caso '$nombre' (llave=$llave)"
  local t0 t1 elapsed
  t0=$(date +%s.%N)
  if [ "$TIMEOUT_SECS" -gt 0 ]; then
    timeout "$TIMEOUT_SECS" bin/secuencial "$CIPH" "$KEYWORD" "$llave" || true
  else
    bin/secuencial "$CIPH" "$KEYWORD" "$llave" || true
  fi
  t1=$(date +%s.%N)
  elapsed=$(echo "$t1 - $t0" | bc)
  echo "secuencial,1,$nombre,$llave,$elapsed" >> "$OUT_CSV"
}

run_parallel() {
  local nombre="$1"
  local llave="$2"
  local programa="$3"

  echo ">> [$programa] caso '$nombre' (llave=$llave)"
  local t0 t1 elapsed
  t0=$(date +%s.%N)
  if [ "$TIMEOUT_SECS" -gt 0 ]; then
    # ADVERTENCIA: el caso 'dificil_no_termina' (2^56/4) puede tardar
    # muchísimo o no terminar con el acercamiento naive. El parámetro
    # timeout limita el tiempo máximo de espera.
    timeout "$TIMEOUT_SECS" mpirun -np "$NP" "bin/$programa" "$CIPH" "$KEYWORD" "$llave" || true
  else
    mpirun -np "$NP" "bin/$programa" "$CIPH" "$KEYWORD" "$llave" || true
  fi
  t1=$(date +%s.%N)
  elapsed=$(echo "$t1 - $t0" | bc)
  echo "$programa,$NP,$nombre,$llave,$elapsed" >> "$OUT_CSV"
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
# NOTA sobre Parte B.2 (bordes de segmento, np=4, 2^56/4 y 2^56/4+1):
# se dejó como análisis teórico en el informe (igual que lo plantea el
# enunciado, en modo hipotético: "un algoritmo secuencial le hubiera
# tomado..."). No se automatiza aquí una demo "en vivo" de esos bordes
# porque, con un texto/keyword tan cortos, decrypts con llaves
# incorrectas pueden coincidir por azar con la keyword buscada (falso
# positivo) antes de llegar a la llave real, lo que vuelve el timing
# de un caso de borde específico no reproducible de forma confiable.
# Ver discusión de falsos positivos y colisiones de paridad DES en el
# informe.
# -----------------------------------------------------------------------

echo ""
echo "Resultados guardados en $OUT_CSV"
