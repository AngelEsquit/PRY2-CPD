#!/usr/bin/env bash
# ---------------------------------------------------------------------
# demo.sh -- demo corta (~1 min) para la presentación.
# 1) cifrar/descifrar con llave 42   2) naive con 2^56/4+1 (instantáneo)
# 3) los 6 programas buscando la misma llave en [0, 2^24-1], np=4
# Uso: ./scripts/demo.sh [llave=12000000]
# ---------------------------------------------------------------------
set -euo pipefail
KEY="${1:-12000000}"; KW="es una prueba de"; C=data/texto_cifrado_demo.bin
N=$((2**24)); M=$((N-1)); BIG=72057594037927935
[ -x bin/naive_mpi ] || make > /dev/null
linea() { grep -E 'Llave encontrada|Tiempo'; }

echo "=== 1. Cifrar y descifrar con llave 42 ==="
bin/cifrar data/texto.txt "$C" 42; bin/descifrar "$C" 42

echo; echo "=== 2. Naive np=4, llave 2^56/4+1 en [0, 2^56-1] (inicio del bloque del proceso 1) ==="
bin/cifrar data/texto.txt "$C" 18014398509481984 > /dev/null
mpirun --oversubscribe -np 4 bin/naive_mpi "$C" "$KW" "$BIG" | linea

echo; echo "=== 3. Misma llave ($KEY) con cada acercamiento, espacio [0, 2^24-1] ==="
bin/cifrar data/texto.txt "$C" "$KEY" > /dev/null
echo "-- secuencial";          bin/secuencial "$C" "$KW" "$M" | linea
echo "-- naive np=4";          mpirun -np 4 bin/naive_mpi "$C" "$KW" "$M" | linea
echo "-- master-worker np=4";  mpirun -np 4 bin/master_worker_mpi "$C" "$KW" "$M" | linea
echo "-- striding np=4";       mpirun -np 4 bin/striding_mpi "$C" "$KW" "$M" | linea
echo "-- shuffled np=4";       mpirun -np 4 bin/shuffled_mpi "$C" "$KW" "$M" | linea
echo "-- hibrido np=4 x 2 hilos"; mpirun --bind-to none -np 4 bin/hybrid_mpi_omp "$C" "$KW" "$M" 2 | linea
rm -f "$C"
