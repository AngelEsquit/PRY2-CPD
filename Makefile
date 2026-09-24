# ---------------------------------------------------------------------
# Makefile - Proyecto 2: Computación Paralela y Distribuida (UVG)
# Compila las utilidades de cifrado/descifrado, la versión secuencial y
# las versiones paralelas (MPI) del bruteforce sobre DES.
# ---------------------------------------------------------------------

CC      = gcc
MPICC   = mpicc
CFLAGS  = -O2 -Wall -Wno-deprecated-declarations
LDFLAGS = -lcrypto

SRC_DIR = src
BIN_DIR = bin

COMMON_SRCS = $(SRC_DIR)/common.c $(SRC_DIR)/des.c

.PHONY: all dirs clean

all: dirs \
     $(BIN_DIR)/cifrar \
     $(BIN_DIR)/descifrar \
     $(BIN_DIR)/secuencial \
     $(BIN_DIR)/naive_mpi \
     $(BIN_DIR)/approach1_mpi \
     $(BIN_DIR)/approach2_mpi

dirs:
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/cifrar: $(SRC_DIR)/cifrar.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/descifrar: $(SRC_DIR)/descifrar.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/secuencial: $(SRC_DIR)/secuencial.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/naive_mpi: $(SRC_DIR)/naive_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/approach1_mpi: $(SRC_DIR)/approach1_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/approach2_mpi: $(SRC_DIR)/approach2_mpi.c $(COMMON_SRCS)
	$(MPICC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)
