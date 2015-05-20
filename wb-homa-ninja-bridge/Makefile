CXX=g++
CXX_PATH := $(shell which g++-4.7)

CC=gcc
CC_PATH := $(shell which gcc-4.7)

ifneq ($(CXX_PATH),)
	CXX=g++-4.7
endif

ifneq ($(CC_PATH),)
	CC=gcc-4.7
endif

#CFLAGS=-Wall -ggdb -std=c++0x -O0 -I.
CFLAGS=-Wall -std=c++0x -Os -I.
LDFLAGS= -lmosquittopp -lmosquitto -ljsoncpp -lwbmqtt

NINJABRIDGE_BIN=wb-homa-ninja-bridge

.PHONY: all clean

all : $(NINJABRIDGE_BIN)


# Ninja blocks bridge
NINJABRIDGE_H=http_helper.h cloud_connection.h local_connection.h
NINJABRIDGE_LDFLAGS= -lcurl

main.o : main.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

http_helper.o : http_helper.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

cloud_connection.o : cloud_connection.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

local_connection.o : local_connection.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}


$(NINJABRIDGE_BIN) : main.o  http_helper.o cloud_connection.o local_connection.o
	${CXX} $^ ${LDFLAGS} ${NINJABRIDGE_LDFLAGS} -o $@


clean :
	-rm -f *.o $(NINJABRIDGE_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib

	install -m 0755  $(NINJABRIDGE_BIN) $(DESTDIR)/usr/bin/$(NINJABRIDGE_BIN)
