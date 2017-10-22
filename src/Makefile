CC=gcc
CFLAGS= -D_POSIX_SOURCE -Wall -Werror -pedantic -std=c99 -D_GNU_SOURCE -pthread

all: server

server: server.c helpers.c
	$(CC) $(CFLAGS) -o server server.c helpers.c


