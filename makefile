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
TEST_DIR=./teste
CFLAGS = -Wall

all: directory clean lib
directory:
	mkdir bin -p

main: t2fs.o
	$(CC) -o $(BIN_DIR)/main $(BIN_DIR)/*.o -g -Wall
t2fs.o: 
	$(CC) -c $(SRC_DIR)/t2fs.c -o $(BIN_DIR)/t2fs.o -Wall 

lib: t2fs.o
	ar crs $(LIB_DIR)/libt2fs.a $(LIB_DIR)/apidisk.o $(LIB_DIR)/bitmap2.o $(BIN_DIR)/t2fs.o 

clean:
	find $(BIN_DIR) $(LIB_DIR) $(TEST_DIR) -type f ! -name 'apidisk.o' ! -name 'bitmap2.o' ! -name '*.c' ! -name 'Makefile' ! -name 'script.sh'  -exec rm -f {} +
	find $(TEST_DIR)/* -type d -exec rm  -r -f {} +
