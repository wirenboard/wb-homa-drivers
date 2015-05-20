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

W1_BIN=wb-homa-w1

.PHONY: all clean

all : $(W1_BIN)

# W1
W1_H=sysfs_w1.h

main.o : main.cpp $(W1_H)
	${CXX} -c $< -o $@ ${CFLAGS}

sysfs_w1.o : sysfs_w1.cpp $(W1_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(W1_BIN) : main.o sysfs_w1.o
	${CXX} $^ ${LDFLAGS} -o $@


clean :
	-rm -f *.o $(W1_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib

	install -m 0755  $(W1_BIN) $(DESTDIR)/usr/bin/$(W1_BIN)
