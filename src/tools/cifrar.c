/*
 * cifrar.c
 * ---------------------------------------------------------------------
 * Utilidad para cifrar un archivo de texto plano con DES usando una
 * llave privada arbitraria pasada como parámetro (Parte B.1).
 *
 * Uso: ./cifrar <archivo_entrada.txt> <archivo_salida> <llave>
 *
 * Entradas:
 *   archivo_entrada.txt - ruta al texto plano a cifrar
 *   archivo_salida      - ruta donde se escribirá el texto cifrado
 *   llave                - llave privada (entero, 0 a 2^56-1)
 * Salidas:
 *   Archivo binario cifrado en 'archivo_salida'.
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
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <archivo_entrada.txt> <archivo_salida> <llave>\n", argv[0]);
        return ERR_EXIT;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];
    long key = atol(argv[3]);

    size_t len;
    unsigned char *buf = load_and_pad_file(in_path, &len);

    des_encrypt(key, buf, len);

    FILE *out = fopen(out_path, "wb");
    if (out == NULL) {
        free(buf);
        die("No se pudo crear el archivo de salida para el texto cifrado.");
    }

    size_t written = fwrite(buf, 1, len, out);
    fclose(out);

    if (written != len) {
        free(buf);
        die("No se pudo escribir completamente el archivo cifrado.");
    }

    printf("Texto cifrado con llave %ld escrito en '%s' (%zu bytes).\n", key, out_path, len);

    free(buf);
    return 0;
}
