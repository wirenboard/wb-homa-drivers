//
// Created by Sergey Skorokhodov on 08.09.16.
//

#ifndef WB_MQTT_SERIAL_BCD_UTILS_H
#define WB_MQTT_SERIAL_BCD_UTILS_H

#include <cstdint>
enum BCDSizes
{
    BCD32_SZ = 4, BCD24_SZ = 3, BCD16_SZ = 2
};

// alignment independent cast of up to four BCD bytes to uint32_t
uint32_t pack_bcd(uint8_t* ps, BCDSizes how_many);

#endif //WB_MQTT_SERIAL_BCD_UTILS_H
