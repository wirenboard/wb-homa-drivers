ifeq ($(DEB_TARGET_ARCH),armel)
CROSS_COMPILE=arm-linux-gnueabi-
endif

CXX=$(CROSS_COMPILE)g++
CXX_PATH := $(shell which $(CROSS_COMPILE)g++-4.7)

CC=$(CROSS_COMPILE)gcc
CC_PATH := $(shell which $(CROSS_COMPILE)gcc-4.7)

ifneq ($(CXX_PATH),)
	CXX=$(CROSS_COMPILE)g++-4.7
endif

ifneq ($(CC_PATH),)
	CC=$(CROSS_COMPILE)gcc-4.7
endif

#CFLAGS=-Wall -ggdb -std=c++0x -O0 -I.
CFLAGS=-Wall -std=c++0x -Os -I.
LDFLAGS= -lmosquittopp -lmosquitto -ljsoncpp -lwbmqtt

TIME_STAMPER_BIN=wb-mqtt-timestamper


.PHONY: all clean

all : $(TIME_STAMPER_BIN)

$(TIME_STAMPER_BIN): main.o
	${CXX} $^ ${LDFLAGS} -o $@

main.o: main.cpp
	${CXX} -c $< -o $@ ${CFLAGS};

clean :
	-rm -f *.o $(TIME_STAMPER_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib

	install -m 0755  $(TIME_STAMPER_BIN) $(DESTDIR)/usr/bin/$(TIME_STAMPER_BIN)
