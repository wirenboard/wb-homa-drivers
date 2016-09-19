#include <cstring>

#include "em_device.h"
#include "crc16.h"

TEMDevice::TEMDevice(PDeviceConfig device_config, PAbstractSerialPort port)
    : TSerialDevice(device_config, port), Config(device_config) {}

void TEMDevice::EnsureSlaveConnected(uint8_t slave, bool force)
{
    if (!force && ConnectedSlaves.find(slave) != ConnectedSlaves.end())
        return;

    ConnectedSlaves.erase(slave);
    Port()->SkipNoise();
    if (!ConnectionSetup(slave))
        throw TSerialDeviceTransientErrorException("failed to establish meter connection");

    ConnectedSlaves.insert(slave);
}

void TEMDevice::EnsureSlaveConnected32(uint32_t slave, bool force)
{
    if (!force && ConnectedSlaves.find(slave) != ConnectedSlaves.end())
        return;

    ConnectedSlaves.erase(slave);
    Port()->SkipNoise();
    if (!ConnectionSetup32(slave))
        throw TSerialDeviceTransientErrorException("failed to establish meter connection");

    ConnectedSlaves.insert(slave);
}

void TEMDevice::WriteCommand(uint8_t slave, uint8_t cmd, uint8_t* payload, int len)
{
    uint8_t buf[MAX_LEN], * p = buf;
    *p++ = slave;
    *p++ = cmd;
    while (len--) {
        *p++ = *payload++;
    }
    WriteCommandImpl(buf, len + 2);
}

void TEMDevice::WriteCommand32(uint32_t slave, uint8_t cmd, uint8_t* payload, int len)
{
    uint8_t buf[MAX_LEN], * p = buf;
    *p++ = static_cast<uint8_t>(slave & 0xff);
    *p++ = static_cast<uint8_t>((slave >> 8) & 0xff);
    *p++ = static_cast<uint8_t>((slave >> 16) & 0xff);
    *p++ = static_cast<uint8_t>((slave >> 24) & 0xff);
    while (len--) {
        *p++ = *payload++;
    }
    WriteCommandImpl(buf, len + 6);
}

void TEMDevice::WriteCommandImpl(uint8_t* payload, int len)
{
    if (len + 2 > MAX_LEN) {
        throw TSerialDeviceException("outgoing command too long");
    }
    auto crc = CRC16::CalculateCRC16(payload, static_cast<uint16_t>(len));
    payload[len] = static_cast<uint8_t>(crc >> 8);
    payload[len + 1] = static_cast<uint8_t>(crc & 0xff);
    Port()->WriteBytes(payload, len + 2);
}

bool TEMDevice::ReadResponse(uint8_t slave, int expectedByte1, uint8_t* payload, int len,
                               TAbstractSerialPort::TFrameCompletePred frame_complete)
{
    uint8_t buf[MAX_LEN], *p = buf;
    int nread = ReadPayload(buf, frame_complete);

    if (*p++ != slave)
        throw TSerialDeviceTransientErrorException("invalid slave id");

    return PostprocessPalyload(p, nread - 1, expectedByte1, payload, len);
}

bool TEMDevice::ReadResponse32(uint32_t slave, int expectedByte1, uint8_t* payload, int len,
                               TAbstractSerialPort::TFrameCompletePred frame_complete)
{
    uint8_t buf[MAX_LEN], *p = payload;
    int nread = ReadPayload(payload, frame_complete);

    uint32_t actualSlave = static_cast<uint32_t>(*p++);
    actualSlave |= static_cast<uint32_t>((*p++<<8));
    actualSlave |= static_cast<uint32_t>((*p++<<16));
    actualSlave |= static_cast<uint32_t>((*p++<<24));

    if (actualSlave != slave)
        throw TSerialDeviceTransientErrorException("invalid slave id");

    return PostprocessPalyload(p, nread - 4, expectedByte1, payload, len);
}

int TEMDevice::ReadPayload(uint8_t* buf, TAbstractSerialPort::TFrameCompletePred frame_complete)
{
    uint8_t* p = buf;
    int nread = Port()->ReadFrame(buf, MAX_LEN, Config->FrameTimeout, frame_complete);
    if (nread < 4)
        throw TSerialDeviceTransientErrorException("frame too short");

    uint16_t crc = CRC16::CalculateCRC16(buf, static_cast<uint16_t>(nread - 2)),
            crc1 = buf[nread - 2],
            crc2 = buf[nread - 1],
            actualCrc = (crc1 << 8) + crc2;
    if (crc != actualCrc)
        throw TSerialDeviceTransientErrorException("invalid crc");
    return nread;
}

bool TEMDevice::PostprocessPalyload(uint8_t* source, int nread, int expectedByte1, uint8_t* dest, int len)
{
    auto p = source;
    const char* msg;
    ErrorType err = CheckForException(source, nread, &msg);
    if (err == NO_OPEN_SESSION)
        return false;
    else if (err != NO_ERROR)
        throw TSerialDeviceTransientErrorException(msg);

    if (expectedByte1 >= 0 && *p++ != expectedByte1)
        throw TSerialDeviceTransientErrorException("invalid command code in the response");

    int actualPayloadSize = static_cast<int>(nread - (p - source));
    if (len >= 0 && len != actualPayloadSize)
        throw TSerialDeviceTransientErrorException("unexpected frame size");
    else
        len = actualPayloadSize;

    std::memcpy(dest, p, static_cast<size_t>(len));
    return true;
}

void TEMDevice::Talk(uint8_t slave, uint8_t cmd, uint8_t* payload, int payload_len,
                       int expected_byte1, uint8_t* resp_payload, int resp_payload_len,
                       TAbstractSerialPort::TFrameCompletePred frame_complete)
{
    EnsureSlaveConnected(slave);
    WriteCommand(slave, cmd, payload, payload_len);
    try {
        while (!ReadResponse(slave, expected_byte1, resp_payload, resp_payload_len, frame_complete)) {
            EnsureSlaveConnected(slave, true);
            WriteCommand(slave, cmd, payload, payload_len);
        }
    } catch (const TSerialDeviceTransientErrorException& e) {
        Port()->SkipNoise();
        throw;
    }
}

void TEMDevice::WriteRegister(PRegister, uint64_t)
{
    throw TSerialDeviceException("EM protocol: writing to registers not supported");
}

PDeviceConfig TEMDevice::DeviceConfig() const
{
    return Config;
}

void TEMDevice::Talk32(uint32_t slave, uint8_t cmd, uint8_t* payload, int payload_len, int expected_byte1,
                       uint8_t* resp_payload, int resp_payload_len,
                       TAbstractSerialPort::TFrameCompletePred frame_complete)
{
    EnsureSlaveConnected32(slave);
    WriteCommand32(slave, cmd, payload, payload_len);
    try {
        while (!ReadResponse32(slave, expected_byte1, resp_payload, resp_payload_len, frame_complete)) {
            EnsureSlaveConnected32(slave, true);
            WriteCommand32(slave, cmd, payload, payload_len);
        }
    } catch (const TSerialDeviceTransientErrorException& e) {
        Port()->SkipNoise();
        throw;
    }
}
