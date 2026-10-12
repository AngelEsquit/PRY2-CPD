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

.PHONY: all dirs clean

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

clean:
	rm -rf $(BIN_DIR)
