CC=gcc
CFLAGS =-g -pthread -lm -pedantic -Wall -std=gnu99
.PHONY: all clean
.DEFAULT_GOAL := all 

all: client server

# Link main from object files
client: client.o commonfunction.o
	$(CC) $(CFLAGS) $^ -o $@
server: server.o commonfunction.o
	$(CC) $(CFLAGS) $^ -o $@

# Compile source files to objects
client.o: client.c commonfunction.h
server.o: server.c commonfunction.h
commonfunction.o: commonfunction.c commonfunction.h

clean:
	rm -f *.o
	rm -f client server

