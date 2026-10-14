/* des.h -- wrapper propio sobre DES de OpenSSL (reemplaza rpc/des_crypt.h,
 * no disponible en sistemas modernos). */

#ifndef DES_H
#define DES_H

#include <stddef.h>

/* Cifra 'buf' in-place (DES-ECB, llave en [0, 2^56-1]). 'len' debe ser
 * múltiplo de 8; usar load_and_pad_file() para garantizarlo. */
void des_encrypt(long key, unsigned char *buf, size_t len);

/* Descifra 'buf' in-place (inversa de des_encrypt). */
void des_decrypt(long key, unsigned char *buf, size_t len);

/* Intenta descifrar 'ciph' con 'key' y busca 'keyword' en el resultado.
 * Retorna 1 si aparece (candidata a llave correcta), 0 si no. */
int try_key(long key, const unsigned char *ciph, size_t len, const char *keyword);

#endif /* DES_H */
