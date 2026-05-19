CC = gcc
CFLAGS = -Wall -Wextra -pedantic
LDFLAGS = -lm

all: servidor cliente

servidor: servidor.c DataStructures/KDTree/kdTree.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

cliente: cliente.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f servidor cliente