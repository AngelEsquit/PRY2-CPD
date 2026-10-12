/*
 * naive_mpi.c
 * ---------------------------------------------------------------------
 * Acercamiento "naive" paralelo con Open MPI (Parte B, basado en
 * bruteforce.c): el espacio de llaves [0, max_key] se divide en 'np'
 * segmentos contiguos de igual tamaño, uno por proceso, y cada proceso
 * recorre su segmento incrementalmente.
 *
 * Coordinación entre procesos:
 *   - Cada proceso posta, antes de iniciar su búsqueda, una recepción no
 *     bloqueante (MPI_Irecv) para el aviso de "llave encontrada" que
 *     pueda enviar cualquier otro proceso (MPI_ANY_SOURCE).
 *   - Cada CHECK_INTERVAL iteraciones se hace un MPI_Test para revisar,
 *     sin bloquear la búsqueda, si ya llegó ese aviso.
 *   - El proceso que encuentra la llave la notifica a todos los demás
 *     con MPI_Send (mensaje punto a punto por cada proceso).
 *   - Un proceso que agota su segmento sin encontrar nada, y sin haber
 *     recibido aviso, se bloquea con MPI_Wait a la espera del aviso de
 *     otro proceso (se asume que la llave sí existe en [0, max_key]).
 *
 * NOTA: este es el acercamiento base cuyo speedup es muy inconsistente
 * y depende fuertemente de en qué segmento caiga la llave solución
 * (ver discusión de la Parte B.2 del enunciado). Los acercamientos
 * alternativos están en master_worker_mpi.c y striding_mpi.c.
 *
 * Uso: mpirun -np <N> ./naive_mpi <archivo_cifrado> <palabra_clave> <max_key>
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "common.h"
#include "des.h"

#define TAG_FOUND 100
#define CHECK_INTERVAL 100000 /* cada cuántas iteraciones se revisa el aviso */

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

    /* División equitativa del espacio de búsqueda entre los procesos. */
    long range_size = (max_key + 1) / num_procs;
    long start = rank * range_size;
    long end = (rank == num_procs - 1) ? max_key : (start + range_size - 1);

    long incoming_key = -1;
    MPI_Request recv_req;
    MPI_Irecv(&incoming_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND,
              MPI_COMM_WORLD, &recv_req);

    double t0 = MPI_Wtime();

    long found_key = -1;
    int found_locally = 0;
    int someone_found = 0;

    for (long key = start; key <= end; key++) {
        if (try_key(key, ciph, len, keyword)) {
            found_key = key;
            found_locally = 1;
            break;
        }

        if ((key - start) % CHECK_INTERVAL == 0) {
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
        /* Ya se recibió el aviso durante el MPI_Test; la solicitud ya
         * fue completada, sólo queda liberarla. */
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
    } else {
        /* Se agotó el segmento propio sin encontrar la llave: se espera
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
        printf("Tiempo paralelo (naive, np=%d): %.6f segundos\n", num_procs, max_elapsed);
    }

    free(ciph);
    MPI_Finalize();
    return 0;
}
