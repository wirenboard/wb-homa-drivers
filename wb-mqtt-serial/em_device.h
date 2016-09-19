#pragma once

#include <string>
#include <memory>
#include <exception>
#include <unordered_set>
#include <functional>
#include <cstdint>

#include "serial_device.h"

// Common device base for electricity meters
class TEMDevice : public TSerialDevice
{
public:
    static const int DefaultTimeoutMs = 1000;

    TEMDevice(PDeviceConfig device_config, PAbstractSerialPort port);
    virtual void WriteRegister(PRegister reg, uint64_t value) override;

protected:
    enum ErrorType
    {
        NO_ERROR,
        NO_OPEN_SESSION,
        OTHER_ERROR
    };
    virtual bool ConnectionSetup(uint8_t slave) = 0;
    virtual bool ConnectionSetup32(uint32_t slave) = 0;
    virtual ErrorType CheckForException(uint8_t* frame, int len, const char** message) = 0;
    void WriteCommand(uint8_t slave, uint8_t cmd, uint8_t* payload, int len);
    void WriteCommand32(uint32_t slave, uint8_t cmd, uint8_t* payload, int len);
    bool ReadResponse(uint8_t slave, int expectedByte1, uint8_t* buf, int len,
                      TAbstractSerialPort::TFrameCompletePred frame_complete = 0);
    bool ReadResponse32(uint32_t slave, int expectedByte1, uint8_t* payload, int len,
                        TAbstractSerialPort::TFrameCompletePred frame_complete = 0);
    void Talk(uint8_t slave, uint8_t cmd, uint8_t* payload, int payload_len,
              int expected_byte1, uint8_t* resp_payload, int resp_payload_len,
              TAbstractSerialPort::TFrameCompletePred frame_complete = 0);
    void Talk32(uint32_t slave, uint8_t cmd, uint8_t* payload, int payload_len,
              int expected_byte1, uint8_t* resp_payload, int resp_payload_len,
              TAbstractSerialPort::TFrameCompletePred frame_complete = 0);
    const int MAX_LEN = 64;
    PDeviceConfig DeviceConfig() const;

private:
    void EnsureSlaveConnected(uint8_t slave, bool force = false);
    void EnsureSlaveConnected32(uint32_t slave, bool force = false);
    void WriteCommandImpl(uint8_t* payload, int len);
    int ReadPayload(uint8_t* buf, TAbstractSerialPort::TFrameCompletePred frame_complete);
    bool PostprocessPalyload(uint8_t* source, int nread, int expectedByte1, uint8_t* dest, int len);
    std::unordered_set<uint32_t> ConnectedSlaves;
    PDeviceConfig Config;
};

typedef std::shared_ptr<TEMDevice> PEMDevice;
