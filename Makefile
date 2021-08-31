# BRC 12-Apr-2012
#

default: 	all


all: 		librmq server 


librmq: 
		(cd rmq; make clean; make)

server:
		(cd amqp-server; make clean; make)


clean: 		
		(cd rmq; make clean)
		(cd amqp-server; make clean)

