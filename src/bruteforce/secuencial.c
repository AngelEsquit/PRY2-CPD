/*
 * secuencial.c
 * ---------------------------------------------------------------------
 * Versión secuencial (sin MPI) del ataque de fuerza bruta sobre DES
 * (Parte A.3). Recorre el espacio de llaves [0, max_key] probando cada
 * una con try_key() hasta encontrar la que descifra el texto cifrado
 * conteniendo la palabra clave dada. Sirve como línea base para medir
 * el speedup de las versiones paralelas.
 *
 * Uso: ./secuencial <archivo_cifrado> <palabra_clave> <max_key>
 *
 * Entradas:
 *   archivo_cifrado - ruta al archivo binario cifrado
 *   palabra_clave    - substring que indica que el descifrado fue exitoso
 *   max_key          - límite superior del espacio de búsqueda (inclusive)
 * Salidas:
 *   Llave encontrada (o mensaje de no encontrada) y tiempo transcurrido,
 *   impresos en stdout.
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "common.h"
#include "des.h"

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <archivo_cifrado> <palabra_clave> <max_key>\n", argv[0]);
        return ERR_EXIT;
    }

    const char *ciph_path = argv[1];
    const char *keyword = argv[2];
    long max_key = atol(argv[3]);

    if (max_key < 0) {
        die("max_key debe ser un valor no negativo.");
    }

    size_t len;
    unsigned char *ciph = load_and_pad_file(ciph_path, &len);

    printf("Búsqueda secuencial en '%s' (palabra clave: \"%s\", rango [0, %ld])...\n",
           ciph_path, keyword, max_key);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    long found_key = -1;
    for (long key = 0; key <= max_key; key++) {
        if (try_key(key, ciph, len, keyword)) {
            found_key = key;
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    if (found_key >= 0) {
        printf("Llave encontrada: %ld\n", found_key);
    } else {
        printf("No se encontró la llave en el rango [0, %ld].\n", max_key);
    }
    printf("Tiempo secuencial: %.6f segundos\n", elapsed);

    free(ciph);
    return 0;
}
