CC      = gcc
LFLAGS = -lhfs -lhmdp
CFLAGS  = -g -O0 -Wall -Werror -std=gnu11

.PHONY : all test clean

all: client clean

client: udp_socket.o client_socket.o hftpd_client.o client_ops.o
		$(CC) $(CFLAGS) -o client udp_socket.o client_socket.o hftpd_client.o client_ops.o $(LFLAGS)

udp_socket.o: ../common/udp_socket.c ../common/udp_socket.h
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

client_socket.o: hftpd_client_socket.c hftpd_client_socket.h
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

hftpd_client.o: hftpd_client.c
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

client_ops.o: hftpd_client_ops.c hftpd_client_ops.h
		$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

clean:
		rm -f *.o *.a

