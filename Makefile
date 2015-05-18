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


GPIO_DIR=wb-homa-gpio
GPIO_BIN=wb-homa-gpio
MODBUS_DIR=wb-homa-modbus
MODBUS_BIN=wb-homa-modbus
MODBUS_LIBS=-lmodbus
MODBUS_OBJS=$(MODBUS_DIR)/modbus_client.o \
  $(MODBUS_DIR)/modbus_config.o $(MODBUS_DIR)/modbus_port.o \
  $(MODBUS_DIR)/modbus_observer.o \
  $(MODBUS_DIR)/uniel.o $(MODBUS_DIR)/uniel_context.o
W1_DIR=wb-homa-w1
W1_BIN=wb-homa-w1
ADC_DIR=wb-homa-adc
ADC_BIN=wb-homa-adc
TEST_LIBS=-lgtest -lpthread

NINJABRIDGE_DIR=wb-homa-ninja-bridge
NINJABRIDGE_BIN=wb-homa-ninja-bridge
TEST_DIR=test
TEST_BIN=wb-homa-test
LOGGER=mqtt-logger
LOGGER_BIN=mqtt-logger


.PHONY: all clean test_fix

all : $(GPIO_DIR)/$(GPIO_BIN) $(MODBUS_DIR)/$(MODBUS_BIN) $(W1_DIR)/$(W1_BIN) $(ADC_DIR)/$(ADC_BIN) $(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN) $(LOGGER)/$(LOGGER_BIN)

# GPIO
$(GPIO_DIR)/main.o : $(GPIO_DIR)/main.cpp 
	${CXX} -c $< -o $@ ${CFLAGS}

$(GPIO_DIR)/sysfs_gpio.o : $(GPIO_DIR)/sysfs_gpio.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(GPIO_DIR)/sysfs_gpio_base_counter.o : $(GPIO_DIR)/sysfs_gpio_base_counter.cpp 
	${CXX} -c $< -o $@ ${CFLAGS}

$(GPIO_DIR)/$(GPIO_BIN) : $(GPIO_DIR)/main.o $(GPIO_DIR)/sysfs_gpio.o $(GPIO_DIR)/sysfs_gpio_base_counter.o
	${CXX} $^ ${LDFLAGS} -o $@

# Modbus
$(MODBUS_DIR)/main.o : $(MODBUS_DIR)/main.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/modbus_client.o : $(MODBUS_DIR)/modbus_client.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/modbus_config.o : $(MODBUS_DIR)/modbus_config.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/modbus_port.o : $(MODBUS_DIR)/modbus_port.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/modbus_observer.o : $(MODBUS_DIR)/modbus_observer.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/uniel.o : $(MODBUS_DIR)/uniel.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/uniel_context.o : $(MODBUS_DIR)/uniel_context.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(MODBUS_DIR)/$(MODBUS_BIN) : $(MODBUS_DIR)/main.o $(MODBUS_OBJS)
	${CXX} $^ ${LDFLAGS} -o $@ $(MODBUS_LIBS)

# W1
W1_H=$(W1_DIR)/sysfs_w1.h

