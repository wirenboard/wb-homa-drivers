#CFLAGS=-Wall -ggdb -std=c++0x
CFLAGS=-Wall -std=c++0x -Os -I.
LDFLAGS= -lmosquittopp -ljsoncpp

COMMON_DIR=common

GPIO_DIR=wb-homa-gpio
GPIO_BIN=wb-homa-gpio
W1_DIR=wb-homa-w1
W1_BIN=wb-homa-w1

COMMON_H=$(COMMON_DIR)/utils.h $(COMMON_DIR)/mqtt_wrapper.h
COMMON_O=$(COMMON_DIR)/mqtt_wrapper.o $(COMMON_DIR)/utils.o

.PHONY: all clean


all : $(GPIO_DIR)/$(GPIO_BIN) $(W1_DIR)/$(W1_BIN)

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


clean :
	-rm -f $(COMMON_DIR)/*.o
	-rm -f $(GPIO_DIR)/*.o $(GPIO_DIR)/$(GPIO_BIN)
	-rm -f $(W1_DIR)/*.o $(W1_DIR)/$(W1_BIN)
