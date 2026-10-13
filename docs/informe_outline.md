# Esqueleto del Informe — Proyecto 2

> Usar este archivo como guía de secciones; el documento final debe entregarse
> siguiendo el formato oficial de informes de UVG (carátula, índice,
> introducción, cuerpo, citas/pie de página, conclusiones/recomendaciones,
> apéndices, y al menos 3 citas bibliográficas). Mapeado directamente a la
> rúbrica del enunciado.

---

## Carátula
- Universidad del Valle de Guatemala
- Computación Paralela y Distribuida — Ciclo 2
- Docente: [nombre del docente]
- Integrantes: [nombres del equipo]
- Fecha: octubre 2026

---

## Índice

1. Introducción
2. Parte A — Antecedentes
3. Parte B — Análisis y acercamientos paralelos
4. Discusión / Conclusiones / Recomendaciones
5. Anexo 1 — Catálogo de funciones
6. Anexo 2 — Bitácora de pruebas y speedups
7. Bibliografía

---

## Introducción

El cifrado DES (Data Encryption Standard) fue el estándar de cifrado simétrico
dominante desde 1977 hasta finales de los años 90. Su principal vulnerabilidad
es el tamaño de la llave (56 bits efectivos), lo que lo hace susceptible a
ataques de fuerza bruta. En este proyecto se implementa dicho ataque de forma
secuencial y con cinco variantes paralelas utilizando Open MPI y OpenMP, con el
objetivo de medir y analizar el speedup obtenido en cada caso.

---

## Parte A — Antecedentes (rúbrica ítem 1, 3 pts)

### A.1 Investigación sobre DES

El algoritmo DES opera sobre bloques de 64 bits con una llave de 64 bits (56
efectivos + 8 de paridad). Sus etapas principales son:

1. **Permutación inicial (IP):** reordena los 64 bits del bloque de entrada
   según una tabla fija.
2. **16 rondas de Feistel:** en cada ronda se divide el bloque en mitad
   izquierda (L) y derecha (R). La función de ronda aplica una función F sobre
   R y la subllave de esa ronda, y el resultado se XOR con L. Al final, L y R
   se intercambian.
3. **Función F:** expande R de 32 a 48 bits, hace XOR con la subllave de 48
   bits, pasa por 8 S-boxes (sustitución no lineal, 6→4 bits cada una) y aplica
   una permutación P.
4. **Generación de subllaves (key schedule):** la llave original pasa por
   permutaciones PC-1 y PC-2 y rotaciones de bits para generar 16 subllaves de
   48 bits.
5. **Permutación final (IP⁻¹):** inversa de IP, produce el texto cifrado.

El descifrado usa el mismo algoritmo con las 16 subllaves en orden inverso.

### A.2 Diagrama de flujo del algoritmo DES

```
Texto plano (64 bits)
        │
        ▼
  Permutación IP
        │
        ▼
┌───────────────┐
│ L₀   │   R₀  │
└───────────────┘
        │  (×16 rondas)
        ▼
  Lᵢ = Rᵢ₋₁
  Rᵢ = Lᵢ₋₁ ⊕ F(Rᵢ₋₁, Kᵢ)
        │
        ▼
  Permutación IP⁻¹
        │
        ▼
Texto cifrado (64 bits)

Función F:
  Rᵢ₋₁ → Expansión E (32→48) → XOR Kᵢ → 8 S-boxes → Permutación P
```

### A.3 Resultados de la versión secuencial

| max_key        | Tiempo (s) | Observaciones             |
|----------------|-----------|---------------------------|
| 100            | ~0.0001   | Rango trivial             |
| 123,456        | ~0.001    | Llave pequeña             |
| 1,000,000      | ~0.010    | ~10⁶ pruebas              |
| (por completar con datos reales del benchmark) | | |

Ver `resultados_benchmark.csv` (generado por `scripts/benchmark.sh`) para los
tiempos completos de cada caso.

### A.4 Evidencia de correcto funcionamiento del wrapper DES

