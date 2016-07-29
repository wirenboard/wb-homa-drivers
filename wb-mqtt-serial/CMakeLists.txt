cmake_minimum_required(VERSION 3.5)

project(wb_mqtt_serial)

#include_directories(${PROJECT_SOURCE_DIR}
#        /Users/ssko/src/googletest/googlemock/include
#        /Users/ssko/src/googletest/googletest/include)
include_directories(SYSTEM /Users/ssko/src/usr/include)
include_directories(${PROJECT_SOURCE_DIR})
link_directories(/Users/ssko/src/usr/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
#set(CMLINF)
file(GLOB MAIN_SOURCES ./*.cpp)
file(GLOB MAIN_HEADERS ./*.h ./*.hpp)
source_group(MAIN_SRC FILES ${MAIN_SOURCES} ${MAIN_HEADERS})

file(GLOB CONFIGS ./*.json)
source_group(CONFIGS_SRC FILES ${CONFIGS})

file(GLOB SERIAL_TEMPLATES wb-mqtt-serial-templates/*.json)
source_group(TEMPLATE_SRC FILES ${SERIAL_TEMPLATES})

file(GLOB TEST_SOURCES test/*.cpp test/*.h)

set(SOURCE_FILES ${MAIN_SOURCES} ${MAIN_HEADERS})

add_executable(wb_mqtt_serial ${SOURCE_FILES})
target_link_libraries(wb_mqtt_serial
        modbus
        jsoncpp
        wbmqtt.0.1
        mosquitto
        mosquittopp)