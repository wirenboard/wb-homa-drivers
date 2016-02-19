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
CFLAGS=-Wall -std=c++0x -std=c++11 -Os -I.
LDFLAGS= -lmosquittopp -lmosquitto -ljsoncpp -lwbmqtt

ADC_BIN=wb-homa-adc


.PHONY: all clean

all : $(ADC_BIN)


# ADC
ADC_H=sysfs_adc.h adc_handler.h lradc_isrc.h

main.o : main.cpp $(ADC_H)
	${CXX} -c $< -o $@ ${CFLAGS}

adc_handler.o : adc_handler.cpp $(ADC_H)
	${CXX} -c $< -o $@ ${CFLAGS}
sysfs_adc.o : sysfs_adc.cpp $(ADC_H)
	${CXX} -c $< -o $@ ${CFLAGS}
imx233.o : imx233.c
	${CC} -c $< -o $@
lradc_isrc.o : lradc_isrc.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(ADC_BIN) : main.o sysfs_adc.o adc_handler.o imx233.o lradc_isrc.o
	${CXX} $^ ${LDFLAGS} -o $@

clean :
	-rm -f *.o $(ADC_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib
	install -d $(DESTDIR)/usr/share/wb-homa-adc
	mkdir -p $(DESTDIR)/etc/wb-configs.d

	install -m 0755  $(ADC_BIN) $(DESTDIR)/usr/bin/$(ADC_BIN)
	install -m 0644  config.json $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.default
	install -m 0644  config.json.wb4 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb4
	install -m 0644  config.json.wb5 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb5
	install -m 0644  config.json.wb55 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb55
	install -m 0644  config.json.wb2.8 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb2.8
	install -m 0644  config.json.wb3.5 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb3.5
	install -m 0644  config.json.netmon1 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.netmon1

	install -m 0644  wb-homa-adc.wbconfigs $(DESTDIR)/etc/wb-configs.d/12wb-homa-adc

	install -m 0644  wb-homa-adc.schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/wb-homa-adc.schema.json
