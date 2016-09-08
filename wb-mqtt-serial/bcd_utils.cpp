//
// Created by Sergey Skorokhodov on 08.09.16.
//

#include <cstddef>
#include "bcd_utils.h"

uint32_t pack_bcd(uint8_t *ps, BCDSizes how_many)
{
    uint32_t ret = 0U;
    auto start = sizeof(ret) - how_many;
    uint8_t *pd = reinterpret_cast<uint8_t *>(&ret);
    for (size_t i = start, j = 0UL; i < sizeof(ret); ++i, ++j) {
        pd[i] = ps[j];
    }
    return ret;
}


