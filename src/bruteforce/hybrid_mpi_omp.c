/*
 * hybrid_mpi_omp.c
 * ---------------------------------------------------------------------
 * Acercamiento alternativo #4: híbrido MPI + OpenMP (dos niveles de
 * paralelismo: procesos distribuidos + hilos de memoria compartida).
 *
 * Nivel 1 (MPI, entre procesos / nodos): el espacio [0, max_key] se
 * divide en 'np' bloques contiguos de igual tamaño, uno por proceso,
 * igual que en naive_mpi.c.
 *
 * Nivel 2 (OpenMP, dentro de cada proceso): el bloque de cada proceso
 * se reparte entre 'nthreads' hilos usando un recorrido intercalado
 * (cada hilo 't' revisa block_start+t, block_start+t+nthreads, ...),
 * el mismo truco de approach2/striding pero aplicado dentro de un solo
 * proceso para evitar que un hilo quede "atascado" con una sub-región
 * desfavorable del bloque.
 *
 * Coordinación:
 *   - Entre procesos: MPI_Irecv/MPI_Test/MPI_Send/MPI_Wait, igual que
 *     en los demás acercamientos. Sólo el hilo 0 de cada proceso hace
 *     llamadas a MPI (modo MPI_THREAD_FUNNELED), para no requerir una
 *     librería MPI con soporte completo para múltiples hilos.
 *   - Entre hilos de un mismo proceso: una bandera compartida
 *     (omp_found) protegida por una región crítica cuando se escribe;
 *     el hilo 0 también refleja en ella los avisos recibidos por MPI
 *     de otros procesos, para que el resto de hilos del mismo proceso
 *     se detenga sin tener que llamar a MPI ellos mismos.
 *
 * Uso: mpirun -np <N> ./hybrid_mpi_omp <archivo_cifrado> <palabra_clave> <max_key> [nthreads]
 * (nthreads por defecto: omp_get_max_threads(), ej. $OMP_NUM_THREADS)
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <omp.h>
#include "common.h"
#include "des.h"

#define TAG_FOUND 400
#define CHECK_INTERVAL 100000 /* cada cuántas iteraciones (por hilo) se revisa el aviso */

int main(int argc, char *argv[])
{
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (provided < MPI_THREAD_FUNNELED && rank == 0) {
        fprintf(stderr,
            "[AVISO] La librería MPI no garantiza MPI_THREAD_FUNNELED; "
            "este programa sólo llama a MPI desde el hilo 0 de cada "
            "proceso, así que debería funcionar igual, pero no está "
            "garantizado por el estándar.\n");
    }

    if (argc < 4) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s <archivo_cifrado> <palabra_clave> <max_key> [nthreads]\n", argv[0]);
        }
        MPI_Finalize();
        return ERR_EXIT;
    }

    const char *ciph_path = argv[1];
    const char *keyword = argv[2];
    long max_key = atol(argv[3]);
    int nthreads = (argc >= 5) ? atoi(argv[4]) : omp_get_max_threads();

    if (max_key < 0 || nthreads <= 0) {
        if (rank == 0) fprintf(stderr, "[ERROR] max_key debe ser no negativo y nthreads positivo.\n");
        MPI_Finalize();
        return ERR_EXIT;
    }
    omp_set_num_threads(nthreads);

    size_t len;
    unsigned char *ciph = load_and_pad_file(ciph_path, &len);

    /* Nivel 1: división equitativa entre procesos (igual que naive_mpi.c). */
    long range_size = (max_key + 1) / num_procs;
    long block_start = rank * range_size;
    long block_end = (rank == num_procs - 1) ? max_key : (block_start + range_size - 1);

    long incoming_key = -1;
    MPI_Request recv_req;
    MPI_Irecv(&incoming_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND,
              MPI_COMM_WORLD, &recv_req);

    double t0 = MPI_Wtime();

    /* Banderas compartidas entre los hilos de este proceso. */
    volatile int omp_found = 0;         /* 1: algún hilo (local o vía MPI) ya encontró la llave */
    volatile int omp_found_via_mpi = 0; /* 1: omp_found se activó por un aviso MPI de otro proceso */
    volatile long omp_found_key = -1;

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int team_size = omp_get_num_threads();
        long checks = 0;

        for (long key = block_start + tid; key <= block_end; key += team_size) {
            if (try_key(key, ciph, len, keyword)) {
                #pragma omp critical(found_update)
                {
                    if (!omp_found) {
                        omp_found = 1;
                        omp_found_key = key;
                    }
                }
                break;
            }

            checks++;
            if (checks % CHECK_INTERVAL == 0) {
                if (omp_found) {
                    break;
                }
                /* Sólo el hilo 0 sondea MPI (MPI_THREAD_FUNNELED). */
                if (tid == 0) {
                    int flag = 0;
                    MPI_Test(&recv_req, &flag, MPI_STATUS_IGNORE);
                    if (flag) {
                        #pragma omp critical(found_update)
                        {
                            if (!omp_found) {
                                omp_found = 1;
                                omp_found_via_mpi = 1;
                                omp_found_key = incoming_key;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    long found_key = omp_found_key;
    int found_locally = omp_found && !omp_found_via_mpi;
    int someone_found = omp_found && omp_found_via_mpi;

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
        /* El hilo 0 ya completó la recepción al detectarla con MPI_Test. */
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
    } else {
        /* Ningún hilo encontró nada en su bloque: se espera (bloqueante)
         * el aviso de otro proceso. Se asume que la llave sí existe
         * dentro de [0, max_key]. */
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
        printf("Tiempo paralelo (hibrido MPI+OpenMP, np=%d, nthreads=%d): %.6f segundos\n",
               num_procs, nthreads, max_elapsed);
    }

    free(ciph);
    MPI_Finalize();
    return 0;
}
