//
// Created by Sergey Skorokhodov on 08.09.16.
//

#include <cstddef>
#include <cassert>
#include "bcd_utils.h"

uint32_t PackBCD(uint8_t *ps, BCDSizes how_many)
{
    uint32_t ret = 0U;
    auto start = sizeof(ret) - how_many;
    uint8_t *pd = reinterpret_cast<uint8_t *>(&ret);
    for (size_t i = start, j = 0UL; i < sizeof(ret); ++i, ++j) {
        pd[i] = ps[j];
    }
    return ret;
}

std::string PackedBCD2String(uint64_t packed, BCDSizes size)
{
    assert(size <= 4);
    int coeff = 1;
    uint32_t r = 0U;
    uint8_t *p = reinterpret_cast<uint8_t *>(&packed) + sizeof(packed) - 1;
    uint8_t *l = p - size;
    while (p >= l) {
        r += (*p & 0x0f) * coeff;
        coeff *= 10;
        r += ((*p >> 4) * coeff);
        coeff *= 10;
        --p;
    }
    return std::to_string(r);
}
