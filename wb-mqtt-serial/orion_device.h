#pragma once

#include <string>
#include <memory>
#include <exception>
#include <stdint.h>

#include "serial_device.h"

class TOrionDevice: public TBasicProtocolSerialDevice<TBasicProtocol<TOrionDevice>> {
public:
    enum RegisterType
    {
        REG_RELAY = 0,
        REG_RELAY_MULTI,
        REG_RELAY_DEFAULT,
        REG_RELAY_DELAY,
    };

    TOrionDevice(PDeviceConfig config, PAbstractSerialPort port, PProtocol protocol);
    virtual uint64_t ReadRegister(PRegister reg);
    virtual void WriteRegister(PRegister reg, uint64_t value);

private:
    uint8_t CrcOrion(const uint8_t *array, int size);
    static uint8_t CrcTable[];
    uint8_t RelayState[5];
};

typedef std::shared_ptr<TOrionDevice> POrionDevice;
