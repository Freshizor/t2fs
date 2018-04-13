#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: t2fs library

t2fs: $(SRC_DIR)/t2fs.c
	$(CC) -c $(SRC_DIR)/t2fs.c -Wall && mv t2fs.o $(BIN_DIR)

library: $(BIN_DIR)/t2fs.o
	ar crs libt2fs.a $(BIN_DIR)/t2fs.o $(LIB_DIR)/apidisk.o $(LIB_DIR)/bitmap2.o && mv libt2fs.a $(LIB_DIR)

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~


