#CC = gcc
#CFLAGS  = -g -O0 -Wall -Werror -std=gnu11 -I/usr/include/hiredis
#LDFLAG = -L.

CC      = gcc
LFLAGS = -lhiredis 
CFLAGS  = -g -O0 -Wall -Werror -std=gnu11 -I/usr/include/hiredis

.PHONY : all test clean

all: hmds clean

hmds: hdb.o command_processor.o server_socket.o server_main.o
	$(CC) $(CFLAGS) -o hmds hdb.o command_processor.o server_socket.o server_main.o $(LFLAGS)

command_processor.o: ../common/command_processor.c ../common/command_processor.h
	$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

server_socket.o: server_socket.c server_socket.h
	$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

server_main.o: server_main.c
	$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

hdb.o: ../hdb/hdb.c ../hdb/hdb.h
	$(CC) $(CFLAGS) -o $@ -c $< $(LFLAGS)

clean:
	rm -f *.o *.a