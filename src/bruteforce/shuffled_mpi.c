/*
 * shuffled_mpi.c
 * ---------------------------------------------------------------------
 * Acercamiento alternativo #3: partición contigua con mapeo
 * pseudo-aleatorio (shuffle) a la llave real.
 *
 * El espacio de índices lógicos [0, max_key] se reparte en 'np' bloques
 * contiguos de igual tamaño, exactamente igual que en naive_mpi.c. La
 * diferencia es que cada índice lógico 'idx' no se usa directamente
 * como llave: se transforma primero con una permutación afín de
 * congruencia lineal
 *
 *     key(idx) = (a * idx + b) mod N        con N = max_key + 1
 *
 * donde 'a' se elige coprimo con N (gcd(a, N) = 1), lo que garantiza
 * que key(idx) es una biyección sobre [0, N-1]: cada llave real del
 * espacio se visita exactamente una vez en total entre todos los
 * procesos, pero el ORDEN en que se visitan dentro del bloque contiguo
 * de cada proceso queda disperso de forma pseudo-aleatoria sobre todo
 * el espacio [0, N-1], en lugar de ser un recorrido secuencial dentro
 * de un segmento compacto.
 *
 * Motivación (igual que approach2/striding, mecanismo distinto):
 * en el acercamiento naive, la posición de la llave solución dentro
 * del segmento de un proceso determina casi por completo el tiempo de
 * cómputo observado (varianza alta). Aquí, como cada proceso recorre
 * una muestra "barajada" que cubre todo el rango [0, N-1] de manera
 * aproximadamente uniforme desde el principio de su bloque, la
 * correlación entre "qué proceso" y "en qué momento del recorrido"
 * aparece la respuesta queda mucho más débil, sin necesidad de un
 * maestro central (approach1) ni de un paso de stride explícito
 * (approach2).
 *
 * 'a' y 'b' se calculan de forma determinista a partir de N, así que
 * todos los procesos obtienen los mismos valores de forma local, sin
 * necesidad de comunicarlos.
 *
 * Coordinación entre procesos: idéntica a naive_mpi.c (MPI_Irecv /
 * MPI_Test para sondeo no bloqueante, MPI_Send / MPI_Wait para el
 * aviso de llave encontrada).
 *
 * Uso: mpirun -np <N> ./shuffled_mpi <archivo_cifrado> <palabra_clave> <max_key>
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "common.h"
#include "des.h"

#define TAG_FOUND 300
#define CHECK_INTERVAL 100000 /* cada cuántas iteraciones se revisa el aviso */

/* Constantes de dispersión (Knuth's multiplicative hash y una variante
 * de la razón áurea), usadas sólo como semillas deterministas para 'a'
 * y 'b'; no requieren ser criptográficamente fuertes. */
#define SEED_A 2654435761UL
#define SEED_B 2246822519UL

static long gcd(long a, long b)
{
    while (b != 0) {
        long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* Multiplicación modular segura ante overflow (a, b, n pueden acercarse
 * a 2^56, cuyo producto no entra en 64 bits): se usa __int128 como
 * acumulador intermedio. */
static long mulmod(long a, long b, long n)
{
    __int128 res = (__int128)a * (__int128)b % (__int128)n;
    return (long)res;
}

/* Encuentra el menor 'a' >= semilla (mod n) tal que gcd(a, n) == 1, que
 * es la condición necesaria para que key(idx) = (a*idx + b) mod n sea
 * una biyección sobre [0, n-1]. */
static long find_coprime_multiplier(long n)
{
    if (n <= 1) {
        return 1;
    }
    long a = (long)(SEED_A % (unsigned long)n);
    if (a == 0) {
        a = 1;
    }
    while (gcd(a, n) != 1) {
        a++;
        if (a >= n) {
            a = 1;
        }
    }
    return a;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (argc != 4) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s <archivo_cifrado> <palabra_clave> <max_key>\n", argv[0]);
        }
        MPI_Finalize();
        return ERR_EXIT;
    }

    const char *ciph_path = argv[1];
    const char *keyword = argv[2];
    long max_key = atol(argv[3]);

    if (max_key < 0) {
        if (rank == 0) fprintf(stderr, "[ERROR] max_key debe ser un valor no negativo.\n");
        MPI_Finalize();
        return ERR_EXIT;
    }

    size_t len;
    unsigned char *ciph = load_and_pad_file(ciph_path, &len);

    /* Espacio de índices lógicos [0, N-1], N = max_key + 1, repartido en
     * bloques contiguos de igual tamaño (misma fórmula que naive_mpi.c). */
    long n = max_key + 1;
    long range_size = n / num_procs;
    long idx_start = rank * range_size;
    long idx_end = (rank == num_procs - 1) ? (n - 1) : (idx_start + range_size - 1);

    long a = find_coprime_multiplier(n);
    long b = (n > 1) ? (long)(SEED_B % (unsigned long)n) : 0;

    long incoming_key = -1;
    MPI_Request recv_req;
    MPI_Irecv(&incoming_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND,
              MPI_COMM_WORLD, &recv_req);

    double t0 = MPI_Wtime();

    long found_key = -1;
    int found_locally = 0;
    int someone_found = 0;

    for (long idx = idx_start; idx <= idx_end; idx++) {
        long key = (n > 1) ? ((mulmod(a, idx, n) + b) % n) : idx;

        if (try_key(key, ciph, len, keyword)) {
            found_key = key;
            found_locally = 1;
            break;
        }

        if ((idx - idx_start) % CHECK_INTERVAL == 0) {
            int flag = 0;
            MPI_Test(&recv_req, &flag, MPI_STATUS_IGNORE);
            if (flag) {
                someone_found = 1;
                found_key = incoming_key;
                break;
            }
        }
    }

    if (found_locally) {
        /* Avisa a todos los demás procesos que ya se encontró la llave. */
        for (int p = 0; p < num_procs; p++) {
            if (p != rank) {
                MPI_Send(&found_key, 1, MPI_LONG, p, TAG_FOUND, MPI_COMM_WORLD);
            }
        }
        MPI_Cancel(&recv_req);
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
    } else if (someone_found) {
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
    } else {
        /* Se agotó el bloque propio sin encontrar la llave: se espera
         * (bloqueante) el aviso de otro proceso. Se asume que la llave
         * sí existe dentro de [0, max_key]. */
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
        if (incoming_key >= 0) {
            found_key = incoming_key;
        }
    }

    double t1 = MPI_Wtime();
    double elapsed = t1 - t0;

    double max_elapsed;
    MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    long candidate = (found_key >= 0) ? found_key : -1;
    long global_found;
    MPI_Reduce(&candidate, &global_found, 1, MPI_LONG, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        if (global_found >= 0) {
            printf("Llave encontrada: %ld\n", global_found);
        } else {
            printf("No se encontró la llave en el rango [0, %ld].\n", max_key);
        }
        printf("Tiempo paralelo (shuffled, np=%d): %.6f segundos\n", num_procs, max_elapsed);
    }

    free(ciph);
    MPI_Finalize();
    return 0;
}
