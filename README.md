# Proyecto 2 — Bruteforce DES con Open MPI

Computación Paralela y Distribuida — Universidad del Valle de Guatemala

Repositorio: <https://github.com/angelesquit/pry2-cpd>

Búsqueda por fuerza bruta de la llave privada usada para cifrar un texto con
DES, en versión secuencial y en cinco acercamientos paralelos con Open MPI
(naive, master-worker dinámico, intercalado/striding, partición con shuffle
pseudo-aleatorio e híbrido MPI+OpenMP). El informe está en `docs/informe.pdf`.

## Demo rápida (~1 minuto, para la presentación)

```bash
make                    # compila todo en bin/
./scripts/demo.sh       # cifra/descifra con llave 42, naive con 2^56/4+1 y los 6 programas
                        # buscando la misma llave en [0, 2^24-1] con np = 4
```

Qué se ve: (1) el texto se cifra y se recupera con la llave 42, (2) naive encuentra la
llave `2^56/4 + 1` en ~0.03 s porque cae al inicio del bloque del proceso 1 (speedup
engañoso), (3) los seis programas encuentran la misma llave con tiempos distintos según
cómo reparten el espacio. Para mostrar la variabilidad por posición de la llave:
`python3 scripts/random_summary.py` (tabla de las 30 llaves aleatorias ya medidas) y abrir
`results/variabilidad_random.png`. El informe completo está en `docs/informe.pdf`.

## Estructura del proyecto

```
├── Makefile
├── README.md
├── data/texto.txt               # texto de prueba del enunciado
├── docs/
│   ├── informe.tex / informe.pdf  # informe final
│   ├── capturas/                  # capturas de pantalla del informe
│   └── informe_outline.md         # borrador inicial del informe
├── scripts/
│   ├── benchmark.sh             # fácil/media/difícil, espacio fijo 2^27 -> resultados_benchmark.csv
│   ├── speedup.py               # speedup y eficiencia + gráficas en results/
│   ├── np_scaling.sh            # variación con np (2,4,8,16) -> resultados_np_scaling.csv
│   ├── random_keys.sh           # 30 llaves aleatorias -> resultados_random.csv
│   ├── random_summary.py        # tabla y gráfica de variabilidad
│   ├── hybrid_threads.sh        # escalado del híbrido con los hilos -> resultados_hybrid_threads.csv
│   ├── hybrid_grid.sh           # 8x1, 4x2, 2x4, 1x8 procesos x hilos -> resultados_hybrid_grid.csv
│   ├── extra_plots.py           # gráficas de np, hilos del híbrido y reparto procesos x hilos
│   └── demo.sh                  # demo corta para la presentación
├── results/                     # gráficas generadas
└── src/
    ├── common/                  # common.c/h (archivos, padding, errores), des.c/h (DES con OpenSSL)
    ├── tools/                   # cifrar.c, descifrar.c
    └── bruteforce/              # secuencial.c, naive_mpi.c, master_worker_mpi.c,
                                 # striding_mpi.c, shuffled_mpi.c, hybrid_mpi_omp.c
```

## Requisitos (Linux / WSL)

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev libssl-dev
sudo apt install -y python3 python3-matplotlib python3-pandas          # gráficas
sudo apt install -y texlive-latex-extra texlive-lang-spanish latexmk   # sólo para compilar el informe
```

En Fedora el `mpirun` puede no estar en el PATH: `export PATH=/usr/lib64/openmpi/bin:$PATH`.
Se usa OpenSSL (`libcrypto`) en lugar de `rpc/des_crypt.h`, que ya no existe
en sistemas modernos.

## Compilación

```bash
make          # genera bin/: cifrar, descifrar, secuencial, naive_mpi, master_worker_mpi,
              #              striding_mpi, shuffled_mpi, hybrid_mpi_omp
