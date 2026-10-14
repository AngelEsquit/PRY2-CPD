/* common.h -- utilidades compartidas: carga de archivos y errores. */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#define ERR_EXIT 1 /* código de salida para errores de validación */

/* Carga 'path' con padding a múltiplo de 8 B; el caller libera con free().
 * Termina el programa (die()) si el archivo no existe o está vacío. */
unsigned char *load_and_pad_file(const char *path, size_t *out_len);

/* Imprime 'msg' en stderr y termina el programa con código ERR_EXIT. */
void die(const char *msg);

#endif /* COMMON_H */
