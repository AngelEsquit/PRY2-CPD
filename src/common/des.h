/*
 * des.h
 * ---------------------------------------------------------------------
 * Wrapper propio sobre la API de DES de OpenSSL (libcrypto). Reemplaza a
 * "rpc/des_crypt.h", que ya no está disponible en sistemas modernos
 * (Parte A.4 del enunciado).
 *
 * Proyecto 2 - Computación Paralela y Distribuida - UVG
 * ---------------------------------------------------------------------
 */

#ifndef DES_H
#define DES_H

#include <stddef.h>

/*
 * Cifra 'len' bytes de 'buf' in-place usando DES en modo ECB con la
 * llave 'key' (0 a 2^56-1). 'len' debe ser múltiplo de 8 (tamaño de
 * bloque de DES); use load_and_pad_file() para garantizarlo.
 */
void des_encrypt(long key, unsigned char *buf, size_t len);

/*
 * Descifra 'len' bytes de 'buf' in-place usando DES en modo ECB con la
 * llave 'key'. Es la operación inversa de des_encrypt().
 */
void des_decrypt(long key, unsigned char *buf, size_t len);

/*
 * Equivalente a la rutina "tryKey" del bruteforce.c de referencia:
 * intenta descifrar 'ciph' (sin modificarlo) con 'key' y busca 'keyword'
 * como substring del resultado usando strstr. Retorna 1 si la palabra
 * clave aparece (indicio fuerte de que 'key' es la llave correcta), 0
 * en caso contrario.
 */
int try_key(long key, const unsigned char *ciph, size_t len, const char *keyword);

#endif /* DES_H */