```bash
# Cifrar con llave 42 y descifrar: debe recuperarse el texto original
bin/cifrar data/texto.txt data/texto_cifrado.bin 42
bin/descifrar data/texto_cifrado.bin 42
# Salida esperada: "Esta es una prueba de proyecto 2"
```

Las funciones `des_encrypt` / `des_decrypt` de `src/common/des.c` son un
wrapper sobre la API de DES de OpenSSL (`DES_ecb_encrypt`), que reemplaza a
`rpc/des_crypt.h` (obsoleto en sistemas modernos).

### A.5 Explicación de rutinas clave

| Función           | Entradas                          | Salida / Efecto                                         |
|-------------------|-----------------------------------|---------------------------------------------------------|
| `des_encrypt`     | `key` (long), `buf`, `len`        | Cifra `buf` in-place (ECB, bloque 8 B)                  |
| `des_decrypt`     | `key` (long), `buf`, `len`        | Descifra `buf` in-place                                 |
| `try_key`         | `key`, `ciph`, `len`, `keyword`   | Retorna 1 si `keyword` aparece en el texto descifrado   |
| `load_and_pad_file` | `path`, `out_len`               | Carga archivo + padding a múltiplo de 8 B               |
| `die`             | `msg`                             | Imprime error en stderr y termina con código `ERR_EXIT` |
| `memcpy`          | `dst`, `src`, `n`                 | Copia `n` bytes; usada en `try_key` para no mutar `ciph`|
| `strstr`          | `haystack`, `needle`              | Busca `needle` en `haystack`; retorna puntero o NULL    |

### A.6 Primitivas MPI utilizadas

| Primitiva       | Uso en el proyecto                                                   |
|-----------------|----------------------------------------------------------------------|
| `MPI_Irecv`     | Postea recepción no bloqueante del aviso "llave encontrada"          |
| `MPI_Test`      | Sondea periódicamente si llegó el aviso, sin bloquear la búsqueda   |
| `MPI_Send`      | Envía la llave encontrada a todos los otros procesos                 |
| `MPI_Wait`      | Espera (bloqueante) a que se complete la recepción pendiente         |
| `MPI_Reduce`    | Recolecta el tiempo máximo y la llave global en el proceso 0        |
| `MPI_Init_thread` | Inicializa MPI con soporte `MPI_THREAD_FUNNELED` (híbrido OMP)   |

---

## Parte B — Análisis y acercamientos (rúbrica ítems 3 y 4, 5 + 10 pts)

### B.1 Cifrado/Descifrado de texto sencillo

```bash
bin/cifrar data/texto.txt data/texto_cifrado.bin 42
# → genera data/texto_cifrado.bin (32 bytes cifrados con DES, llave=42)

bin/descifrar data/texto_cifrado.bin 42
# → imprime: "Esta es una prueba de proyecto 2"
```

_(Insertar captura de pantalla del terminal aquí)_

### B.2 Pruebas con `-np 4`, texto de referencia del enunciado

Texto: `"Esta es una prueba de proyecto 2"`
Palabra clave: `"es una prueba de"`

#### a) Llave `123456`

```bash
bin/cifrar data/texto.txt data/texto_cifrado.bin 123456
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 123456
```
_(Tiempo esperado: muy rápido, la llave cae en el primer cuarto del espacio)_

#### b) Llave `18014398509481983` (2⁵⁶/4 — borde entre proceso 0 y proceso 1)

```bash
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 18014398509481983
```
_(Puede no terminar en tiempo razonable con naive; ver ADVERTENCIA en benchmark.sh)_

#### c) Llave `18014398509481984` (2⁵⁶/4 + 1)

```bash
mpirun -np 4 bin/naive_mpi data/texto_cifrado.bin "es una prueba de" 18014398509481984
```
_(La llave está al inicio del segmento del proceso 1: speedup alto artificialmente)_

#### d) Reflexión sobre el comportamiento

