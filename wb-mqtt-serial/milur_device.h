#pragma once

#include <string>
#include <memory>
#include <exception>
#include <cstdint>

#include "em_device.h"

class TMilurDevice : public TEMDevice
{
public:
    static const int DefaultTimeoutMs = 1000;
    static const int FrameTimeoutMs = 50;
    enum RegisterType
    {
        REG_PARAM = 0,
        REG_POWER,
        REG_ENERGY,
        REG_FREQ,
        REG_POWERFACTOR,
        REG32_PARAM,
        REG32_POWER,
        REG32_ENERGY,
        REG32_FREQ,
        REG32_POWERFACTOR
    };

    TMilurDevice(PDeviceConfig device_config, PAbstractSerialPort port);
    virtual ~TMilurDevice() override;
    virtual uint64_t ReadRegister(PRegister reg) override;
    virtual void Prepare() override;


protected:
    virtual bool ConnectionSetup(uint8_t slave) override;
    virtual ErrorType CheckForException(uint8_t* frame, int len, const char** message) override;
    uint64_t BuildIntVal(uint8_t* p, int sz) const;
    uint64_t BuildBCB32(uint8_t* psrc) const;
    int GetExpectedSize(int type) const;
    virtual bool ConnectionSetup32(uint32_t slave) override;
};

typedef std::shared_ptr<TMilurDevice> PMilurDevice;
