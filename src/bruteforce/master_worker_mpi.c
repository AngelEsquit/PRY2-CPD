/* master_worker_mpi.c -- acercamiento alternativo #1: master-worker con
 * bloques dinámicos. El rank 0 no busca: reparte bloques de 'chunk_size'
 * llaves bajo demanda a los trabajadores (rank >= 1), que piden el
 * siguiente bloque al terminar el suyo. Evita que un proceso quede
 * atascado con un segmento grande (problema del naive), a costa de una
 * ronda de mensajes por bloque. Mensajes: TAG_WORK [inicio,fin] maestro
 * -> trabajador, TAG_RESULT [llave|-1, rank] trabajador -> maestro,
 * TAG_STOP cuando ya no hay más trabajo.
 * Uso: mpirun -np <N> ./master_worker_mpi <archivo_cifrado> <palabra_clave> <max_key> [chunk_size]
 * (requiere N >= 2) */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "common.h"
#include "des.h"

#define TAG_WORK   1
#define TAG_RESULT 2
#define TAG_STOP   3

static long next_block_end(long next_key, long chunk_size, long max_key)
{
    long candidate = next_key + chunk_size - 1;
    return (candidate > max_key) ? max_key : candidate;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (argc < 4) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s <archivo_cifrado> <palabra_clave> <max_key> [chunk_size]\n", argv[0]);
        }
        MPI_Finalize();
        return ERR_EXIT;
    }
    if (num_procs < 2) {
        if (rank == 0) {
            fprintf(stderr, "[ERROR] Este acercamiento requiere al menos 2 procesos (1 maestro + 1 trabajador).\n");
        }
        MPI_Finalize();
        return ERR_EXIT;
    }

    const char *ciph_path = argv[1];
    const char *keyword = argv[2];
    long max_key = atol(argv[3]);
    long chunk_size = (argc >= 5) ? atol(argv[4]) : 50000;

    if (max_key < 0 || chunk_size <= 0) {
        if (rank == 0) fprintf(stderr, "[ERROR] max_key y chunk_size deben ser positivos.\n");
        MPI_Finalize();
        return ERR_EXIT;
    }

    size_t len;
    unsigned char *ciph = load_and_pad_file(ciph_path, &len);

    if (rank == 0) {
        printf("Búsqueda master-worker en '%s' (palabra clave: \"%s\", rango [0, %ld], "
               "np=%d, chunk=%ld)...\n", ciph_path, keyword, max_key, num_procs, chunk_size);
    }

    double t0 = MPI_Wtime();
    long global_found = -1;

    if (rank == 0) {
        /* --- Maestro: reparte trabajo bajo demanda y recolecta resultados --- */
        long next_key = 0;
        int workers_active = 0;

        /* Reparto inicial: un bloque a cada trabajador disponible. */
        for (int p = 1; p < num_procs && next_key <= max_key; p++) {
            long block[2] = { next_key, next_block_end(next_key, chunk_size, max_key) };
            MPI_Send(block, 2, MPI_LONG, p, TAG_WORK, MPI_COMM_WORLD);
            next_key = block[1] + 1;
            workers_active++;
        }
        /* Si hay más trabajadores que bloques posibles, se les detiene de una vez. */
        for (int p = workers_active + 1; p < num_procs; p++) {
            long stop_signal[2] = { -1, -1 };
            MPI_Send(stop_signal, 2, MPI_LONG, p, TAG_STOP, MPI_COMM_WORLD);
        }

        int found_flag = 0;
        while (workers_active > 0) {
            MPI_Status status;
            long result[2]; /* result[0]: -1=no encontrada, >=0=llave; result[1]: rank del trabajador */
            MPI_Recv(result, 2, MPI_LONG, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);
            int worker = status.MPI_SOURCE;

            if (result[0] >= 0 && !found_flag) {
                found_flag = 1;
                global_found = result[0];
            }

            if (found_flag || next_key > max_key) {
                long stop_signal[2] = { -1, -1 };
                MPI_Send(stop_signal, 2, MPI_LONG, worker, TAG_STOP, MPI_COMM_WORLD);
                workers_active--;
            } else {
                long block[2] = { next_key, next_block_end(next_key, chunk_size, max_key) };
                MPI_Send(block, 2, MPI_LONG, worker, TAG_WORK, MPI_COMM_WORLD);
                next_key = block[1] + 1;
            }
        }
    } else {
        /* --- Trabajador: procesa bloques hasta recibir TAG_STOP --- */
        while (1) {
            MPI_Status status;
            long block[2];
            MPI_Recv(block, 2, MPI_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_STOP) {
                break;
            }

            long result[2] = { -1, rank };
            for (long key = block[0]; key <= block[1]; key++) {
                if (try_key(key, ciph, len, keyword)) {
                    result[0] = key;
                    break;
                }
            }
            MPI_Send(result, 2, MPI_LONG, 0, TAG_RESULT, MPI_COMM_WORLD);
        }
    }

    double t1 = MPI_Wtime();
    double elapsed = t1 - t0;
    double max_elapsed;
    MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        if (global_found >= 0) {
            printf("Llave encontrada: %ld\n", global_found);
        } else {
            printf("No se encontró la llave en el rango [0, %ld].\n", max_key);
        }
        printf("Tiempo paralelo (master-worker, np=%d, chunk=%ld): %.6f segundos\n",
               num_procs, chunk_size, max_elapsed);
    }

    free(ciph);
    MPI_Finalize();
    return 0;
}