make clean    # elimina bin/
make help     # lista los targets
make test-seq # prueba de humo (llave 42, búsqueda secuencial)
```

## Uso

Todos los programas de búsqueda reciben `<archivo_cifrado> <palabra_clave> <max_key>`
e imprimen el archivo, la palabra clave, la llave encontrada y el tiempo de búsqueda.

### 1. Cifrar y descifrar (Parte B.1)

```bash
bin/cifrar data/texto.txt data/texto_cifrado.bin 42
bin/descifrar data/texto_cifrado.bin 42      # imprime "Esta es una prueba de proyecto 2"
```

### 2. Búsqueda secuencial (Parte A.3)

```bash
bin/secuencial data/texto_cifrado.bin "es una prueba de" <max_key>
```

### 3. Naive con 4 procesos (Parte B.2)

Usar `max_key = 2^56 - 1 = 72057594037927935`. Así el rango se divide en 4 bloques
de 2^54 llaves y la posición de la llave dentro del bloque es la que pide el
enunciado (pasar la propia llave como `max_key` la deja siempre al final).

```bash
M=72057594037927935
bin/cifrar data/texto.txt data/texto_cifrado.bin 123456
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" $M   # ~0.09 s

bin/cifrar data/texto.txt data/texto_cifrado.bin 18014398509481984        # 2^56/4 + 1
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" $M   # ~0.03 s

