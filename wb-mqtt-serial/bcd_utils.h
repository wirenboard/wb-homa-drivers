//
// Created by Sergey Skorokhodov on 08.09.16.
//

#ifndef WB_MQTT_SERIAL_BCD_UTILS_H
#define WB_MQTT_SERIAL_BCD_UTILS_H

#include <cstdint>
#include <string>

enum BCDSizes
{
    BCD8_SZ = 1, BCD16_SZ = 2, BCD24_SZ = 3, BCD32_SZ = 4,
};

// alignment independent cast of up to four BCD bytes to uint32_t
uint32_t PackBCD(uint8_t *ps, BCDSizes how_many);
std::string PackedBCD2String(uint64_t packed, BCDSizes size);

#endif //WB_MQTT_SERIAL_BCD_UTILS_H
