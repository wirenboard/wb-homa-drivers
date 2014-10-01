#pragma once
#include <memory>
#include <gtest/gtest.h>

#include "testlog.h"
#include "../wb-homa-modbus/modbus_client.h"
#include <modbus/modbus.h> // for constants such as MODBUS_MAX_READ_BITS, etc.

struct TRegisterRange {
    TRegisterRange(int start = 0, int end = 0)
        : Start(start), End(end)
    {
        assert(End >= Start);
        assert(End - Start <= 65536);
    }

    int ValidateIndex(const std::string& name, int index) const
    {
        if (index < Start || index > End) {
            ADD_FAILURE() << name << " index is out of range: " << index <<
                " (must be " << Start << ".." << End << ")";
            return Start;
        }
        return index;
    }

    int Size() const
    {
        return End - Start;
    }

    int Start, End;
};

template<typename T>
class TRegisterSet {
public:
    TRegisterSet(const std::string& name, const TRegisterRange& range)
        : Name(name), Range(range)
    {
        // Allocate zero-filled array of values.
        // It always should contain at least one value
        // so that operator[] can always return some
        // reference, even in the case of erroneous access.
        values = new T[std::max(1, Range.Size())]();
    }

    ~TRegisterSet() {
        delete[] values;
    }

    T& operator[] (int index) {
        return values[Range.ValidateIndex(Name, index) - Range.Start];
    }

    const T& operator[] (int index) const {
        return values[Range.ValidateIndex(Name, index) - Range.Start];
    }
    }
private:
    std::string Name;
    TRegisterRange Range;
    T* values;
};

struct TFakeSlave
{
    TFakeSlave(const TRegisterRange& coil_range = TRegisterRange(),
               const TRegisterRange& discrete_range = TRegisterRange(),
               const TRegisterRange& holding_range = TRegisterRange(),
               const TRegisterRange& input_range = TRegisterRange())
        : Coils("coil", coil_range),
          Discrete("discrete input", discrete_range),
          Holding("holding register", holding_range),
          Input("input register", input_range) {}
    TRegisterSet<uint8_t> Coils;
    TRegisterSet<uint8_t> Discrete;
    TRegisterSet<uint16_t> Holding;
    TRegisterSet<uint16_t> Input;
};

typedef std::shared_ptr<TFakeSlave> PFakeSlave;

class TFakeModbusContext: public TModbusContext
{
public:
    void Connect();
    void Disconnect();
    void SetDebug(bool debug);
    void SetSlave(int slave_addr);
    void ReadCoils(int addr, int nb, uint8_t *dest);
    void WriteCoil(int addr, int value);
    void ReadDisceteInputs(int addr, int nb, uint8_t *dest);
    void ReadHoldingRegisters(int addr, int nb, uint16_t *dest);
    void WriteHoldingRegisters(int addr, int nb, const uint16_t *data);
    void ReadInputRegisters(int addr, int nb, uint16_t *dest);
    void USleep(int usec);

    void ExpectDebug(bool debug)
    {
        ASSERT_EQ(Debug, debug);
    }

    PFakeSlave GetSlave(int slave_addr);
    PFakeSlave AddSlave(int slave_addr, PFakeSlave slave);

private:
    friend class TFakeModbusConnector;
    TFakeModbusContext(TLoggedFixture& fixture): Fixture(fixture) {}

    TLoggedFixture& Fixture;
    bool Connected = false;
    bool Debug = false;
    std::map<int, PFakeSlave> Slaves;
    PFakeSlave CurrentSlave;
};

typedef std::shared_ptr<TFakeModbusContext> PFakeModbusContext;

class TFakeModbusConnector: public TModbusConnector
{
public:
    TFakeModbusConnector(const TModbusConnectionSettings& expected_settings,
                         TLoggedFixture& fixture)
        : ExpectedSettings(expected_settings), Fixture(fixture) {}
    PModbusContext CreateContext(const TModbusConnectionSettings& settings);
    PFakeModbusContext GetContext() const;
    PFakeSlave GetSlave(int slave_addr);
    PFakeSlave AddSlave(int slave_addr, PFakeSlave slave);
    PFakeSlave AddSlave(int slave_addr,
                        const TRegisterRange& coil_range = TRegisterRange(),
                        const TRegisterRange& discrete_range = TRegisterRange(),
                        const TRegisterRange& holding_range = TRegisterRange(),
                        const TRegisterRange& input_range = TRegisterRange());
private:
    TModbusConnectionSettings ExpectedSettings;
    TLoggedFixture& Fixture;
    PFakeModbusContext Context;
};

typedef std::shared_ptr<TFakeModbusConnector> PFakeModbusConnector;
