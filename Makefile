GCC = gcc 

APPS = server client

all: $(APPS)

server: server.c
	$(GCC) -o server server.c

client: client.c
	$(GCC) -o client client.c

clean:
	rm -f $(APPS)