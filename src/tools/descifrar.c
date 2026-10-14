/* descifrar.c -- descifra un archivo cifrado con DES-ECB e imprime el
 * texto resultante (Parte B.1). Carga el binario cifrado tal cual (ya
 * viene en múltiplos de 8 B desde cifrar.c) y aplica des_decrypt() con
 * la misma llave usada para cifrar; si la llave es incorrecta, imprime
 * igualmente el resultado (datos ilegibles), sin detectar el error por
 * sí solo -- la validación de "llave correcta" la hace try_key() en el
 * bruteforce, no este programa.
 * Uso: ./descifrar <archivo_cifrado> <llave> */

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

    des_decrypt(key, buf, len); /* descifra todo el buffer in-place, bloque a bloque */

    printf("Texto descifrado con llave %ld:\n", key);
    fwrite(buf, 1, len, stdout);
    printf("\n");

    free(buf);
    return 0;
}
