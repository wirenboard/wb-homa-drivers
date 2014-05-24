CXX=g++
CXX_PATH := $(shell which g++-4.7)

ifneq ($(CXX_PATH),)
	CXX=g++-4.7
endif


#CFLAGS=-Wall -ggdb -std=c++0x -O0 -I.
CFLAGS=-Wall -std=c++0x -Os -I.
LDFLAGS= -lmosquittopp -lmosquitto -ljsoncpp

COMMON_DIR=common

GPIO_DIR=wb-homa-gpio
GPIO_BIN=wb-homa-gpio
W1_DIR=wb-homa-w1
W1_BIN=wb-homa-w1

NINJABRIDGE_DIR=wb-homa-ninja-bridge
NINJABRIDGE_BIN=wb-homa-ninja-bridge


COMMON_H=$(COMMON_DIR)/utils.h $(COMMON_DIR)/mqtt_wrapper.h
COMMON_O=$(COMMON_DIR)/mqtt_wrapper.o $(COMMON_DIR)/utils.o

.PHONY: all clean


all : $(GPIO_DIR)/$(GPIO_BIN) $(W1_DIR)/$(W1_BIN) $(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN)

$(COMMON_DIR)/utils.o : $(COMMON_DIR)/utils.cpp $(COMMON_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(COMMON_DIR)/mqtt_wrapper.o : $(COMMON_DIR)/mqtt_wrapper.cpp $(COMMON_H)
	${CXX} -c $< -o $@ ${CFLAGS}


# GPIO
$(GPIO_DIR)/main.o : $(GPIO_DIR)/main.cpp $(COMMON_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(GPIO_DIR)/sysfs_gpio.o : $(GPIO_DIR)/sysfs_gpio.cpp $(COMMON_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(GPIO_DIR)/$(GPIO_BIN) : $(GPIO_DIR)/main.o $(GPIO_DIR)/sysfs_gpio.o  $(COMMON_O)
	${CXX} $^ ${LDFLAGS} -o $@

# W1
W1_H=$(W1_DIR)/sysfs_w1.h

$(W1_DIR)/main.o : $(W1_DIR)/main.cpp $(COMMON_H) $(W1_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(W1_DIR)/sysfs_w1.o : $(W1_DIR)/sysfs_w1.cpp $(COMMON_H) $(W1_H)
	${CXX} -c $< -o $@ ${CFLAGS}


$(W1_DIR)/$(W1_BIN) : $(W1_DIR)/main.o $(W1_DIR)/sysfs_w1.o $(COMMON_O)
	${CXX} $^ ${LDFLAGS} -o $@

# Ninja blocks bridge
NINJABRIDGE_H=$(NINJABRIDGE_DIR)/http_helper.h $(NINJABRIDGE_DIR)/cloud_connection.h $(NINJABRIDGE_DIR)/local_connection.h
NINJABRIDGE_LDFLAGS= -lcurl

$(NINJABRIDGE_DIR)/main.o : $(NINJABRIDGE_DIR)/main.cpp $(COMMON_H) $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(NINJABRIDGE_DIR)/http_helper.o : $(NINJABRIDGE_DIR)/http_helper.cpp $(COMMON_H) $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(NINJABRIDGE_DIR)/cloud_connection.o : $(NINJABRIDGE_DIR)/cloud_connection.cpp $(COMMON_H) $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(NINJABRIDGE_DIR)/local_connection.o : $(NINJABRIDGE_DIR)/local_connection.cpp $(COMMON_H) $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}


$(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN) : $(NINJABRIDGE_DIR)/main.o  $(NINJABRIDGE_DIR)/http_helper.o $(NINJABRIDGE_DIR)/cloud_connection.o $(NINJABRIDGE_DIR)/local_connection.o $(COMMON_O)
	${CXX} $^ ${LDFLAGS} ${NINJABRIDGE_LDFLAGS} -o $@



clean :
	-rm -f $(COMMON_DIR)/*.o
	-rm -f $(GPIO_DIR)/*.o $(GPIO_DIR)/$(GPIO_BIN)
	-rm -f $(W1_DIR)/*.o $(W1_DIR)/$(W1_BIN)
	-rm -f $(NINJABRIDGE_DIR)/*.o $(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN)
