# Proyecto 2 — Bruteforce DES con Open MPI

Computación Paralela y Distribuida — Universidad del Valle de Guatemala

Búsqueda por fuerza bruta de la llave privada usada para cifrar un texto con
DES, en versión secuencial y en tres acercamientos paralelos con Open MPI
(naive, master-worker dinámico e intercalado/striding).

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
    ├── common.h / common.c    # carga de archivos, padding, manejo de errores
    ├── des.h / des.c          # wrapper de DES sobre OpenSSL (libcrypto)
    ├── cifrar.c               # utilidad: cifra un archivo con una llave dada
    ├── descifrar.c            # utilidad: descifra un archivo con una llave dada
    ├── secuencial.c           # búsqueda secuencial (Parte A.3)
    ├── naive_mpi.c            # acercamiento naive (bloques contiguos, base bruteforce.c)
    ├── approach1_mpi.c        # alternativa 1: master-worker con bloques dinámicos
    └── approach2_mpi.c        # alternativa 2: recorrido intercalado (striding)
```

## Requisitos (Linux / WSL)

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev libssl-dev
```

Se usa OpenSSL (`libcrypto`) en lugar de `rpc/des_crypt.h`, que ya no está
disponible en sistemas modernos (ver `src/des.c`).

## Compilación

```bash
make
```

Genera los binarios en `bin/`: `cifrar`, `descifrar`, `secuencial`,
`naive_mpi`, `approach1_mpi`, `approach2_mpi`.

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

Cifrar primero con la llave objetivo, luego romperla con 4 procesos:

```bash
bin/cifrar data/texto.txt data/texto_cifrado.bin 123456
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 123456

# 2^56/4 = 18014398509481983  -> puede tardar mucho / no terminar
bin/cifrar data/texto.txt data/texto_cifrado.bin 18014398509481983
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 18014398509481983

# 2^56/4 + 1 = 18014398509481984
bin/cifrar data/texto.txt data/texto_cifrado.bin 18014398509481984
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 18014398509481984
```

### 4. Acercamientos alternativos (Parte B.3–B.5)

**Master-worker dinámico** (requiere ≥ 2 procesos; rank 0 es el maestro y no
busca):

```bash
mpirun -np 4 bin/approach1_mpi data/texto_cifrado.bin "es una prueba de" <max_key> [chunk_size]
```

**Intercalado / striding**:

```bash
mpirun -np 4 bin/approach2_mpi data/texto_cifrado.bin "es una prueba de" <max_key>
```

### 5. Batería de pruebas para el informe

```bash
chmod +x scripts/benchmark.sh
./scripts/benchmark.sh
```

Genera `resultados_benchmark.csv` con los tiempos de cada acercamiento para
las llaves fácil/media/difícil y los casos base del enunciado (usar como
evidencia del Anexo 2).

## Notas de diseño

- **Espacio de llaves**: se trabaja con el rango `[0, 2^56 - 1]`, igual que
  el `bruteforce.c` de referencia (ver comentarios en `src/des.c` sobre la
  conversión de `long` a bloque DES).
- **Terminación anticipada (naive / interleaved)**: cada proceso posta un
  `MPI_Irecv` no bloqueante antes de buscar, revisa con `MPI_Test` cada
  100,000 iteraciones, y usa `MPI_Send`/`MPI_Wait` para avisar y esperar el
  aviso de que otro proceso ya encontró la llave.
- **Master-worker**: reparte bloques de tamaño configurable (`chunk_size`,
  por defecto 50,000) bajo demanda, para evitar que un proceso quede con un
  segmento grande mientras otros ya terminaron.
- **Manejo de memoria**: todo `malloc`/`calloc` tiene su `free`
  correspondiente; se valida cada asignación y apertura de archivo
  (programación defensiva).

## Pendiente para completar el proyecto

- [ ] Investigación y diagrama de flujo de DES (Parte A.1–A.2, en el informe).
- [ ] Llenar `docs/informe_outline.md` con el contenido completo.
- [ ] Correr `scripts/benchmark.sh` y capturar evidencia (Anexo 2).
- [ ] (Extra) Cluster MPI en 2+ máquinas.
- [ ] (Extra) Versión híbrida con OpenMP.
- [ ] (Extra) 3+ acercamientos adicionales.
