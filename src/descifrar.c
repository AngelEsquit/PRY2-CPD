/*
 * descifrar.c
 * ---------------------------------------------------------------------
 * Utilidad para descifrar un archivo cifrado con DES, dada la llave
 * privada original. Imprime el texto resultante en pantalla (Parte
 * B.1: captura de pantalla evidenciando cifrado/descifrado correcto).
 *
 * Uso: ./descifrar <archivo_cifrado> <llave>
 *
 * Entradas:
 *   archivo_cifrado - ruta al archivo binario cifrado
 *   llave            - llave privada usada originalmente para cifrar
 * Salidas:
 *   Texto descifrado impreso en stdout.
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "des.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_cifrado> <llave>\n", argv[0]);
        return ERR_EXIT;
    }

    const char *ciph_path = argv[1];
    long key = atol(argv[2]);

    size_t len;
    unsigned char *buf = load_and_pad_file(ciph_path, &len);

    des_decrypt(key, buf, len);

    printf("Texto descifrado con llave %ld:\n", key);
    fwrite(buf, 1, len, stdout);
    printf("\n");

    free(buf);
    return 0;
}
