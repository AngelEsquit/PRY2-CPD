/*
 * approach2_mpi.c
 * ---------------------------------------------------------------------
 * Acercamiento alternativo #2: recorrido intercalado ("striding").
 *
 * En lugar de dividir el espacio en bloques contiguos (como en el
 * acercamiento naive), cada proceso 'rank' revisa las llaves
 * rank, rank+np, rank+2*np, rank+3*np, ... Así, en todo momento, todos
 * los procesos están explorando aproximadamente la misma "profundidad"
 * relativa dentro del espacio total. Esto evita el efecto descrito en
 * el enunciado donde la llave solución cae al inicio del segmento de un
 * proceso (speedup artificialmente alto e inconsistente) o al final de
 * otro (speedup ~1), suavizando la varianza del tiempo paralelo medido.
 *
 * Coordinación: igual que en naive_mpi.c, mediante MPI_Irecv/MPI_Test
 * para el sondeo no bloqueante y MPI_Send/MPI_Wait para el aviso de
 * llave encontrada.
 *
 * Uso: mpirun -np <N> ./approach2_mpi <archivo_cifrado> <palabra_clave> <max_key>
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "common.h"
#include "des.h"

#define TAG_FOUND 200
#define CHECK_INTERVAL 100000

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

    long incoming_key = -1;
    MPI_Request recv_req;
    MPI_Irecv(&incoming_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND,
              MPI_COMM_WORLD, &recv_req);

    double t0 = MPI_Wtime();

    long found_key = -1;
    int found_locally = 0;
    int someone_found = 0;
    long checks = 0;

    for (long key = rank; key <= max_key; key += num_procs) {
        if (try_key(key, ciph, len, keyword)) {
            found_key = key;
            found_locally = 1;
            break;
        }

        checks++;
        if (checks % CHECK_INTERVAL == 0) {
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
        /* Se agotó el rango propio sin encontrar la llave: se espera
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
        printf("Tiempo paralelo (interleaved, np=%d): %.6f segundos\n", num_procs, max_elapsed);
    }

    free(ciph);
    MPI_Finalize();
    return 0;
}
