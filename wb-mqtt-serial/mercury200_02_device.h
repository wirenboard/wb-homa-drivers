#pragma once

#include <cstdint>
#include <cstddef>

#include <list>
#include <array>
#include <string>
#include <memory>
#include <exception>
#include <unordered_map>

#include "serial_device.h"

class TMercury20002Device : public TSerialDevice
{
public:
    enum RegisterType
    {
        REG_ENERGY_VALUE = 0,
        REG_PARAM_VALUE16 = 1,
        REG_PARAM_VALUE24 = 2,
    };

    TMercury20002Device(PDeviceConfig config, PAbstractSerialPort port);
    virtual ~TMercury20002Device();
    virtual uint64_t ReadRegister(PRegister reg);
    virtual void WriteRegister(PRegister reg, uint64_t value);
    virtual void EndPollCycle();

private:
    struct TEnergyValues
    {
        uint32_t values[4];
    };
    const TMercury20002Device::TEnergyValues& ReadEnergyValues(uint32_t slave);

    struct TParamValues
    {
        uint32_t values[3];
    };
    const TMercury20002Device::TParamValues& ReadParamValues(uint32_t slave);
    // buf expected to be 7 bytes long
    void FillCommand(uint8_t* buf, uint32_t id, uint8_t cmd) const;
    int RequestResponse(uint32_t slave, uint8_t cmd, uint8_t* response) const;
    bool BadHeader(uint32_t slave_expected, uint8_t cmd_expected, uint8_t* response) const;

    bool CRCInvalid(uint8_t* buf, int sz) const;

    std::unordered_map<uint32_t, TEnergyValues> EnergyCache;
    std::unordered_map<uint32_t, TParamValues> ParamCache;
};

typedef std::shared_ptr<TMercury20002Device> PMercury20002Device;
