# Proyecto 2 — Bruteforce DES con Open MPI

Computación Paralela y Distribuida — Universidad del Valle de Guatemala

Búsqueda por fuerza bruta de la llave privada usada para cifrar un texto con
DES, en versión secuencial y en cinco acercamientos paralelos con Open MPI
(naive, master-worker dinámico, intercalado/striding, partición con shuffle
pseudo-aleatorio e híbrido MPI+OpenMP).

## Estructura del proyecto

```
Proyecto 2/
├── Makefile
├── README.md
├── data/
│   └── texto.txt              # texto de prueba del enunciado (Parte B)
├── docs/
│   └── informe_outline.md     # esqueleto del informe mapeado a la rúbrica
├── scripts/
│   └── benchmark.sh           # corre las pruebas de tiempos y las guarda en CSV
└── src/
    ├── common/
    │   ├── common.h / common.c   # carga de archivos, padding, manejo de errores
    │   └── des.h / des.c         # wrapper de DES sobre OpenSSL (libcrypto)
    ├── tools/
    │   ├── cifrar.c              # utilidad: cifra un archivo con una llave dada
    │   └── descifrar.c           # utilidad: descifra un archivo con una llave dada
    └── bruteforce/
        ├── secuencial.c          # búsqueda secuencial (Parte A.3)
        ├── naive_mpi.c           # acercamiento naive (bloques contiguos, base bruteforce.c)
        ├── master_worker_mpi.c   # alternativa 1: master-worker con bloques dinámicos
        ├── striding_mpi.c        # alternativa 2: recorrido intercalado (striding)
        ├── shuffled_mpi.c        # alternativa 3: bloques contiguos + shuffle pseudo-aleatorio
        └── hybrid_mpi_omp.c      # alternativa 4: híbrido MPI (procesos) + OpenMP (hilos)
```

## Requisitos (Linux / WSL)

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev libssl-dev
```

Se usa OpenSSL (`libcrypto`) en lugar de `rpc/des_crypt.h`, que ya no está
disponible en sistemas modernos (ver `src/common/des.c`).

## Compilación

```bash
make
```

Genera los binarios en `bin/`: `cifrar`, `descifrar`, `secuencial`,
`naive_mpi`, `master_worker_mpi`, `striding_mpi`, `shuffled_mpi`,
`hybrid_mpi_omp`.

```bash
make clean   # elimina bin/
```

## Uso

### 1. Cifrar y descifrar un texto (Parte B.1)

```bash
bin/cifrar data/texto.txt data/texto_cifrado.bin 42
bin/descifrar data/texto_cifrado.bin 42
```

### 2. Búsqueda secuencial (Parte A.3)

```bash
bin/secuencial data/texto_cifrado.bin "es una prueba de" <max_key>
```

### 3. Acercamiento naive (Parte B.2)

> `max_key` debe ser `2^56 - 1` (72057594037927935) en estas pruebas: así el rango se
> divide en 4 segmentos de 2^54 llaves y la posición de la llave dentro del segmento
> es la que pide el enunciado. Pasar la propia llave como `max_key` la deja siempre al
> final del último segmento.

Cifrar primero con la llave objetivo, luego romperla con 4 procesos:

```bash
bin/cifrar data/texto.txt data/texto_cifrado.bin 123456
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 72057594037927935   # max_key = 2^56-1

# 2^56/4 = 18014398509481983  -> puede tardar mucho / no terminar
bin/cifrar data/texto.txt data/texto_cifrado.bin 18014398509481983
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 72057594037927935   # max_key = 2^56-1

