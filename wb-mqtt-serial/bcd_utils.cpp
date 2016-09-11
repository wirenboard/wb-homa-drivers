#include <cstddef>
#include <cassert>
#include "bcd_utils.h"

#if defined(__ORDER_LITTLE_ENDIAN__)

// Placing BCD bytes to little endian integer in a way it reads
// the same as original byte sequence:
// bytes: {0x12, 0x34, 0x56, 0x78}
// integer value: 0x12345678
// actual bytes in integer: 0x78563412
uint32_t PackBCD(uint8_t* bytes, BCDSizes size)
{
    uint32_t ret = 0U;
    switch (size) {
    case BCDSizes::BCD32_SZ:
        ret |= (*bytes++ << 24);
    case BCDSizes::BCD24_SZ:
        ret |= (*bytes++ << 16);
    case BCDSizes::BCD16_SZ:
        ret |= (*bytes++ << 8);
    case BCDSizes::BCD8_SZ:
        ret |= (*bytes << 0);
    }
    return ret;
}

#elif defined(__ORDER_BIG_ENDIAN__)
// Placing BCD bytes to big endian integer in a way it reads
// the same as original byte sequence:
// bytes: {0x12, 0x34, 0x56, 0x78}
// integer value: 0x12345678
// actual bytes in integer: 0x12345678
uint32_t PackBCD(uint8_t *ps, BCDSizes size)
{
    uint32_t ret = 0U;
    switch(size){
    case BCDSizes::BCD32_SZ:
        ret |= (*bytes++ << 0);
    case BCDSizes::BCD24_SZ:
        ret |= (*bytes++ << 8);
    case BCDSizes::BCD16_SZ:
        ret |= (*bytes++ << 16);
    case BCDSizes::BCD8_SZ:
        ret |= (*bytes << 24);
    }
    return ret;
}
#else
#   error Neither __ORDER_LITTLE_ENDIAN__ or __ORDER_BIG_ENDIAN__ defined!
#endif

#if defined(__ORDER_LITTLE_ENDIAN__)

uint64_t PackedBCD2Int(uint64_t packed, BCDSizes size)
{
    int exp = 1;
    uint32_t result = 0;
    auto p = reinterpret_cast<uint8_t*>(&packed);
    uint8_t* limit = p + size;
    while (p < limit) {
        result += (*p & 0x0f) * exp;
        exp *= 10;
        result += ((*p >> 4) * exp);
        exp *= 10;
        ++p;
    }
    return result;
}

#elif defined(__ORDER_BIG_ENDIAN__)

uint64_t PackedBCD2Int(uint64_t packed, BCDSizes size)
{
  auto exp = 1;
  uint32_t result = 0U;
  auto p = reinterpret_cast<uint8_t *>(&packed);
  uint8_t *limit = p + (sizeof(v) - size - 1);
  p += (sizeof(v) - 1);
  while (p > limit) {
    result += (*p & 0x0f) * exp;
    exp *= 10;
    result += ((*p >> 4) * exp);
    exp *= 10;
    --p;
  }
  return result;
}

#else
#   error Neither __ORDER_LITTLE_ENDIAN__ or __ORDER_BIG_ENDIAN__ defined!
#endif
