/*
 * common.c
 * ---------------------------------------------------------------------
 * Implementación de utilidades compartidas: carga defensiva de archivos
 * y manejo centralizado de errores.
 * ---------------------------------------------------------------------
 */

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *msg)
{
    fprintf(stderr, "[ERROR] %s\n", msg);
    exit(ERR_EXIT);
}

unsigned char *load_and_pad_file(const char *path, size_t *out_len)
{
    if (path == NULL || out_len == NULL) {
        die("Parámetros inválidos al cargar el archivo (ruta o puntero de salida nulos).");
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "[ERROR] No se pudo abrir el archivo '%s'.\n", path);
        exit(ERR_EXIT);
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        die("No se pudo determinar el tamaño del archivo de entrada.");
    }
    long raw_len = ftell(fp);
    rewind(fp);

    if (raw_len <= 0) {
        fclose(fp);
        die("El archivo de entrada está vacío o su tamaño no pudo leerse.");
    }

    /* Redondea hacia arriba al siguiente múltiplo de 8 (bloque DES). */
    size_t padded_len = ((size_t)raw_len + 7) / 8 * 8;
    if (padded_len == 0) {
        padded_len = 8;
    }

    unsigned char *buf = calloc(padded_len, sizeof(unsigned char));
    if (buf == NULL) {
        fclose(fp);
        die("No se pudo reservar memoria para el contenido del archivo.");
    }
    memset(buf, ' ', padded_len); /* padding con espacios en blanco */

    size_t read_bytes = fread(buf, 1, (size_t)raw_len, fp);
    fclose(fp);

    if (read_bytes != (size_t)raw_len) {
        free(buf);
        die("No se pudo leer el contenido completo del archivo de entrada.");
    }

    *out_len = padded_len;
    return buf;
}
