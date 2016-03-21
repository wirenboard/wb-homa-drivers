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

ZWAY_BIN=wb-mqtt-zway

.PHONY: all clean

all : $(ZWAY_BIN)

# ZWAY
ZWAY_H=Razberry.h  ZWaveBase.h

%.o : %.cpp $(NINJABRIDGE_H)
	$(CXX) $(CFLAGS) -c $< -o $@

$(ZWAY_BIN) : main.o mqtt_zway.o  Razberry.o  ZWaveBase.o
	$(CXX) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean :
	-rm -f *.o $(ZWAY_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin

	install -m 0755  $(ZWAY_BIN) $(DESTDIR)/usr/bin/$(ZWAY_BIN)
	install -m 0644  config.json $(DESTDIR)/etc/wb-mqtt-zway.conf
