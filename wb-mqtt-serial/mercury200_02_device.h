#pragma once

#include <list>
#include <vector>
#include <string>
#include <memory>
#include <exception>
#include <unordered_map>
#include <cstdint>

#include "em_device.h"

class TMercury20002Device: public TEMDevice {
public:
    static const int DefaultTimeoutMs = 1000;
    enum RegisterType {
        REG_VALUE_ARRAY = 0,
        REG_PARAM = 1
    };

    TMercury20002Device(PDeviceConfig, PAbstractSerialPort port);
    uint64_t ReadRegister(PRegister reg);
    void EndPollCycle();

protected:
    virtual bool ConnectionSetup(uint8_t slave);
    virtual ErrorType CheckForException(uint8_t* frame, int len, const char** message);

private:
    struct TValueArray {
        uint32_t values[4];
    };
    enum ParamSubtype {
        Voltage = 0,
        Current = 1,
        Power = 2,
    };
    const TValueArray& ReadValueArray(uint32_t slave, uint32_t address);
    uint32_t ReadParam(uint32_t slave, uint32_t address);

};

typedef std::shared_ptr<TMercury20002Device> PMercury20002Device;
