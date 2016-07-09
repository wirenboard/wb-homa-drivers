#include <iostream>
#include "mercury200_02_device.h"
#include "crc16.h"

REGISTER_PROTOCOL("mercury200", TMercury20002Device, TRegisterTypes({
            { TMercury20002Device::REG_ENERGY_VALUE, "enegry", "power_consumption", U32, true },
            { TMercury20002Device::REG_PARAM_VALUE, "param", "value", U32, true }
        }));

TMercury20002Device::TMercury20002Device(PDeviceConfig device_config, PAbstractSerialPort port)
    : TEMDevice(device_config, port) {}

bool TMercury20002Device::ConnectionSetup(uint8_t slave)
{
    // there is nothing needed to be done in this method
    return true;
}

TEMDevice::ErrorType TMercury20002Device::CheckForException(uint8_t* frame, int len, const char** message)
{
    *message = 0;
    if (len != 4 || (frame[1] & 0x0f) == 0)
        return TEMDevice::NO_ERROR;
    switch (frame[1] & 0x0f) {
    case 1:
        *message = "Invalid command or parameter";
        break;
    case 2:
        *message = "Internal meter error";
        break;
    case 3:
        *message = "Insufficient access level";
        break;
    case 4:
        *message = "Can't correct the clock more than once per day";
        break;
    case 5:
        *message = "Connection closed";
        return TEMDevice::NO_OPEN_SESSION;
    default:
        *message = "Unknown error";
    }
    return TEMDevice::OTHER_ERROR;
}

const TMercury20002Device::TEnergyValues& TMercury20002Device::ReadEnergyValues(uint32_t slave, uint32_t address)
{
    auto it = EnergyCache.find(slave);
    if (it != EnergyCache.end()) {
        return it->second;
    }

    // durty hack
    uint8_t cmdBuf[3] = { 0x00, 0x00, 0x00 };
    uint8_t actualSlave = 0x00U;
    uint8_t actualCmd = (slave >> 16) & 0xffU;
    cmdBuf[0] = (slave >> 8) & 0xffU;
    cmdBuf[1] = slave & 0xffU;
    cmdBuf[2] = 0x26U;
    uint8_t buf[MAX_LEN], *p = buf;
    Talk(actualSlave, actualCmd, cmdBuf, 3, -1, buf, 16);
    TEnergyValues a;
    for (int i = 0; i < 4; i++, p += 4) {
        a.values[i] = ((uint32_t)p[1] << 24) +
                      ((uint32_t)p[0] << 16) +
                      ((uint32_t)p[3] << 8 ) +
                       (uint32_t)p[2];
    }

    return EnergyCache.insert(std::make_pair(slave, a)).first->second;
}

const TMercury20002Device::TParamValues& TMercury20002Device::ReadParamValues(uint32_t slave, uint32_t address)
{
    auto it = ParamCache.find(slave);
    if(it != ParamCache.end()) {
        return it->second;
    }

    // durty hack
    uint8_t cmdBuf[3] = { 0x00, 0x00, 0x00 };
    uint8_t actualSlave = 0x00U;
    uint8_t actualCmd = (slave >> 16) & 0xffU;
    cmdBuf[0] = (slave >> 8) & 0xffU;
    cmdBuf[1] = slave & 0xffU;
    cmdBuf[2] = 0x63U;
    uint8_t buf[MAX_LEN], *p = buf;
    Talk(actualSlave, actualCmd, cmdBuf, 3, -1, buf, 7);
    TParamValues a;
    a.values[0] = ((uint32_t)buf[0] >> 8) + (uint32_t)buf[1];
    a.values[1] = ((uint32_t)buf[2] >> 8) + (uint32_t)buf[3];
    a.values[2] = ((uint32_t)p[5] << 16) +
                  ((uint32_t)p[4] << 8 ) +
                   (uint32_t)p[6];
    return ParamCache.insert(std::make_pair(slave, a)).first->second;
}

uint64_t TMercury20002Device::ReadRegister(PRegister reg)
{
    switch (reg->Type) {
    case REG_ENERGY_VALUE:
        return ReadEnergyValues(reg->Slave->Id, reg->Address).values[reg->Address & 0x03];
    case REG_PARAM_VALUE:
        return ReadParamValues(reg->Slave->Id, reg->Address).values[reg->Address & 0x03];
    default:
        throw TSerialDeviceException("mercury200.02: invalid register type");
    }
}

void TMercury20002Device::EndPollCycle()
{
    EnergyCache.clear();
    ParamCache.clear();
}
