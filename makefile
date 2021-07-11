CC = gcc
CFLAGS = -Wall -Wl,-rpath,./ -L . -lAPI
scriptfold = tests
all: server client libAPI.so
.PHONY: clean, test1, test2

server:	server.c libAPI.so  ThreadPool.c FileMemory.c  config.c  workerFunctions.c 
	$(CC) $(CFLAGS) -pthread  $^ -o $@ 

client: client.c  libAPI.so
	$(CC) $(CFLAGS) $^  -o $@

libAPI.so: API.o
	$(CC) $^ -shared -o $@

API.o: API.c
	$(CC) API.c -c -fPIC -o $@

test1: clean all 
	-mkdir ./tests/test1
	chmod +x $(scriptfold)/test1.sh
	chmod +x $(scriptfold)/createFiles.sh
	$(scriptfold)/test1.sh $(scriptfold)/configs/config_t1.txt 

test2: clean all 
	-mkdir ./tests/test2
	chmod +x $(scriptfold)/test2.sh
	chmod +x $(scriptfold)/createFiles.sh
	$(scriptfold)/test2.sh $(scriptfold)/configs/config_t2.txt sock_t2

clean:
	 -rm -rf ./tests/test1/
	 -rm -rf ./tests/test2/
	 -rm -f ./*.o
	 -rm -f ./*.so
	 -rm -f ./socket_t*
	 -rm -f ./server 
	 -rm -f ./client
