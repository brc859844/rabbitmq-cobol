CC = gcc
INC = -I. -I../rmq
DEFS = -DAMQP091
CFLAGS = $(DEFS) -fpic -Wno-deprecated-declarations -Wall
LDPATH = 


all: 		server cobol


server: 	list.o hash.o server.o utils.o
		$(CC) -o amqp-server server.o list.o hash.o utils.o $(LDPATH) -lrabbitmq -ldl 

list.o: 	list.c list.h
		$(CC) $(CFLAGS) $(INC) -c list.c

hash.o: 	hash.c hash.h
		$(CC) $(CFLAGS) $(INC) -c hash.c

server.o: 	server.c list.h hash.h
		$(CC) $(CFLAGS) $(INC) -c server.c

utils.o: 	utils.c utils.h
		$(CC) $(CFLAGS) $(INC) -c utils.c

cobol: 		uars.cbl
	        cobc -fimplicit-init -m -free uars.cbl

clean:
		rm -f *.o
		rm -f amqp-server
		rm -f *.so

