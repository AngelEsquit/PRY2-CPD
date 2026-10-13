# ---------------------------------------------------------------------
# Makefile - Proyecto 2: Computación Paralela y Distribuida (UVG)
# Compila las utilidades de cifrado/descifrado, la versión secuencial y
# las versiones paralelas (MPI) del bruteforce sobre DES.
# ---------------------------------------------------------------------

CC       = gcc
MPICC    = mpicc
CFLAGS   = -O2 -Wall -Wno-deprecated-declarations -Isrc/common
OMPFLAGS = -fopenmp
LDFLAGS  = -lcrypto

SRC_DIR       = src
COMMON_DIR    = $(SRC_DIR)/common
TOOLS_DIR     = $(SRC_DIR)/tools
BRUTE_DIR     = $(SRC_DIR)/bruteforce
BIN_DIR       = bin

COMMON_SRCS = $(COMMON_DIR)/common.c $(COMMON_DIR)/des.c

.PHONY: all dirs clean help test-seq

all: dirs \
     $(BIN_DIR)/cifrar \
     $(BIN_DIR)/descifrar \
     $(BIN_DIR)/secuencial \
     $(BIN_DIR)/naive_mpi \
     $(BIN_DIR)/master_worker_mpi \
     $(BIN_DIR)/striding_mpi \
     $(BIN_DIR)/shuffled_mpi \
     $(BIN_DIR)/hybrid_mpi_omp

dirs:
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/cifrar: $(TOOLS_DIR)/cifrar.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/descifrar: $(TOOLS_DIR)/descifrar.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/secuencial: $(BRUTE_DIR)/secuencial.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/naive_mpi: $(BRUTE_DIR)/naive_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/master_worker_mpi: $(BRUTE_DIR)/master_worker_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/striding_mpi: $(BRUTE_DIR)/striding_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/shuffled_mpi: $(BRUTE_DIR)/shuffled_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/hybrid_mpi_omp: $(BRUTE_DIR)/hybrid_mpi_omp.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) $(OMPFLAGS) -o $@ $^ $(LDFLAGS)

# -----------------------------------------------------------------------
# test-seq: prueba de humo rápida usando sólo la versión secuencial.
# Cifra data/texto.txt con la llave 42 y luego la rompe buscando en [0, 100].
# Útil para verificar que todo compila y funciona antes de correr benchmarks.
# -----------------------------------------------------------------------
test-seq: $(BIN_DIR)/cifrar $(BIN_DIR)/descifrar $(BIN_DIR)/secuencial
	@echo "--- test-seq: cifrar con llave 42 ---"
	$(BIN_DIR)/cifrar data/texto.txt data/texto_cifrado.bin 42
	@echo "--- test-seq: descifrar con llave 42 ---"
	$(BIN_DIR)/descifrar data/texto_cifrado.bin 42
	@echo "--- test-seq: búsqueda secuencial en [0, 100] ---"
	$(BIN_DIR)/secuencial data/texto_cifrado.bin "es una prueba de" 100
	@echo "--- test-seq: OK ---"

# -----------------------------------------------------------------------
# help: muestra los targets disponibles y su propósito.
# -----------------------------------------------------------------------
help:
	@echo ""
	@echo "Proyecto 2 — Bruteforce DES con Open MPI"
	@echo "Computación Paralela y Distribuida — UVG"
	@echo ""
	@echo "Targets disponibles:"
	@echo "  all        Compila todos los binarios en bin/  (default)"
	@echo "  clean      Elimina bin/"
	@echo "  test-seq   Prueba de humo: cifra con llave=42 y la busca secuencialmente"
	@echo "  help       Muestra este mensaje"
	@echo ""
	@echo "Binarios generados en bin/:"
	@echo "  cifrar             Cifra un archivo con DES (llave dada)"
	@echo "  descifrar          Descifra un archivo con DES (llave dada)"
	@echo "  secuencial         Bruteforce secuencial"
	@echo "  naive_mpi          Bruteforce MPI naive (bloques contiguos)"
	@echo "  master_worker_mpi  Bruteforce MPI master-worker dinámico"
	@echo "  striding_mpi       Bruteforce MPI intercalado (striding)"
	@echo "  shuffled_mpi       Bruteforce MPI con shuffle pseudo-aleatorio"
	@echo "  hybrid_mpi_omp     Bruteforce híbrido MPI + OpenMP"
	@echo ""

clean:
	rm -rf $(BIN_DIR)
