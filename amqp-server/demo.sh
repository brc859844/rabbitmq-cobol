#!/bin/sh

./amqp-server -d -U guest -P guest -h 10.10.116.196 -l ./uars.so -q JUNK -s SVC1:my_svc1 
