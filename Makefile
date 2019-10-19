# Makefile for client server program
CFLAGS=-Wall -O2 -pedantic

# Target to compile and build both client and server
all: clean server client

# Target to compile server
server:
	mkdir -p build
	gcc -I include src/server.c -o build/server -lrt -lpthread $(CFLAGS)
	
# Target to compile client
client: 
	mkdir -p build
	gcc -I include src/client.c -o build/client -lpthread $(CFLAGS)

clean: 
	rm -rf build/*
