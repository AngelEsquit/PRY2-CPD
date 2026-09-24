/*
 * common.h
 * ---------------------------------------------------------------------
 * Utilidades compartidas por todos los programas del proyecto: carga y
 * validación de archivos, y manejo defensivo de errores.
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/* Código de salida usado ante errores de validación (programación
 * defensiva: argumentos inválidos, archivos inexistentes, etc.). */
#define ERR_EXIT 1

/*
 * Carga el contenido de 'path' en un buffer nuevo reservado con malloc,
 * rellenando con espacios (' ') hasta el siguiente múltiplo de 8 bytes
 * (tamaño de bloque de DES). Guarda la longitud final (con padding) en
 * *out_len. Si el archivo no existe, no se puede leer, o está vacío,
 * termina el programa con un mensaje de error (die()).
 *
 * El llamador es responsable de liberar el buffer retornado con free().
 */
unsigned char *load_and_pad_file(const char *path, size_t *out_len);

/*
 * Imprime 'msg' como error a stderr, con un prefijo estándar, y termina
 * el programa con código ERR_EXIT.
 */
void die(const char *msg);

#endif /* COMMON_H */