$(W1_DIR)/main.o : $(W1_DIR)/main.cpp $(W1_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(W1_DIR)/sysfs_w1.o : $(W1_DIR)/sysfs_w1.cpp $(W1_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(W1_DIR)/$(W1_BIN) : $(W1_DIR)/main.o $(W1_DIR)/sysfs_w1.o
	${CXX} $^ ${LDFLAGS} -o $@

# ADC
ADC_H=$(ADC_DIR)/sysfs_adc.h $(ADC_DIR)/adc_handler.h

$(ADC_DIR)/main.o : $(ADC_DIR)/main.cpp $(ADC_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(ADC_DIR)/adc_handler.o : $(ADC_DIR)/adc_handler.cpp $(ADC_H)
	${CXX} -c $< -o $@ ${CFLAGS}
$(ADC_DIR)/sysfs_adc.o : $(ADC_DIR)/sysfs_adc.cpp $(ADC_H)
	${CXX} -c $< -o $@ ${CFLAGS}
$(ADC_DIR)/imx233.o : $(ADC_DIR)/imx233.c
	${CC} -c $< -o $@

$(ADC_DIR)/$(ADC_BIN) : $(ADC_DIR)/main.o $(ADC_DIR)/sysfs_adc.o $(ADC_DIR)/adc_handler.o $(ADC_DIR)/imx233.o
	${CXX} $^ ${LDFLAGS} -o $@

# Ninja blocks bridge
NINJABRIDGE_H=$(NINJABRIDGE_DIR)/cloud_connection.h $(NINJABRIDGE_DIR)/local_connection.h
NINJABRIDGE_LDFLAGS= -lcurl

$(NINJABRIDGE_DIR)/main.o : $(NINJABRIDGE_DIR)/main.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(NINJABRIDGE_DIR)/cloud_connection.o : $(NINJABRIDGE_DIR)/cloud_connection.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}

$(NINJABRIDGE_DIR)/local_connection.o : $(NINJABRIDGE_DIR)/local_connection.cpp $(NINJABRIDGE_H)
	${CXX} -c $< -o $@ ${CFLAGS}


$(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN) : $(NINJABRIDGE_DIR)/main.o $(NINJABRIDGE_DIR)/cloud_connection.o $(NINJABRIDGE_DIR)/local_connection.o
	${CXX} $^ ${LDFLAGS} ${NINJABRIDGE_LDFLAGS} -o $@

$(TEST_DIR)/testlog.o: $(TEST_DIR)/testlog.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(TEST_DIR)/modbus_test.o: $(TEST_DIR)/modbus_test.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(TEST_DIR)/fake_modbus.o: $(TEST_DIR)/fake_modbus.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(TEST_DIR)/fake_mqtt.o: $(TEST_DIR)/fake_mqtt.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(TEST_DIR)/main.o: $(TEST_DIR)/main.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(TEST_DIR)/$(TEST_BIN): $(MODBUS_OBJS) \
  $(TEST_DIR)/testlog.o $(TEST_DIR)/modbus_test.o $(TEST_DIR)/fake_modbus.o \
  $(TEST_DIR)/fake_mqtt.o $(TEST_DIR)/main.o
	${CXX} $^ ${LDFLAGS} -o $@ $(TEST_LIBS) $(MODBUS_LIBS)

#mqtt-logger
$(LOGGER)/$(LOGGER_BIN): $(LOGGER)/main.o
	${CXX} $^ ${LDFLAGS} -o $@
$(LOGGER)/main.o: $(LOGGER)/main.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

test_fix: $(TEST_DIR)/$(TEST_BIN)
	valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_BIN) || \
          if [ $$? = 180 ]; then \
            echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
            exit 1; \
          else $(TEST_DIR)/abt.sh show; exit 1; fi

clean :
	-rm -f $(GPIO_DIR)/*.o $(GPIO_DIR)/$(GPIO_BIN)
	-rm -f $(MODBUS_DIR)/*.o $(MODBUS_DIR)/$(MODBUS_BIN)
	-rm -f $(W1_DIR)/*.o $(W1_DIR)/$(W1_BIN)
	-rm -f $(ADC_DIR)/*.o $(ADC_DIR)/$(ADC_BIN)
	-rm -f $(NINJABRIDGE_DIR)/*.o $(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN)
	-rm -f $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_BIN)
	-rm -f $(LOGGER)/*.o $(LOGGER)/$(LOGGER_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib
	install -d $(DESTDIR)/usr/share/wb-homa-modbus
	install -d $(DESTDIR)/usr/share/wb-homa-gpio
	install -d $(DESTDIR)/usr/share/wb-homa-adc

	install -m 0644  $(GPIO_DIR)/config.json.wbsh4 $(DESTDIR)/usr/share/wb-homa-gpio/wb-homa-gpio.conf.wbsh4
	install -m 0644  $(GPIO_DIR)/config.json.wbsh3 $(DESTDIR)/usr/share/wb-homa-gpio/wb-homa-gpio.conf.wbsh3
	install -m 0644  $(GPIO_DIR)/config.json.default $(DESTDIR)/usr/share/wb-homa-gpio/wb-homa-gpio.conf.default
	install -m 0644  $(GPIO_DIR)/config.json.mka3 $(DESTDIR)/usr/share/wb-homa-gpio/wb-homa-gpio.conf.mka3
	install -m 0755  $(GPIO_DIR)/$(GPIO_BIN) $(DESTDIR)/usr/bin/$(GPIO_BIN)
	install -m 0644  $(MODBUS_DIR)/config.json $(DESTDIR)/etc/wb-homa-modbus.conf.sample
	install -m 0755  $(MODBUS_DIR)/$(MODBUS_BIN) $(DESTDIR)/usr/bin/$(MODBUS_BIN)
	cp -r  $(MODBUS_DIR)/wb-homa-modbus-templates $(DESTDIR)/usr/share/wb-homa-modbus/templates
	install -m 0755  $(W1_DIR)/$(W1_BIN) $(DESTDIR)/usr/bin/$(W1_BIN)
	install -m 0755  $(ADC_DIR)/$(ADC_BIN) $(DESTDIR)/usr/bin/$(ADC_BIN)
	install -m 0644  $(ADC_DIR)/config.json $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.default
	install -m 0644  $(ADC_DIR)/config.json.wb4 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb4
	install -m 0644  $(ADC_DIR)/config.json.wb2.8 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb2.8
	install -m 0644  $(ADC_DIR)/config.json.wb3.5 $(DESTDIR)/usr/share/wb-homa-adc/wb-homa-adc.conf.wb3.5
	install -m 0755  $(NINJABRIDGE_DIR)/$(NINJABRIDGE_BIN) $(DESTDIR)/usr/bin/$(NINJABRIDGE_BIN)

	install -m 0755  $(LOGGER)/$(LOGGER_BIN) $(DESTDIR)/usr/bin/$(LOGGER_BIN)
