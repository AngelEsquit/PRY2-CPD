/* cifrar.c -- cifra un archivo de texto plano con DES-ECB (Parte B.1).
 * Carga el archivo de entrada, lo rellena a múltiplo de 8 B (bloque DES)
 * y lo cifra in-place con des_encrypt() usando la llave dada; el
 * resultado binario se escribe tal cual a disco, sin reinterpretarlo
 * como texto. La misma llave debe pasarse luego a descifrar.c para
 * recuperar el original.
 * Uso: ./cifrar <archivo_entrada.txt> <archivo_salida> <llave> */

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
    unsigned char *buf = load_and_pad_file(in_path, &len); /* padding a múltiplo de 8 B */

    des_encrypt(key, buf, len); /* cifra todo el buffer in-place, bloque a bloque */

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
