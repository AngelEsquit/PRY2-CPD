/* des.c -- wrapper de DES sobre libcrypto (OpenSSL): cifrado, descifrado
 * y verificación de llave (try_key) usados por todos los binarios. */

#include "des.h"
#include <openssl/des.h>
#include <string.h>
#include <stdlib.h>

/* Empaqueta 'lkey' (56 bits efectivos) en el bloque de 8 bytes que espera
 * DES: 7 bits útiles por byte, bit 0 siempre en 0. DES_set_key_unchecked
 * ignora el bit 0 de cada byte (paridad); si se usaran los 8 bits crudos,
 * dos llaves que sólo difirieran en esos bits colisionarían al mismo key
 * schedule (verificado: 777 y 520 colisionaban antes de este fix). */
static void long_to_des_key(long lkey, DES_cblock *des_key)
{
    unsigned char *key_bytes = (unsigned char *)des_key;
    for (int i = 0; i < 8; i++) {
        unsigned char bits7 = (unsigned char)(lkey & 0x7F);
        key_bytes[i] = (unsigned char)(bits7 << 1); /* bit 0 = 0 (paridad) */
        lkey >>= 7;
    }
}

/* Cifra 'buf' in-place con DES-ECB y 'key'. 'len' debe ser múltiplo de 8
 * (tamaño de bloque DES); se deriva un único key schedule de 'key' y se
 * reutiliza para cifrar cada bloque de 8 bytes por separado (modo ECB:
 * sin encadenamiento entre bloques, cada uno se cifra de forma
 * independiente con la misma llave). */
void des_encrypt(long key, unsigned char *buf, size_t len)
{
    DES_cblock des_key;
    DES_key_schedule schedule;

    long_to_des_key(key, &des_key);
    DES_set_key_unchecked(&des_key, &schedule); /* deriva las 16 subllaves de ronda */

    for (size_t i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(buf + i), (DES_cblock *)(buf + i),
                         &schedule, DES_ENCRYPT);
    }
}

/* Descifra 'buf' in-place con DES-ECB y 'key': mismo key schedule que
 * des_encrypt, pero las subllaves se aplican en orden inverso dentro de
 * DES_ecb_encrypt (flag DES_DECRYPT) -- es la operación inversa exacta,
 * bloque a bloque, de des_encrypt con la misma llave. */
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

/* Prueba si 'key' descifra 'ciph' a un texto que contiene 'keyword'.
 * Retorna 1 si coincide, 0 si no. Usada por el bruteforce en todas las
 * versiones (secuencial y paralelas). */
int try_key(long key, const unsigned char *ciph, size_t len, const char *keyword)
{
    unsigned char *temp = malloc(len + 1);
    if (temp == NULL) {
        return 0; /* sin memoria: se cuenta como intento fallido, no se aborta */
    }

    memcpy(temp, ciph, len); /* copia: nunca mutar 'ciph' entre intentos */
    temp[len] = '\0';

    des_decrypt(key, temp, len);
    int found = (strstr((char *)temp, keyword) != NULL);

    free(temp);
    return found;
}
