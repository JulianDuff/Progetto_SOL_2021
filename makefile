all: server client libAPI.so

server:	server.c libAPI.so DataStr.c
	gcc server.c -pthread -Wl,-rpath,./ -L . -lAPI -o server

client: client.c  libAPI.so
	gcc client.c -Wl,-rpath,./ -L . -lAPI -o client

libAPI.so: API.o
	gcc -shared -o libAPI.so API.o

API.o: API.c
	gcc API.c -c -fPIC -o API.o