# 2^56/4 + 1 = 18014398509481984
bin/cifrar data/texto.txt data/texto_cifrado.bin 18014398509481984
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 72057594037927935   # max_key = 2^56-1
```

### 4. Acercamientos alternativos (Parte B.3–B.5)

**Master-worker dinámico** (requiere ≥ 2 procesos; rank 0 es el maestro y no
busca):

```bash
mpirun -np 4 bin/master_worker_mpi data/texto_cifrado.bin "es una prueba de" <max_key> [chunk_size]
```

**Intercalado / striding**:

```bash
mpirun -np 4 bin/striding_mpi data/texto_cifrado.bin "es una prueba de" <max_key>
```

**Partición contigua con shuffle pseudo-aleatorio**:

```bash
mpirun -np 4 bin/shuffled_mpi data/texto_cifrado.bin "es una prueba de" <max_key>
```

**Híbrido MPI + OpenMP** (nthreads por defecto: `$OMP_NUM_THREADS` /
núcleos disponibles):

```bash
mpirun -np 4 bin/hybrid_mpi_omp data/texto_cifrado.bin "es una prueba de" <max_key> [nthreads]
```

### 5. Batería de pruebas para el informe

```bash
chmod +x scripts/benchmark.sh
./scripts/benchmark.sh
```

Uso: `./scripts/benchmark.sh [np] [timeout_s] [hilos_openmp]` (por defecto `4 0 2`).
Genera `resultados_benchmark.csv` con los tiempos de cada acercamiento para
las llaves fácil/media/difícil y el caso base (evidencia del Anexo 2). El
espacio de búsqueda es fijo (`[0, 2^27 - 1]`) y sólo cambia la posición de la
llave. `tiempo_segundos` es el tiempo interno de cada programa (sin el
arranque de `mpirun`); `tiempo_wall_segundos` es el tiempo de pared del
comando completo. `speedup.py` divide la eficiencia entre `np x hilos`.
`resultados_np_scaling.csv` tiene la variación con np = 2, 4, 8, 16 (caso difícil).

## Notas de diseño

- **Espacio de llaves**: se trabaja con el rango `[0, 2^56 - 1]`, igual que
  el `bruteforce.c` de referencia (ver comentarios en `src/common/des.c` sobre la
  conversión de `long` a bloque DES).
- **Terminación anticipada (naive / interleaved)**: cada proceso posta un
  `MPI_Irecv` no bloqueante antes de buscar, revisa con `MPI_Test` cada
  100,000 iteraciones, y usa `MPI_Send`/`MPI_Wait` para avisar y esperar el
  aviso de que otro proceso ya encontró la llave.
- **Master-worker**: reparte bloques de tamaño configurable (`chunk_size`,
  por defecto 50,000) bajo demanda, para evitar que un proceso quede con un
  segmento grande mientras otros ya terminaron.
- **Shuffle (permutación afín)**: cada proceso conserva un bloque contiguo de
  *índices lógicos* (igual que naive), pero cada índice se transforma en la
  llave real a través de `key(idx) = (a*idx + b) mod N`, con `a` coprimo con
  `N = max_key + 1`. Esto garantiza que cada llave se visita exactamente una
  vez en total, pero la dispersa por todo `[0, N-1]` en lugar de dejarla en
  un segmento compacto, atacando la misma varianza del naive por otro medio.
- **Híbrido MPI + OpenMP**: añade un segundo nivel de paralelismo *dentro*
  del bloque de cada proceso, repartiéndolo entre hilos con el mismo truco
  de recorrido intercalado que usa `striding_mpi.c`, pero a nivel de hilos
  en memoria compartida. Sólo el hilo 0 de cada proceso llama a funciones de
  MPI (`MPI_THREAD_FUNNELED`).
- **Manejo de memoria**: todo `malloc`/`calloc` tiene su `free`
  correspondiente; se valida cada asignación y apertura de archivo
  (programación defensiva).
- **Llaves y paridad de DES**: DES ignora el bit menos significativo de cada
  byte de la llave (paridad). `des.c` empaqueta sólo 7 bits útiles por byte,
  así que cada `long` en `[0, 2^56 - 1]` produce un key schedule distinto (no
  hay colisiones entre llaves; ver el comentario de `long_to_des_key`).
- **Limitación conocida (falsos positivos)**: la validación es por
  *substring*. Con un texto y palabra clave cortos, una llave incorrecta
  podría, por azar, descifrar a algo que contenga la palabra clave. No se
  observó en las pruebas. Sólo en ese caso dos procesos podrían "encontrar" una
  llave casi a la vez, y el segundo aviso de `MPI_Send` quedaría sin receptor
  (cada proceso mantiene un único `MPI_Irecv`). `master_worker_mpi.c` no tiene
  este problema porque el maestro serializa las decisiones.

### 5. Calcular speedup y eficiencia

```bash
python3 scripts/speedup.py resultados_benchmark.csv
```

Imprime una tabla de speedup/eficiencia por acercamiento y caso, y genera
gráficas PNG en `results/` si `matplotlib` y `pandas` están instalados.

## Pendiente para completar el proyecto

- [x] Investigación y diagrama de flujo de DES (ver `docs/informe_outline.md`, secciones A.1–A.2).
- [/] Llenar `docs/informe_outline.md` con el contenido completo (esqueleto terminado; faltan capturas de pantalla y datos de benchmark).
- [x] Correr `scripts/benchmark.sh` y capturar evidencia (Anexo 2). Variación con np: `resultados_np_scaling.csv`.
- [ ] (Extra) Cluster MPI en 2+ máquinas.
- [x] (Extra) Versión híbrida con OpenMP (`hybrid_mpi_omp.c`).
- [x] (Extra) Acercamientos adicionales: shuffle pseudo-aleatorio
      (`shuffled_mpi.c`) + híbrido MPI/OpenMP (2 de 3+ para el máximo extra).

### Comandos útiles

```bash
make help        # muestra todos los targets disponibles
make test-seq    # prueba de humo: cifra con llave=42 y la busca secuencialmente
```
