# CSCE 438: Distributed objects programming
# HW1: A Chat Room Service
# 
# makefile
# Vincent Velarde, Kyle Rowland
# January 2017
#
# NOTES:
#  "make" to compile server and client programs
#  "clean" to remove server and client programs
#

all: server client

client: crc.cpp
	g++ -o client  -g -w -Wall -O1 -std=c++11 crc.cpp -pthread

server: crsc.cpp
	g++ -o server   -g -w -Wall -O1 -std=c++11  crsc.cpp  -pthread

clean:
	\rm server client *.o
