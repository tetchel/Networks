CC      = gcc
LFLAGS = -lhdb
#-Werror
CFLAGS  = -g -O0 -Wall -std=gnu11

.PHONY : all test clean

all: hftps clean

hftps: udp_socket.o server_socket.o server_main.o
		$(CC) $(CFLAGS) -o hftps udp_socket.o server_socket.o server_main.o $(LFLAGS)

udp_socket.o: ../common/udp_socket.c ../common/udp_socket.h
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

server_socket.o: server_socket.c server_socket.h
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

server_main.o: server_main.c
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

clean:
		rm -f *.o *.a

