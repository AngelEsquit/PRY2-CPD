/*
 * des.c
 * ---------------------------------------------------------------------
 * Implementación del wrapper de DES sobre libcrypto (OpenSSL).
 *
 * Técnica de conversión de llave: se distribuye la llave (un 'long' en
 * el rango [0, 2^56-1]) en un arreglo de 8 bytes, un byte por cada 8
 * bits, igual que el bruteforce.c de referencia (mpitutorial/Argonne).
 * DES_set_key_unchecked no valida ni fuerza la paridad de cada byte, y
 * el algoritmo DES internamente sólo usa 7 de los 8 bits de cada byte
 * para construir el key schedule (el bit restante es, tradicionalmente,
 * un bit de paridad). Por eso el espacio de llaves efectivo que se
 * recorre en este proyecto es de 2^56, tal como indica el enunciado.
 * ---------------------------------------------------------------------
 */

#include "des.h"
#include <openssl/des.h>
#include <string.h>
#include <stdlib.h>

/* Convierte una llave 'long' al bloque de 8 bytes que espera DES. */
static void long_to_des_key(long lkey, DES_cblock *des_key)
{
    unsigned char *key_bytes = (unsigned char *)des_key;
    for (int i = 0; i < 8; i++) {
        key_bytes[i] = (unsigned char)(lkey & 0xFF);
        lkey >>= 8;
    }
}

void des_encrypt(long key, unsigned char *buf, size_t len)
{
    DES_cblock des_key;
    DES_key_schedule schedule;

    long_to_des_key(key, &des_key);
    DES_set_key_unchecked(&des_key, &schedule);

    for (size_t i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(buf + i), (DES_cblock *)(buf + i),
                         &schedule, DES_ENCRYPT);
    }
}

void des_decrypt(long key, unsigned char *buf, size_t len)
{
    DES_cblock des_key;
    DES_key_schedule schedule;

    long_to_des_key(key, &des_key);
    DES_set_key_unchecked(&des_key, &schedule);

    for (size_t i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(buf + i), (DES_cblock *)(buf + i),
                         &schedule, DES_DECRYPT);
    }
}

int try_key(long key, const unsigned char *ciph, size_t len, const char *keyword)
{
    /* Se trabaja sobre una copia temporal: 'ciph' nunca debe modificarse,
     * pues cada intento de llave debe partir del mismo texto cifrado. */
    unsigned char *temp = malloc(len + 1);
    if (temp == NULL) {
        /* Programación defensiva: si no hay memoria disponible, se
         * reporta como intento fallido en lugar de abortar el programa
         * en medio de una búsqueda distribuida. */
        return 0;
    }

    memcpy(temp, ciph, len);
    temp[len] = '\0';

    des_decrypt(key, temp, len);

    int found = (strstr((char *)temp, keyword) != NULL);

    free(temp);
    return found;
}
