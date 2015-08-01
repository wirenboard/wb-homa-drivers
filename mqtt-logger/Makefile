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

LOGGER_BIN=mqtt-logger

.PHONY: all clean

all : $(LOGGER_BIN)


#mqtt-logger
$(LOGGER_BIN): main.o
	${CXX} $^ ${LDFLAGS} -o $@
main.o: main.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

clean :
	-rm -f *.o $(LOGGER_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib

	install -m 0755  $(LOGGER_BIN) $(DESTDIR)/usr/bin/$(LOGGER_BIN)
