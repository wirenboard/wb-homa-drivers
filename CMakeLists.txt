cmake_minimum_required(VERSION 3.5)

project(wb_mqtt_serial)

#include_directories(${PROJECT_SOURCE_DIR}
#        /Users/ssko/src/googletest/googlemock/include
#        /Users/ssko/src/googletest/googletest/include)
include_directories(SYSTEM /Users/ssko/src/usr/include)
include_directories(
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/wb-mqtt-serial)
include_directories(
        /Users/ssko/src/googletest/googlemock/include
        /Users/ssko/src/googletest/googletest/include)
link_directories(/Users/ssko/src/usr/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
#set(CMLINF)
file(GLOB MAIN_SOURCES wb-mqtt-serial/register.cpp
        wb-mqtt-serial/poll_plan.cpp
        wb-mqtt-serial/serial_client.cpp
        wb-mqtt-serial/register_handler.cpp
        wb-mqtt-serial/serial_config.cpp
        wb-mqtt-serial/serial_port_driver.cpp
        wb-mqtt-serial/serial_observer.cpp
        wb-mqtt-serial/serial_port.cpp
        wb-mqtt-serial/serial_device.cpp
        wb-mqtt-serial/uniel_device.cpp
        wb-mqtt-serial/ivtm_device.cpp
        wb-mqtt-serial/crc16.cpp
        wb-mqtt-serial/modbus_device.cpp
        wb-mqtt-serial/em_device.cpp
        wb-mqtt-serial/milur_device.cpp
        wb-mqtt-serial/mercury230_device.cpp
        wb-mqtt-serial/mercury200_02_device.cpp
        wb-mqtt-serial/pulsar_device.cpp
        wb-mqtt-serial/bcd_utils.cpp)
file(GLOB MAIN_HEADERS wb-mqtt-serial/*.h wb-mqtt-serial/*.hpp)
source_group(MAIN_SRC FILES ${MAIN_SOURCES} ${MAIN_HEADERS})

file(GLOB CONFIGS wb-mqtt-serial/*.json)
source_group(CONFIGS_SRC FILES ${CONFIGS})

file(GLOB SERIAL_TEMPLATES wb-mqtt-serial-templates/*.json)
source_group(TEMPLATE_SRC FILES ${SERIAL_TEMPLATES})

file(GLOB TEST_SOURCES wb-mqtt-serial/test/*.cpp test/*.h)

set(SOURCE_FILES wb-mqtt-serial/main.cpp ${MAIN_SOURCES} ${MAIN_HEADERS})

add_executable(wb_mqtt_serial ${SOURCE_FILES})
target_link_libraries(wb_mqtt_serial
        modbus
        jsoncpp
        wbmqtt.0.1
        mosquitto
        mosquittopp)

add_executable(wb-homa-test ${SOURCE_FILES} ${TEST_SOURCES} ${MAIN_SOURCES} ${MAIN_HEADERS})
target_link_libraries(wb-homa-test
        pthread
        modbus
        jsoncpp
        wbmqtt.0.1
        mosquitto
        mosquittopp)
