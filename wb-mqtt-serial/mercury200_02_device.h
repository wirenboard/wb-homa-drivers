#pragma once

#include <list>
#include <vector>
#include <string>
#include <memory>
#include <exception>
#include <unordered_map>
#include <cstdint>

#include "serial_device.h"

class TMercury20002Device : public TSerialDevice {
public:
    static const int DefaultTimeoutMs = 1000;
    enum RegisterType {
        REG_ENERGY_VALUE = 0,
        REG_PARAM_VALUE = 1
    };

    TMercury20002Device(PDeviceConfig, PAbstractSerialPort port);

    virtual ~TMercury20002Device();

    virtual uint64_t ReadRegister(PRegister reg);

    virtual void WriteRegister(PRegister reg, uint64_t value);

    virtual void EndPollCycle();

protected:
    // virtual bool ConnectionSetup(uint8_t slave);
    // virtual ErrorType CheckForException(uint8_t* frame, int len, const char** message);

private:
    struct TEnergyValues {
        uint32_t values[4];
    };

    const TEnergyValues &ReadEnergyValues(uint32_t slave, uint32_t address);

    struct TParamValues {
        uint32_t values[3];
    };

    const TParamValues &ReadParamValues(uint32_t slave, uint32_t address);

    // buf must be 7 bytes long
    void FillCommand(uint8_t *buf, uint32_t id, uint8_t cmd);

    int SendRequest(uint32_t slave, uint8_t cmd, uint8_t *response);


    std::unordered_map<uint32_t, TEnergyValues> EnergyCache;
    std::unordered_map<uint32_t, TParamValues> ParamCache;
};

typedef std::shared_ptr<TMercury20002Device> PMercury20002Device;
