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

OBJ=main.o dbinit.o dbmqtt.o db_rpc.o dbtimer.o
DB_CONFCONVERT=wb-mqtt-db-confconvert

.PHONY: all clean

all : $(DB_BIN)


$(DB_BIN): $(OBJ) $(SQLITECPP_OBJ)
	${CXX} $^ ${LDFLAGS} -o $@

%.o: %.cpp
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
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed/schemas

	install -m 0755  $(DB_BIN) $(DESTDIR)/usr/bin/$(DB_BIN)
	install -m 0755  $(DB_CONFCONVERT) $(DESTDIR)/usr/bin/$(DB_CONFCONVERT)
	install -m 0755  config.json $(DESTDIR)/etc/wb-mqtt-db.conf

	install -m 0644  wb-mqtt-db.wbconfigs $(DESTDIR)/etc/wb-configs.d/16wb-mqtt-db
	install -m 0644  wb-mqtt-db.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-mqtt-db.schema.json

