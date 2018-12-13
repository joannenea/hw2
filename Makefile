all: server.c client.c
	gcc -std=c11 -g -O0 -pthread -o server server.c
	gcc -std=c11 -g -O0 -pthread -o client client.c
clean:
	rm server
	rm client
