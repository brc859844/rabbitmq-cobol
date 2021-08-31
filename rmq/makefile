# Totally brain-dead makefile to build COBOL interface API (what there is of it) and examples. Assumes gcc and
# GnuCOBOL.
# 

CC = gcc
DEFS = -DAMQP091
CFLAGS = -fpic -Wno-deprecated-declarations -Wall
INC = -I. 
CCFLAGS = $(CFLAGS) $(INC) $(DEFS)


all: 		clean rmq.o rmqcbl.o demo01 demo02 demo03 demo04 demo05 func1 func2

rmq.o:		rmq.c rmq.h 	
		$(CC) -c $(CCFLAGS) rmq.c -o rmq.o

rmqcbl.o: 	rmqcbl.c rmq.h
		$(CC) -c $(CCFLAGS) rmqcbl.c -o rmqcbl.o

demo01: 	demo01.cbl rmq.o rmqcbl.o
		cobc -free -c -x demo01.cbl
		cobc -x -o demo01 demo01.o rmq.o rmqcbl.o -lrabbitmq -lpthread

demo02: 	demo02.cbl rmq.o rmqcbl.o
		cobc -free -c -x demo02.cbl
		cobc -x -o demo02 demo02.o rmq.o rmqcbl.o -lrabbitmq -lpthread

demo03: 	demo03.cbl rmq.o rmqcbl.o
		cobc -free -c -x demo03.cbl
		cobc -x -o demo03 demo03.o rmq.o rmqcbl.o -lrabbitmq -lpthread

demo04: 	demo04.cbl rmq.o rmqcbl.o
		cobc -free -c -x demo04.cbl
		cobc -x -o demo04 demo04.o rmq.o rmqcbl.o -lrabbitmq -lpthread


demo05: 	demo05.cbl rmq.o rmqcbl.o
		cobc -free -c -x demo05.cbl
		cobc -x -o demo05 demo05.o rmq.o rmqcbl.o -lrabbitmq -lpthread

func1: 		func1.cbl 
		cobc -free -fimplicit-init -fstatic-call -m func1.cbl

func2: 		func2.cbl 
		cobc -free -fimplicit-init -fstatic-call -m func2.cbl

clean:
		rm -f *.o 
		rm -f demo01
		rm -f demo02
		rm -f demo03
		rm -f demo04
		rm -f demo05
		rm -f func1.so
		rm -f func2.so

