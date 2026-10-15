#!/usr/bin/env python3
"""Llaves "interesantes" para la demo (N = 2^24, np = 4) y su costo predicho.

Imprime, para cada llave, cuántas llaves debe probar el proceso más lento
de cada acercamiento (~ tiempo, a ~3.2M llaves/s por núcleo). No ejecuta nada.
Uso: python3 scripts/lucky_keys.py [N=16777216] [np=4]
"""
import sys
from math import gcd

N = int(sys.argv[1]) if len(sys.argv) > 1 else 2**24
P = int(sys.argv[2]) if len(sys.argv) > 2 else 4
SEED_A, SEED_B = 2654435761, 2246822519   # mismas constantes que shuffled_mpi.c
a = SEED_A % N or 1
while gcd(a, N) != 1:
    a += 1
b = SEED_B % N
a_inv = pow(a, -1, N)
R = N // P                                 # tamaño de bloque (naive/shuffled)

def idx_of(key):                           # inverso de key = (a*idx + b) mod N
    return (a_inv * (key - b)) % N

def costs(key):
    naive = key % R if key // R < P else key - (P - 1) * R
    stride = key // P
    mw = key // (P - 1)
    i = idx_of(key)
    shuf = i % R if i // R < P else i - (P - 1) * R
    return naive, stride, mw, shuf

cases = {
    "suerte shuffled (inicio de bloque)": (a * R * 2 + b) % N,   # idx = 2R
    "suerte naive (inicio de bloque 2)":  2 * R,
    "suerte naive+stride (llave chica)":  42,
    "mala naive (fin del bloque 3)":      N - 1,
    "mala naive (fin del bloque 0)":      R - 1,
    "mala shuffled (fin de bloque 1)":    (a * (2 * R - 1) + b) % N,
    "demo por defecto":                   12_000_000,
}
print(f"N=2^{N.bit_length()-1}, np={P}, a={a}, b={b}\n")
print(f"{'caso':38} {'llave':>10} | {'naive':>9} {'stride':>9} {'m-w':>9} {'shuf':>9}  (llaves/proceso)")
for name, k in cases.items():
    print(f"{name:38} {k:>10} | " + " ".join(f"{c:>9}" for c in costs(k)))