El tiempo observado en el acercamiento naive depende casi exclusivamente de en
qué segmento contiguo caiga la llave solución. Si cae al inicio del segmento de
algún proceso, ese proceso la encontrará rápido y avisará a los demás: el tiempo
total es corto. Si cae al final del segmento más largo, el tiempo se aproxima al
secuencial. Esto explica la alta varianza del speedup con naive y motiva los
acercamientos alternativos.

### B.3–B.5 Acercamientos alternativos

#### Acercamiento 1 — Master-Worker dinámico (`master_worker_mpi.c`)

El proceso 0 actúa como maestro: distribuye bloques pequeños (`chunk_size`,
por defecto 50,000 llaves) bajo demanda a los trabajadores (rank 1..N-1). Cuando
un trabajador termina, pide el siguiente bloque. Esto elimina la varianza del
naive porque ningún proceso queda bloqueado esperando terminar un segmento
desfavorablemente grande.

**Diagrama de flujo:**
```
Maestro (rank=0):             Trabajador (rank≥1):
  reparte bloque inicial  →     recibe [inicio, fin]
  loop:                         loop:
    recibe resultado        ←     busca en [inicio, fin]
    si llave hallada:             envía resultado
      send TAG_STOP a todos  →    si TAG_STOP: termina
    si hay más trabajo:           si TAG_WORK: repite
      send TAG_WORK          →
```

#### Acercamiento 2 — Intercalado / Striding (`striding_mpi.c`)

Cada proceso `rank` recorre las llaves `rank, rank+np, rank+2*np, ...` en lugar
de un bloque contiguo. Todos los procesos exploran simultáneamente la misma
"profundidad relativa" del espacio, eliminando la correlación entre posición de
la llave y ventaja del proceso.

#### Acercamiento 3 — Partición contigua + shuffle pseudo-aleatorio (`shuffled_mpi.c`)

Misma partición en bloques contiguos que el naive, pero cada índice lógico `idx`
se mapea a la llave real mediante `key(idx) = (a*idx + b) mod N`, donde `a` es
coprimo con `N = max_key + 1`. Esto garantiza una biyección sobre `[0, N-1]`
que "baraja" el orden de exploración de cada proceso de forma determinista y
pseudo-aleatoria.

#### Acercamiento 4 — Híbrido MPI + OpenMP (`hybrid_mpi_omp.c`)

Añade un segundo nivel de paralelismo: dentro del bloque de cada proceso MPI,
los hilos OpenMP recorren el sub-bloque con stride igual al número de hilos
(igual que striding, pero entre hilos en memoria compartida). Solo el hilo 0 de
cada proceso llama a funciones MPI (`MPI_THREAD_FUNNELED`).

### B.5 Comparativa de tiempos (fácil / media / difícil)

_(Completar con datos de `resultados_benchmark.csv` generados por `scripts/benchmark.sh`.
Usar `scripts/speedup.py` para calcular speedup y eficiencia automáticamente.)_

| Caso   | Secuencial | naive | master-w. | striding | shuffled | híbrido |
|--------|-----------|-------|-----------|---------|---------|---------|
| fácil  | —         | —     | —         | —       | —       | —       |
| media  | —         | —     | —         | —       | —       | —       |
| difícil| —         | —     | —         | —       | —       | —       |

---

## Discusión / Conclusiones / Recomendaciones (rúbrica ítem 3, 5 pts)

- **Varianza del naive:** el acercamiento naive expone claramente que la
  distribución estática de trabajo puede resultar muy desigual según la
  posición de la llave. Los enfoques striding y master-worker reducen esta
  varianza de forma efectiva.
- **Overhead de comunicación:** el acercamiento master-worker introduce tráfico
  MPI adicional (una ronda de mensajes por chunk), lo que puede reducir la
  eficiencia en sistemas con latencia de red alta.
- **Beneficio del híbrido:** la combinación MPI + OpenMP aprovecha la memoria
  compartida dentro de cada nodo, reduciendo la presión de comunicación MPI
  respecto a lanzar más procesos MPI puros.
- **Limitación de DES:** el espacio efectivo de 56 bits hace que la fuerza
  bruta completa sea computacionalmente inviable en hardware común. Las pruebas
  con llaves "pequeñas" (< 2²⁰) son suficientes para medir speedup.

