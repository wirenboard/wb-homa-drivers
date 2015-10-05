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
CPPFLAGS=$(CFLAGS)
LDFLAGS= -lmosquittopp -lmosquitto -ljsoncpp -lwbmqtt -lsqlite3

DB_BIN=wb-mqtt-db
SQLITECPP_DIR=SQLiteCpp
SQLITECPP_OBJ := $(patsubst %.cpp,%.o,$(wildcard $(SQLITECPP_DIR)/*.cpp))


.PHONY: all clean

all : $(DB_BIN)


$(DB_BIN): main.o $(SQLITECPP_OBJ)
	${CXX} $^ ${LDFLAGS} -o $@

main.o: main.cpp
	${CXX} -c $< -o $@ ${CFLAGS};




clean :
	-rm -f *.o $(DB_BIN)
	-rm -f $(SQLITECPP_DIR)/*.o



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib
	install -d $(DESTDIR)/var/lib/wirenboard
	install -d $(DESTDIR)/var/lib/wirenboard/db

	install -m 0755  $(DB_BIN) $(DESTDIR)/usr/bin/$(DB_BIN)
	install -m 0755  config.json $(DESTDIR)/etc/wb-mqtt-db.conf