bin/cifrar data/texto.txt data/texto_cifrado.bin 18014398509481983        # 2^56/4
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" $M   # no termina
```

### 4. Acercamientos alternativos (Parte B.3 a B.5)

```bash
mpirun -np 4 bin/master_worker_mpi data/texto_cifrado.bin "es una prueba de" <max_key> [chunk_size]  # np >= 2, rank 0 es maestro
mpirun -np 4 bin/striding_mpi      data/texto_cifrado.bin "es una prueba de" <max_key>
mpirun -np 4 bin/shuffled_mpi      data/texto_cifrado.bin "es una prueba de" <max_key>
mpirun --bind-to none -np 4 bin/hybrid_mpi_omp data/texto_cifrado.bin "es una prueba de" <max_key> [hilos]
```

> **Importante para el híbrido**: Open MPI 5 ata cada proceso a un solo núcleo por defecto,
> y los hilos de OpenMP terminan compartiéndolo (speedup de sólo ~1.55x sin importar cuántos
> hilos). Hay que lanzar con `--bind-to none` (o `--map-by slot:PE=<hilos>`).

### Llaves de ejemplo (espacio `[0, 2^24-1]`, `np = 4`)

Cada programa corre solo; basta cifrar con la llave y lanzar el que se quiera
(`export PATH=/usr/lib64/openmpi/bin:$PATH` en Fedora). Ejemplo con la llave "suerte" del shuffle:

```bash
bin/cifrar data/texto.txt data/c.bin 7064183
bin/secuencial data/c.bin "es una prueba de" 16777215
mpirun -np 4 bin/shuffled_mpi data/c.bin "es una prueba de" 16777215   # ~0.06 s
```

O con la demo: `./scripts/demo.sh suerte-shuffled` (también acepta una llave numérica).
`python3 scripts/lucky_keys.py` imprime cuántas llaves prueba el proceso más lento de cada
acercamiento (verificado contra corridas reales).

| Preset / llave | naive | striding | master-worker | shuffled | Qué muestra |
|---|---|---|---|---|---|
| `suerte-shuffled` 7064183 | 0.98 s | 0.60 s | 0.75 s | **0.06 s** | el shuffle cae justo al inicio de un bloque |
| `suerte-naive` 8388608 (= 2·N/4) | **0.09 s** | 0.75 s | 0.87 s | 0.66 s | naive instantáneo, inicio del bloque 2 |
| `mala-naive` 16777215 (= N-1) | 1.31 s | 1.32 s | 1.75 s | 0.41 s | final del último bloque: naive y striding hacen todo el trabajo |
| `mala-shuffled` 3428550 | 1.12 s | **0.32 s** | **0.36 s** | 1.34 s | peor caso del shuffle; striding y master-worker no lo notan |
| `chica` 42 | ~0 | ~0 | ~0 | ~1 s | llave pequeña: gana todo lo que recorre en orden, shuffled no |
| 12000000 (demo) | 1.16 s | 1.01 s | 1.26 s | 0.06 s | secuencial 3.78 s |

El tiempo de un acercamiento depende de *dónde* cae la llave en su orden de recorrido, no
solo de `np`. Striding (~`llave/np`) y master-worker (~`llave/(np-1)`) son predecibles;
naive y shuffled dependen de la posición dentro del bloque. Los casos "suerte" tienen un
piso de ~0.06-0.09 s por el arranque de MPI.

### 5. Reproducir las pruebas del informe

```bash
./scripts/benchmark.sh 4 180 2      # [np] [timeout_s] [hilos OpenMP]; ~4 min -> resultados_benchmark.csv
python3 scripts/speedup.py          # tabla de speedup/eficiencia y gráficas en results/
./scripts/np_scaling.sh             # np = 2,4,8,16 (caso difícil), ~2 min -> resultados_np_scaling.csv
./scripts/random_keys.sh 30 4 2 42  # [K] [np] [hilos] [semilla]; ~7 min -> resultados_random.csv
python3 scripts/random_summary.py   # media, peor caso, dispersión y results/variabilidad_random.png
./scripts/hybrid_threads.sh         # híbrido con 1..20 hilos (np=1), ~3 min
./scripts/hybrid_grid.sh            # 8x1, 4x2, 2x4, 1x8 sobre 30 llaves, ~4 min
python3 scripts/extra_plots.py      # results/np_scaling.png, hybrid_hilos.png, hybrid_grid.png
```

- `benchmark.sh` usa un espacio fijo `[0, 2^27 - 1]` y sólo cambia la posición de la
  llave (fácil `N/2+1`, media `N/2+N/8`, difícil `N/7+N/13+1`, más la base 123456).
  `tiempo_segundos` es el tiempo interno del programa (sin el arranque de `mpirun`) y
  `tiempo_wall_segundos` el del comando completo. La eficiencia se divide entre `np x hilos`.
- `random_keys.sh` usa `N = 2^24` para que 30 llaves quepan en unos 7 minutos.

### 6. Compilar el informe

```bash
cd docs && latexmk -pdf informe.tex
```

## Notas de diseño

- **Espacio de llaves**: `[0, 2^56 - 1]`. DES ignora el bit menos significativo de
  cada byte de la llave (paridad), así que `des.c` empaqueta 7 bits útiles por byte
  y cada `long` produce un key schedule distinto.
- **Terminación anticipada (naive, striding, shuffled, híbrido)**: cada proceso posta
  un `MPI_Irecv`, revisa con `MPI_Test` cada 100 000 iteraciones y quien encuentra
  la llave avisa con `MPI_Send` al resto.
- **Master-worker**: el rank 0 reparte bloques de `chunk_size` (50 000 por defecto)
  bajo demanda y no busca, por lo que rinde `np - 1`.
- **Striding**: el proceso `r` prueba `r, r+np, r+2np, ...`. Todos avanzan al mismo
  ritmo y el speedup es `~np` para cualquier llave.
- **Shuffled**: bloques contiguos de índices, pero cada índice se transforma con
  `key = (a*idx + b) mod N` y `gcd(a, N) = 1`. Es una biyección, así que cada llave
  se prueba exactamente una vez.
- **Híbrido**: cada proceso reparte su bloque entre hilos OpenMP con recorrido
  intercalado. Sólo el hilo 0 llama a MPI (`MPI_THREAD_FUNNELED`).
- **Memoria**: cada `malloc`/`calloc` tiene su `free` y se valida cada asignación y
  apertura de archivo.
- **Limitación conocida (falsos positivos)**: la validación es por *substring*. Con un
  texto y palabra clave cortos, una llave incorrecta podría contener la palabra clave
  por azar. No se observó en las pruebas. Sólo en ese caso dos procesos podrían
  "encontrar" una llave casi a la vez y el segundo `MPI_Send` quedaría sin receptor.
  `master_worker_mpi` no tiene este problema porque el maestro serializa las decisiones.

## Resultados principales (resumen)

Detalle y análisis en `docs/informe.pdf`. Con `np = 4` y 30 llaves aleatorias, naive y
striding tienen el mismo tiempo medio, como predice el modelo (`N/2p` iteraciones). Striding
es el único con speedup consistente (~`np`), naive y shuffled varían de ~0.3x a más de 30x,
master-worker rinde `np - 1` y el híbrido (2 hilos, `--bind-to none`) tiene el menor tiempo
medio. Con los hilos libres el híbrido escala casi linealmente (7.7x con 8 hilos) y, a igual
número de núcleos, 1 proceso x 8 hilos supera a 8 procesos.

## Estado

- [x] Secuencial, naive y 4 acercamientos alternativos funcionando.
- [x] Informe con diagramas, catálogo de funciones y bitácora de pruebas.
- [x] Extra: híbrido MPI + OpenMP y 3+ acercamientos adicionales.
- [ ] Extra: cluster MPI en 2+ máquinas (no realizado).