---

## Anexo 1 — Catálogo de funciones y librerías (rúbrica ítem 5, 5 pts)

### `des_encrypt(long key, unsigned char *buf, size_t len)` — `src/common/des.c`
- **Entradas:** `key` (long, 0–2⁵⁶-1), `buf` (buffer a cifrar, múltiplo de 8 B), `len` (tamaño)
- **Salidas:** modifica `buf` in-place (cifrado DES ECB)
- **Descripción:** convierte `key` a bloque de 8 bytes, llama a `DES_set_key_unchecked` y `DES_ecb_encrypt` de OpenSSL por cada bloque de 8 bytes.

### `des_decrypt(long key, unsigned char *buf, size_t len)` — `src/common/des.c`
- Igual que `des_encrypt` pero con flag `DES_DECRYPT`.

### `try_key(long key, const unsigned char *ciph, size_t len, const char *keyword)` — `src/common/des.c`
- **Entradas:** `key`, `ciph` (texto cifrado, no modificado), `len`, `keyword`
- **Salidas:** `int` — 1 si `keyword` aparece en el descifrado, 0 si no
- **Descripción:** copia `ciph` en un buffer temporal, llama a `des_decrypt`, busca `keyword` con `strstr`.

### `load_and_pad_file(const char *path, size_t *out_len)` — `src/common/common.c`
- **Entradas:** `path` (ruta al archivo), `out_len` (puntero de salida)
- **Salidas:** puntero a buffer (caller debe liberar con `free`), longitud en `*out_len`
- **Descripción:** abre el archivo, calcula padding al múltiplo de 8 más cercano, alloca con `calloc`, rellena con espacios, lee el contenido.

### `die(const char *msg)` — `src/common/common.c`
- **Entradas:** `msg` (cadena de error)
- **Salidas:** no retorna (llama a `exit(ERR_EXIT)`)
- **Descripción:** imprime `[ERROR] msg` en stderr y termina el proceso.

---

## Anexo 2 — Bitácora de pruebas y speedups (rúbrica ítem 6, 4 pts)

_(Completar con capturas de pantalla y el CSV generado por `scripts/benchmark.sh`.
Calcular speedups con `scripts/speedup.py`.)_

Pasos para reproducir:
```bash
make
chmod +x scripts/benchmark.sh
./scripts/benchmark.sh 4 300    # np=4, timeout=5 min por caso
python3 scripts/speedup.py      # genera tabla + gráficas en results/
```

---

## Extras (créditos adicionales opcionales)

- Cluster MPI en 2+ máquinas (hasta 20%) — pendiente.
- OpenMP + MPI híbrido (hasta 10%) — **implementado** (`src/bruteforce/hybrid_mpi_omp.c`).
- 3+ approaches adicionales (hasta 5%) — **4 alternativas implementadas**
  (master-worker, striding, shuffled, híbrido), cumple con holgura.

---

## Bibliografía

1. Stallings, W. (2017). *Cryptography and Network Security: Principles and
   Practice* (7ª ed.). Pearson. — Capítulo 3: Block Ciphers and the Data
   Encryption Standard.

2. National Institute of Standards and Technology (NIST). (1999).
   *Data Encryption Standard (DES)* (FIPS PUB 46-3). U.S. Department of
   Commerce. Recuperado de https://csrc.nist.gov/publications/detail/fips/46/3/final

3. Gropp, W., Lusk, E., & Skjellum, A. (2014). *Using MPI: Portable Parallel
   Programming with the Message-Passing Interface* (3ª ed.). MIT Press.

4. OpenSSL Project. (2024). *DES routines — OpenSSL 3.x documentation*.
   Recuperado de https://www.openssl.org/docs/man3.0/man3/DES_ecb_encrypt.html

5. OpenMP Architecture Review Board. (2021). *OpenMP Application Programming
   Interface* (versión 5.2). Recuperado de https://www.openmp.org/specifications/
