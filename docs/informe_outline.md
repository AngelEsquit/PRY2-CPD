# Esqueleto del Informe — Proyecto 2

> Usar este archivo como guía de secciones; el documento final debe entregarse
> siguiendo el formato oficial de informes de UVG (carátula, índice,
> introducción, cuerpo, citas/pie de página, conclusiones/recomendaciones,
> apéndices, y al menos 3 citas bibliográficas). Mapeado directamente a la
> rúbrica del enunciado.

## Carátula
- Universidad, curso, docente, integrantes del equipo, fecha.

## Índice

## Introducción

## Parte A — Antecedentes (rúbrica ítem 1, 3 pts)
1. **Investigación sobre DES**: pasos para cifrar/descifrar un texto
   (Feistel, permutaciones inicial/final, generación de subllaves, 16
   rondas, etc.). Citar fuentes.
2. **Diagrama de flujo del algoritmo DES** (puede ir en 1 página adicional).
3. Resultados de la versión secuencial con distintas longitudes de espacio
   de búsqueda (tabla de tiempos).
4. Evidencia de que `bruteforce.c` (o el wrapper `des.c`/`des.h` de este
   proyecto) funciona correctamente.
5. Explicación de las rutinas:
   - `des_encrypt` / `des_decrypt` (equivalentes a `encrypt`/`decrypt`)
   - `try_key` (equivalente a `tryKey`)
   - `memcpy`
   - `strstr`
6. Explicación de las primitivas MPI usadas: `MPI_Irecv`, `MPI_Send`,
   `MPI_Wait` (ver comentarios en `src/naive_mpi.c`).

## Parte B — Análisis y acercamientos (rúbrica ítems 3 y 4, 5 + 10 pts)
1. Captura de pantalla: cifrado/descifrado de un texto sencillo con una
   llave sencilla (ej. 42), usando `bin/cifrar` y `bin/descifrar`.
2. Pruebas con `-np 4`, texto: *"Esta es una prueba de proyecto 2"*,
   palabra clave: *"es una prueba de"*:
   - a. Llave `123456L`
   - b. Llave `18014398509481983L` (2^56/4) — puede no terminar
   - c. Llave `18014398509481984L` (2^56/4 + 1)
   - d. Reflexión sobre el comportamiento del tiempo en función de la llave
3. Descripción de los 2 acercamientos alternativos implementados:
   - **Acercamiento 1** — Master-Worker dinámico (`src/approach1_mpi.c`)
   - **Acercamiento 2** — Recorrido intercalado / striding (`src/approach2_mpi.c`)
4. Diagramas de flujo / pseudocódigo de cada acercamiento.
5. Pruebas con llaves fácil, media y difícil (fórmulas del enunciado,
   adaptadas al número de procesos usado), comparando naive vs.
   alternativas vs. secuencial. Usar `scripts/benchmark.sh` como base.

## Discusión / Conclusiones / Recomendaciones (rúbrica ítem 3, 5 pts)
- Retos encontrados, aprendizajes, y soluciones adoptadas.

## Anexo 1 — Catálogo de funciones y librerías (rúbrica ítem 5, 5 pts)
Para cada función (`des_encrypt`, `des_decrypt`, `try_key`,
`load_and_pad_file`, `die`, y las rutinas de cada acercamiento MPI):
- Entradas: nombre, tipo, descripción
- Salidas: nombre, tipo, descripción
- Descripción del funcionamiento

## Anexo 2 — Bitácora de pruebas y speedups (rúbrica ítem 6, 4 pts)
- Tablas y capturas de pantalla de cada corrida (usar `resultados_benchmark.csv`
  generado por `scripts/benchmark.sh` como base).

## Extras (créditos adicionales opcionales)
- Cluster MPI en 2+ máquinas (hasta 20%)
- OpenMP + MPI híbrido (hasta 10%)
- 3+ approaches adicionales (hasta 5%)

## Bibliografía
(mínimo 3 citas relevantes y confiables)
