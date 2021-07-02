all: server client libAPI.so

server:	server.c libAPI.so DataStr.c ThreadPool.c DataStr.c FileMemory.c FileMemory.h ThreadPool.h config.c config.h workerFunctions.c workerFunctions.h
	gcc server.c ThreadPool.c DataStr.c FileMemory.c config.c workerFunctions.c -Wall -pthread -Wl,-rpath,./ -L . -lAPI -o server

client: client.c  libAPI.so
	gcc client.c -Wall -Wl,-rpath,./ -L . -lAPI -o client

libAPI.so: API.o
	gcc -shared -o libAPI.so API.o

API.o: API.c
	gcc API.c -c -fPIC -o API.o
