CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread

all: echo-server echo-client

echo-server: echo-server.c
	$(CC) $(CFLAGS) -o $@ $<

echo-client: echo-client.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f echo-server echo-client